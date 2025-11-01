#include "../inc/ns.h"
#include "../inc/ip.h"
#include "../../client/inc/flags.h"
#include <pthread.h>

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
        if(flag == VIEW){
            //no need to send the packet to storage server
            strcpy(msg,"ACK for the VIEW");
        }
        else if(flag == READ_REQ_NS){
            //no need to send the packet to the storage server we just need to send the ip and port of the storage to the client
            strcpy(msg,"ACK for the READ");

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

    printf("[Thread %ld] Connection with %s closed.\n", pthread_self(), client_ip);
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

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on %s:%d...\n", NS_IP, NS_PORT);

    while(1){

        client_len = sizeof(client_addr); 
        memset(&client_addr, 0, sizeof(client_addr));
        new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
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

    close(server_fd);
    return 0;
}
