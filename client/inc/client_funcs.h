#include "../../cmn_inc.h"
typedef struct inp_command{
    int n; // num of arguments in a command
    char cmd[MAX_WORDS_IN_INP][MAX_WORD_SIZE]; // command split into individual arguments
}command_str;
void parsing(char * inp_cmd,command_str * command_struct);
void print_parsed(command_str * command_struct);
int client_ss_read(char *bufer,char *ip,int port,int size);
void Unpack(char *buffer,u_int32_t *,char **);
int recv_all(int sock, void* buffer, int length);
int send_all(int sock, const void* buffer, int length);