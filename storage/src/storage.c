#include "../../name_server/inc/ip.h"
#include "../inc/storage.h"
#include "../../cmn_inc.h"
#include <pthread.h>
#include <time.h>
#include <assert.h>
void* NS_listener_thread (void *arg);
void* Client_listener_thread (void *arg);
void* Handle_NS (void* arg);
void* Handle_Client (void* arg);
int Pack(Packet* pkt , char * buff) {
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

void Unpack(char* buffer, uint32_t* flag, char** cmd_string) {
    char *ptr = buffer;
    
    uint32_t flag_net;
    memcpy(&flag_net, ptr, sizeof(uint32_t));
    *flag = ntohl(flag_net); // Convert back from Network Order
    ptr += sizeof(uint32_t);
    
    *cmd_string = ptr; 
}
pthread_mutex_t FILES_C_AND_W = PTHREAD_MUTEX_INITIALIZER;
int main() {
    int server_fd, client_fd;
    struct sockaddr_in ns_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    socklen_t ns_len = sizeof(ns_addr);

    /************** 1. Create server socket **************/
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&ns_addr, 0, sizeof(ns_addr));
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_addr.s_addr = INADDR_ANY;   // Any local interface
    ns_addr.sin_port = 0;                   // OS picks a free port

    /************** 2. Bind **************/
    if (bind(server_fd, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /************** 3. Find assigned IP + port **************/
    if (getsockname(server_fd, (struct sockaddr *)&ns_addr, &ns_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }

    int ns_port = ntohs(ns_addr.sin_port);
    printf("[SS] NS socket listening on port %d\n", ns_port);

    listen(server_fd, 20);

    /************** 1. Create client socket **************/
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;   // Any local interface
    client_addr.sin_port = 0;                   // OS picks a free port

    /************** 2. Bind **************/
    if (bind(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    /************** 3. Find assigned IP + port **************/
    if (getsockname(client_fd, (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }

    int client_port = ntohs(client_addr.sin_port);
    printf("[SS] Client socket listening on port %d\n", client_port);

    listen(client_fd, 20);


    /************** 4. Register with Name Server **************/
    int ns_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_sock < 0) {
        perror("NS socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in ns_server_addr;
    ns_server_addr.sin_family = AF_INET;
    ns_server_addr.sin_port = htons(NS_PORT_SS);

    if (inet_pton(AF_INET, NS_IP, &ns_server_addr.sin_addr) <= 0) {
        perror("Invalid NS_IP");
        exit(EXIT_FAILURE);
    }

    if (connect(ns_sock, (struct sockaddr *)&ns_server_addr, sizeof(ns_server_addr)) < 0) {
        perror("Could not connect to Name Server");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in my_addr_for_ns;
    socklen_t my_addr_len = sizeof(my_addr_for_ns);
    if (getsockname(ns_sock, (struct sockaddr *)&my_addr_for_ns, &my_addr_len) < 0) {
        perror("getsockname on ns_sock failed");
        exit(EXIT_FAILURE);
    }
    char my_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &my_addr_for_ns.sin_addr, my_ip, sizeof(my_ip));
    // format: "REGISTER <ip> <ns_port> <client_port>"
    char reg_msg[64];
    printf("Storage Server running on %s:%d\n", my_ip, ns_port);
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER %s %d %d", my_ip, ns_port, client_port);

    Packet pkt;
    memset(&pkt, 0 , sizeof(pkt));
    strncpy(pkt.req_cmd, reg_msg, sizeof(pkt.req_cmd)-1);
    pkt.req_cmd[sizeof(pkt.req_cmd)-1] = '\0';
    pkt.REQ_FLAG = REG_SS;

    char buffer[BUFFER_SIZE];
    int bytes_to_send = Pack(&pkt, buffer);

    send(ns_sock, buffer, bytes_to_send, 0);
    printf("Sent registration to Name Server: %s\n", reg_msg);
    char recv_buff[BUFFER_SIZE];
    memset(recv_buff,0,BUFFER_SIZE);
    if(recv(ns_sock,recv_buff,BUFFER_SIZE,0)< 0){
        printf(RED"Error in recieving packet\n"NORMAL);
        return 0;
    }
    printf("Server says: %s\n",recv_buff);
    close(ns_sock);

    pthread_t ns_thread, client_thread;

    pthread_create(&ns_thread, NULL, NS_listener_thread, &server_fd);
    pthread_detach(ns_thread);

    pthread_create(&client_thread, NULL, Client_listener_thread, &client_fd);
    pthread_detach(client_thread);

    printf("[SS] Threads launched: NS & Client listeners active.\n");

    // main thread can now sleep forever
    while(1) sleep(1000);

    return 0;
}

// pack the struct into a buffer so that i can send the pckt to the name server


void* NS_listener_thread(void *arg) {
    int ns_listen_fd = *(int*)arg;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    while (1) {
        int ns_fd = accept(ns_listen_fd, (struct sockaddr*)&addr, &len);
        if (ns_fd < 0) continue;

        pthread_t tid;
        pthread_create(&tid, NULL, Handle_NS, (void*)(long)ns_fd);
        pthread_detach(tid);
    }
}

void* Client_listener_thread(void *arg) {
    int client_listen_fd = *(int*)arg;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    while (1) {
        int client_fd = accept(client_listen_fd, (struct sockaddr*)&addr, &len);
        if (client_fd < 0) continue;

        client_args_t* args = malloc(sizeof(client_args_t));
        inet_ntop(AF_INET, &addr.sin_addr, args->client_ip, INET_ADDRSTRLEN);
        args->client_socket = client_fd;

        printf("[SS] Client connected from %s:%d\n",
               args->client_ip, ntohs(addr.sin_port));

        pthread_t tid;
        pthread_create(&tid, NULL, Handle_Client, args);
        pthread_detach(tid);
    }
}

void* Handle_NS (void* arg) {
    int ns_fd = (int)(long)arg;
    char buffer[BUFFER_SIZE];
    char *msg = "ACK - Command Received\n";

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int r = recv(ns_fd, buffer, sizeof(buffer), 0);

        if (r <= 0) {
            if (r == 0) { 
                printf("[Thread %ld] Name Server %s disconnected.\n", pthread_self(), NS_IP);
            }
            else { 
                perror("[Thread] recv failed\n");
            }
            break;
        }

        uint32_t flag = -1;
        char* cmd_string;
        Unpack(buffer, &flag, &cmd_string);

        printf("[Thread %ld] Name Server %s Flag: %u, Cmd: %s\n", pthread_self(), NS_IP, flag, cmd_string);
    
        
        if (flag == CREATE_REQ) {
            // create file
            printf("Recived the create cmd req\n");
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            pthread_mutex_lock(&FILES_C_AND_W);
            FILE *fp;
            int send_flag = -1;
            //need to check whether the file with the same name exists already
            fp=fopen(filename,"r");
            if(fp != NULL){
                send_flag = FILE_ALREADY_EXISTS;
                fclose(fp);
                pthread_mutex_unlock(&FILES_C_AND_W);
            }
            else{
                //file doesnt exists so i will create that file
                fp = fopen(filename,"w");
                assert(fp != NULL);
                send_flag = Success;
                pthread_mutex_unlock(&FILES_C_AND_W);
            }
            char temp_buffer[BUFFER_SIZE];
            temp_buffer[0]='\0';
            Packet pkt;
            pkt.REQ_FLAG = send_flag;
            int bytes_to_send = Pack(&pkt,temp_buffer);
            if(send(ns_fd,temp_buffer,bytes_to_send,0) < 0){
                printf(RED"ERROR in sending the ack\n"NORMAL);
            }

        }
        else if (flag == DELETE) {
            // delete file
            printf("Recived the delete cmd req\n");
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            pthread_mutex_lock(&FILES_C_AND_W);
            int send_flag = -1;
            if(remove(filename)==0){
                send_flag = Success;
            }
            else {
                send_flag = FILE_DOESNT_EXIST;
            }
            pthread_mutex_unlock(&FILES_C_AND_W);
            char temp_buffer[BUFFER_SIZE];
            temp_buffer[0]='\0';
            Packet pkt;
            pkt.REQ_FLAG = send_flag;
            int bytes_to_send = Pack(&pkt,temp_buffer);
            if(send(ns_fd,temp_buffer,bytes_to_send,0) < 0){
                printf(RED"ERROR in sending the ack\n"NORMAL);
            }
        }
        else if (flag == EXEC) {
            // send contents of file line by line
        }
    }

    close(ns_fd);
    pthread_exit(NULL);
}

void* Handle_Client (void* arg) {
    client_args_t* args = (client_args_t*)arg;
    int new_socket = args->client_socket;
    char client_ip[INET_ADDRSTRLEN];
    char buffer[BUFFER_SIZE] = {0};
    strcpy(client_ip, args->client_ip);
    char msg[MAX_WORDS_IN_INP * MAX_WORD_SIZE];
    free(args);

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int r = recv(new_socket, buffer, sizeof(buffer), 0);

        if (r <= 0) {
            if (r == 0) { 
                printf("[Thread %ld] Client %s disconnected.\n", pthread_self(), client_ip);
            }
            else { 
                perror("[Thread] recv failed\n");
            }
            break;
        }

        uint32_t flag = -1;
        char* filename;
        Unpack(buffer, &flag, &filename); // client sends the filename in place of cmd_string as we know what cmd it is by the flag

        printf("[Thread %ld] Client %s Flag: %u, Cmd: %s", pthread_self(), client_ip, flag, filename);


        if (flag == READ_REQ_SS) {
            FILE* fp = fopen(filename, "r");
            if (fp == NULL) {
                perror("Error opening file");
                continue;
            }
            char read_buffer[BUFFER_SIZE];
            size_t bytesRead;

            while ((bytesRead = fread(read_buffer, 1, BUFFER_SIZE-1, fp)) > 0) { 
                Packet pkt;
                memset(&pkt, 0 , sizeof(pkt));
                read_buffer[bytesRead] = '\0';
                strncpy(pkt.req_cmd, read_buffer, sizeof(pkt.req_cmd)-1);
                pkt.req_cmd[sizeof(pkt.req_cmd)-1] = '\0';
                pkt.REQ_FLAG = READ_DATA;
                printf("%s\n",pkt.req_cmd);
                char send_data[BUFFER_SIZE];
                int bytes_to_send = Pack(&pkt, send_data);

                send(new_socket, send_data, bytes_to_send, 0);
            }
            fclose(fp);

            Packet end;
            memset(&end, 0, sizeof(end));
            end.REQ_FLAG = READ_END;
            char signal_end[BUFFER_SIZE];
            int bytes = Pack(&end, signal_end);
            send(new_socket, signal_end, bytes, 0);
            printf("Sent the end packet\n");
        }
        else if (flag == WRITE_REQ) {
            // write file
        }
        else if (flag == STREAM) {
            // send contents of file line by line
        }
    }

    close(new_socket);
    pthread_exit(NULL);
}
