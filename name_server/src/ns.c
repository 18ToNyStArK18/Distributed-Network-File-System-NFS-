#include "../inc/ns.h"
#include "../inc/ip.h"
#include "../inc/heap.h"
#include "assert.h"
#include <pthread.h>
#include <string.h>

#define BUFFER_SIZE 1024
userdatabase users;
int ns_port, client_port; // ns_port is for ss and ns connection, client_port is for client and ss connection
char ss_ip[40];
Hashmap *hash;
MinHeap *minheap;

void* Client_listener_thread (void *arg);
void* Storage_listener_thread (void *arg);
void* Handle_storage_server(void* arg);

typedef struct{
    int client_socket;
    int storage_socket;
    char client_ip[INET_ADDRSTRLEN];
} client_args_t;

void Unpack(char* buffer, uint32_t* flag, char** cmd_string) {
    char* ptr = buffer;

    ptr += sizeof(uint32_t); // skip length

    uint32_t flag_net;
    memcpy(&flag_net, ptr, sizeof(uint32_t));
    *flag = ntohl(flag_net);
    ptr += sizeof(uint32_t);

    *cmd_string = ptr;
}

int Pack(Packet* pkt, char* buff) {
    memset(buff, 0, BUFFER_SIZE);

    uint32_t flag_net = htonl(pkt->REQ_FLAG);
    uint32_t msg_len = strlen(pkt->req_cmd) + 1;       // include null terminator
    uint32_t total_len = sizeof(uint32_t) + msg_len;   // flag + string
    uint32_t total_len_net = htonl(total_len);

    char* ptr = buff;

    memcpy(ptr, &total_len_net, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, &flag_net, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, pkt->req_cmd, msg_len);
    ptr += msg_len;

    return ptr - buff;
}

int recv_all(int sock, void* buffer, int length) {
    int total = 0, n;
    while (total < length) {
        n = recv(sock, (char*)buffer + total, length - total, 0);
        if (n <= 0) return n; // error or disconnect
        total += n;
    }
    return total;
}

int send_all(int sock, const void* buffer, int length) {
    int total = 0;
    int n;

    while (total < length) {
        n = send(sock, (const char*)buffer + total, length - total, 0);
        if (n <= 0) {
            return n; // error or disconnected
        }
        total += n;
    }

    return total;
}

void* Handle_client(void* arg){
    client_args_t* args = (client_args_t*)arg;
    int new_socket = args->client_socket;
    char client_ip[INET_ADDRSTRLEN];
    strcpy(client_ip, args->client_ip); // Make a local copy
    char msg[MAX_WORDS_IN_INP * MAX_WORD_SIZE];
    free(args);
    char username_of_client[1024];

    char buffer[1024] = {0};

    while (1) {
        uint32_t net_packet_len;
    
        // --- Step 1: Read packet length (4 bytes) ---
        if (recv_all(new_socket, &net_packet_len, sizeof(uint32_t)) <= 0) {
            printf("[Thread %ld] Client %s disconnected.\n", pthread_self(), client_ip);
            removeusername(username_of_client,&users);
            break;
        }

        uint32_t packet_len = ntohl(net_packet_len);

        // Sanity check
        if (packet_len > BUFFER_SIZE) {
            printf(RED"ERROR: Packet too large (%u bytes)! Disconnecting client.\n"NORMAL, packet_len);
            break;
        }

        char buffer[BUFFER_SIZE];

        // --- Step 2: Read the rest of the packet ---
        if (recv_all(new_socket, buffer, packet_len) <= 0) {
            printf("[Thread %ld] Client %s disconnected.\n", pthread_self(), client_ip);
            removeusername(username_of_client,&users);
            break;
        }

        // --- Step 3: Unpack ---
        uint32_t flag;
        char *cmd_string;
        Unpack(buffer, &flag, &cmd_string);

        printf("\n[Thread %ld] Client %s Flag: %u, Cmd: %s\n", pthread_self(), client_ip, flag, cmd_string);
        if (flag == USER_REG) {
            int ret = reg_user(cmd_string, &users);
            uint32_t result_flag;

            if (ret == 0) {
                result_flag = Success;
                strcpy(username_of_client, cmd_string);
                printf(GREEN "Client with username: %s successfully connected.\n" NORMAL, cmd_string);
            }
            else if (ret == -1) {
                result_flag = USER_ACTIVE_ALR;
            }
            else {
                result_flag = NO_USER_SLOTS;
            }
        
            Packet pkt;
            memset(&pkt, 0, sizeof(pkt));          
            pkt.REQ_FLAG = result_flag;
            strcpy(pkt.req_cmd, cmd_string);       
        
            int bytes_to_send = Pack(&pkt, buffer);

            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send USER_REG length.\n" NORMAL);
            }
        
            if (send_all(new_socket, buffer, bytes_to_send) <= 0) {
                printf(RED "[NS] ERROR: Failed to send USER_REG response.\n" NORMAL);
            }
        }
        else if(flag == VIEW_N){
            print_view(username_of_client,&users,hash,0,0,new_socket);    
        }
        else if(flag == VIEW_A){
            print_view(username_of_client,&users,hash,1,0,new_socket);
        }
        else if(flag == VIEW_AL){
            print_view(username_of_client,&users,hash,1,1,new_socket);
        }
        else if (flag == VIEW_L) {
            print_view(username_of_client,&users,hash,0,1,new_socket);
        }
        else if (flag == LIST) {
            int num_of_users = users.num_of_users;
            Packet pkt;  

            for (int i = 0; i < num_of_users; i++) {

                memset(&pkt, 0, sizeof(pkt));       
                pkt.REQ_FLAG = LIST_DATA;

                char user_state[16];
                strcpy(user_state, users.username_arr[i].active ? "active" : "inactive");

                snprintf(pkt.req_cmd, sizeof(pkt.req_cmd), "%s %s\n",
                        users.username_arr[i].username, user_state);

                int bytes_to_send = Pack(&pkt, buffer);

                uint32_t net_len = htonl(bytes_to_send);
                if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                    printf(RED "[NS] ERROR: Failed to send LIST length.\n" NORMAL);
                }

                if (send_all(new_socket, buffer, bytes_to_send) <= 0) {
                    printf(RED "LIST: send failed\n" NORMAL);
                    break;
                }
            }

            memset(&pkt, 0, sizeof(pkt));
            pkt.REQ_FLAG = LIST_END;
            strcpy(pkt.req_cmd, "END");

            int bytes_to_send = Pack(&pkt, buffer);
            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send LIST packets length.\n" NORMAL);
            }
            send_all(new_socket, buffer, bytes_to_send);

            printf(GREEN "\n[NS] LIST packets sent successfully.\n" NORMAL);
        }
        else if (flag == READ_REQ_NS || flag == STREAM) {
            Packet pkt;
            memset(&pkt, 0, sizeof(pkt));   

            if (hash == NULL) {
                pkt.REQ_FLAG = FILE_DOESNT_EXIST;
                printf("Hashmap not initialized!\n");
            }
            else {
                filelocation loc;
                char filename[MAX_FILE_NAME_SIZE];
                strcpy(filename, cmd_string);

                print_details(filename, hash);

                if (get_file_location(hash, filename, &loc)) {

                    if (can_read(hash, filename, username_of_client) == -1) {
                        pkt.REQ_FLAG = NO_access;
                        printf("NO ACCESS\n");
                    }
                    else {
                        pkt.REQ_FLAG = SS_IP_PORT;
                        snprintf(pkt.req_cmd, sizeof(pkt.req_cmd), "%s %d", loc.ip, loc.ss_port);
                    }
                } 
                else {
                    printf("NO FILE\n");
                    pkt.REQ_FLAG = FILE_DOESNT_EXIST;
                }
            }

            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt, send_buff);
            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send READ length.\n" NORMAL);
            }

            if (send_all(new_socket, send_buff, bytes_to_send) <= 0) {
                printf(RED "[NS] ERROR sending SS_IP_PORT response\n" NORMAL);
            } else {
                printf(GREEN "[NS] Sent SS IP + Port info successfully\n" NORMAL);
            }
        }
        else if(flag == CREATE_REQ){
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename, cmd_string);

            printf("[NS] Forwarding CREATE to SS...\n");

            Packet forward_pkt;
            memset(&forward_pkt, 0, sizeof(forward_pkt));
            forward_pkt.REQ_FLAG = CREATE_REQ;
            strcpy(forward_pkt.req_cmd, filename);

            char forward_buff[BUFFER_SIZE];
            int forward_size = Pack(&forward_pkt, forward_buff);

            Node send;
            heap_peek(minheap, &send);

            int a = send_to_SS(forward_buff, send.ss_ip, send.ns_port, forward_size);

            printf("[NS] Sending ACK back to client...\n");

            Packet reply;
            memset(&reply, 0, sizeof(reply));
            reply.REQ_FLAG = (a == 0) ? Success : FILE_ALREADY_EXISTS;

            char reply_buff[BUFFER_SIZE];
            int reply_size = Pack(&reply, reply_buff);
            uint32_t net_len = htonl(reply_size);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send CREATE length.\n" NORMAL);
            }

            send_all(new_socket, reply_buff, reply_size);

            printf(GREEN "[NS] ACK sent successfully.\n" NORMAL);

            if (a == 0) {
                if(add_file_to_user(filename,username_of_client,&users)==-1)
                        printf("ERROR\n");
                if(add_file(hash, filename, ss_ip, client_port, username_of_client,ns_port) == -1){
                    printf(RED "[NS] ERROR storing file in hashmap\n" NORMAL);
                } else {
                    print(hash);
                    send.num_files++;
                    heap_fix(minheap, 0);
                }
            }
        }
        else if(flag == INFO){
            //no need to send to the Storage server
            //we will anyway store the data about the files and the users in the ns so just print int
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            print_info(hash,filename,new_socket);
        }
        else if (flag == DELETE) {
            printf("[NS] Forwarding DELETE to SS...\n");

            Packet forward_pkt;
            memset(&forward_pkt, 0, sizeof(forward_pkt));
            forward_pkt.REQ_FLAG = DELETE;
            strcpy(forward_pkt.req_cmd, cmd_string);   // filename

            char forward_buff[BUFFER_SIZE];
            int forward_size = Pack(&forward_pkt, forward_buff);

            char delete_ip[INET_ADDRSTRLEN];
            int delete_port;
            find_ip_by_filename(cmd_string, hash, delete_ip, &delete_port);

            int a = send_to_SS(forward_buff, delete_ip, delete_port, forward_size);

            printf("[NS] Sending ACK back to client...\n");

            Packet reply_pkt;
            memset(&reply_pkt, 0, sizeof(reply_pkt));
            reply_pkt.REQ_FLAG = (a == 0) ? Success : FILE_DOESNT_EXIST;

            char reply_buff[BUFFER_SIZE];
            int reply_size = Pack(&reply_pkt, reply_buff);

            uint32_t net_len = htonl(reply_size);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send DELETE length.\n" NORMAL);
            }

            send_all(new_socket, reply_buff, reply_size);

            printf(GREEN "[NS] ACK sent successfully.\n" NORMAL);

            if (a == 0) {
                if (delete_file(hash, cmd_string) == -1) {
                    printf(RED "[NS] ERROR removing file from hashmap\n" NORMAL);
                }
                else{
                    for (int i = 0; i < minheap->size; i++) {
                        if (strcmp(minheap->arr[i].ss_ip, delete_ip) == 0 && minheap->arr[i].ns_port == delete_port) {
                            minheap->arr[i].num_files--;
                            heap_fix(minheap, i);
                            break;
                        }
                    }
                }
            }
        }
        else if(flag == ADDACCESS_r){
            char filename[MAX_FILE_NAME_SIZE];
            char username[MAX_WORD_SIZE];
            sscanf(cmd_string, "ADDACCESS -R %s %s", filename, username);
            int b = is_owner(username_of_client,filename,hash);
            if(b != 1){
                printf("NOT OWNER\n");
            }
            int a;
            if(b==1){
                a = add_r_access(hash, filename, username);
                if (a == 1)
                    add_file_to_user(filename, username, &users);
            }
            Packet pkt;
            memset(&pkt, 0, sizeof(pkt));   
            pkt.REQ_FLAG = (a == 1 && b == 1) ? Success : Fail;
            if(b==-1)
                pkt.REQ_FLAG = Not_owner;
            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt, send_buff);

            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send ADDACCESS_r length.\n" NORMAL);
            }

            send_all(new_socket, send_buff, bytes_to_send);   
        }
        else if(flag == ADDACCESS_w){

            char filename[MAX_FILE_NAME_SIZE];
            char username[MAX_WORD_SIZE];
            sscanf(cmd_string, "ADDACCESS -W %s %s", filename, username);
            int b = is_owner(username_of_client,filename,hash);
            if(b != 1){
                printf("NOT OWNER\n");
            }
            int a;
            if(b==1){
                a = add_w_access(hash, filename, username);
                if (a == 1)
                    add_file_to_user(filename, username, &users);
            }
            Packet pkt;
            memset(&pkt, 0, sizeof(pkt));     
            pkt.REQ_FLAG = ((a == 1 && b == 1) ? Success : Fail);
            if(b==-1)
                pkt.REQ_FLAG = Not_owner;

            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt, send_buff);

            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send ADDACCESS_w length.\n" NORMAL);
            }

            send_all(new_socket, send_buff, bytes_to_send);  
        }
        else if(flag == REMACCESS){
            char filename[MAX_FILE_NAME_SIZE];
            char username[MAX_WORD_SIZE];
            sscanf(cmd_string, "REMACCESS %s %s", filename, username);
            int b = is_owner(username_of_client,filename,hash);
            if(b != 1){
                printf("NOT OWNER\n");
            }
            int a;
            if(b==1){
                a = rem_access(hash, filename, username);
                if (a == 1)
                    delete_file_from_user(filename, username, &users);
            }
            Packet pkt;
            memset(&pkt, 0, sizeof(pkt));     
            pkt.REQ_FLAG = ((a == 1  && b == 1) ? Success : Fail);
            if(b==-1)
                pkt.REQ_FLAG = Not_owner;

            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt, send_buff);

            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send REMACCESS length.\n" NORMAL);
            }

            send_all(new_socket, send_buff, bytes_to_send);  
        }
        else if(flag == EXEC){
            //need to send the packet to the ss and the ss will send line by line and will be executed in the ns
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            filelocation loc;
            if(get_file_location(hash,filename,&loc)){
                if (can_read(hash, filename, username_of_client) == -1) {
                    Packet pkt;
                    pkt.REQ_FLAG = NO_access;
                    printf("NO ACCESS\n");
                    char send_buff[1024];
                    int bytes_to_send = Pack(&pkt,send_buff);
                    uint32_t net_len = htonl(bytes_to_send);
                    send_all(new_socket,&net_len,sizeof(net_len));
                    send_all(new_socket,send_buff,bytes_to_send);
                    continue;
                }

                execute_file(filename,loc.ip,loc.ns_ss_port,new_socket); 
            }
            else{
                Packet pkt;
                pkt.REQ_FLAG = FILE_DOESNT_EXIST;
                char send_buff[BUFFER_SIZE];
                int bytes_to_send = Pack(&pkt, send_buff);

                uint32_t net_len = htonl(bytes_to_send);
                if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                    printf(RED "[NS] ERROR: Failed to send REMACCESS length.\n" NORMAL);
                }

                send_all(new_socket, send_buff, bytes_to_send);
            }
            strcpy(msg,"ACK for the EXEC");

        }
        else if(flag == UNDO){
            // need to store somewhere which line is changed by the user and then change it back
            strcpy(msg,"ACK for the UNDO");

        }
        else if(flag == WRITE_REQ){
            // need to send the ip and the port of the ss to the client
            strcpy(msg,"ACK for the WRITE_REQ");

        }

    }

    close(new_socket);
    pthread_exit(NULL);
}

int main() {
    hash = create_hashmap(1024);
    minheap = heap_init();
    if (!minheap) {
        perror("heap init failed.");
    }
    int server_fd;
    struct sockaddr_in address;
    users.num_of_users = 0;
    for(int i=0;i<MAX_USERS;i++){
        users.username_arr[i].active = 0;
        users.username_arr[i].files = NULL;
    }
    //setting up the socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //this will make tell the os that i can use the same ip as soon as a program stops using this without any delay
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    //just initializations
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(NS_PORT);
    if (inet_pton(AF_INET, NS_IP, &address.sin_addr) <= 0) {
        perror("Invalid IP address in NS_IP");
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int storage_fd;
    struct sockaddr_in ss_addr;

    if ((storage_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&ss_addr, 0, sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(NS_PORT_SS);

    if (inet_pton(AF_INET, NS_IP, &ss_addr.sin_addr) <= 0) {
        perror("Invalid IP address in NS_IP");
        exit(EXIT_FAILURE);
    }
    if (bind(storage_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        perror("bind failed");
        close(storage_fd);
        exit(EXIT_FAILURE);
    }


    if (listen(storage_fd, 20) < 0) {
        perror("listen failed");
        close(storage_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 20) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening for clients on %s:%d...\n", NS_IP, NS_PORT);
    printf("Server listening for storage servers on %s:%d...\n", NS_IP, NS_PORT_SS);

    pthread_t client_thread, storage_thread;

    pthread_create(&client_thread, NULL, Client_listener_thread, &server_fd);
    pthread_create(&storage_thread, NULL, Storage_listener_thread, &storage_fd);

    pthread_detach(client_thread);
    pthread_detach(storage_thread);

    while(1) sleep(1000);

    return 0;
}

void* Client_listener_thread (void *arg) {

    int server_fd = *(int*)arg;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while(1){
        memset(&client_addr, 0, sizeof(client_addr));
        int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (new_socket < 0) {
            perror("accept failed");
            continue; 
        }
        client_args_t* args = (client_args_t *)malloc(sizeof(client_args_t));
        if(args == NULL){
            printf("Malloc Failed\n");
            continue;
        }

        args->client_socket = new_socket;
        inet_ntop(AF_INET,&client_addr.sin_addr , args->client_ip , INET_ADDRSTRLEN);
        //create a thread for each client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, Handle_client, (void*)args) != 0) {
            perror("[Main] pthread_create failed");
            free(args); // Clean up
            close(new_socket);
            continue;
        }

        pthread_detach(thread_id);
    }
}
void* Storage_listener_thread (void *arg) {
    int ss_fd = *(int *)arg;
    struct sockaddr_in ss_addr;
    socklen_t ss_len = sizeof(ss_addr);

    // This loop just accepts new SS connections
    while(1) {
        int new_ss_socket = accept(ss_fd,(struct sockaddr *)&ss_addr,&ss_len);
        if (new_ss_socket < 0) {
            perror("SS Accept Failed");
            continue;
        }

        client_args_t* args = (client_args_t *)malloc(sizeof(client_args_t));
        if(args == NULL){
            printf("Malloc Failed\n");
            close(new_ss_socket);
            continue;
        }

        args->client_socket = new_ss_socket;
        inet_ntop(AF_INET, &ss_addr.sin_addr, args->client_ip, INET_ADDRSTRLEN);

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, Handle_storage_server, (void*)args) != 0) {
            perror("[Main] pthread_create for SS failed");
            free(args);
            close(new_ss_socket);
            continue;
        }

        pthread_detach(thread_id);
    }
}


void* Handle_storage_server(void* arg) {
    client_args_t* args = (client_args_t*)arg;
    int new_ss_socket = args->client_socket;
    char ss_ip_str[INET_ADDRSTRLEN];
    strcpy(ss_ip_str, args->client_ip);
    free(args);

    printf("[Thread %ld] New Storage Server connected from %s.\n", pthread_self(), ss_ip_str);

    while (1) {

        // --- Step 1: read packet length ---
        uint32_t net_packet_len;
        if (recv_all(new_ss_socket, &net_packet_len, sizeof(uint32_t)) <= 0) {
            printf("[Thread %ld] Storage Server %s disconnected.\n", pthread_self(), ss_ip_str);
            break;
        }

        uint32_t packet_len = ntohl(net_packet_len);
        if (packet_len > BUFFER_SIZE) {
            printf(RED "[NS] ERROR: Storage Server packet too large (%u bytes)\n" NORMAL, packet_len);
            break;
        }

        char buffer[BUFFER_SIZE];

        // --- Step 2: read full packet ---
        if (recv_all(new_ss_socket, buffer, packet_len) <= 0) {
            printf("[Thread %ld] Storage Server %s disconnected.\n", pthread_self(), ss_ip_str);
            break;
        }

        // --- Step 3: Unpack packet ---
        uint32_t flag;
        char* cmd_string;
        Unpack(buffer, &flag, &cmd_string);

        printf("[Thread %ld] SS %s Flag: %u, Cmd: %s\n", pthread_self(), ss_ip_str, flag, cmd_string);

        if (flag == REG_SS) {

            // Parse: REGISTER <ip> <ns_port> <client_port>
            sscanf(cmd_string, "REGISTER %s %d %d", ss_ip, &ns_port, &client_port);

            printf("---- Storage Server Registered ----\n");
            printf("IP: %s\n", ss_ip);
            printf("NS_PORT: %d\n", ns_port);
            printf("CLIENT_PORT: %d\n", client_port);

            Node node;
            node_init(&node, ss_ip, client_port, ns_port);
            heap_push(minheap, node);

            // Build ACK response packet
            Packet reply;
            memset(&reply, 0, sizeof(reply));
            reply.REQ_FLAG = Success;
            strcpy(reply.req_cmd, "SS Registered");

            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&reply, send_buff);

            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(new_ss_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[NS] ERROR: Failed to send REG_SS length.\n" NORMAL);
            }

            send_all(new_ss_socket, send_buff, bytes_to_send);
        }
        else {
            printf("[Thread %ld] *** INVALID REG PACKET FROM SS %s ***\n", pthread_self(), ss_ip_str);
        }
    }

    close(new_ss_socket);
    pthread_exit(NULL);
}
