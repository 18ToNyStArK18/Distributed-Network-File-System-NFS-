#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../../cmn_inc.h"


int send_to_SS(char *buff,char *ss_ip,int ss_port,int size);
void Unpack(char* buffer, uint32_t* flag, char** cmd_string);