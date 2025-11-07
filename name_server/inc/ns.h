#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../../cmn_inc.h"


typedef struct filename_foruser{
    char filename[MAX_FILE_NAME_SIZE];
    struct filename_foruser *next;
}filename_foruser;
typedef struct{
    char username[USERNAME_SIZE];
    int active;
    filename_foruser *files;
}userdata;
typedef struct{
    int num_of_users;
    userdata username_arr[MAX_USERS];
}userdatabase;

typedef struct{
    int ss_port;
    char ip[INET_ADDRSTRLEN];
}filelocation;

typedef struct rw_access{
    char username[USERNAME_SIZE];
    struct rw_access *next;
}rw_access;

typedef struct Hashnode{
    char *filename;
    filelocation location;
    char Owner[MAX_WORD_SIZE];
    struct Hashnode *next;
    struct rw_access *read;
    struct rw_access *write;
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
int add_file(Hashmap *map,char *filename,char *ip,int port,char *username);
//remove a file data
int delete_file(Hashmap *map,char *filename);
//fetch the data of the file
int get_file_location(Hashmap *map, char*filename, filelocation* out);
//free Hashmap
void free_hashmap(Hashmap *map);
int add_r_access(Hashmap *map,char *filename,char *username);
int add_w_access(Hashmap *map,char *filename,char *username);
int rem_access(Hashmap *map,char *filename,char *username);
int can_read(Hashmap *map,char *filename,char *username);
int can_write(Hashmap *map,char *filename,char *username);
int add_file_to_user(char *filename,char *username,userdatabase *users);
int delete_file_from_user(char *filename,char *username,userdatabase *users);
void print(Hashmap *map);
void print_details(char *filename,Hashmap *map);
