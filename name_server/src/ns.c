#include "../inc/ns.h"
#include "../inc/ip.h"
#include "assert.h"
#include <pthread.h>
#include <string.h>

#define BUFFER_SIZE 1024
userdatabase users;
int ns_port, client_port; // ns_port is for ss and ns connection, client_port is for client and ss connection
char ss_ip[40];
Hashmap *hash;

void* Client_listener_thread (void *arg);
void* Storage_listener_thread (void *arg);
void* Handle_storage_server(void* arg);

typedef struct{
    int client_socket;
    int storage_socket;
    char client_ip[INET_ADDRSTRLEN];
} client_args_t;
void Unpack(char* buffer, uint32_t* flag, char** cmd_string) {
    char *ptr = buffer;
    
    uint32_t flag_net;
    memcpy(&flag_net, ptr, sizeof(uint32_t));
    *flag = ntohl(flag_net); // Convert back from Network Order
    ptr += sizeof(uint32_t);
    
    *cmd_string = ptr; 
}
int Pack(Packet* pkt , char * buff){
    memset(buff, 0 ,BUFFER_SIZE);
    char *ptr = buff;

    uint32_t flag = htonl(pkt->REQ_FLAG);
    memcpy(ptr,&flag,sizeof(uint32_t));

    ptr += sizeof(uint32_t);

    int cmd_len = strlen(pkt->req_cmd)+1;
    memcpy(ptr,pkt->req_cmd,cmd_len);
    
    ptr += cmd_len;
    return ptr - buff;
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
        memset(buffer, 0, sizeof(buffer)); // Clear buffer for new packet

        // Wait for a packet from THIS client
        int bytes = recv(new_socket, buffer, sizeof(buffer) - 1, 0);

        // Check if client disconnected or error
        if (bytes <= 0) {
            if (bytes == 0) {
                printf("[Thread %ld] Client %s disconnected.\n", pthread_self(), client_ip);
                removeusername(username_of_client,&users);
            } else {
                perror("[Thread] recv failed");
            }
            break; // Exit the loop
        }

        uint32_t flag = -1;
        char * cmd_string;
        Unpack(buffer, &flag, &cmd_string);

        printf("\n[Thread %ld] Client %s Flag: %u, Cmd: %s\n", pthread_self(), client_ip, flag, cmd_string);
        if(flag == USER_REG){ 
            //1) need to find if the username is already present if yes make the active flag 1 if its already 1 error
            //2) If we username is not there then add the username and when the client disconnects dont forget to make the active to 0
            int ret = reg_user(cmd_string,&users);
            uint32_t flag;
            if(ret == 0){
                flag = Success;
                strcpy(username_of_client,cmd_string);
                printf(GREEN"Client with username: %s\n Successfully connected\n"NORMAL,cmd_string);
            }
            else if(ret == -1)
                flag = USER_ACTIVE_ALR;
            else
                flag = NO_USER_SLOTS;
            Packet pkt;
            pkt.REQ_FLAG = flag;
            int bytes_to_send = Pack(&pkt,buffer);
            if(send(new_socket,buffer,bytes_to_send,0)<0)
                printf("Error\n");
        }
        else if(flag == LIST){
            //no need to send the packet to storage server
            int indx = 0;
            int num_of_users = users.num_of_users;
            Packet pkt;
            for(int i=0;i<num_of_users;i++){
                pkt.REQ_FLAG = VIEW_DATA;
                char message[1024];
                char user_state[30];
                strcpy(user_state, (users.username_arr[i].active == 0) ? "in-active" : "active");
                snprintf(message, sizeof(message), "%.1000s %.20s\n", users.username_arr[i].username, user_state);
                strcpy(pkt.req_cmd,message);
                int bytes_to_send = Pack(&pkt,buffer);
                if(send(new_socket,buffer,bytes_to_send,0)<0){
                    printf("Error in sending the file\n");
                }
                usleep(10000);
            }
            pkt.REQ_FLAG = VIEW_END;
            int bytes_to_send = Pack(&pkt,buffer);
            if(send(new_socket,buffer,bytes_to_send,0)<0){
                printf("Error in sending the file\n");
            }
            printf("\nVIEW Packets Sent Successfully\n");

        }
        else if(flag == READ_REQ_NS){
            //no need to send the packet to the storage server we just need to send the ip and port of the storage to the client
            Packet pkt;
            //if file found
            if(hash == NULL) {
                pkt.REQ_FLAG = FILE_DOESNT_EXIST;
                printf("Hashmap not intitalized!\n");
            }
            else{
            filelocation loc;
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            print_details(filename,hash);
            if (get_file_location(hash,filename, &loc)) {
                if(can_read(hash,filename,username_of_client)==-1)
                    pkt.REQ_FLAG = FILE_DOESNT_EXIST;
                else{
                    pkt.REQ_FLAG = SS_IP_PORT;
                    sprintf(pkt.req_cmd,"%s %d",loc.ip,loc.ss_port);           
                }
            }
            //if file not found send the FILE_DOESNT_EXIST flag
            else {
                pkt.REQ_FLAG = FILE_DOESNT_EXIST;
            }
            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt,send_buff);
            send(new_socket,send_buff,bytes_to_send,0);
            printf("[NS] READ_REQ received , sent the ss_ip and port\n");
            }
        }
        else if(flag == CREATE_REQ){
            //we need to send a packet to the storage server so that it can create the files in that storege server
            //we need to add that file in our ns database where we are storing the files present in a storage server
            //need to send the same buffer to the storage server with the ss_ip and ns_port
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            printf("Sending the packet to the ss\n");
            int a = send_to_SS(buffer,ss_ip,ns_port,bytes);
            printf("Sending the ack to the client\n");
            Packet pkt;
            pkt.REQ_FLAG = a==0 ? Success : FILE_ALREADY_EXISTS;
            int bytes_to_send = Pack(&pkt,buffer);
            if(send(new_socket,buffer,bytes_to_send,0)<0){
                printf("send the ack to the client is failed\n");
                continue;
            }
            printf("Sent Successfully\n");
            if(!a){
               if(add_file(hash,filename,ss_ip,client_port,username_of_client)==-1){
                    printf("Error in storing the file in the Hashmap\n");
                }
               printf("Printing the hashmap\n");
               print(hash);
            }

        }
        else if(flag == INFO){
            //no need to send to the Storage server
            //we will anyway store the data about the files and the users in the ns so just print int
            strcpy(msg,"ACK for the INFO");

        }
        else if(flag == DELETE){
            //need to send the packet to ss and then update it in the ns database
            printf("Sending the packet to the ss\n");
            int a = send_to_SS(buffer,ss_ip,ns_port,bytes);
            printf("Sending the ack to the client\n");
            Packet pkt;
            pkt.REQ_FLAG = a==0 ? Success : FILE_DOESNT_EXIST;
            int bytes_to_send = Pack(&pkt,buffer);
            if(send(new_socket,buffer,bytes_to_send,0)<0){
                printf("send the ack to the client is failed\n");
                continue;
            }
            if(!a){
               if(delete_file(hash,cmd_string)==-1){
                    printf("Error in removing the file in the Hashmap\n");
                }
            }

        }
        else if(flag == STREAM){
            //no need to send the packet to the storage server we just need to send the ip and port of the storage to the client
            Packet pkt;
            //if file found
            filelocation loc;
            if(get_file_location(hash, cmd_string, &loc)) {
                if(can_read(hash,cmd_string,username_of_client)==-1)
                    pkt.REQ_FLAG = FILE_DOESNT_EXIST;
                else{
                    pkt.REQ_FLAG = SS_IP_PORT;
                    sprintf(pkt.req_cmd,"%s %d",loc.ip,loc.ss_port);           
                }
            }
            //if file not found send the FILE_DOESNT_EXIST flag
            else {
                pkt.REQ_FLAG = FILE_DOESNT_EXIST;
            }
            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt,send_buff);
            send(new_socket,send_buff,bytes_to_send,0);
            printf("[NS] STREAM received , sent the ss_ip and port\n");
        }
        else if(flag == ADDACCESS_r){
            //change in the database directly and send the ack back
            char filename[MAX_FILE_NAME_SIZE];
            char username[MAX_WORD_SIZE];
            sscanf(cmd_string,"ADDACCESS -R %s %s\n",filename,username);
            //need to add a cond to check is he the owner of the file
            int a = add_r_access(hash,filename,username);
            if(a==1)
                add_file_to_user(filename,username,&users);
            Packet pkt;
            pkt.REQ_FLAG = a == 1 ? Success : Fail;
            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt,send_buff);
            if(send(new_socket,send_buff,bytes_to_send,0) < 0){
                printf("Failed to send\n");
            }

        }
        else if(flag == ADDACCESS_w){
            //same as prev
            char filename[MAX_FILE_NAME_SIZE];
            char username[MAX_WORD_SIZE];
            sscanf(cmd_string,"ADDACCESS -W %s %s\n",filename,username);

            //need to add a cond to check is he the owner of the file
            int a = add_w_access(hash,filename,username);
            Packet pkt;
            pkt.REQ_FLAG = a == 1 ? Success : Fail;
            if(a==1)
                add_file_to_user(filename,username,&users);
            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt,send_buff);
            if(send(new_socket,send_buff,bytes_to_send,0) < 0){
                printf("Failed to send\n");
            }

        }
        else if(flag == REMACCESS){
            char filename[MAX_FILE_NAME_SIZE];
            char username[MAX_WORD_SIZE];
            sscanf(cmd_string,"REMACCESS %s %s",filename,username);

            //need to add a cond to check is he the owner of the file
            int a = rem_access(hash,filename,username);
            Packet pkt;
            if(a==1)
                delete_file_from_user(filename,username,&users);
            pkt.REQ_FLAG = a == 1 ? Success : Fail;
            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt,send_buff);
            if(send(new_socket,send_buff,bytes_to_send,0) < 0){
                printf("Failed to send\n");
            }
        }
        else if(flag == EXEC){
            //need to send the packet to the ss and the ss will send line by line and will be executed in the ns
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
        else if (flag == REG_SS) { 
            // need to store all the ips and ports of storage servers in a hash map
            strcpy(msg,"ACK for the REG_SS");

        }


    }

    close(new_socket);
    pthread_exit(NULL);
}

int main() {
    hash = create_hashmap(1024);
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

    char buffer[BUFFER_SIZE];
    char *msg = "ACK - SS Registered\n"; 

    memset(buffer, 0, sizeof(buffer));
    // --- FIX: Receive from the new_ss_socket ---
    int bytes = recv(new_ss_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
        if (bytes == 0) {
            printf("[Thread %ld] Storage Server %s disconnected before registering.\n", pthread_self(), ss_ip_str);
        } else {
            perror("[Thread] recv from SS failed");
        }
    } else {
        // We received the one packet
        uint32_t flag = -1;
        char *cmd_string;
        Unpack(buffer, &flag, &cmd_string);

        printf("[Thread %ld] SS %s Flag: %u, Cmd: %s", pthread_self(), ss_ip_str, flag, cmd_string);

        if (flag == REG_SS) {
            // --- DANGER: RACE CONDITION ---
            // You MUST use a mutex to protect these global variables
            // pthread_mutex_lock(&ss_list_mutex); 
            sscanf(cmd_string, "REGISTER %s %d %d", ss_ip, &ns_port, &client_port);
            // pthread_mutex_unlock(&ss_list_mutex);

            printf("--- SS REGISTRATION ---\n");
            printf("IP: %s, NS_PORT: %d, CLIENT_PORT: %d\n", ss_ip, ns_port, client_port);

            // Send ACK
            send(new_ss_socket, msg, strlen(msg), 0);
        } else {
            printf("[Thread %ld] SS %s sent wrong flag %u.\n", pthread_self(), ss_ip_str, flag);
        }
    }

    // This thread's job is done, close the socket.
    printf("[Thread %ld] SS registration complete. Closing connection to %s.\n", pthread_self(), ss_ip_str);
    close(new_ss_socket);
    pthread_exit(NULL);
}
