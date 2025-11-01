#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>
#include <arpa/inet.h>
#include "../inc/flags.h"
#include "../../Name_server/inc/ip.h"

// define the macros for the communication in tcp
#define max_inp 1024
#define max_username 1024

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[0m"
#define BUFFER_SIZE 1024


// pack the struct into a buffer so that i can send the pckt to the name server
int Pack(Packet_CS_NS* pkt , char * buff){
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

int main(){
    char inp_cmd[max_inp];
    char user_name[max_username];
    printf(GREEN"Enter your user name: "NORMAL);
    fgets(user_name,max_username,stdin);
    printf(GREEN"Logging in as :%s\n"NORMAL,user_name);
    printf(GREEN"Enter quit to exit\n"GREEN);


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


    while(1){
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


        Packet_CS_NS pkt;
        memset(&pkt,0,sizeof(pkt));
        strcpy(pkt.req_cmd,inp_cmd);

        //based on the command_type we need to send  packets with the different flags to the NS
        if(strncmp(command_type,"VIEW",4)==0){
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
            if(recv(client_socket,recv_buff,BUFFER_SIZE,0)< 0){
                printf(RED"Error in recieving packet\n"NORMAL);
                continue;
            }
            printf("Server says :%s\n",recv_buff);
        }
        else if(strcmp(command_type,"READ")==0){
            //Read a file
            pkt.REQ_FLAG = READ_REQ_NS;

        }
        else if(strcmp(command_type,"CREATE")==0){
            //Create a file
            //the user/client who creates the file become the owner of the file
            pkt.REQ_FLAG = CREATE_REQ;
        }
        else if(strcmp(command_type,"INFO")==0){
            //For the INFO
            pkt.REQ_FLAG = INFO;

        }
        else if(strcmp(command_type,"DELETE")==0){
            //Deleing a file
            pkt.REQ_FLAG = INFO;
        }
        else if(strcmp(command_type,"STREAM")==0){
            //Stream the file
            pkt.REQ_FLAG = STREAM;
        }
        else if(strcmp(command_type,"ADDACCESS")==0){
            //Adding access to users for read and write perms
            if(strcmp(parsed.cmd[1],"-R")==0)
                pkt.REQ_FLAG = ADDACCESS_r;
            else
                pkt.REQ_FLAG = ADDACCESS_w;
        }
        else if(strcmp(command_type, "REMACCESS")==0){
            //Remove all the access
            pkt.REQ_FLAG = REMACCESS;
        }
        else if(strcmp(command_type,"EXEC")==0){
            //execute the commands in that file
            pkt.REQ_FLAG = EXEC;
        }
        else if(strcmp(command_type,"UNDO")==0){
            //undo the previous change in a file
            //if a user changes something we need to store the previous state when cmd is undo that buffer state will become the current state
            pkt.REQ_FLAG = UNDO;
        }
        else{
            printf(RED"Unknown Command : %s\n"NORMAL,inp_cmd);
            continue;
        }
    }
    close(client_socket);
    printf(GREEN"Connection closed.\n"NORMAL);

}
