#include "../inc/ns.h"
#include <stdint.h>
#include <sys/socket.h>

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
        printf("Invalid IP %s...\n",ss_ip);
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

int add_file(Hashmap *map, char *filename, char *ip, int port,char *username,int ns_ss_port){
    long hash = hash_fucn(filename);
    int idx = abs(hash) % map->size;

    Hashnode *current = map->buckets[idx];
    while(current != NULL){
        if(strcmp(filename,current->filename) == 0)
            return -1; //file name exists already

    }
    Hashnode *newnode = (Hashnode *)malloc(sizeof(Hashnode));
    newnode->location.ss_port = port;
    newnode->location.ns_ss_port = ns_ss_port;
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
int delete_file_from_every_user(char *filename,userdatabase *users){
    int i = 0;
    int n = users->num_of_users;
    for (int i=0;i<n;i++){
        filename_foruser *it = users->username_arr[i].files,*prev=NULL;
        while(it != NULL){
            if(strcmp(it->filename,filename)==0){
                if(prev)
                    prev->next = it->next;
                else
                    users->username_arr[i].files = it->next;
                free(it);
                break;
            }
            prev = it;
            it = it->next;
        }
    }
    return -1;
}
int delete_file(Hashmap *map,char *filename,userdatabase *users){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];
    Hashnode *prev = NULL;

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            delete_file_from_every_user(filename,users); 
            if (prev == NULL) {
                map->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }

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
            filename_foruser *it = users->username_arr[i].files;
            while(it){
                if(strcmp(it->filename,filename)==0)
                    return 1;
                it = it->next;
            }
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
            time_t current_time;
            time(&current_time);
            struct tm *local_time;
            local_time = localtime(&current_time);
            char time_string[100];
            strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M", local_time);
            strcpy(current->time,time_string);
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
            time_t current_time;
            time(&current_time);
            struct tm *local_time;
            local_time = localtime(&current_time);
            char time_string[100];
            strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M", local_time);
            strcpy(current->time,time_string);

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
            printf("%s ip: %s port(ns_SS): %d  port(client_ss): %d\n",curr->filename,curr->location.ip,curr->location.ns_ss_port,curr->location.ss_port);
            rw_access *it;
            it = curr->read;
            while(it){
                printf("%s\n",it->username);
                it = it->next;
            }
            it = curr->write;
            while(it){
                printf("%s\n",it->username);
                it=it->next;
            }
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
void print_file_data(Hashmap *map,char *filename,char *buffer){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while(current){
        if(strcmp(current->filename,filename)==0){
            //print the details of the file
            sprintf(buffer,"| %-13s | %5d | %5d | %-16s | %-10s |\n",filename,current->wc,current->chars,current->time,current->Owner);
            return;
        }
    }
    printf("NOOO\n");
}
void print_view(char *username, userdatabase *users, Hashmap *map, int a, int l, int socket){

    if(l){
        char temp_buff[1024];
        Packet pkt;
        pkt.REQ_FLAG = VIEW_DATA;
        sprintf(temp_buff,"| %-13s | %-5s | %-5s | %-16s | %-10s |\n","Filename","Words","Chars","Last Access Time","Owner");
        strcpy(pkt.req_cmd,temp_buff);
        int bytes_to_send = Pack(&pkt,temp_buff);
        uint32_t net_len = htonl(bytes_to_send);
        send_all(socket,&net_len,sizeof(net_len));
        send_all(socket,temp_buff,bytes_to_send);
    }
    if(!a){
        int n=users->num_of_users;
        for(int i=0;i<n;i++){
            if(strcmp(users->username_arr[i].username,username)==0){
                printf("Found the user\n");
                char buffer[2048],filename[MAX_FILE_NAME_SIZE];
                filename_foruser *fl = users->username_arr[i].files;
                Packet pkt;
                pkt.REQ_FLAG = VIEW_DATA;
                while(fl){
                    strcpy(filename,fl->filename);
                    if(!l){
                        sprintf(buffer,"-->%s\n",filename);
                    }
                    else{
                        print_file_data(map,filename, buffer);
                    }
                    strcpy(pkt.req_cmd,buffer);

                    int bytes_to_send = Pack(&pkt,buffer);
                    uint32_t net_len = htonl(bytes_to_send);

                    if(send_all(socket,&net_len,sizeof(net_len)) <=0){
                        printf(RED"ERROR\n"NORMAL);
                    }
                    if(send_all(socket,buffer,bytes_to_send) <= 0){
                        printf(RED"ERROR\n"NORMAL);
                    }
                    fl = fl->next;
                }
            }
        }
    }
    else{
        int sz = map->size;
        Packet pkt;
        pkt.REQ_FLAG = VIEW_DATA;
        char filename[MAX_FILE_NAME_SIZE];
        char buffer[2048];
        for(int i=0;i<sz;i++){
            Hashnode *current = map->buckets[i];
            while(current){
                strcpy(filename,current->filename);
                if(!l){
                    sprintf(buffer,"-->%s\n",filename);
                }
                else{
                    print_file_data(map,filename, buffer);
                }
                strcpy(pkt.req_cmd,buffer);

                int bytes_to_send = Pack(&pkt,buffer);
                uint32_t net_len = htonl(bytes_to_send);

                if(send_all(socket,&net_len,sizeof(net_len)) <=0){
                    printf(RED"ERROR\n"NORMAL);
                }
                if(send_all(socket,buffer,bytes_to_send) <= 0){
                    printf(RED"ERROR\n"NORMAL);
                }
                current = current->next;
            }
        }
    }
    Packet pkt;
    pkt.REQ_FLAG = VIEW_END;
    char buffer[BUFFER_SIZE];
    int bytes_to_send = Pack(&pkt,buffer);
    uint32_t net_len = htonl(bytes_to_send);

    if(send_all(socket,&net_len,sizeof(net_len)) <=0){
        printf(RED"ERROR\n"NORMAL);
    }
    if(send_all(socket,buffer,bytes_to_send) <= 0){
        printf(RED"ERROR\n"NORMAL);
    }
    printf("Success\n");
    return;
}
int is_owner(char *username, char *filename,Hashmap *map){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {

            if(strcmp(current->Owner,username)==0)
                return 1;
            return -1;
        }
        current = current->next;
    }
    return -1;
}
void print_info(Hashmap *map, char *filename, int socket){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            char buffer[1024];
            Packet pkt;
            pkt.REQ_FLAG = INFO_DATA;
            sprintf(buffer,"Filename: %s  Owner: %s  Wordcount: %d  Size: %d LAST-ACCESS: %s\nREAD -->",current->filename,current->Owner,current->wc,current->chars,current->time);
            strcpy(pkt.req_cmd,buffer);
            int bytes_to_send = Pack(&pkt,buffer);
            uint32_t net_len = htonl(bytes_to_send);

            if(send_all(socket,&net_len,sizeof(net_len)) <=0){
                printf(RED"ERROR\n"NORMAL);
            }
            if(send_all(socket,buffer,bytes_to_send) <= 0){
                printf(RED"ERROR\n"NORMAL);
            }

            rw_access *it = current->read;
            while(it){
                sprintf(buffer,"%s|",it->username);
                strcpy(pkt.req_cmd,buffer);
                bytes_to_send = Pack(&pkt,buffer);
                net_len = htonl(bytes_to_send);

                if(send_all(socket,&net_len,sizeof(net_len)) <=0){
                    printf(RED"ERROR\n"NORMAL);
                }
                if(send_all(socket,buffer,bytes_to_send) <= 0){
                    printf(RED"ERROR\n"NORMAL);
                }
                it=it->next;
            }
            sprintf(buffer,"\nWRITE-->");
            strcpy(pkt.req_cmd,buffer);
            bytes_to_send = Pack(&pkt,buffer);
            net_len = htonl(bytes_to_send);

            if(send_all(socket,&net_len,sizeof(net_len)) <=0){
                printf(RED"ERROR\n"NORMAL);
            }
            if(send_all(socket,buffer,bytes_to_send) <= 0){
                printf(RED"ERROR\n"NORMAL);
            }
            it = current->write; 
            while(it){
                sprintf(buffer,"%s|",it->username);
                strcpy(pkt.req_cmd,buffer);
                bytes_to_send = Pack(&pkt,buffer);
                net_len = htonl(bytes_to_send);

                if(send_all(socket,&net_len,sizeof(net_len)) <=0){
                    printf(RED"ERROR\n"NORMAL);
                }
                if(send_all(socket,buffer,bytes_to_send) <= 0){
                    printf(RED"ERROR\n"NORMAL);
                }
                it=it->next;
            }

            pkt.REQ_FLAG = INFO_END;
            bytes_to_send = Pack(&pkt,buffer);
            net_len = htonl(bytes_to_send);

            if(send_all(socket,&net_len,sizeof(net_len)) <=0){
                printf(RED"ERROR\n"NORMAL);
            }
            if(send_all(socket,buffer,bytes_to_send) <= 0){
                printf(RED"ERROR\n"NORMAL);
            }
            return;
        }
        current = current->next;
    }
    char buffer[BUFFER_SIZE];
    Packet pkt;
    int bytes_to_send;
    uint32_t net_len;
    pkt.REQ_FLAG = INFO_END;
    bytes_to_send = Pack(&pkt,buffer);
    net_len = htonl(bytes_to_send);
    if(send_all(socket,&net_len,sizeof(net_len)) <=0){
        printf(RED"ERROR\n"NORMAL);
    }
    if(send_all(socket,buffer,bytes_to_send) <= 0){
        printf(RED"ERROR\n"NORMAL);
    }
}
void execute_file(char *filename, char *ip, int port, int client_socket){
    int ss_socket;
    struct sockaddr_in ss_addr;
    if((ss_socket = socket(AF_INET,SOCK_STREAM,0)) < 0){
        printf("Failed\n");
        return;
    }
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(port);
    if(inet_pton(AF_INET,ip,&ss_addr.sin_addr) <= 0){
        printf("Invalid IP %s...\n",ip);
        return;
    }
    if(connect(ss_socket,(struct sockaddr *)&ss_addr,sizeof(ss_addr)) < 0){
        printf("Connection failed\n");
        return;
    }

    Packet pkt;
    pkt.REQ_FLAG = EXEC;
    strcpy(pkt.req_cmd,filename);
    char send_buff[BUFFER_SIZE];
    int payload = Pack(&pkt,send_buff);
    uint32_t net_len = htonl(payload);
    send_all(ss_socket,&net_len,sizeof(net_len));
    send_all(ss_socket,send_buff,payload);

    printf("Sent EXEC %s to the storage server %s:%d\n",filename,ip,port);
    char recv_buffer[BUFFER_SIZE];
    uint32_t reply_net_len;
    int reply_len;
    FILE *fp = fopen(filename,"w"); 
    while(1){
        recv_all(ss_socket,&reply_net_len,sizeof(reply_net_len));
        reply_len = ntohl(reply_net_len);
        recv_all(ss_socket,recv_buffer,reply_len);
        uint32_t flag;
        char *cmd_str;
        Unpack(recv_buffer,&flag,&cmd_str);
        if(flag == EXEC_DATA)
            fputs(cmd_str,fp);
        else
            break;
    }
    close(ss_socket);
    fclose(fp);

    chmod(filename,0700);
    printf("Got the entirefile and the execution of the file starts\n");
    char temp_file[MAX_FILE_NAME_SIZE];
    sprintf(temp_file,"/bin/bash %s",filename);
    FILE *pipe = popen(temp_file,"r");
    char line_buff[BUFFER_SIZE];
    Packet out_pkt;
    out_pkt.REQ_FLAG = EXEC_DATA;
    while(fgets(line_buff,sizeof(line_buff),pipe)){
        strcpy(out_pkt.req_cmd,line_buff);
        int bytes_to_send = Pack(&out_pkt,send_buff);
        net_len = htonl(bytes_to_send);
        send_all(client_socket,&net_len,sizeof(net_len));
        send_all(client_socket,send_buff,bytes_to_send);
    }
    pclose(pipe);
    out_pkt.REQ_FLAG = EXEC_END;
    int bytes_to_send = Pack(&out_pkt,send_buff);
    net_len = htonl(bytes_to_send);
    send_all(client_socket,&net_len,sizeof(net_len));
    send_all(client_socket, send_buff, bytes_to_send);
    return;
}

void find_ip_by_filename(char *filename, Hashmap *map, char* ip, int* port){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            strcpy(ip, current->location.ip);
            *port = current->location.ns_ss_port;
            return;
        }
        current = current->next;
    }
    return;
}

int is_file_present(char *filename, Hashmap *map){
    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;

    Hashnode *current = map->buckets[index];

    while(current){
        if(strcmp(filename,current->filename)==0)
            return -1;
        current = current->next;
    }
    return 1;

}

int update_filename(char *filename,Hashmap *map, int client_port , int ns_port){

    long hash = hash_fucn(filename);
    int index = abs(hash) % map->size;
    Hashnode *current = map->buckets[index];

    while(current){
        if(strcmp(current->filename,filename)==0){
            current->location.ns_ss_port = ns_port;
            current->location.ss_port = client_port;
            return 1;
        }
    }
    printf("Unexpected file registered\n");
    return -1;
}
