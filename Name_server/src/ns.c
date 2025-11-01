#include "../inc/ns.h"
#include "../inc/ip.h"

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024] = {0};
    char *msg = "Hello from TCP server\n";

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(NS_PORT);

    if (inet_pton(AF_INET, NS_IP, &address.sin_addr) <= 0) {
        perror("Invalid IP address in NS_IP");
        exit(EXIT_FAILURE);
    }

    // Bind to NS_IP:NS_PORT
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on %s:%d...\n", NS_IP, NS_PORT);

    // Listen
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Accept connection
    memset(&client_addr, 0, sizeof(client_addr));
    new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (new_socket < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    printf("Client connected from IP: %s, Port: %d\n", client_ip, ntohs(client_addr.sin_port));

    // Receive data
    recv(new_socket, buffer, sizeof(buffer), 0);
    printf("Client says: %s\n", buffer);

    // Reply
    send(new_socket, msg, strlen(msg), 0);

    // Close connections
    close(new_socket);
    close(server_fd);

    return 0;
}
