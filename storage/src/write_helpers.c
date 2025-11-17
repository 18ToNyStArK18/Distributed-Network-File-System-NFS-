#include "../inc/storage.h"

FileModel* get_or_create_file_model(const char *filename) {
    extern FileModel *global_models[];
    extern int global_model_count;
    extern  pthread_mutex_t global_models_lock;

    pthread_mutex_lock(&global_models_lock);
    for (int i = 0 ; i < global_model_count ; i++) {
        if (strcmp(global_models[i]->filename, filename) == 0) {
            FileModel *fm = global_models[i];
            pthread_mutex_unlock(&global_models_lock);
            return fm;
        }
    }

    FileModel *fm = calloc(1, sizeof(FileModel));
    strcpy(fm->filename, filename);
    fm->head = NULL;
    fm->writer_count = 0;
    pthread_mutex_init(&fm->list_lock, NULL);
    pthread_mutex_init(&fm->writer_count_lock, NULL);

    // TO ADD: load the original file content into LL (can add in WRITE also, let's see)
    FILE *fp = fopen(filename, "r");
    if (fp) {
        printf("file opened\n");
        SentenceNode *tail = NULL;
        char *sentence = NULL;
        size_t sent_cap = 0, sent_len =0;

        int c;
        while ((c = fgetc(fp)) != EOF) {
            if (sent_len + 2 >= sent_cap) {
                sent_cap = (sent_cap == 0 ? 64 : sent_cap * 2);
                sentence = realloc(sentence, sent_cap);
            }

            sentence[sent_len++] = c;

            if (c == '.' || c == '?' || c == '!' || c == '\n') {
                sentence[sent_len] = '\0';

                SentenceNode *node = calloc(1, sizeof(SentenceNode));
                node->text = strdup(sentence);
                pthread_rwlock_init(&node->lock, NULL);
                node->next = NULL;

                if (!fm->head) fm->head = node;
                else tail->next = node;

                tail = node;

                sent_len = 0;
            }
        }

        if (sent_len > 0) {
            sentence[sent_len] = '\0';

            SentenceNode *node = calloc(1, sizeof(SentenceNode));
            node->text = strdup(sentence);
            pthread_rwlock_init(&node->lock, NULL);
            node->next = NULL;
            if (!fm->head) fm->head = node;
            else tail->next = node;
        }

        free(sentence);
        fclose(fp);
    }

    global_models[global_model_count++] = fm;
    pthread_mutex_unlock(&global_models_lock);
    return fm;
}

int save_to_disk(FileModel *fm) {
    if (!fm) return -1;
    printf("a\n");
    pthread_mutex_lock(&fm->list_lock); // optional acc to gpt, have to check
    printf("b\n");
    FILE* fp = fopen(fm->filename, "w");
    printf("c\n");
    if (!fp) {
        pthread_mutex_unlock(&fm->list_lock);
        printf("d\n");
        return -1;
    }
    SentenceNode *cur = fm->head;
    while (cur) {
        printf("e\n");
        if(cur->text) 
            fprintf(fp, "%s", cur->text);
        cur = cur->next;
    }
    fclose(fp);
    printf("f\n");
    pthread_mutex_unlock(&fm->list_lock);
    return 0;
}

WriteSession* start_write(FileModel* fm, int sentence_index) {
    if (!fm) return NULL;

    WriteSession *ws = calloc(1, sizeof(WriteSession));
    ws->sentence_index = sentence_index;
    ws->delimiter_added = false;
    ws->sentence_text = NULL;

    pthread_mutex_lock(&fm->writer_count_lock);
    fm->writer_count++;
    pthread_mutex_unlock(&fm->writer_count_lock);

    return ws;
}

void end_write(FileModel *fm, WriteSession *ws) {
    if (!ws) return;
    if(ws->sentence_text) free(ws->sentence_text);
    free(ws);

    pthread_mutex_lock(&fm->writer_count_lock);
    fm->writer_count--;
    int writers_left = fm->writer_count;
    pthread_mutex_unlock(&fm->writer_count_lock);
    if (writers_left == 0) {
        save_to_disk(fm);
    }
}

int update_sentence(SentenceNode *node, char *words, int word_index) {
    word_index--;
    char *sentence = node->text;
    int word_count = 0, len = 0, delimeter_count = 0, insert = -1, words_len = strlen(words);
    
    if(sentence)
        len = strlen(sentence);
    else
        insert = 0;

    for (int i = 0 ; i < len ; i++) {
        if (word_count == word_index) insert = i + 1;
        if (sentence && sentence[i] == ' ') word_count++;
    }

    for (int i = 0 ; i < words_len ; i++) {
        if (words[i] == '.' || words[i] == '?' || words[i] == '!' || words[i] == '\n') delimeter_count++;
    }

    if (insert == -1) {
        return -1;
    }

    printf("dl:%d,wc%d,i%d\n",delimeter_count,word_count,insert);

    if (sentence) {
        if (delimeter_count == 0) {
            char *newbuf = malloc(len + strlen(words) + 2);
            memcpy(newbuf, sentence, insert);
            newbuf[insert] = ' ';
            strcat(newbuf + insert + 1, words);
            strcat(newbuf + insert + 1 + strlen(words), sentence + insert);

            free(node->text);
            node->text = newbuf;
            printf("%s\n",node->text);
        }

        else if (delimeter_count > 0) {
            bool first_time = true;       
            int prev_idx = 0;
            SentenceNode* cur = node, *prev = NULL;
            for (int i = 0 ; i < words_len ; i++) {
                if (words[i] == '.' || words[i] == '?' || words[i] == '!' || words[i] == '\n') {
                    if (first_time) {
                        char *newbuf = malloc(insert + i + 2);
                        memcpy(newbuf, sentence, insert);
                        memcpy(newbuf + insert, words, i + 1);
                        newbuf[insert + i + 1] = '\0';

                        free(cur->text);
                        cur->text = newbuf;

                        first_time = false;
                        prev_idx = i+1;

                        prev = cur;
                        cur = cur->next;
                    }
                    else {
                        if (cur) {
                            SentenceNode *after = cur->next;

                            SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                            pthread_rwlock_init(&new_node->lock, NULL);
                            new_node->next = after;

                            cur->next = new_node;
                            char* newbuf = malloc(i - prev_idx + 2); 
                            memcpy(newbuf, words + prev_idx, i - prev_idx + 1);
                            newbuf[i - prev_idx + 1] = '\0';
                            // no need to concatenate anything as it is a whole sentence in itself

                            prev_idx = i+1;

                            free(cur->text);
                            cur->text = newbuf;
                            prev = cur;
                            cur = cur->next;
                        }
                        else {
                            SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                            pthread_rwlock_init(&new_node->lock, NULL);
                            new_node->next = NULL;

                            char* newbuf = malloc(i - prev_idx + 2); 
                            memcpy(newbuf, words + prev_idx, i - prev_idx + 1);
                            newbuf[i - prev_idx + 1] = '\0';
                            // no need to concatenate anything as it is a whole sentence in itself

                            new_node->text = newbuf;
                            prev_idx = i+1;
                            prev->next = new_node;
                            prev = new_node;
                            cur = new_node->next;
                        }
                    }
                }
            }

            if (prev_idx < words_len) {
                // no delimiter at the end of words, need to merge it with the remainder of the original sentence
                int tail_len = words_len - prev_idx;
                int rest_len = strlen(sentence + insert);

                char *merged = malloc(tail_len + rest_len + 1);
                memcpy(merged, words + prev_idx, tail_len);
                memcpy(merged + tail_len, sentence + insert, rest_len + 1);

                if (cur) {
                    SentenceNode *after = cur->next;
                    SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                    pthread_rwlock_init(&new_node->lock, NULL);
                    new_node->text = merged;
                    new_node->next = after;
                    cur->next = new_node;
                }
                else {
                    SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                    pthread_rwlock_init(&new_node->lock, NULL);
                    new_node->text = merged;
                    new_node->next = NULL;
                    prev->next = new_node;
                }

            }
            else {
                if (cur) {
                    SentenceNode* after = cur->next;

                    if (*(sentence + insert) != '\0') {
                        int rest_len = strlen(sentence + insert);
                        char *rest = malloc(rest_len + 1);
                        memcpy(rest, sentence + insert, rest_len + 1);

                        SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                        pthread_rwlock_init(&new_node->lock, NULL);
                        new_node->text = rest;
                        new_node->next = after;
                        cur->next = new_node;
                    }
                }

                else {
                    if (*(sentence + insert) != '\0') {
                        int rest_len = strlen(sentence + insert);
                        char *rest = malloc(rest_len + 1);
                        memcpy(rest, sentence + insert, rest_len + 1);

                        SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                        pthread_rwlock_init(&new_node->lock, NULL);
                        new_node->text = rest;
                        new_node->next = NULL;
                        prev->next = new_node;
                    }
                }
            }
        }
    }

    else {
        // TO DO: inserting a sentence for the first time
        bool first_time = true;       
        int prev_idx = 0;
        SentenceNode* cur = node, *prev = NULL;
        for (int i = 0 ; i < words_len ; i++) {
            if (words[i] == '.' || words[i] == '?' || words[i] == '!' || words[i] == '\n') {
                if (first_time) {
                    printf("This reached\n");
                    char *newbuf = malloc(insert + i + 2); // insert = 0 over here
                                                           // memcpy(newbuf, sentence, insert);
                    memcpy(newbuf + insert, words, i + 1);
                    newbuf[insert + i + 1] = '\0';
                    cur->text = newbuf;
                    first_time = false;
                    prev_idx = i+1;
                    prev = cur;
                    cur = cur->next;
                }
                else {
                    printf("How the fuck this reached\n");
                    SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                    pthread_rwlock_init(&new_node->lock, NULL);
                    new_node->next = NULL;
                    char* newbuf = malloc(i - prev_idx + 2); 
                    memcpy(newbuf, words + prev_idx, i - prev_idx + 1);
                    newbuf[i - prev_idx + 1] = '\0';
                    // no need to concatenate anything as it is a whole sentence in itself
                    new_node->text = newbuf;
                    prev_idx = i+1;
                    prev->next = new_node;
                    prev = new_node;
                    cur = new_node->next;
                }
            }
        }
    }

    return 0;
}

// int session_append_chunk(WriteSession *ws, const char *chunk) {
//     size_t old = ws->sentence_text ? strlen(ws->sentence_text) : 0;
//     size_t add = strlen(chunk);
//     char *newbuf = realloc(ws->sentence_text, old + add + 1);
//     if (!newbuf) return -1;
//     memcpy(newbuf + old, chunk, add+1);
//     ws->sentence_text = newbuf;
//     return 0;
// }

// int apply_write(FileModel* fm, WriteSession* ws) {
//     if (!fm || !ws) return -1;

//     pthread_mutex_lock(&fm->list_lock);

//     SentenceNode *prev = NULL, *cur = fm->head;
//     int idx = 0;
//     while(cur && idx < ws->sentence_index) {
//         prev = cur;
//         cur = cur->next;
//         idx++;
//     }

//     if (cur == NULL) {
//         pthread_mutex_unlock(&fm->list_lock);
//         return -2;
//     }

//     pthread_rwlock_wrlock(&cur->lock);

//     char *new_text = ws->sentence_text ? strdup(ws->sentence_text) : strdup("");
//     if (!new_text) {
//         pthread_rwlock_unlock(&cur->lock);
//         pthread_mutex_unlock(&fm->list_lock);
//         return -1;
//     }

//     char *p = new_text;
//     char *start = p;
//     SentenceNode *last_inserted = NULL;
//     SentenceNode *first_new = NULL;

//     while (*p) {
//         if (*p == '.' || *p == '!' || *p == '?' || *p == '\n') {
//             size_t len = p - start + 1;
//             char *chunk = malloc(len + 1);
//            memcpy(chunk, start, len);
//            chunk[len] = '\0';

//            SentenceNode *node = calloc(1, sizeof(SentenceNode));
//            node->text = chunk;
//            pthread_rwlock_init(&node->lock, NULL);
//            node->next = NULL;

//            if (!first_new) first_new = node;
//            if (last_inserted) last_inserted->next = node;
//            last_inserted = node;

//            p++;
//            while (*p == ' ') p++;
//            start = p;
//         }
//         else {
//             p++;
//         }
//     }

//     if (start && *start) {
//         size_t len = strlen(start);
//         char *chunk = malloc(len + 2);
//         memcpy(chunk, start, len);
//         chunk[len] = '\0';
//         SentenceNode *node = calloc(1, sizeof(SentenceNode));
//         node->text = chunk;
//         pthread_rwlock_init(&node->lock, NULL);
//         node->next = NULL;
//         if (!first_new) first_new = node;
//         if (last_inserted) last_inserted->next = node;
//         last_inserted = node;
//     }

//     SentenceNode *after = cur->next;
//     free(cur->text);
//     pthread_rwlock_unlock(&cur->lock);
//     pthread_rwlock_destroy(&cur->lock);
//     free(cur);

//     if (prev) prev->next = first_new;
//     else fm->head = first_new;

//     if (last_inserted) last_inserted->next = after;

//     pthread_mutex_unlock(&fm->list_lock);

//     // Note: each new node already has its own rwlock initialized. If you maintain
//     // versions per-sentence, update them here (e.g. increment version counters).
//     // Example (pseudo):
//     // update_versions_for_inserted_nodes(fm, ws->sentence_index, number_of_new_nodes);

//     free(new_text);
//     return 0;
// }
