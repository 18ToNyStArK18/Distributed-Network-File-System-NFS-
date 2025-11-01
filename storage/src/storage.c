#include "../../Name_server/inc/ip.h"
#include "../inc/storage.h"
#include "../../client/inc/flags.h"

void Unpack(char* buffer, uint32_t* flag, char** cmd_string);
void* Handle_NS (void* arg);
int Pack(Packet* pkt , char * buff);

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address, client_addr, local_addr;
    socklen_t client_len = sizeof(client_addr);
    socklen_t local_len = sizeof(local_addr);
    char *msg = "Hello from Storage server\n";

    /************** 1. Create server socket **************/
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;   // Any local interface
    address.sin_port = 0;                   // OS picks a free port

    /************** 2. Bind **************/
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /************** 3. Find assigned IP + port **************/
    if (getsockname(server_fd, (struct sockaddr *)&local_addr, &local_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }

    char my_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, my_ip, sizeof(my_ip));
    int my_port = ntohs(local_addr.sin_port);

    printf("Storage Server running on %s:%d\n", my_ip, my_port);

    /************** 4. Register with Name Server **************/
    int ns_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_sock < 0) {
        perror("NS socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in ns_addr;
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_port = htons(NS_PORT);

    if (inet_pton(AF_INET, NS_IP, &ns_addr.sin_addr) <= 0) {
        perror("Invalid NS_IP");
        exit(EXIT_FAILURE);
    }

    if (connect(ns_sock, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) < 0) {
        perror("Could not connect to Name Server");
        exit(EXIT_FAILURE);
    }

    // format: "REGISTER <ip> <port>"
    char reg_msg[64];
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER %s %d", my_ip, my_port);

    Packet pkt;
    memset(&pkt, 0 , sizeof(pkt));
    strcpy(pkt.req_cmd, reg_msg);
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

    listen(server_fd, 10);
    printf("[SS] Waiting for commands from NS.\n");

    // to add here: multi threading b/w the NS and SS for Create, Delete, Execute
    while (1) {
        int ns_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        pthread_t tid;
        pthread_create(&tid, NULL, Handle_NS, (void*)(long)ns_fd);
        pthread_detach(tid);
    }

    /************** 5. Listen for incoming clients **************/
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for client...\n");

    // to add here: multi threading b/w the client and SS for Read, Write, LiveStream
    memset(&client_addr, 0, sizeof(client_addr));
    new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (new_socket < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    printf("Client connected from IP %s:%d\n",
        client_ip, ntohs(client_addr.sin_port));

    /************** 6. Handle one client **************/
    recv(new_socket, buffer, sizeof(buffer), 0);
    printf("Client says: %s\n", buffer);

    send(new_socket, msg, strlen(msg), 0);

    /************** 7. Cleanup **************/
    close(new_socket);
    close(server_fd);

    return 0;
}

// pack the struct into a buffer so that i can send the pckt to the name server
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

        printf("[Thread %ld] Name Server %s Flag: %u, Cmd: %s", pthread_self(), NS_IP, flag, cmd_string);
    
        send(ns_fd, msg, strlen(msg), 0);  
        
        if (flag == CREATE_REQ) {
            // create file
        }
        else if (flag == DELETE) {
            // delete file
        }
        else if (flag == EXEC) {
            // send contents of file line by line
        }
    }

    close(ns_fd);
    pthread_exit(NULL);
}