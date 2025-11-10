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
typedef struct Packet{
    int REQ_FLAG;
    char req_cmd[MAX_WORDS_IN_INP*MAX_WORD_SIZE];
} Packet;
int Pack(Packet* pkt , char * buff);

//create flags
#define CREATE_REQ 1
#define FILE_ALREADY_EXISTS 2

//Read flags
#define READ_REQ_NS 4
#define SS_IP_PORT 41
#define READ_REQ_SS 5
#define READ_DATA 51
#define READ_END 52
#define FILE_DOESNT_EXIST 6

//some basic flags
#define VIEW_N 11
#define VIEW_A 111
#define VIEW_L 112
#define VIEW_AL 113
#define VIEW_DATA 114
#define VIEW_END 115
#define INFO 12
#define INFO_DATA 121
#define INFO_END 122
#define DELETE 13
#define STREAM 14
#define STREAM_DATA 141
#define STREAM_END 142
#define LIST 15
#define LIST_DATA 151
#define LIST_END 152
#define ADDACCESS_r 16
#define ADDACCESS_w 17
#define REMACCESS 18
#define EXEC 19
#define UNDO 20

//Write flags (need to think about the logic
#define WRITE_REQ 21
#define WRITE_DATA 211
#define WRITE_END 212

// Register storage ip and port
#define REG_SS 22
#define USER_REG 23
#define USER_ACTIVE_ALR 24
#define NO_USER_SLOTS 25
#define MAX_USERS 1024
#define USERNAME_SIZE 1024
#define REG_FILES 26

#define MAX_FILE_NAME_SIZE 1024
#define Success 27
#define Fail 28



//some error flags
#define connection_issues 29
#define Not_owner 30
#define NO_access 31


//EXEC
#define EXEC_DATA 32
#define EXEC_END 33
