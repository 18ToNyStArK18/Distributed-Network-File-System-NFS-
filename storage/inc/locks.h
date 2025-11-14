#include <pthread.h>
#include <sys/types.h>
#include "../../cmn_inc.h"

#define MAX_CONNS 20

typedef struct {
    off_t start;
    off_t end;
    pthread_rwlock_t lock;
} SentenceLock;

typedef struct {
    char filename[MAX_FILE_NAME_SIZE];
    SentenceLock *sentences;
    int sentence_count;
    int* versions;
} FileLockTable;

FileLockTable file_locks[MAX_CONNS];
int file_lock_count = 0;
pthread_mutex_t master_table_lock = PTHREAD_MUTEX_INITIALIZER;
