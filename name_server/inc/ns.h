#include <netinet/in.h>
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

typedef struct{
    int ss_port;
    char ip[INET_ADDRSTRLEN];
}filelocation;

typedef struct Hashnode{
    char filename[MAX_FILE_NAME_SIZE];
    filelocation location;
    struct Hashnode *next;
}Hashnode;


typedef struct Hashmap{
    int size;
    Hashnode **buckets;
}Hashmap;
int send_to_SS(char *buff,char *ss_ip,int ss_port,int size);
void Unpack(char* buffer, uint32_t* flag, char** cmd_string);
int reg_user(char *cmd_string,userdatabase *users);
void removeusername(char *,userdatabase *);


//hash map functions to get the location of the file
Hashmap * create_hashmap(int size);
//add a file name
int add_file(Hashmap *map,char *filename,char *ip,int port);
//remove a file data
int delete_file(Hashmap *map,char *filename);
//fetch the data of the file
filelocation *get_file_location(Hashmap *map,char*filename);
//free Hashmap
void free_hashmap(Hashmap *map);
