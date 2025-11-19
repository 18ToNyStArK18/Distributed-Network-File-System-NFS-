#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/param.h>
#include "../../cmn_inc.h"

#define BUFFER_SIZE 1024
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[0m"

typedef struct{
    int client_socket;
    char client_ip[INET_ADDRSTRLEN];
} client_args_t;

#define MAX_FILES 128

typedef struct SentenceNode {
    char *text;
    pthread_rwlock_t lock;
    struct SentenceNode *next;    
} SentenceNode;

typedef struct {
    char filename[MAX_FILE_NAME_SIZE];
    SentenceNode *head;
    int writer_count;
    pthread_mutex_t list_lock, writer_count_lock;
} FileModel;

typedef struct {
    char *sentence_text;   // local working copy
    int sentence_index;    // where to apply changes
    bool delimiter_added;
} WriteSession;

typedef struct {
    int index;
    char words[BUFFER_SIZE];
} SentenceChanges;

FileModel* get_or_create_file_model(const char *filename);
WriteSession* start_write(FileModel* fm, int sentence_index);
void end_write(FileModel *fm, WriteSession *ws, FileModel *prev);
int update_sentence(SentenceNode *node, char *words, int word_index);
void print_file(FileModel *fm);
void delete_file(char *filename);
FileModel* get_or_create_prev_file_model(const char *filename);
void copy_LL(FileModel* src, FileModel* dst);
int save_to_disk(FileModel *fm,FileModel *prev);
