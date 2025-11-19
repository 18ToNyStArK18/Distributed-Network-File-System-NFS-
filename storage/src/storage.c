#include "../../name_server/inc/ip.h"
#include "../inc/storage.h"
#include "../../cmn_inc.h"
#include "../inc/locks.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>

int recv_all(int sock, void* buffer, int length);
int send_all(int sock, const void* buffer, int length);
void* NS_listener_thread (void *arg);
void* Client_listener_thread (void *arg);
void* Handle_NS (void* arg);
void* Handle_Client (void* arg);
FileLockTable* get_or_build_sentence_table(const char *filename);
int Pack(Packet* pkt , char * buff);
void Unpack(char* buffer, uint32_t* flag, char** cmd_string); 
int send_err(int sock){
    Packet pkt;
    pkt.REQ_FLAG = Fail;
    char buff[BUFFER_SIZE];

    int bytes_to_send = Pack(&pkt,buff);
    uint32_t net_len = htonl(bytes_to_send);
    send_all(sock,&net_len,sizeof(uint32_t));
    send_all(sock,buff,bytes_to_send);
    return 1;

}


pthread_mutex_t FILES_C_AND_W = PTHREAD_MUTEX_INITIALIZER;

// ========= GLOBAL FILE MODEL TABLE =========
FileModel *global_models[MAX_FILES];   // array of pointers
int global_model_count = 0;            // how many file models currently loaded
pthread_mutex_t global_models_lock = PTHREAD_MUTEX_INITIALIZER;
FileModel *prev_models[MAX_FILES];
int global_prev_count = 0;

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
    memset(&pkt, 0, sizeof(pkt));
    strncpy(pkt.req_cmd, reg_msg, sizeof(pkt.req_cmd)-1);
    pkt.req_cmd[sizeof(pkt.req_cmd)-1] = '\0';
    pkt.REQ_FLAG = REG_SS;

    char buffer[BUFFER_SIZE];
    int bytes_to_send = Pack(&pkt, buffer);

    uint32_t len_net = htonl(bytes_to_send);
    send_all(ns_sock, &len_net, sizeof(len_net));
    send_all(ns_sock, buffer, bytes_to_send);

    printf("Sent registration to Name Server: %s\n", reg_msg);
    
    printf("Sending all the files to the name_server\n");
    DIR *dir_stream;
    struct dirent *dir_entry;
    dir_stream = opendir("data");
    if (dir_stream == NULL) {
        perror("Could not open directory");
        return 1;
    }
    while ((dir_entry = readdir(dir_stream)) != NULL) {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }
        if (dir_entry->d_type == DT_REG) {
            Packet pkt_f;
            pkt_f.REQ_FLAG = ss_files;
            strcpy(pkt_f.req_cmd,dir_entry->d_name);
            bytes_to_send = Pack(&pkt_f,buffer);
            len_net = htonl(bytes_to_send);
            send_all(ns_sock , &len_net, sizeof(uint32_t));
            send_all(ns_sock, buffer , bytes_to_send);
            printf("%s\n", dir_entry->d_name);
        }
    }
    closedir(dir_stream);
    pkt.REQ_FLAG = ss_files_end;
    bytes_to_send = Pack(&pkt,buffer);
    len_net = htonl(bytes_to_send);
    send_all(ns_sock , &len_net, sizeof(uint32_t));
    send_all(ns_sock, buffer , bytes_to_send);
    printf("All files sent Succesfully\n");
    uint32_t ack_len_net;
    if (recv_all(ns_sock, &ack_len_net, sizeof(ack_len_net)) <= 0) {
        printf(RED "Error receiving response length\n" NORMAL);
        close(ns_sock);
        return 0;
    }
    uint32_t ack_len = ntohl(ack_len_net);

    char recv_buff[BUFFER_SIZE];
    recv_all(ns_sock, recv_buff, ack_len);
    printf("Server says: %s\n", recv_buff);

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
        uint32_t net_packet_len;

        if(recv_all(ns_fd,&net_packet_len,sizeof(uint32_t))<=0){
            printf("[Thread %ld] Name_server disconnected.\n",pthread_self());
            break;
        }
        uint32_t packet_len = ntohl(net_packet_len);

        if(packet_len > BUFFER_SIZE){
            printf(RED"Error: Packet too large\n"NORMAL);        
        }

        memset(buffer, 0, BUFFER_SIZE);

        if(recv_all(ns_fd,buffer,packet_len)<=0){
            printf("[Thread %ld] Name_server disconnected\n",pthread_self());
            break;
        }

        uint32_t flag = -1;
        char* cmd_string;
        Unpack(buffer, &flag, &cmd_string);

        printf("[Thread %ld] Name Server %s Flag: %u, Cmd: %s\n", pthread_self(), NS_IP, flag, cmd_string);


        if (flag == CREATE_REQ) {
            printf("Received CREATE request\n");

            char filename[MAX_FILE_NAME_SIZE];

            sprintf(filename,"data/%s",cmd_string);
            pthread_mutex_lock(&FILES_C_AND_W);

            FILE *fp = fopen(filename, "r");
            int send_flag;

            if (fp != NULL) {
                // File already exists
                fclose(fp);
                send_flag = FILE_ALREADY_EXISTS;
            } 
            else {
                // Create new file
                fp = fopen(filename, "w");
                if (fp == NULL) {
                    send_flag = Fail;
                } 
                else {
                    send_flag = Success;
                    fclose(fp);
                }
            }

            pthread_mutex_unlock(&FILES_C_AND_W);

            // Now send response to Name Server
            char temp_buffer[BUFFER_SIZE];
            Packet pkt;
            memset(&pkt, 0, sizeof(pkt));
            pkt.REQ_FLAG = send_flag;

            int bytes_to_send = Pack(&pkt, temp_buffer);

            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(ns_fd, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[SS] ERROR: Failed to send CREATE length.\n" NORMAL);
            }

            if (send_all(ns_fd, temp_buffer, bytes_to_send) < 0 ) {
                printf(RED "ERROR sending CREATE ack to NS\n" NORMAL);
            }

            printf("[SS] CREATE for file '%s' -> %s\n", filename, send_flag == Success ? "Success" : (send_flag == FILE_ALREADY_EXISTS ? "Already Exists" : "Fail"));
        }
        else if (flag == DELETE) {
            printf("Received DELETE request\n");

            char filename[MAX_FILE_NAME_SIZE];
            sprintf(filename,"data/%s",cmd_string);
            pthread_mutex_lock(&FILES_C_AND_W);

            int send_flag;
            if (remove(filename) == 0) {
                send_flag = Success;
            } 
            else {
                send_flag = FILE_DOESNT_EXIST;
            }

            pthread_mutex_unlock(&FILES_C_AND_W);

            Packet pkt;
            memset(&pkt, 0, sizeof(pkt));
            pkt.REQ_FLAG = send_flag;

            char temp_buffer[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt, temp_buffer);

            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(ns_fd, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "[SS] ERROR: Failed to send DELETE length.\n" NORMAL);
            }

            if (send_all(ns_fd, temp_buffer, bytes_to_send) < 0) {
                printf(RED "ERROR sending DELETE ack to NS\n" NORMAL);
            }
            //delete that file_pointer also
            delete_file(filename);
            printf("[SS] DELETE request for '%s' -> %s\n", filename, send_flag == Success ? "Success" : "File Does Not Exist");
        }
        else if (flag == EXEC) {
            // send contents of file line by line
            char filename[MAX_FILE_NAME_SIZE];
            sprintf(filename,"data/%s",cmd_string);
            FILE *fp = fopen(filename,"r");
            char line_buffer[1000];
            Packet pkt;
            pkt.REQ_FLAG = EXEC_DATA;
            char send_buff[BUFFER_SIZE];
            int payload;
            while(fgets(line_buffer,sizeof(line_buffer),fp)){
                strcpy(pkt.req_cmd,line_buffer);
                payload = Pack(&pkt,send_buff);
                uint32_t net_len = htonl(payload);
                send_all(ns_fd,&net_len,sizeof(net_len));
                send_all(ns_fd,send_buff,payload);
            }
            pkt.REQ_FLAG = EXEC_END;
            payload = Pack(&pkt,send_buff);
            uint32_t net_len = htonl(payload);
            send_all(ns_fd,&net_len,sizeof(net_len));
            send_all(ns_fd,send_buff,payload);
            printf("Sent the entire file\n");
        }

        else if (flag == UNDO) {
            char filename[MAX_FILE_NAME_SIZE];
            sprintf(filename,"data/%s",cmd_string);
            Packet pkt;

            for (int i = 0 ; i < global_model_count ; i++) {
                if (strcmp(global_models[i]->filename, filename) == 0 ) {
                    if (1) {
                        pthread_mutex_lock(&global_models_lock);
                        copy_LL(prev_models[i], global_models[i]);
                        pthread_mutex_lock(&global_models[i]->writer_count_lock);
                        int count = global_models[i]->writer_count;
                        pthread_mutex_unlock(&global_models[i]->writer_count_lock);
                        if(count == 0)
                            save_to_disk(global_models[i],prev_models[i]);
                        pthread_mutex_unlock(&global_models_lock);
                        pkt.REQ_FLAG = Success;
                    }
                    else {
                        pkt.REQ_FLAG = Fail;
                    }

                    break;
                }
            }
            
            char send_buff[BUFFER_SIZE];
            int bytes_to_send = Pack(&pkt, send_buff);
            uint32_t net_len = htonl(bytes_to_send);
            if (send_all(ns_fd, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "Failed to send UNDO Success length\n" NORMAL);
            } 
            if (send_all(ns_fd, send_buff, bytes_to_send) <= 0) {
                printf(RED "Failed to send UNDO Success\n" NORMAL);
            }
            printf("Sent UNDO Success\n");
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

    while (1) {
        // --- Read length header (4 bytes) ---
        uint32_t net_len;
        int r = recv_all(new_socket, &net_len, sizeof(net_len));
        if (r <= 0) {
            if (r == 0) {
                printf("[Thread %ld] Client %s disconnected.\n", pthread_self(), client_ip);
            } 
            else {
                perror("[Thread] recv_all(len) failed");
            }
            break;
        }

        uint32_t body_len = ntohl(net_len);            // size of (flag + payload)
        if (body_len > BUFFER_SIZE) {
            fprintf(stderr, "Packet too large: %u\n", body_len);
            break;
        }

        // --- Read packet body (flag + payload) ---
        char body[BUFFER_SIZE];
        if (recv_all(new_socket, body, body_len) <= 0) {
            perror("[Thread] recv_all(body) failed");
            break;
        }

        uint32_t flag = (uint32_t)-1;
        char* file = NULL;
        Unpack(body, &flag, &file);
        char filename[MAX_FILE_NAME_SIZE];
        sprintf(filename,"data/%s",file);
        printf("[Thread %ld] Client %s Flag: %u, Cmd: %s\n", pthread_self(), client_ip, flag, strlen(filename) ? filename : "(null)");

        if (flag == READ_REQ_SS) {
            FileModel *fm = get_or_create_file_model(filename);
            if (!fm) {
                printf(RED "[SS] failed to load file model in READ\n" NORMAL);
                continue;
            }
            char temp_file[MAX_FILE_NAME_SIZE], prev_filename[MAX_FILE_NAME_SIZE + 10];
            int temp;
            sscanf(file, "%s %d", temp_file, &temp);
            sprintf(prev_filename, "tmp/%s", temp_file);

            FileModel* prev_fm = get_or_create_prev_file_model(prev_filename); 

            pthread_mutex_lock(&fm->list_lock);

            SentenceNode* cur = fm->head;
            int sentence_idx = 0;

            pthread_mutex_unlock(&fm->list_lock);

            while (cur) {
                pthread_rwlock_rdlock(&cur->lock);

                int len = 0, offset = 0;
                if (cur->text) len = strlen(cur->text);
            
                while (offset < len) {
                    int chunk = MIN(BUFFER_SIZE - 1, len - offset);

                    Packet pkt;
                    memset(&pkt, 0, sizeof(pkt));
                    pkt.REQ_FLAG = READ_DATA;
                    memcpy(pkt.req_cmd, cur->text + offset, chunk);
                    pkt.req_cmd[chunk] = '\0';

                    char send_buff[BUFFER_SIZE];
                    int bytes_to_send = Pack(&pkt, send_buff);
                    uint32_t net_len = htonl(bytes_to_send);

                    if (send_all(new_socket, &net_len, sizeof(net_len)) < 0) {
                        printf(RED "[SS] Failed to send READ DATA length\n" NORMAL);
                        break;;
                    }

                    if (send_all(new_socket, send_buff, bytes_to_send) < 0) {
                        printf(RED "[SS] Failed to send READ_DATA\n", NORMAL);
                        break;
                    }

                    offset += chunk;
                }

                pthread_rwlock_unlock(&cur->lock);
                cur = cur->next;
            }

            // Send READ_END
            Packet end;
            memset(&end, 0, sizeof(end));
            end.REQ_FLAG = READ_END;

            char end_buff[BUFFER_SIZE];
            int end_len = Pack(&end, end_buff);
            uint32_t net_len = htonl(end_len);

            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "ERROR: Failed to send READ length.\n" NORMAL);
            }
            send_all(new_socket, end_buff, end_len);

            printf("Sent READ_END\n");
        }
        else if (flag == WRITE_REQ) {
            // write file
            char write_filename[MAX_FILE_NAME_SIZE];
            int sentence_idx;
            SentenceChanges changes[1000];
            int changes_indx = 0;
            sscanf(filename, "%s %d", write_filename, &sentence_idx); // because first line of WRITE is file + sentence idx
            printf("[Thread %ld] Writiing to file: %s, sentence: %d\n", pthread_self(), write_filename, sentence_idx);

            char content[BUFFER_SIZE];

            while (1) { 
                if (recv_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                    printf(RED "ERROR: Failed to read WRITE length.\n" NORMAL);
                }

                uint32_t content_len = ntohl(net_len);
                if (recv_all(new_socket, content,content_len) <= 0) {
                    printf(RED "ERROR: Failed to read WRITE content.\n" NORMAL);
                }

                uint32_t flag;
                char* payload = NULL;

                Unpack(content, &flag, &payload);
                if(strncmp(payload, "ETIRW",5)==0) {
                    printf("End of WRITE session for file: %s", write_filename);
                    break;
                }

                int word_index;
                char words[BUFFER_SIZE];
                sscanf(payload, "%d %[^\n]", &word_index, words);
                changes[changes_indx].index = word_index;
                strcpy(changes[changes_indx++].words,words);
                // save all of this in an array of structs so that it can be used to change the sentence later
            }
            char temp_file[MAX_FILE_NAME_SIZE], prev_filename[MAX_FILE_NAME_SIZE + 10];
            int temp;
            sscanf(file, "%s %d", temp_file, &temp);
            sprintf(prev_filename, "tmp/%s", temp_file);

            FileModel *fm = get_or_create_file_model(write_filename);
            FileModel* prev_fm = get_or_create_prev_file_model(prev_filename); 
            printf("A\n");
            if (!fm) {
                printf(RED "[SS] ERROR: Failed to load the file model\n" NORMAL);
                send_err(new_socket);
                continue;
            }


            SentenceNode *target = fm->head, *prev = NULL;
            int idx = 0;

            while (target && idx < sentence_idx) {
                prev = target;
                target = target->next;
                idx++;
            }

            if (!target) {
                if (idx == sentence_idx) {
                    // appending a new sentence
                    SentenceNode *append = calloc(1, sizeof(SentenceNode));
                    append->text = NULL;
                    pthread_rwlock_init(&append->lock, NULL);
                    append->next = NULL;
                    if (prev) {
                        prev->next = append;
                    }
                    else {
                        fm->head = append;
                    }
                    target = append;
                }
                else {
                    printf("Sentence index out of range\n");
                    send_err(new_socket);
                    continue;
                }
            }
            //no need to lock i feel

            WriteSession *ws = start_write(fm, sentence_idx);
            if (!ws) {
                printf(RED "[SS] ERROR: Failed to start write session\n" NORMAL);
                send_err(new_socket);
                continue;
            }
             
            printf("B\n");
            // copy from fm to prev_fm
            copy_LL(fm, prev_fm);
            printf("C\n");
            pthread_rwlock_wrlock(&target->lock);
            // actually make the changes to the sentence
            int err = 0;
            for(int i=0;i<changes_indx;i++){
                // change one by one 
                //func(sentence,word,wordindx) --> iterate until that word indx and update that sentence and check for the delimters and change the sentences linked list
                if(update_sentence(target, changes[i].words, changes[i].index)==-1){
                    pthread_rwlock_unlock(&target->lock);
                    // decremet the writers and then change the file pointer to prev state;
                    send_err(new_socket);   
                    err =1;
                    break;
                }
                print_file(fm);
            }
            if(err)
                continue;
            pthread_rwlock_unlock(&target->lock);

            end_write(fm, ws, prev_fm);
            Packet send_pkt;
            send_pkt.REQ_FLAG = Success;
            char send_buffer[BUFFER_SIZE];
            int bytes_to_send = Pack(&send_pkt,send_buffer);
            uint32_t net_len2 = htonl(bytes_to_send);
            send_all(new_socket,&net_len2,sizeof(uint32_t));
            send_all(new_socket,send_buffer,bytes_to_send);
            //end write to update the number of writer on this file
            printf("update\n");
        }
        else if (flag == STREAM) {
            // send contents of file line by line
            FileModel *fm = get_or_create_file_model(filename);
            if (!fm) {
                printf(RED "[SS] failed to load file model in READ\n" NORMAL);
                continue;
            }

            char temp_file[MAX_FILE_NAME_SIZE], prev_filename[MAX_FILE_NAME_SIZE + 10];
            int temp;
            sscanf(file, "%s %d", temp_file, &temp);
            sprintf(prev_filename, "tmp/%s", temp_file);

            FileModel* prev_fm = get_or_create_prev_file_model(prev_filename); 

            pthread_mutex_lock(&fm->list_lock);

            SentenceNode* cur = fm->head;

            pthread_mutex_unlock(&fm->list_lock);

            while (cur) {
                pthread_rwlock_rdlock(&cur->lock);

                char *text = cur->text;
                if (!text) {
                    pthread_rwlock_unlock(&cur->lock);
                    cur = cur->next;
                    continue;
                }

                int len = strlen(text), pos = 0, w = 0;

                char word[BUFFER_SIZE];

                while (pos < len) {
                    word[w++] = text[pos];

                    if (text[pos] == ' ' || pos == len - 1) {
                        word[w] = '\0';

                        Packet pkt;
                        memset(&pkt, 0, sizeof(pkt));
                        pkt.REQ_FLAG = STREAM_DATA;
                        strncpy(pkt.req_cmd, word, sizeof(pkt.req_cmd) - 1);

                        char send_buff[BUFFER_SIZE];
                        int bytes_to_send = Pack(&pkt, send_buff);
                        uint32_t net_len = htonl(bytes_to_send);

                        if (send_all(new_socket, &net_len, sizeof(net_len)) < 0) {
                            printf(RED "[SS] Failed to send STREAM DATA length\n" NORMAL);
                            break;
                        }

                        if (send_all(new_socket, send_buff, bytes_to_send) < 0) {
                            printf(RED "[SS] Failed to send STREAM_DATA\n", NORMAL);
                            break;
                        }

                        usleep(100000);

                        w = 0;
                    }

                    pos++;
                }

                pthread_rwlock_unlock(&cur->lock);
                cur = cur->next;
            }

            // Send STREAM_END
            Packet end;
            memset(&end, 0, sizeof(end));
            end.REQ_FLAG = STREAM_END;

            char end_buff[BUFFER_SIZE];
            int end_len = Pack(&end, end_buff);
            uint32_t net_len = htonl(end_len);

            if (send_all(new_socket, &net_len, sizeof(net_len)) <= 0) {
                printf(RED "ERROR: Failed to send STREAM length.\n" NORMAL);
            }
            send_all(new_socket, end_buff, end_len);

            printf("Sent STREAM_END\n");
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
    entry->versions = calloc(entry->sentence_count, sizeof(int));
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
