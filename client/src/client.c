#include "../inc/client_funcs.h"
#include "../../name_server/inc/ip.h"
//NOTE
//while sending the username '\n' is also included idk how to remove it
// define the macros for the communication in tcp
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
void Unpack(char* buffer, uint32_t* flag, char** cmd_string) {
    char *ptr = buffer;
    
    uint32_t flag_net;
    memcpy(&flag_net, ptr, sizeof(uint32_t));
    *flag = ntohl(flag_net); // Convert back from Network Order
    ptr += sizeof(uint32_t);
    
    *cmd_string = ptr; 
}

int main(){
    char inp_cmd[max_inp];
    char user_name[max_username];
    printf(GREEN"Enter your user name: "NORMAL);
    fgets(user_name,max_username,stdin);
    user_name[strlen(user_name)-1]='\0';
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
    //send the username to the ns server
    Packet username;
    username.REQ_FLAG = USER_REG;
    strcpy(username.req_cmd,user_name);

    int bytes_to_send =  Pack(&username,buffer);
    if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
        printf(RED"Unable to send the username to the server\n"NORMAL);
        exit(0);
    }
    uint32_t username_flag;
    char *username_buffer;
    if(recv(client_socket,buffer,BUFFER_SIZE,0)< 0){
        printf(RED"Error in recieving packet\n"NORMAL);
    }
    Unpack(buffer,&username_flag,&username_buffer);
    if(username_flag == USER_ACTIVE_ALR){
        printf(RED"\nThis username is already logged in please logout from that device to login again\n"NORMAL);
        exit(-1);
    }
    else if(username_flag == NO_USER_SLOTS){
        printf(RED"Number of users reached the limit\n"NORMAL);
        exit(-1);
    }
    assert(username_flag == Success);
    printf(GREEN"Successfully registered the username of the client\n Server Says: %s\n"NORMAL,buffer);
    printf(GREEN"Logging in as :%s"NORMAL,user_name);
    printf(GREEN"Enter quit to exit\n\n"NORMAL);
    while(1){
        printf("Enter the command : ");
        fgets(inp_cmd, max_inp-1, stdin);
        if(strcmp(inp_cmd,"quit\n")==0){
            printf(GREEN"Exiting\n"NORMAL);
            return 0;
        }


        command_str parsed;
        parsing(inp_cmd,&parsed);
        print_parsed(&parsed);
        char command_type[MAX_WORD_SIZE];
        assert(parsed.n != 0); // so that the inp_cmd has more than 0 words
        strcpy(command_type,parsed.cmd[0]);


        Packet pkt;
        memset(&pkt,0,sizeof(pkt));
        strcpy(pkt.req_cmd,inp_cmd);

        //based on the command_type we need to send  packets with the different flags to the NS
        if(strncmp(command_type,"LIST",4)==0){
            //list 
            pkt.REQ_FLAG = LIST;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            uint32_t flag = -1;
            char *cmd_string;
            while(1){
                if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                    printf(RED"Error in recieving packet\n"NORMAL);
                    continue;
                }   
                Unpack(recv_buff,&flag,&cmd_string);
                if(flag != VIEW_DATA)
                    break;
                printf("%s\n",cmd_string);
            }

        }
        else if(strncmp(command_type,"VIEW",4)==0){
            //view 
            pkt.REQ_FLAG = VIEW;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            uint32_t flag = -1;
            char *cmd_string;
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == Fail){
                printf(RED"Command failed\n"NORMAL);
                continue;
            }
            assert(flag == Success);
            while(1){
                if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                    printf(RED"Error in recieving packet\n"NORMAL);
                    continue;
                }   
                Unpack(recv_buff,&flag,&cmd_string);
                if(flag != VIEW_DATA)
                    break;
                printf("%s\n",cmd_string);
            }
        }
        else if(strncmp(command_type,"READ",4)==0){
            //Read a file
            strcpy(pkt.req_cmd,parsed.cmd[1]);
            pkt.REQ_FLAG = READ_REQ_NS;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag;
            char *cmd_str;
            Unpack(recv_buff,&flag,&cmd_str);
            if(flag == SS_IP_PORT)
                printf(GREEN"SS_ip and port recieved Successfully\n"NORMAL"%s\n",cmd_str);
            else{
                printf("FILE_DOESNT_EXIST\n");
                continue;
            }
            char ss_ip[40];
            int port;
            sscanf(cmd_str,"%s %d",ss_ip,&port);
            //now i have the port and the ip of the storage server where the file is located now i need to req to that server
            Packet pkt;
            pkt.REQ_FLAG = READ_REQ_SS;
            strcpy(pkt.req_cmd,parsed.cmd[1]);
            int bytes = Pack(&pkt,buffer);

            int a = client_ss_read(buffer,ss_ip,port,bytes);
            if(a==0)
                printf(GREEN"\nReading the file is done\n"NORMAL);
            else
                printf(RED"\nError in reading the file\n"NORMAL);
        }
        else if(strncmp(command_type,"CREATE",6)==0){
            //Create a file
            //the user/client who creates the file become the owner of the file
            pkt.REQ_FLAG = CREATE_REQ;
            strcpy(pkt.req_cmd,parsed.cmd[1]);
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == Success){
                printf(GREEN"File created Successfully\n"NORMAL);
            }
            else if(flag == FILE_ALREADY_EXISTS){
                printf(RED"File with the same name already exists\n"NORMAL);
            }
            else {
                printf(RED"File is not creted\n"NORMAL);
            }

        }
        else if(strncmp(command_type,"INFO",4)==0){
            //For the INFO
            pkt.REQ_FLAG = INFO;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == Fail){
                printf(RED"Command Failed\n"NORMAL);
                continue;
            }
            assert(flag == Success);
            while(1){
                if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                    printf(RED"Error in recieving packet\n"NORMAL);
                    continue;
                }   
                Unpack(recv_buff,&flag,&cmd_string);
                if(flag == INFO_END)
                    break;
                else
                    printf("%s",cmd_string);
            }

        }
        else if(strncmp(command_type,"DELETE",6)==0){
            //Deleing a file
            pkt.REQ_FLAG = DELETE;
            strcpy(pkt.req_cmd,parsed.cmd[1]);
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == FILE_DOESNT_EXIST){
                printf(RED"Requested file does not exist\n"NORMAL);
                continue;
            }
            else if(flag == Fail){
                printf(RED"Command Failed\n"NORMAL);
                continue;
            }
            assert(flag == Success);
            printf(GREEN"File deletion Success\n"NORMAL);
        }
        else if(strncmp(command_type,"STREAM",6)==0){
            //Stream the file
            pkt.REQ_FLAG = STREAM;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag =1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == FILE_DOESNT_EXIST){
                printf(RED"FILE_DOESNT_EXIST\n"NORMAL);
                continue;
            }
            //if file exists i will the get the packet with the storage server ip and port
            assert(flag == Success);
            char ss_ip[40];
            int port;
            sscanf(cmd_string,"%s %d",ss_ip,&port);
            //now i have the port and the ip of the storage server where the file is located now i need to req to that server
            //new connection logic


        }
        else if(strncmp(command_type,"ADDACCESS",9)==0){
            //Adding access to users for read and write perms
            if(strcmp(parsed.cmd[1],"-R")==0)
                pkt.REQ_FLAG = ADDACCESS_r;
            else
                pkt.REQ_FLAG = ADDACCESS_w;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == Success)
                printf(GREEN"Added access Successfully\n"NORMAL);
            else
                printf(RED"Command Failed\n"NORMAL);
        }
        else if(strncmp(command_type, "REMACCESS",9)==0){
            //Remove all the access
            pkt.REQ_FLAG = REMACCESS;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == Success)
                printf(GREEN"Removed access Successfully\n"NORMAL);
            else
                printf(RED"Command Failed\n"NORMAL);
        }
        else if(strncmp(command_type,"EXEC",4)==0){
            //execute the commands in that file
            pkt.REQ_FLAG = EXEC;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == Success)
                printf(GREEN"Executed the file Successfully\n"NORMAL);
            else
                printf(RED"Command Failed\n"NORMAL);

        }
        else if(strncmp(command_type,"UNDO",4)==0){
            //undo the previous change in a file
            //if a user changes something we need to store the previous state when cmd is undo that buffer state will become the current state
            pkt.REQ_FLAG = UNDO;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            printf("Server says :%s\n",recv_buff);
            uint32_t flag = -1;
            char *cmd_string;
            Unpack(recv_buff,&flag,&cmd_string);
            if(flag == Success)
                printf(GREEN"Undo Executed Successfully\n"NORMAL);
            else
                printf(RED"Command Failed\n"NORMAL);

        }
        else if(strncmp(command_type,"WRITE",5)==0){
            //undo the previous change in a file
            //if a user changes something we need to store the previous state when cmd is undo that buffer state will become the current state
            pkt.REQ_FLAG = WRITE_REQ;
            int bytes_to_send = Pack(&pkt,buffer);

            //send the packet to the Name_server

            if(send(client_socket,buffer,bytes_to_send , 0) <= 0){
                printf(RED"Unable to send to the server\n"NORMAL);
                continue;
            }
            printf(GREEN"Packet sent Successfully\n"NORMAL); 
            //response from the Name_server
            char recv_buff[BUFFER_SIZE];
            memset(recv_buff,0,BUFFER_SIZE);
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            printf("Server says :%s\n",recv_buff);

        }
        else{
            printf(RED"Unknown Command : %s\n"NORMAL,inp_cmd);
            continue;
        }
    }
    close(client_socket);
    printf(GREEN"Connection closed.\n"NORMAL);
}
