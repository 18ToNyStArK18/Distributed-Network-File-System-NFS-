#include "client/inc/client_funcs.h"

typedef struct Packet{
    int REQ_FLAG;
    char req_cmd[MAX_WORDS_IN_INP*MAX_WORD_SIZE];
} Packet;


//create flags
#define CREATE_REQ 1
#define FILE_ALREADY_EXISTS 2

//Read flags
#define READ_REQ_NS 4
#define READ_REQ_SS 5
#define FILE_DOESNT_EXIST 6
#define READ_NS_FAIL 9
#define READ_SS_FAIL 10

//some basic flags
#define VIEW 11
#define VIEW_DATA 111
#define VIEW_END 112
#define INFO 12
#define INFO_DATA 121
#define INFO_END 122
#define DELETE 13
#define STREAM 14
#define LIST 15
#define ADDACCESS_r 16
#define ADDACCESS_w 17
#define REMACCESS 18
#define EXEC 19
#define UNDO 20

//Write flags (need to think about the logic
#define WRITE_REQ 21

// Register storage ip and port
#define REG_SS 22

#define Success 67
#define Fail 69
