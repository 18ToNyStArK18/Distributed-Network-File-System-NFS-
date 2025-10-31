#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include "../inc/client_header.h"

// define the macros for the communication in tcp
#define max_inp 1024

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[0m"




int main(){
    char inp_cmd[max_inp];
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
