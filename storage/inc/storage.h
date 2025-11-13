#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <dirent.h>
#include "../cmn_inc.h"

#define BUFFER_SIZE 1024
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[0m"

typedef struct{
    int client_socket;
    char client_ip[INET_ADDRSTRLEN];
} client_args_t;

#define MAX_OPERATIONS 4096

typedef struct {
    char filename[MAX_FILE_NAME_SIZE];
    int sentence_index;
    char new_sentence[BUFFER_SIZE];
    int version;
    char username[USERNAME_SIZE];
} WriteOp;

typedef struct {
    char filename[MAX_FILE_NAME_SIZE];
    WriteOp ops[MAX_OPERATIONS];   
    int op_count;                  
    uint64_t next_version;         
    pthread_mutex_t lock;          
} OpLog;