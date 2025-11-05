#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../../cmn_inc.h"


typedef struct{
    char username[USERNAME_SIZE];
    int active;
}userdata;

typedef struct{
    int num_of_users;
    userdata username_arr[MAX_USERS];
}userdatabase;


int send_to_SS(char *buff,char *ss_ip,int ss_port,int size);
void Unpack(char* buffer, uint32_t* flag, char** cmd_string);
int reg_user(char *cmd_string,userdatabase *users);
void removeusername(char *,userdatabase *);
