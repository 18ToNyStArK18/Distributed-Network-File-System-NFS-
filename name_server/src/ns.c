#include "../inc/ns.h"
#include "../inc/ip.h"
#include "../../cmn_inc.h"
#include <pthread.h>

#define BUFFER_SIZE 1024

int ns_port, client_port;
char ss_ip[40];

void* Client_listener_thread (void *arg);
void* Storage_listener_thread (void *arg);

typedef struct{
    int client_socket;
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

void* Handle_client(void* arg){
    client_args_t* args = (client_args_t*)arg;
    int new_socket = args->client_socket;
    char client_ip[INET_ADDRSTRLEN];
    strcpy(client_ip, args->client_ip); // Make a local copy
    char msg[MAX_WORDS_IN_INP * MAX_WORD_SIZE];
    free(args);

    char buffer[1024] = {0};

    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Clear buffer for new packet

        // Wait for a packet from THIS client
        int bytes = recv(new_socket, buffer, sizeof(buffer) - 1, 0);

        // Check if client disconnected or error
        if (bytes <= 0) {
            if (bytes == 0) {
                printf("[Thread %ld] Client %s disconnected.\n", pthread_self(), client_ip);
            } else {
                perror("[Thread] recv failed");
            }
            break; // Exit the loop
        }

        uint32_t flag = -1;
        char * cmd_string;
        Unpack(buffer, &flag, &cmd_string);
        if(flag == USER_REG){ 
            strcpy(msg,"ACK fo the user");
        }
        if(flag == VIEW){
            //no need to send the packet to storage server
            strcpy(msg,"ACK for the VIEW");
        }
        else if(flag == READ_REQ_NS){
            //no need to send the packet to the storage server we just need to send the ip and port of the storage to the client
            sprintf(msg,"%s %d",ss_ip,client_port);           

        }
        else if(flag == CREATE_REQ){
            //we need to send a packet to the storage server so that it can create the files in that storege server
            //we need to add that file in our ns database where we are storing the files present in a storage server
            strcpy(msg,"ACK for the CREATE_REQ");

        }
        else if(flag == INFO){
            //no need to send to the Storage server
            //we will anyway store the data about the files and the users in the ns so just print int
            strcpy(msg,"ACK for the INFO");

        }
        else if(flag == DELETE){
            //need to send the packet to ss and then update it in the ns database
            strcpy(msg,"ACK for the DELETE");

        }
        else if(flag == STREAM){
            //I just need to send the ss ip and port to the client
            strcpy(msg,"ACK for the STREAM");

        }
        else if(flag == ADDACCESS_r){
            //change in the database directly and send the ack back
            strcpy(msg,"ACK for the ADDACCESS_r");

        }
        else if(flag == ADDACCESS_w){
            //same as prev
            strcpy(msg,"ACK for the ADDACCESS_w");

        }
        else if(flag == REMACCESS){
            //same as prev
            strcpy(msg,"ACK for the REMACCESS");

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

        printf("[Thread %ld] Client %s Flag: %u, Cmd: %s", pthread_self(), client_ip, flag, cmd_string);

        send(new_socket,msg, strlen(msg), 0);
    }

    close(new_socket);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024] = {0};

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
    socklen_t ss_len = sizeof(ss_addr);

    if ((storage_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&ss_addr, 0, sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = NS_PORT_SS;

    if (inet_pton(AF_INET, NS_IP, &ss_addr.sin_addr) <= 0) {
        perror("Invalid IP address in NS_IP");
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        perror("bind failed");
        close(storage_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 20) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(storage_fd, 20) < 0) {
        perror("listen failed");
        close(storage_fd);
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
    int ss_fd = (int)(long)arg;
    char buffer[BUFFER_SIZE];
    char *msg = "ACK - Command Received\n";

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int r = recv(ss_fd, buffer, sizeof(buffer), 0);

        if (r <= 0) {
            if (r == 0) { 
                printf("[Thread %ld] Storage Server %s disconnected.\n", pthread_self(), NS_IP);
            }
            else { 
                perror("[Thread] recv failed\n");
            }
            break;
        }

        sscanf(buffer, "REGISTER %s %d %d", ss_ip, &ns_port, &client_port);
        // need to implement hash map to store these
    }
}
