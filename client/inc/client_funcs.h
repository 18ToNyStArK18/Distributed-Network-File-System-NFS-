#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
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




#define MAX_WORDS_IN_INP 30 
#define MAX_WORD_SIZE 1024
#define max_inp 1024
#define max_username 1024
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[0m"
#define BUFFER_SIZE 1024

typedef struct inp_command{
    int n; // num of arguments in a command
    char cmd[MAX_WORDS_IN_INP][MAX_WORD_SIZE]; // command split into individual arguments
}command_str;
void parsing(char * inp_cmd,command_str * command_struct);
void print_parsed(command_str * command_struct);
