#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
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
    int ns_ss_port;
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
    int wc;
    int chars;
    int lines;
    char time[100];
    struct Hashnode *next;
    struct rw_access *read;
    struct rw_access *write;
}Hashnode;
typedef struct{
    char filename[MAX_FILE_NAME_SIZE];
    char ip[INET6_ADDRSTRLEN];
    int c_ss_port;
    int ns_ss_port;
}cache_node;
typedef struct Hashmap{
    int size;
    Hashnode **buckets;
}Hashmap;
int send_to_SS(char *buff,char *ss_ip,int ss_port,int size);
int Pack(Packet* pkt, char* buff);
void Unpack(char* buffer, uint32_t* flag, char** cmd_string);
int recv_all(int sock, void* buffer, int length);
int send_all(int sock, const void* buffer, int length);
int reg_user(char *cmd_string,userdatabase *users);
void removeusername(char *,userdatabase *);


Hashmap * create_hashmap(int size);
int add_file(Hashmap *map,char *filename,char *ip,int port,char *username,int ns_ss_port);
int delete_file(Hashmap *map,char *filename,userdatabase *users);
int get_file_location(Hashmap *map, char*filename, filelocation* out);
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
void print_view(char *username,userdatabase *users,Hashmap *map,int a, int l,int socket);
int is_owner(char *username,char *filename,Hashmap *map);
void print_info(Hashmap *map,char *filename,int socket);
void execute_file(char *filename,char *ip,int port,int client_socket);
void find_ip_by_filename(char *filename, Hashmap *map, char* ip, int* port);
int is_file_present(char *filename,Hashmap *map);
int update_filename(char *filename,Hashmap *map,int client_port,int ns_port);
int hash_fucn(char *s);
int update_meta(char *filename,Hashmap *map,int wc,int lc,int cc);
