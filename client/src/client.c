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
#include "../inc/client_header.h"
#include "../../Name_server/inc/ip.h"

// define the macros for the communication in tcp
#define max_inp 1024
#define max_username 1024

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[0m"
#define CLIENT_IP "127.0.0.1"
#define CLIENT_PORT 12345
#define BUFFER_SIZE 1024
int main(){
    char inp_cmd[max_inp];
    char user_name[max_username];
    printf(GREEN"Enter your user name: "NORMAL);
    scanf("%s",user_name);
    printf(GREEN"Logging in as :%s\n"NORMAL,user_name);
    printf(GREEN"Enter quit to exit\n"GREEN);
    while(1){
        fgets(inp_cmd, max_inp-1, stdin);
        if(strcmp(inp_cmd,"quit")==0){
            printf(GREEN"Exiting\n"NORMAL);
            return 0;
        }
        command_str parsed;
        parsing(inp_cmd,&parsed);
        print_parsed(&parsed);
        char command_type[MAX_WORD_SIZE];
        assert(parsed.n != 0); // so that the inp_cmd has more than 0 words
        strcpy(command_type,parsed.cmd[0]);


        // the tcp socket connection for the client
        int client_socket;
        struct sockaddr_in server_addr;       
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        char buffer[BUFFER_SIZE];
        char *msg = "HI\n";
        assert(client_socket >= 0);


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

        if(write(client_socket,msg,strlen(msg)) < 0){
            perror("ERROR writing to socket");
            close(client_socket);
            exit(1);
        }


        memset(buffer, 0, BUFFER_SIZE);
        if (read(client_socket, buffer, BUFFER_SIZE - 1) < 0) {
            perror("ERROR reading from socket");
        } else {
            printf("Server reply: %s\n", buffer);
        }

        // Close the connection
        close(client_socket);
        printf(GREEN"Connection closed.\n"NORMAL);
        //based on the command_type we need to send different packets to the NS
        if(strcmp(command_type,"VIEW")==0){
            //view packets

        }
        else if(strcmp(command_type,"READ")==0){
            //Read a file


        }
        else if(strcmp(command_type,"CREATE")==0){
            //Create a file
            //the user/client who creates the file become the owner of the file


        }
        else if(strcmp(command_type,"INFO")==0){
            //For the INFO

        }
        else if(strcmp(command_type,"DELETE")==0){
            //Deleing a file

        }
        else if(strcmp(command_type,"STREAM")==0){
            //Stream the file

        }
        else if(strcmp(command_type,"ADDACCESS")==0){
            //Adding access to users for read and write perms

        }
        else if(strcmp(command_type, "REMACCESS")==0){
            //Remove all the access

        }
        else if(strcmp(command_type,"EXEC")==0){
            //execute the commands in that file

        }
        else if(strcmp(command_type,"UNDO")==0){
            //undo the previous change in a file
            //if a user changes something we need to store the previous state when cmd is undo that buffer state will become the current state

        }
    }
}
