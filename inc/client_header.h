#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_WORDS_IN_INP 30 
#define MAX_WORD_SIZE 1024

typedef struct inp_command{
    int n;
    char cmd[MAX_WORDS_IN_INP][MAX_WORD_SIZE];
}command_str;

void parsing(char * inp_cmd,command_str * command_struct);
void print_parsed(command_str * command_struct);
