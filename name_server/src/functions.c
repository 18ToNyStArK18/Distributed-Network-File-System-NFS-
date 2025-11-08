#include "../inc/ns.h"
#include <sys/socket.h>
void Unpack(char* buffer, uint32_t* flag, char** cmd_string) {
    char* ptr = buffer;

    ptr += sizeof(uint32_t); // skip length

    uint32_t flag_net;
    memcpy(&flag_net, ptr, sizeof(uint32_t));
    *flag = ntohl(flag_net);
    ptr += sizeof(uint32_t);

    *cmd_string = ptr;
}

int Pack(Packet* pkt, char* buff) {
    memset(buff, 0, BUFFER_SIZE);

    uint32_t flag_net = htonl(pkt->REQ_FLAG);
    uint32_t msg_len = strlen(pkt->req_cmd) + 1;       // include null terminator
    uint32_t total_len = sizeof(uint32_t) + msg_len;   // flag + string
    uint32_t total_len_net = htonl(total_len);

    char* ptr = buff;

    memcpy(ptr, &total_len_net, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, &flag_net, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(ptr, pkt->req_cmd, msg_len);
    ptr += msg_len;

    return ptr - buff;
}

int recv_all(int sock, void* buffer, int length) {
    int total = 0, n;
    while (total < length) {
        n = recv(sock, (char*)buffer + total, length - total, 0);
        if (n <= 0) return n; // error or disconnect
        total += n;
    }
    return total;
}

int send_all(int sock, const void* buffer, int length) {
    int total = 0;
    int n;

    while (total < length) {
        n = send(sock, (const char*)buffer + total, length - total, 0);
        if (n <= 0) {
            return n; // error or disconnected
        }
        total += n;
    }

    return total;
}
int send_to_SS(char *buff, char *ss_ip, int ss_port, int size) {

    int ss_sock;
    struct sockaddr_in ss_addr;

    if ((ss_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);

    if (inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr) <= 0) {
        printf("Invalid IP\n");
        close(ss_sock);
        return -1;
    }

    if (connect(ss_sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        printf("Connection failed\n");
        close(ss_sock);
        return -1;
    }

    uint32_t net_len = htonl(size);
    if (send_all(ss_sock, &net_len, sizeof(net_len)) <= 0) {
        printf("Send length failed\n");
        close(ss_sock);
        return -1;
    }

    if (send_all(ss_sock, buff, size) <= 0) {
        printf("Send data failed\n");
        close(ss_sock);
        return -1;
    }

    printf("Sent request to SS (%s:%d)\n", ss_ip, ss_port);

    uint32_t reply_len_net;
    if (recv_all(ss_sock, &reply_len_net, sizeof(reply_len_net)) <= 0) {
        printf("Recv length failed\n");
        close(ss_sock);
        return -1;
    }

    int reply_len = ntohl(reply_len_net);
    if (reply_len > BUFFER_SIZE) {
        printf("Reply too large\n");
        close(ss_sock);
        return -1;
    }

    char recv_buffer[BUFFER_SIZE];
    if (recv_all(ss_sock, recv_buffer, reply_len) <= 0) {
        printf("Recv reply failed\n");
        close(ss_sock);
        return -1;
    }

    uint32_t flag;
    char *cmd_str;
    Unpack(recv_buffer, &flag, &cmd_str);

    close(ss_sock);

    return (flag == Success) ? 0 : -1;
}
int reg_user(char * username, userdatabase *users){
    int user_idx = -1;
    for(int i=0;i<users->num_of_users;i++){
        if(strcmp(username,users->username_arr[i].username)==0)
            user_idx = i;
    }
    if( user_idx == -1){
        //user not registered
        if(users->num_of_users == MAX_USERS)
            return -2;
        strcpy(users->username_arr[users->num_of_users].username,username);
        users->username_arr[users->num_of_users].active = 1;
        users->num_of_users++;
        return 0;   
    }
    else{
        if(users->username_arr[user_idx].active == 1)
            return -1;
        users->username_arr[user_idx].active = 1;
        return 0;      

    }
}
void removeusername(char *username,userdatabase *database){

    for(int i=0;i<database->num_of_users;i++){
        if(strcmp(username,database->username_arr[i].username)==0){
            database->username_arr[i].active = 0;
            printf("Made USER %s In-active\n",username);
            return;
        }
    }


}

int hash_fucn(char *str){
    long hash = 5381; // magic number
    int c;
    while((c= *str++)){
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}
Hashmap *create_hashmap(int size){
    if(size < 1)
        return NULL;
    Hashmap *map = (Hashmap *)malloc(sizeof(Hashmap));
    if(map == NULL)
        return NULL;
    map->size = size;
    map->buckets = (Hashnode **)malloc(sizeof(Hashnode*) * size);

    memset(map->buckets,0,sizeof(Hashnode *) * size);
    return map;
}

int add_file(Hashmap *map, char *filename, char *ip, int port,char *username){
    long hash = hash_fucn(filename);
    int idx = abs(hash) % map->size;

    Hashnode *current = map->buckets[idx];
    while(current != NULL){
        if(strcmp(filename,current->filename) == 0)
            return -1; //file name exists already

    }
    Hashnode *newnode = (Hashnode *)malloc(sizeof(Hashnode));
    newnode->location.ss_port = port;
    newnode->filename = strdup(filename);
    rw_access *read_a = (rw_access *)malloc(sizeof(rw_access));
    rw_access *write_a = (rw_access *)malloc(sizeof(rw_access));
    strcpy(read_a->username,username);
    strcpy(write_a->username,username);
    read_a->next = newnode->read;
    write_a->next = newnode->write;
    newnode->read = read_a;
    newnode->write = write_a;
    strcpy(newnode->Owner,username);
    strcpy(newnode->location.ip,ip);
    newnode->next = map->buckets[idx];
    map->buckets[idx] = newnode;
    printf("Added the file to the Hashmap\n");
    return 1;
}

int get_file_location(Hashmap *map, char *filename, filelocation *out){
    // printf("get_file_location: start\n");
    // printf("get_file_location: map=%p, filename=%s, out=%p\n", map, filename, out);
    
    if (map == NULL) {
        printf("get_file_location: map is NULL!\n");
        return 0;
    }
    
    if (filename == NULL) {
        printf("get_file_location: filename is NULL!\n");
        return 0;
    }
    
    long hash = hash_fucn(filename);
    
    int index = abs(hash) % map->size;
    
    Hashnode *current = map->buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            // printf("get_file_location: found! copying location\n");
            *out = current->location;
            printf("Filename: %s IP: %s Port: %d\n",filename,out->ip,out->ss_port);
            return 1;
        }
        current = current->next;
    }
    return 0;
}

int delete_file(Hashmap *map,char *filename){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];
    Hashnode *prev = NULL;

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            
            if (prev == NULL) {
                map->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }

            free(current->filename);             
            free(current);
            printf("Deleted the file from the database\n");
            return 1; // Done
        }
        prev = current;
        current = current->next;
    }
    return -1;
}

void free_hashmap(Hashmap *map) {
    if (map == NULL) return;

    for (int i = 0; i < map->size; i++) {
        Hashnode *current = map->buckets[i];
        while (current != NULL) {
            Hashnode *temp = current;
            current = current->next;
            
            free(temp->filename);
            free(temp);
        }
    }
    
    free(map->buckets); 
    free(map);          
}
int alr_has_access(rw_access *rw , char *username){
    rw_access *it = rw;
    while(rw){
        if(strcmp(it->username,username)==0)
            return 1;
        rw=rw->next;
    }
    return -1;

}
int add_r_access(Hashmap *map,char *filename,char *username){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            
            rw_access *read_a = (rw_access *)malloc(sizeof(rw_access));
            if(alr_has_access(current->read,username)==1)
                return 1;
            strcpy(read_a->username,username);
            read_a->next=current->read;
            current->read = read_a;
            printf("Added the read access to the file %s for the user %s\n",filename,username);
            return 1;
        }
        current = current->next;
    }
    return -1;

}
int add_file_to_user(char *filename, char *username, userdatabase *users){
    int i = 0;
    int n = users->num_of_users;
    for (int i=0;i<n;i++){
        if(strcmp(username,users->username_arr[i].username)==0){

            filename_foruser *add_file = (filename_foruser *)malloc(sizeof(filename_foruser));
            add_file->next = users->username_arr[i].files;
            strcpy(add_file->filename,filename);
            users->username_arr[i].files = add_file;
            printf("Added filename: %s to the files_list of the %s\n",filename,username);
            return 1;
        }
    }
    return -1;
}
int add_w_access(Hashmap *map,char *filename,char *username){
    if(add_r_access(map,filename,username)==-1)
        return -1;
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            if(alr_has_access(current->write,username)==1)
                return 1;
            rw_access *write_a = (rw_access *)malloc(sizeof(rw_access));
            strcpy(write_a->username,username);
            write_a->next=current->write;
            current->write = write_a;
            printf("Added the write access to the file %s for the user %s\n",filename,username);
            return 1;
        }
        current = current->next;
    }
    return -1;
}
int rem_access(Hashmap *map,char *filename,char *username){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];
    int flag = 0;
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            //for read
            rw_access *it=current->read,*prev=NULL;
            while(it != NULL){
                if(strcmp(it->username,username)==0){
                    if(prev){
                        prev->next = it->next;
                        free(it);
                    }
                    else{
                        current->read = it->next;
                        free(it);
                    }
                    flag = 1;
                    break;

                }

                prev = it;
                it = it->next;
            }
            //for write
            it=current->write,prev=NULL;
            while(it != NULL){
                if(strcmp(it->username,username)==0){
                    if(prev){
                        prev->next = it->next;
                        free(it);
                    }
                    else{
                        current->write = it->next;
                        free(it);
                    }
                    flag = 1;
                    break;

                }

                prev = it;
                it = it->next;
            }

            if(flag){
                printf("Removed the perms for the user: %s for the file; %s",username,filename);
                return 1;
            }
            else
                return -1;
        }
        current = current->next;
    }
    return -1;
}
int can_read(Hashmap *map,char *filename,char *username){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            rw_access *it = current->read;
            while(it != NULL){
                if(strcmp(it->username,username)==0){
                    return 1;
                }
                it= it->next;
            }
            return -1;
        }
        current = current->next;
    }
    return -1;
}
int can_write(Hashmap *map,char *filename,char *username){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {

            rw_access *it = current->write;
            while(it != NULL){
                if(strcmp(it->username,username)==0){
                    return 1;
                }
                it= it->next;
            }
            return -1;
        }
        current = current->next;
    }
    return -1;
}

int delete_file_from_user(char *filename, char *username, userdatabase *users){
    int i = 0;
    int n = users->num_of_users;
    for (int i=0;i<n;i++){
        if(strcmp(username,users->username_arr[i].username)==0){
            filename_foruser *it = users->username_arr[i].files,*prev=NULL;
            while(it != NULL){
                if(strcmp(it->filename,filename)==0){
                    if(prev)
                        prev->next = it->next;
                    else
                        users->username_arr[i].files = it->next;
                    free(it);
                    return 1;
                }
                prev = it;
                it = it->next;
            }
            return -1;
        }
    }
    return -1;
}
void print(Hashmap *map){
    
    int size = map->size;
    for(int i=0;i<size;i++){
        Hashnode *curr = map->buckets[i];
        while(curr != NULL){
            printf("%s\n",curr->filename);
            curr = curr->next;
        }
    }


}
void print_details(char *filename, Hashmap *map){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            printf("Filename:%s\n",filename);
            printf("--> SS_IP : %s  SS_PORT : %d\n",current->location.ip,current->location.ss_port);
            //can read
            printf("Read users\n-->");
            rw_access *it = current->read;
            while(it){
                printf("%s ",it->username);
                it = it->next;
            }
            //can write
            printf("\nWrite users\n-->");
            it = current->write;
            while(it){
                printf("%s ",it->username);
                it = it->next;
            }
            printf("\n");
        }
        current = current->next;
    }
    return;

}
