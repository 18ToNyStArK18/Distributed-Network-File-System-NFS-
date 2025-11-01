#include "../inc/client_header.h"

void parsing(char *inp_cmd,command_str* command_struct){
    // initializing the command_struct
    command_struct->n = 0;
    for(int i=0;i<MAX_WORDS_IN_INP;i++)
        command_struct->cmd[i][0]='\0';
    int inp_size = strlen(inp_cmd);
    int i = 0;
    while(i < inp_size){
        while(i < inp_size && inp_cmd[i]==' ')
            i++;
        int start = i;
        if(i > inp_size || inp_cmd[i] == '\n' || inp_cmd[i] == '\0')
            break;
        while(i < inp_size && inp_cmd[i] != ' ')
            i++;
        strncpy(command_struct->cmd[command_struct->n],inp_cmd+start,i-start+1);
        command_struct->n++;
    }
    return;
}

void print_parsed(command_str *command_struct){
    int n = command_struct->n;
    for(int i=0;i<n;i++)
        printf("%dth argument : %s\n",i,command_struct->cmd[i]);
}
