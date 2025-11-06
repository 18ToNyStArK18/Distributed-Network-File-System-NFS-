#include <pthread.h>
#include <sys/types.h>

#define MAX_CONNS 20

typedef struct {
    off_t start;
    off_t end;
    pthread_rwlock_t lock;
} SentenceLock;

typedef struct {
    char filename[128];
    SentenceLock *sentences;
    int sentence_count;
} FileLockTable;

FileLockTable file_locks[MAX_CONNS];
int file_lock_count = 0;
pthread_mutex_t master_table_lock = PTHREAD_MUTEX_INITIALIZER;
