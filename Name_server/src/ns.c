#include "../inc/ns.h"
#include "../inc/ip.h"
#include "../../client/inc/flags.h"

void Unpack(char* buffer, uint32_t* flag, char** cmd_string) {
    char *ptr = buffer;
    
    // 1. Unpack the flag
    uint32_t flag_net;
    memcpy(&flag_net, ptr, sizeof(uint32_t));
    *flag = ntohl(flag_net); // Convert back from Network Order
    ptr += sizeof(uint32_t);
    
    // 2. Get a pointer to the command string
    *cmd_string = ptr; 
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024] = {0};
    char *msg = "ACK - Command Received\n"; // A better default message

    // --- 1. Setup Socket ---
    // (Your socket, setsockopt, and bind code is correct)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
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

    // --- 3. Listen (Called ONCE, outside the loop) ---
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on %s:%d...\n", NS_IP, NS_PORT);

    // --- 4. Main Server Loop (Accepts new clients) ---
    while(1){
        
        client_len = sizeof(client_addr); 
        memset(&client_addr, 0, sizeof(client_addr));
        new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (new_socket < 0) {
            perror("accept failed");
            continue; 
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Client connected from IP: %s\n", client_ip);

        // --- 5. NEW Inner Loop (Handles ONE client's requests) ---
        // (In your final project, you will spawn a new thread here
        // and this loop will be inside that thread's function)
        while (1) {
            memset(buffer, 0, sizeof(buffer)); // Clear buffer for new packet
            
            // Wait for a packet from THIS client
            int bytes = recv(new_socket, buffer, sizeof(buffer) - 1, 0);

            // Check if client disconnected or error
            if (bytes <= 0) {
                if (bytes == 0) {
                    printf("Client %s disconnected.\n", client_ip);
                } else {
                    perror("recv failed");
                }
                break; // Exit the INNER loop
            }

            uint32_t flag = -1;
            char * cmd_string;
            Unpack(buffer,&flag,&cmd_string);
            printf("[Client %s] Flag: %u, Cmd: %s", client_ip, flag, cmd_string); // cmd_string has \n

            // --- TODO: Process the command based on 'flag' ---
            // e.g., if (flag == VIEW) { ... }
            
            // Send a reply
            // (You'll change 'msg' based on the command)
            send(new_socket, msg, strlen(msg), 0);
        }
        // --- End of Inner Loop ---

        // --- 6. Close THIS client's socket ---
        // This runs after the inner loop breaks (client disconnected)
        close(new_socket);
        printf("Connection with %s closed. Waiting for next client...\n", client_ip);
    }
    
    // This part is now unreachable, which is fine for a server
    close(server_fd);
    return 0;
}
