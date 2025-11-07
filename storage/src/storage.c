#include "../../name_server/inc/ip.h"
#include "../inc/storage.h"
#include "../../cmn_inc.h"
#include "../inc/locks.h"
#include <pthread.h>
#include <time.h>
#include <assert.h>

void* NS_listener_thread (void *arg);
void* Client_listener_thread (void *arg);
void* Handle_NS (void* arg);
void* Handle_Client (void* arg);
FileLockTable* get_or_build_sentence_table(const char *filename);
int Pack(Packet* pkt , char * buff);
void Unpack(char* buffer, uint32_t* flag, char** cmd_string); 

pthread_mutex_t FILES_C_AND_W = PTHREAD_MUTEX_INITIALIZER;

int main() {
    int server_fd, client_fd;
    struct sockaddr_in ns_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    socklen_t ns_len = sizeof(ns_addr);

    /************** 1. Create server socket **************/
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&ns_addr, 0, sizeof(ns_addr));
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_addr.s_addr = INADDR_ANY;   // Any local interface
    ns_addr.sin_port = 0;                   // OS picks a free port

    /************** 2. Bind **************/
    if (bind(server_fd, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /************** 3. Find assigned IP + port **************/
    if (getsockname(server_fd, (struct sockaddr *)&ns_addr, &ns_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }

    int ns_port = ntohs(ns_addr.sin_port);
    printf("[SS] NS socket listening on port %d\n", ns_port);

    listen(server_fd, MAX_CONNS);

    /************** 1. Create client socket **************/
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;   // Any local interface
    client_addr.sin_port = 0;                   // OS picks a free port

    /************** 2. Bind **************/
    if (bind(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    /************** 3. Find assigned IP + port **************/
    if (getsockname(client_fd, (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }

    int client_port = ntohs(client_addr.sin_port);
    printf("[SS] Client socket listening on port %d\n", client_port);

    listen(client_fd, MAX_CONNS);


    /************** 4. Register with Name Server **************/
    int ns_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_sock < 0) {
        perror("NS socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in ns_server_addr;
    ns_server_addr.sin_family = AF_INET;
    ns_server_addr.sin_port = htons(NS_PORT_SS);

    if (inet_pton(AF_INET, NS_IP, &ns_server_addr.sin_addr) <= 0) {
        perror("Invalid NS_IP");
        exit(EXIT_FAILURE);
    }

    if (connect(ns_sock, (struct sockaddr *)&ns_server_addr, sizeof(ns_server_addr)) < 0) {
        perror("Could not connect to Name Server");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in my_addr_for_ns;
    socklen_t my_addr_len = sizeof(my_addr_for_ns);
    if (getsockname(ns_sock, (struct sockaddr *)&my_addr_for_ns, &my_addr_len) < 0) {
        perror("getsockname on ns_sock failed");
        exit(EXIT_FAILURE);
    }
    char my_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &my_addr_for_ns.sin_addr, my_ip, sizeof(my_ip));
    // format: "REGISTER <ip> <ns_port> <client_port>"
    char reg_msg[64];
    printf("Storage Server running on %s:%d\n", my_ip, ns_port);
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER %s %d %d", my_ip, ns_port, client_port);

    Packet pkt;
    memset(&pkt, 0 , sizeof(pkt));
    strncpy(pkt.req_cmd, reg_msg, sizeof(pkt.req_cmd)-1);
    pkt.req_cmd[sizeof(pkt.req_cmd)-1] = '\0';
    pkt.REQ_FLAG = REG_SS;

    char buffer[BUFFER_SIZE];
    int bytes_to_send = Pack(&pkt, buffer);

    send(ns_sock, buffer, bytes_to_send, 0);
    printf("Sent registration to Name Server: %s\n", reg_msg);
    char recv_buff[BUFFER_SIZE];
    memset(recv_buff,0,BUFFER_SIZE);
    if(recv(ns_sock,recv_buff,BUFFER_SIZE,0)< 0){
        printf(RED"Error in recieving packet\n"NORMAL);
        return 0;
    }
    printf("Server says: %s\n",recv_buff);

    // TO ADD: sending the initial list of files in storage
    /*DIR* d;
    struct dirent *dir;
    d = opendir(".");

    if (d) {
        while ((dir = readdir(d)) != NULL) { 
            Packet p;
            memset(&p, 0, sizeof(p));
            strncpy(pkt.req_cmd, dir->d_name, sizeof(pkt.req_cmd)-1);
            pkt.req_cmd[sizeof(pkt.req_cmd)-1] = '\0';
            pkt.REQ_FLAG = REG_FILES;

            char file_buffer[BUFFER_SIZE];
            int file_bytes_to_send = Pack(&p, file_buffer);

            send(ns_sock, file_buffer, file_bytes_to_send, 0);
            printf("Sent file registration to Name Server: %s\n", dir->d_name);
            usleep(10000);
        }
    }
    */
    close(ns_sock);

    pthread_t ns_thread, client_thread;

    pthread_create(&ns_thread, NULL, NS_listener_thread, &server_fd);
    pthread_detach(ns_thread);

    pthread_create(&client_thread, NULL, Client_listener_thread, &client_fd);
    pthread_detach(client_thread);

    printf("[SS] Threads launched: NS & Client listeners active.\n");

    // main thread can now sleep forever
    while(1) sleep(1000);

    return 0;
}

// pack the struct into a buffer so that i can send the pckt to the name server

int Pack(Packet* pkt , char * buff) {
    memset(buff, 0 ,BUFFER_SIZE);
    char *ptr = buff;

    uint32_t flag = htonl(pkt->REQ_FLAG);
    memcpy(ptr,&flag,sizeof(uint32_t));

    ptr += sizeof(uint32_t);

    int cmd_len = strlen(pkt->req_cmd)+1;
    memcpy(ptr,pkt->req_cmd,cmd_len);
    
    ptr += cmd_len;
    return ptr - buff;
}

void Unpack(char* buffer, uint32_t* flag, char** cmd_string) {
    char *ptr = buffer;
    
    uint32_t flag_net;
    memcpy(&flag_net, ptr, sizeof(uint32_t));
    *flag = ntohl(flag_net); // Convert back from Network Order
    ptr += sizeof(uint32_t);
    
    *cmd_string = ptr; 
}


void* NS_listener_thread(void *arg) {
    int ns_listen_fd = *(int*)arg;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    while (1) {
        int ns_fd = accept(ns_listen_fd, (struct sockaddr*)&addr, &len);
        if (ns_fd < 0) continue;

        pthread_t tid;
        pthread_create(&tid, NULL, Handle_NS, (void*)(long)ns_fd);
        pthread_detach(tid);
    }
}

void* Client_listener_thread(void *arg) {
    int client_listen_fd = *(int*)arg;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    while (1) {
        int client_fd = accept(client_listen_fd, (struct sockaddr*)&addr, &len);
        if (client_fd < 0) continue;

        client_args_t* args = malloc(sizeof(client_args_t));
        inet_ntop(AF_INET, &addr.sin_addr, args->client_ip, INET_ADDRSTRLEN);
        args->client_socket = client_fd;

        printf("[SS] Client connected from %s:%d\n",
               args->client_ip, ntohs(addr.sin_port));

        pthread_t tid;
        pthread_create(&tid, NULL, Handle_Client, args);
        pthread_detach(tid);
    }
}

void* Handle_NS (void* arg) {
    int ns_fd = (int)(long)arg;
    char buffer[BUFFER_SIZE];
    // char *msg = "ACK - Command Received\n";

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int r = recv(ns_fd, buffer, sizeof(buffer), 0);

        if (r <= 0) {
            if (r == 0) { 
                printf("[Thread %ld] Name Server %s disconnected.\n", pthread_self(), NS_IP);
            }
            else { 
                perror("[Thread] recv failed\n");
            }
            break;
        }

        uint32_t flag = -1;
        char* cmd_string;
        Unpack(buffer, &flag, &cmd_string);

        printf("[Thread %ld] Name Server %s Flag: %u, Cmd: %s\n", pthread_self(), NS_IP, flag, cmd_string);
    
        
        if (flag == CREATE_REQ) {
            // create file
            printf("Recived the create cmd req\n");
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            pthread_mutex_lock(&FILES_C_AND_W);
            FILE *fp;
            int send_flag = -1;
            //need to check whether the file with the same name exists already
            fp=fopen(filename,"r");
            if(fp != NULL){
                send_flag = FILE_ALREADY_EXISTS;
                fclose(fp);
                pthread_mutex_unlock(&FILES_C_AND_W);
            }
            else{
                //file doesnt exists so i will create that file
                fp = fopen(filename,"w");
                assert(fp != NULL);
                send_flag = Success;
                pthread_mutex_unlock(&FILES_C_AND_W);
            }
            char temp_buffer[BUFFER_SIZE];
            temp_buffer[0]='\0';
            Packet pkt;
            pkt.REQ_FLAG = send_flag;
            int bytes_to_send = Pack(&pkt,temp_buffer);
            if(send(ns_fd,temp_buffer,bytes_to_send,0) < 0){
                printf(RED"ERROR in sending the ack\n"NORMAL);
            }

        }
        else if (flag == DELETE) {
            // delete file
            printf("Recived the delete cmd req\n");
            char filename[MAX_FILE_NAME_SIZE];
            strcpy(filename,cmd_string);
            pthread_mutex_lock(&FILES_C_AND_W);
            int send_flag = -1;
            if(remove(filename)==0){
                send_flag = Success;
            }
            else {
                send_flag = FILE_DOESNT_EXIST;
            }
            pthread_mutex_unlock(&FILES_C_AND_W);
            char temp_buffer[BUFFER_SIZE];
            temp_buffer[0]='\0';
            Packet pkt;
            pkt.REQ_FLAG = send_flag;
            int bytes_to_send = Pack(&pkt,temp_buffer);
            if(send(ns_fd,temp_buffer,bytes_to_send,0) < 0){
                printf(RED"ERROR in sending the ack\n"NORMAL);
            }
        }
        else if (flag == EXEC) {
            // send contents of file line by line
        }
    }

    close(ns_fd);
    pthread_exit(NULL);
}

void* Handle_Client (void* arg) {
    client_args_t* args = (client_args_t*)arg;
    int new_socket = args->client_socket;
    char client_ip[INET_ADDRSTRLEN];
    char buffer[BUFFER_SIZE] = {0};
    strcpy(client_ip, args->client_ip);
    // char msg[MAX_WORDS_IN_INP * MAX_WORD_SIZE];
    printf("Client IP: %s Client Port: %d\n", client_ip, new_socket);
    free(args);

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int r = recv(new_socket, buffer, sizeof(buffer), 0);

        if (r <= 0) {
            if (r == 0) { 
                printf("[Thread %ld] Client %s disconnected.\n", pthread_self(), client_ip);
            }
            else { 
                perror("[Thread] recv failed\n");
            }
            break;
        }

        uint32_t flag = -1;
        char* filename;
        Unpack(buffer, &flag, &filename); // client sends the filename in place of cmd_string as we know what cmd it is by the flag

        printf("[Thread %ld] Client %s Flag: %u, Cmd: %s\n", pthread_self(), client_ip, flag, filename);


        if (flag == READ_REQ_SS) {
            FILE* fp = fopen(filename, "r");
            if (fp == NULL) {
                perror("Error opening file");
                continue;
            }

            FileLockTable *table = get_or_build_sentence_table(filename);
            if (!table) { 
                fclose(fp);
                perror("get_or_build_sentence_table failed");
                continue;
            }

            char sentence[BUFFER_SIZE];
            int idx = 0, sentence_idx = 0;
            int c;
            bool lock_held = false;

            while (1) {

                if (idx == 0 && sentence_idx < table->sentence_count) {
                    pthread_rwlock_rdlock(&table->sentences[sentence_idx].lock);
                    lock_held = true;
                }

                c = fgetc(fp);
                if (c == EOF) {
                    if (lock_held) {
                        pthread_rwlock_unlock(&table->sentences[sentence_idx].lock);
                        lock_held = false;
                    }
                    break;
                }

                sentence[idx++] = c; // write after acquiring lock to prevent race

                // End of sentence detected
                if (c == '.' || c == '!' || c == '?' || c == '\n') {
                    sentence[idx] = '\0'; // null terminate sentence

                    Packet pkt;
                    memset(&pkt, 0, sizeof(pkt));
                    pkt.REQ_FLAG = READ_DATA;
                    char send_buff[BUFFER_SIZE];
                    strcpy(pkt.req_cmd, sentence);
                    int bytes_to_send = Pack(&pkt, send_buff);

                    if (send(new_socket, send_buff, bytes_to_send,0) < 0) {
                        perror("send failed");
                        if (lock_held) {
                            pthread_rwlock_unlock(&table->sentences[sentence_idx].lock);
                            lock_held = false;
                        }
                        break;
                    }

                    usleep(10000); //so that multiple packets will not be merged by tcp

                    if (lock_held) {
                        pthread_rwlock_unlock(&table->sentences[sentence_idx].lock);
                        lock_held = false;
                    }

                    idx = 0; // reset for next sentence
                    sentence_idx++;
                }
                
                if (idx >= BUFFER_SIZE - 2) {
                    sentence[idx] = '\0';

                    Packet pkt;
                    memset(&pkt, 0, sizeof(pkt));
                    pkt.REQ_FLAG = READ_DATA;

                    strcpy(pkt.req_cmd, sentence);
                    char send_buff[BUFFER_SIZE];
                    int bytes_to_send = Pack(&pkt, send_buff);
                    send(new_socket, send_buff, bytes_to_send,0);

                    // DON'T RELEASE LOCK HERE AS WE HAVE NOT FINISHED READING SENTENCE
                    // IT'S JUST THAT THE BUFFER IS FULL

                    idx = 0;
                }
            }
            if (idx > 0) {
                sentence[idx] = '\0'; // Null-terminate the last chunk
                Packet pkt;
                memset(&pkt, 0, sizeof(pkt));
                pkt.REQ_FLAG = READ_DATA;
                strcpy(pkt.req_cmd, sentence);

                char send_buff[BUFFER_SIZE];
                int bytes_to_send = Pack(&pkt, send_buff);
                send(new_socket, send_buff, bytes_to_send, 0);
                usleep(10000);
                // no need to release here as lock is already released at eof or after last sentence
            }

            fclose(fp);

            // Send END flag
            Packet end;
            memset(&end, 0, sizeof(end));
            end.REQ_FLAG = READ_END;

            char end_buff[BUFFER_SIZE];
            int bytes = Pack(&end, end_buff);
            usleep(10000);
            send(new_socket, end_buff, bytes,0);

            printf("Sent READ_END\n");
        }   

        else if (flag == WRITE_REQ) {
            // write file
        }
        else if (flag == STREAM) {
            // send contents of file line by line
        }
    }

    close(new_socket);
    pthread_exit(NULL);
}

FileLockTable* get_or_build_sentence_table(const char *filename) {
    pthread_mutex_lock(&master_table_lock);

    // Check if already exists
    for (int i = 0; i < file_lock_count; i++) {
        if (strcmp(file_locks[i].filename, filename) == 0) {
            pthread_mutex_unlock(&master_table_lock);
            return &file_locks[i];
        }
    }

    // Create new entry
    FileLockTable *entry = &file_locks[file_lock_count++];
    strcpy(entry->filename, filename);

    // First pass: count sentences
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        pthread_mutex_unlock(&master_table_lock);
        return NULL;
    }

    int c;
    off_t pos = 0, start = 0;
    entry->sentence_count = 0;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '.' || c == '!' || c == '?') {
            entry->sentence_count++;
        }
        pos++;
    }

    rewind(fp);

    // Allocate table
    entry->sentences = calloc(entry->sentence_count, sizeof(SentenceLock));
    int idx = 0;
    pos = 0; start = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (c == '.' || c == '!' || c == '?') {
            entry->sentences[idx].start = start;
            entry->sentences[idx].end = pos;
            pthread_rwlock_init(&entry->sentences[idx].lock, NULL);
            idx++;
            start = pos + 1;
        }
        pos++;
    }

    fclose(fp);
    pthread_mutex_unlock(&master_table_lock);
    return entry;
}
