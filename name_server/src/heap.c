#include "../inc/heap.h"

void node_init(Node* out, char *ss_ip, int client_port, int ns_port) {
    strcpy(out->ss_ip, ss_ip);
    out->client_port = client_port;
    out->ns_port = ns_port;
    out->num_files = 0;
}

static int node_less(const Node *a, const Node *b) {
    if (a->num_files != b->num_files) return a->num_files < b->num_files;
    if (a->client_port != b->client_port) return a->client_port < b->client_port;
    int s = strcmp(a->ss_ip, b->ss_ip);
    if (s != 0) return s < 0;
    return a->ns_port < b->ns_port;
}

static void swap_nodes(Node *a, Node *b) {
    Node tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sift_up(MinHeap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (node_less(&h->arr[idx], &h->arr[parent])) {
            swap_nodes(&h->arr[idx], &h->arr[parent]);
            idx = parent;
        } else break;
    }
}

static void sift_down(MinHeap *h, int idx) {
    int n = h->size;
    while (1) {
        int l = 2 * idx + 1;
        int r = 2 * idx + 2;
        int smallest = idx;
        if (l < n && node_less(&h->arr[l], &h->arr[smallest])) smallest = l;
        if (r < n && node_less(&h->arr[r], &h->arr[smallest])) smallest = r;
        if (smallest == idx) break;
        swap_nodes(&h->arr[idx], &h->arr[smallest]);
        idx = smallest;
    }
}

MinHeap* heap_init() {
    MinHeap *h = malloc(sizeof(MinHeap));
    if (!h) return NULL;
    h->arr = malloc(sizeof(Node) * INITIAL_CAP);
    if (!h->arr) {
        free(h);
        return NULL;
    }
    h->size = 0;
    h->capacity = INITIAL_CAP;
    return h;
}

void heap_push(MinHeap* h, Node x) {

    for(int i=0;i<h->size;i++){
        if(strcmp(h->arr[i].ss_ip,x.ss_ip)==0){
            h->arr[i] = x;
            printf("Heap changed\n");
            return;
        }

    }

    if (!h) return;
    if (h->size >= h->capacity) {
        int newcap = h->capacity * 2;
        Node *tmp = realloc(h->arr, sizeof(Node) * newcap);
        if (!tmp) {
            return;
        }
        h->arr = tmp;
        h->capacity = newcap;
    }
    h->arr[h->size] = x; 
    sift_up(h, h->size);
    h->size++;
}

int heap_pop(MinHeap* h) {
    if (!h || h->size == 0) return -1;
    if (h->size == 1) {
        h->size = 0;
        return 0;
    }
    h->arr[0] = h->arr[h->size - 1];
    h->size--;
    sift_down(h, 0);
    return 0;
}

int heap_peek(MinHeap* h, Node *out) {
    if (!h || h->size == 0 || !out) return -1;
    *out = h->arr[0];
    return 0;
}

void heap_fix(MinHeap *h, int index) {
    sift_up(h, index);
    sift_down(h, index);
}
