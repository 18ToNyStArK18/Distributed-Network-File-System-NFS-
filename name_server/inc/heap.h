#include "../cmn_inc.h"

#define INITIAL_CAP 20

typedef struct {
    char ss_ip[INET_ADDRSTRLEN];
    int client_port;
    int ns_port;
    int num_files;
} Node;

typedef struct {
    Node* arr;
    int size;
    int capacity;
} MinHeap;

void node_init(Node* out, char *ss_ip, int client_port, int ss_port);

MinHeap* heap_init();
void heap_push(MinHeap* h, Node x);
int heap_pop(MinHeap* h);
int heap_peek(MinHeap* h, Node* out);
void heap_fix(MinHeap *h, int index);