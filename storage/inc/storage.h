#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[0m"

typedef struct{
    int client_socket;
    char client_ip[INET_ADDRSTRLEN];
} client_args_t;

