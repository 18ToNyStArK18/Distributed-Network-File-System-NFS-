#include "../../Name_server/inc/ip.h"
#include "../inc/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address, client_addr, local_addr;
    socklen_t client_len = sizeof(client_addr);
    socklen_t local_len = sizeof(local_addr);
    char buffer[1024] = {0};
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

    send(ns_sock, reg_msg, strlen(reg_msg), 0);
    printf("Sent registration to Name Server: %s\n", reg_msg);

    close(ns_sock);

    /************** 5. Listen for incoming clients **************/
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for client...\n");

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
