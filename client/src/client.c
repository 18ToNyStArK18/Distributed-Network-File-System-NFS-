#include "../inc/client_funcs.h"
#include "../../name_server/inc/ip.h"
#include <stdint.h>
//NOTE
//while sending the username '\n' is also included idk how to remove it
// define the macros for the communication in tcp
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

int main(){
    char inp_cmd[max_inp];
    char user_name[max_username];
    printf("\n                       Welcome to the Docs++\n");
    printf(RED"Enter quit to exit\n"NORMAL);
    printf("Enter your user name: ");
    fgets(user_name,max_username,stdin);
    user_name[strlen(user_name)-1]='\0';//to consume the '\n' at the end of the username
    printf("\n--------USERNAME: %s--------\n",user_name);
    // the tcp socket connection for the client
    int client_socket;
    struct sockaddr_in server_addr;       
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    assert(client_socket >= 0);
    char buffer[BUFFER_SIZE];    
    // server_addr
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NS_PORT);

    if(inet_pton(AF_INET,NS_IP,&server_addr.sin_addr) <= 0){
        printf(RED"Invalid address: %s\n"NORMAL, NS_IP);
        perror("inet_pton failed");
        close(client_socket);
        exit(1);
    }
    // got the server ip in the req format
    //conneting to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf(RED"Failed to connect to %s:%d\n"NORMAL, NS_IP, NS_PORT);
        perror("ERROR connecting");
        close(client_socket);
        exit(1);
    }
    printf(GREEN"Successfully connected to Name Server!\n"NORMAL);
    // --- Send USER_REG packet ---
    Packet username;
    memset(&username, 0, sizeof(username));
    username.REQ_FLAG = USER_REG;
    strncpy(username.req_cmd, user_name, sizeof(username.req_cmd)-1);

    int payload_len = Pack(&username, buffer);

    // Send length header first
    uint32_t net_len = htonl(payload_len);
    if (send_all(client_socket, &net_len, sizeof(net_len)) <= 0) {
        printf(RED "Error sending username length\n" NORMAL);
        exit(1);
    }

    // Send packet body
    if (send_all(client_socket, buffer, payload_len) <= 0) {
        printf(RED "Unable to send the username to the server\n" NORMAL);
        exit(1);
    }

    // --- Receive response length ---
    uint32_t response_len_net;
    if (recv_all(client_socket, &response_len_net, sizeof(response_len_net)) <= 0) {
        printf(RED "Error receiving username response length\n" NORMAL);
        exit(1);
    }
    uint32_t response_len = ntohl(response_len_net);

    // --- Receive response packet body ---
    char recv_buff[BUFFER_SIZE];
    if (recv_all(client_socket, recv_buff, response_len) <= 0) {
        printf(RED "Error receiving username response\n" NORMAL);
        exit(1);
    }

    // --- Decode ---
    uint32_t username_flag;
    char *username_buffer;

    Unpack(recv_buff, &username_flag, &username_buffer);

    if(username_flag == USER_ACTIVE_ALR){
        printf(RED "\nThis username is already logged in.\nPlease logout from that device.\n" NORMAL);
        exit(-1);
    }
    else if(username_flag == NO_USER_SLOTS){
        printf(RED "\nMaximum number of users reached.\n" NORMAL);
        exit(-1);
    }

    assert(username_flag == Success);
    printf(GREEN "\nUsername successfully registered.\nReady for commands.\n" NORMAL);

    while(1){
        printf(" %s> ",user_name);
        fgets(inp_cmd, max_inp-1, stdin);
        if(strcmp(inp_cmd,"quit\n")==0){
            printf(GREEN"Bye\n"NORMAL);
            return 0;
        }
        command_str parsed;
        parsing(inp_cmd,&parsed);
        if(parsed.n ==0)
            continue;
        printf("\n--------Parsed command--------\n");
        print_parsed(&parsed);
        printf("--------------------------------\n");
        char command_type[MAX_WORD_SIZE];
        strcpy(command_type,parsed.cmd[0]);

        Packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        strcpy(pkt.req_cmd, inp_cmd);

        //based on the command_type we need to send packets with the different flags to the NS
        if(strncmp(command_type,"LIST",4)==0){
            printf("[%s] Requested List\n",user_name);
            pkt.REQ_FLAG = LIST;

            int payload_len = Pack(&pkt, buffer);

            // Send [length][packet]
            uint32_t net_len = htonl(payload_len);
            send_all(client_socket, &net_len, sizeof(net_len));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN "LIST request sent successfully\n" NORMAL);

            // Now receive multiple LIST_DATA packets until LIST_END
            while (1) {
                uint32_t resp_len_net;
                if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                    printf(RED "Lost connection while receiving LIST\n" NORMAL);
                    break;
                }

                uint32_t resp_len = ntohl(resp_len_net);
                if (resp_len > BUFFER_SIZE) {
                    printf(RED "LIST packet too large\n" NORMAL);
                    break;
                }

                char recv_buff[BUFFER_SIZE];
                if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                    printf(RED "Error receiving LIST packet\n" NORMAL);
                    break;
                }

                uint32_t flag;
                char *cmd_string;
                Unpack(recv_buff, &flag, &cmd_string);

                if (flag == LIST_DATA) {
                    printf("%s", cmd_string);  // PRINT USER LIST LINE
                }
                else if (flag == LIST_END) {
                    printf(GREEN "\nFinished receiving LIST\n" NORMAL);
                    break;
                }
                else {
                    printf(RED "Unexpected flag %u in LIST response\n" NORMAL, flag);
                    break;
                }
            }
        }
        else if(strncmp(command_type,"REQ_ACCESS",10)==0){
            if(parsed.cmd[1][1]=='R')
                pkt.REQ_FLAG = REQ_ACCESS_R;
            else
                pkt.REQ_FLAG = REQ_ACCESS_W;
            strcpy(pkt.req_cmd,parsed.cmd[2]);
            int bytes_to_send = Pack(&pkt,buffer);
            uint32_t net_len = htonl(bytes_to_send);
            send_all(client_socket,&net_len,sizeof(uint32_t));
            send_all(client_socket,buffer,bytes_to_send);
            printf("Request Sent Successfully\n");            
        }
        else if(strncmp(command_type,"VIEW_REQS",9)==0){
            pkt.REQ_FLAG = VIEW_REQS;
            char send_buffer[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt,send_buffer);
            int net_len = htonl(bytes_to_send);
            send_all(client_socket,&net_len,sizeof(uint32_t));
            send_all(client_socket,send_buffer,bytes_to_send);

            char recv_buff[BUFFER_SIZE];
            uint32_t flag;
            char *cmd_str;
            while(1){
                uint32_t recv_net_len;
                recv_all(client_socket,&recv_net_len,sizeof(uint32_t));
                uint32_t recv_len = ntohl(recv_net_len);
                recv_all(client_socket,recv_buff,recv_len);
                Unpack(recv_buff,&flag,&cmd_str);
                if(flag == VIEW_REQS_END)
                    break;
                else if(flag != VIEW_REQS_DATA){
                    printf("Wrong flag %d\n",flag);
                    break;
                }
                char owner[MAX_WORD_SIZE],perms,username[MAX_WORD_SIZE],filename[MAX_WORD_SIZE];
                sscanf(cmd_str,"%s %c %s %s",owner,&perms,username,filename);
                printf("User: %s is requesting %c access for the file %s Enter Y to grant N to deny\n",username,perms,filename); 
                char Res[3];
                fgets(Res,2,stdin);
                if(Res[0]=='Y'){
                    pkt.REQ_FLAG = Success;
                    bytes_to_send = Pack(&pkt,send_buffer);
                    net_len = htonl(bytes_to_send);
                    send_all(client_socket,&net_len,sizeof(uint32_t));
                    send_all(client_socket,send_buffer,bytes_to_send);
                }
                else{
                    pkt.REQ_FLAG = Fail;
                    bytes_to_send = Pack(&pkt,send_buffer);
                    net_len = htonl(bytes_to_send);
                    send_all(client_socket,&net_len,sizeof(uint32_t));
                    send_all(client_socket,send_buffer,bytes_to_send);
                }
            }
        }
        else if(strncmp(command_type,"VIEW",4)==0){
            //view
            printf("[%s] Requested VIEW\n", user_name);
            char view_type[5];
            if(parsed.n != 1)
                strcpy(view_type,parsed.cmd[1]);
            if(parsed.n==1)
                pkt.REQ_FLAG = VIEW_N;
            else if(strcmp(view_type,"-a")==0)
                pkt.REQ_FLAG = VIEW_A;
            else if(strcmp(view_type,"-l")==0)
                pkt.REQ_FLAG = VIEW_L;
            else if(strcmp(view_type,"-la")==0 || strcmp(view_type,"-al")==0)
                pkt.REQ_FLAG = VIEW_AL;
            else{
                printf(RED"UNKNOWN FLAG\n"NORMAL);
                continue;
            }
            strcpy(pkt.req_cmd,inp_cmd);
            int payload_len = Pack(&pkt, buffer);

            // send length
            uint32_t net_len = htonl(payload_len);
            send_all(client_socket, &net_len, sizeof(net_len));
            // send packet
            send_all(client_socket, buffer, payload_len);

            printf(GREEN "VIEW request sent successfully\n" NORMAL);

            // Receive first response (Success or Fail)
            uint32_t resp_len_net;
            uint32_t resp_len,flag;
            char *cmd_string;
            printf("\n--------OUTPUT of VIEW--------\n");

            // Now receive all VIEW_DATA lines until VIEW_END
            while (1) {

                // get next packet length
                if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                    printf(RED "VIEW connection ended unexpectedly\n" NORMAL);
                    break;
                }
                resp_len = ntohl(resp_len_net);
                if (resp_len > BUFFER_SIZE) {
                    printf(RED "VIEW packet overflow\n" NORMAL);
                    break;
                }

                // read packet body
                if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                    printf(RED "Error receiving VIEW data\n" NORMAL);
                    break;
                }

                Unpack(recv_buff, &flag, &cmd_string);

                if (flag == VIEW_DATA) {
                    printf("%s", cmd_string);
                }
                else if (flag == VIEW_END) {
                    break;
                }
                else {
                    printf(RED "Unexpected VIEW flag %u\n" NORMAL, flag);
                    break;
                }
            }

            printf("---------------------------------\n");
        }
        else if(strncmp(command_type,"READ",4)==0){
            printf("[%s] Requested READ Filename: %s\n", user_name, parsed.cmd[1]);
            pkt.REQ_FLAG = READ_REQ_NS;
            strcpy(pkt.req_cmd, parsed.cmd[1]);   
            int payload_len = Pack(&pkt, buffer);

            // send [length][packet] to NS
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN "READ request sent successfully to NS\n" NORMAL);

            // Receive NS response (which contains SS IP + port, or error)
            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED "Failed to recv READ response\n" NORMAL);
                continue;
            }
            uint32_t resp_len = ntohl(resp_len_net);
            recv_all(client_socket, buffer, resp_len);

            uint32_t flag;
            char *cmd_str;
            Unpack(buffer, &flag, &cmd_str);

            if(flag == NO_access){
                printf(RED "Nice try You don't have the access\n" NORMAL);
                continue;
            }
            if(flag == FILE_DOESNT_EXIST){
                printf(RED "File Doesnt exists\n"NORMAL);    
                continue;
            }

            printf(GREEN "Storage Server for file: %s\n" NORMAL, cmd_str);

            // Parse SS location
            char ss_ip[64];
            int port;
            sscanf(cmd_str, "%s %d", ss_ip, &port);

            // Send READ request directly to SS
            memset(&pkt, 0, sizeof(pkt));
            pkt.REQ_FLAG = READ_REQ_SS;
            strcpy(pkt.req_cmd, parsed.cmd[1]);
            payload_len = Pack(&pkt, buffer);
            printf("ip: %s port: %d\n",ss_ip,port); 
            printf("\n--------FILE DATA--------\n");
            int result = client_ss_read(buffer, ss_ip, port, payload_len);

            if(result == 0)
                printf(GREEN "\nReading complete\n" NORMAL);
            else
                printf(RED "\nError during file read\n" NORMAL);
        }
        else if(strncmp(command_type,"CREATE",6)==0){
            //Create a file
            printf("[%s] Requested CREATE Filename : %s\n",user_name,parsed.cmd[1]);

            pkt.REQ_FLAG = CREATE_REQ;
            strcpy(pkt.req_cmd,parsed.cmd[1]);

            int bytes_to_send = Pack(&pkt,buffer);
            uint32_t net_len = htonl(bytes_to_send);

            if (send_all(client_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "ERROR: Failed to send CREATE length.\n" NORMAL);
                continue;
            }

            if (send_all(client_socket, buffer, bytes_to_send) <= 0) {
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }

            printf(GREEN"Packet sent Successfully\n"NORMAL); 

            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED "Error receiving packet length\n" NORMAL);
                continue;
            }

            uint32_t packet_len = ntohl(resp_len_net);
            if (packet_len > BUFFER_SIZE) {
                printf(RED "Packet too large (%u bytes)\n" NORMAL, packet_len);
                continue;
            }

            if (recv_all(client_socket, buffer, packet_len) <= 0) {
                printf(RED "Error receiving packet body\n" NORMAL);
                continue;
            }

            uint32_t flag = -1;
            char *cmd_string;
            Unpack(buffer,&flag,&cmd_string);

            if(flag == Success){
                printf(GREEN"File created Successfully\n"NORMAL);
            }
            else if(flag == FILE_ALREADY_EXISTS){
                printf(RED"File with the same name already exists\n"NORMAL);
            }
            else {
                printf(RED"File is not created\n"NORMAL);
            }
        }
        else if(strncmp(command_type,"INFO",4)==0){
            printf("[%s] Requested INFO Filename: %s\n", user_name, parsed.cmd[1]);
            pkt.REQ_FLAG = INFO;
            strcpy(pkt.req_cmd,parsed.cmd[1]);

            int payload_len = Pack(&pkt, buffer);

            // Send request to Name Server
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN "INFO request sent to NS\n" NORMAL);

            // Receive ACK response first
            uint32_t resp_len_net,flag=-1;
            char *cmd_str;

            uint32_t resp_len = ntohl(resp_len_net);
            printf("\n--------INFO DATA--------\n");

            // Receive multiple INFO_DATA packets until INFO_END
            while (1) {
                // Read length prefix
                if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                    printf(RED "Connection closed while receiving INFO\n" NORMAL);
                    break;
                }

                resp_len = ntohl(resp_len_net);
                recv_all(client_socket, buffer, resp_len);

                Unpack(buffer, &flag, &cmd_str);

                if (flag == INFO_END)
                    break;

                printf("%s", cmd_str);
            }

            printf("\n---------------------------\n");
        }
        else if(strncmp(command_type,"DELETE",6)==0){
            printf("[%s] Requested DELETE Filename : %s\n", user_name, parsed.cmd[1]);
            pkt.REQ_FLAG = DELETE;
            strcpy(pkt.req_cmd,parsed.cmd[1]);

            int payload_len = Pack(&pkt, buffer);

            // ---- SEND REQUEST TO NS ----
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN "DELETE request sent to NS\n" NORMAL);

            // ---- RECEIVE RESPONSE ----
            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED "Failed to receive DELETE response\n" NORMAL);
                continue;
            }

            uint32_t resp_len = ntohl(resp_len_net);
            recv_all(client_socket, buffer, resp_len);

            uint32_t flag;
            char *cmd_string;
            Unpack(buffer, &flag, &cmd_string);

            if(flag == FILE_DOESNT_EXIST){
                printf(RED "Requested file does not exist\n" NORMAL);
                continue;
            }
            else if(flag == Fail){
                printf(RED "Command Failed\n" NORMAL);
                continue;
            }
            else if(flag == Not_owner){
                printf(RED"Nice Try but you are not the owner\n"NORMAL);
                continue;
            }

            assert(flag == Success);
            printf(GREEN "File deletion Success\n" NORMAL);
        }
        else if(strncmp(command_type,"STREAM",6)==0){
            //Stream the file
            printf("[%s] Requested STREAM Filename: %s\n", user_name, parsed.cmd[1]);

            pkt.REQ_FLAG = STREAM;
            strcpy(pkt.req_cmd, parsed.cmd[1]);   // Make sure filename is set
            int payload_len = Pack(&pkt, buffer);

            // ---- SEND TO NAME SERVER (with length prefix) ----
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN "Packet sent Successfully\n" NORMAL);

            // ---- RECEIVE RESPONSE (length + packet) ----
            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED "Error receiving response length\n" NORMAL);
                continue;
            }

            uint32_t resp_len = ntohl(resp_len_net);
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff, 0, BUFFER_SIZE);

            if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                printf(RED "Error receiving response packet\n" NORMAL);
                continue;
            }

            uint32_t flag = -1;
            char *cmd_str;
            Unpack(recv_buff, &flag, &cmd_str);
            if(flag == NO_access){
                printf(RED "Nice try You don't have the access\n" NORMAL);
                continue;
            }
            if(flag == FILE_DOESNT_EXIST){
                printf(RED "File Doesnt exists\n"NORMAL);    
                continue;
            }
            printf(GREEN "Storage Server for file: %s\n" NORMAL, cmd_str);

            // Parse SS location
            char ss_ip[64];
            int port;
            sscanf(cmd_str, "%s %d", ss_ip, &port);

            // Send READ request directly to SS
            memset(&pkt, 0, sizeof(pkt));
            pkt.REQ_FLAG = STREAM;
            strcpy(pkt.req_cmd, parsed.cmd[1]);
            payload_len = Pack(&pkt, buffer);
            printf("ip: %s port: %d\n",ss_ip,port); 
            printf("\n--------FILE DATA--------\n");
            int result = client_ss_stream(buffer, ss_ip, port, payload_len);

            if(result == 0)
                printf(GREEN "\nStreaming complete\n" NORMAL);
            else
                printf(RED "\nError during file STREAM\n" NORMAL);
        }
        else if(strncmp(command_type,"ADDACCESS",9)==0){
            // Adding access to users for read and write perms
            if(strcmp(parsed.cmd[1],"-R")==0){
                pkt.REQ_FLAG = ADDACCESS_r;
                printf("[%s] Requested ADDACCESS_r to Filename: %s for User: %s\n",
                        user_name, parsed.cmd[2], parsed.cmd[3]);
            }
            else{
                pkt.REQ_FLAG = ADDACCESS_w;
                printf("[%s] Requested ADDACCESS_w to Filename: %s for User: %s\n",
                        user_name, parsed.cmd[2], parsed.cmd[3]);
            }

            int payload_len = Pack(&pkt, buffer);

            // ---- SEND TO NAME SERVER (with length prefix) ----
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN"Packet sent Successfully\n"NORMAL);

            // ---- RECEIVE RESPONSE (with length prefix) ----
            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED "Error receiving response length\n" NORMAL);
                continue;
            }
            uint32_t resp_len = ntohl(resp_len_net);

            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);

            if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                printf(RED "Error receiving packet\n" NORMAL);
                continue;
            }

            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff, &flag, &cmd_string);

            if(flag == Success)
                printf(GREEN"Added access Successfully\n"NORMAL);
            else if (flag == Not_owner)
                printf(RED"Nice Try but you are not the owner\n"NORMAL);
            else
                printf(RED"Command failed\n"NORMAL);
        }
        else if(strncmp(command_type, "REMACCESS",9)==0){
            // Remove all access
            pkt.REQ_FLAG = REMACCESS;
            printf("[%s] Requested REMACCESS to Filename: %s for User: %s\n",
                    user_name, parsed.cmd[1], parsed.cmd[2]);

            int payload_len = Pack(&pkt, buffer);

            // ---- SEND packet length + packet ----
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN"Packet sent Successfully\n"NORMAL);

            // ---- RECEIVE response length ----
            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED "Error receiving response length\n" NORMAL);
                continue;
            }
            uint32_t resp_len = ntohl(resp_len_net);

            // ---- RECEIVE full response ----
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff, 0, BUFFER_SIZE);

            if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                printf(RED "Error receiving response\n" NORMAL);
                continue;
            }

            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff, &flag, &cmd_string);
            if(flag == Success)
                printf(GREEN"Added access Successfully\n"NORMAL);
            else if (flag == Not_owner)
                printf(RED"Nice Try but you are not the owner\n"NORMAL);
            else
                printf(RED"Command failed\n"NORMAL);

        }
        else if(strncmp(command_type,"EXEC",4)==0){
            // execute the commands in that file
            pkt.REQ_FLAG = EXEC;
            strcpy(pkt.req_cmd,parsed.cmd[1]);

            int payload_len = Pack(&pkt, buffer);

            // ---- SEND packet length + packet ----
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN"Packet sent Successfully\n"NORMAL);

            // ---- RECEIVE response length ----
            while(1){
                uint32_t resp_len_net;
                if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                    printf(RED "Error receiving response length\n" NORMAL);
                    continue;
                }
                uint32_t resp_len = ntohl(resp_len_net);

                // ---- RECEIVE full response ----
                char recv_buff[BUFFER_SIZE];
                memset(recv_buff, 0, BUFFER_SIZE);

                if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                    printf(RED "Error receiving EXEC response\n" NORMAL);
                    continue;
                }

                uint32_t flag = -1;
                char *cmd_str;
                Unpack(recv_buff,&flag,&cmd_str);
                if(flag == FILE_DOESNT_EXIST){
                    printf(RED"File doesn't exist\n"NORMAL);
                    break;
                }
                else if(flag == NO_access){
                    printf(RED"Nice try but you don't have the access to execute\n"NORMAL);
                    break;
                }
                else if(flag == EXEC_END)
                    break;
                else
                    printf("%s",cmd_str);

            }
        }
        else if(strncmp(command_type,"UNDO",4)==0){
            printf("[%s] Requested UNDO Filename: %s\n", user_name, parsed.cmd[1]);

            pkt.REQ_FLAG = UNDO;
            int payload_len = Pack(&pkt, buffer);

            // ---- SEND packet length + data ----
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN"UNDO packet sent successfully\n"NORMAL);

            // ---- RECEIVE response length ----
            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED "Error receiving UNDO response length\n" NORMAL);
                continue;
            }
            uint32_t resp_len = ntohl(resp_len_net);

            // ---- RECEIVE full response ----
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);

            if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                printf(RED "Error receiving UNDO response data\n" NORMAL);
                continue;
            }

            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff, &flag, &cmd_string);

            if(flag == Success)
                printf(GREEN"Undo Executed Successfully\n"NORMAL);
            else
                printf(RED"Command Failed\n"NORMAL);
        }
        else if(strncmp(command_type,"WRITE",5)==0){
            printf("[%s] Requested WRITE Filename: %s\n", user_name, parsed.cmd[1]);

            pkt.REQ_FLAG = WRITE_REQ;
            int payload_len = Pack(&pkt, buffer);

            // ---- SEND packet length + packet ----
            uint32_t len_net = htonl(payload_len);
            send_all(client_socket, &len_net, sizeof(len_net));
            send_all(client_socket, buffer, payload_len);

            printf(GREEN"Packet sent Successfully\n"NORMAL); 

            // ---- RECEIVE response length ----
            uint32_t resp_len_net;
            if (recv_all(client_socket, &resp_len_net, sizeof(resp_len_net)) <= 0) {
                printf(RED"Error receiving WRITE response length\n"NORMAL);
                continue;
            }
            uint32_t resp_len = ntohl(resp_len_net);

            // ---- RECEIVE response packet ----
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff, 0, BUFFER_SIZE);

            if (recv_all(client_socket, recv_buff, resp_len) <= 0) {
                printf(RED"Error receiving WRITE response data\n"NORMAL);
                continue;
            }

            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff, &flag, &cmd_string);

            if(flag == Success)
                printf(GREEN"WRITE command acknowledged by NS\n"NORMAL);
            else
                printf(RED"WRITE command failed\n"NORMAL);
        }
        else{
            printf(RED"Unknown Command : %s\n"NORMAL,inp_cmd);
            continue;
        }
    }
    close(client_socket);
    printf(GREEN"Connection closed.\n"NORMAL);
}
