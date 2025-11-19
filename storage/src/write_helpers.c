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
    pthread_rwlock_init(&fm->for_delete, NULL);

    // TO ADD: load the original file content into LL (can add in WRITE also, let's see)
    FILE *fp = fopen(filename, "r");
    if (fp) {
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

FileModel* get_or_create_prev_file_model(const char *filename) {
    extern FileModel *prev_models[];
    extern int global_prev_count;
    extern  pthread_mutex_t global_models_lock;

    pthread_mutex_lock(&global_models_lock);
    for (int i = 0 ; i < global_prev_count ; i++) {
        if (strcmp(prev_models[i]->filename, filename) == 0) {
            FileModel *fm = prev_models[i];
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
    pthread_rwlock_init(&fm->for_delete, NULL);

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

    prev_models[global_prev_count++] = fm;
    pthread_mutex_unlock(&global_models_lock);
    return fm;
}

int save_to_disk(FileModel *fm, FileModel *prev) {
    if (!fm || !prev) return -1;

    pthread_mutex_lock(&prev->list_lock);
    FILE *fp = fopen(prev->filename, "w");
    if (!fp) {
        pthread_mutex_unlock(&prev->list_lock);
        return -1;
    }
    SentenceNode* current = prev->head;
    while (current) {
        if (current->text) {
            fprintf(fp, "%s", current->text);
        }
        current = current->next;
    }
    fclose(fp);
    pthread_mutex_unlock(&prev->list_lock);

    pthread_mutex_lock(&fm->list_lock); // optional acc to gpt, have to check
    fp = fopen(fm->filename, "w");
    if (!fp) {
        pthread_mutex_unlock(&fm->list_lock);
        return -1;
    }
    SentenceNode *cur = fm->head;
    while (cur) {
        if(cur->text) 
            fprintf(fp, "%s", cur->text);
        cur = cur->next;
    }
    fclose(fp);

    
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

void end_write(FileModel *fm, WriteSession *ws, FileModel *prev) {
    if (!ws) return;
    if(ws->sentence_text) free(ws->sentence_text);
    free(ws);

    pthread_mutex_lock(&fm->writer_count_lock);
    fm->writer_count--;
    int writers_left = fm->writer_count;
    pthread_mutex_unlock(&fm->writer_count_lock);
    if (writers_left == 0) {
        save_to_disk(fm, prev);
    }
}

int update_sentence(SentenceNode *node, char *words, int word_index) {
    char *sentence = node->text;
    int word_count = 0, len = 0, delimeter_count = 0, insert = -1, words_len = strlen(words);
    
    if(sentence)
        len = strlen(sentence);

    for (int i = 0 ; i < len ; i++) {
        if (word_count == word_index && insert == -1) insert = i;
        if (sentence && (sentence[i] == ' ' || sentence[i] == '.' || sentence[i] == '?' || sentence[i] == '!')) word_count++;
    }

    if (word_count == word_index && insert == -1) insert = len-1;

    if (word_index == 0)
        insert = 0;

    for (int i = 0 ; i < words_len ; i++) {
        if (words[i] == '.' || words[i] == '?' || words[i] == '!') delimeter_count++;
    }

    if (insert == -1) {
        return -1;
    }

    printf("dl:%d,wc%d,i%d\n",delimeter_count,word_count,insert);

    if (sentence) {
        if (delimeter_count == 0) {
            char *newbuf = malloc(len + words_len + 2);
            memcpy(newbuf, sentence, insert);
            
            if (insert == len - 1) {
                newbuf[insert] = ' ';
                memcpy(newbuf + insert + 1, words, words_len);
                memcpy(newbuf + insert + 1 + words_len, sentence + insert, len - insert);
                newbuf[len + words_len + 1] = '\0';
            }
            else {
                memcpy(newbuf + insert, words, words_len);
                memcpy(newbuf + insert + words_len, sentence + insert, len - insert);
                newbuf[len + words_len] = '\0';
            }

            free(node->text);
            node->text = newbuf;
            printf("%s\n",node->text);
        }

        else if (delimeter_count > 0) {
            bool first_time = true;       
            int prev_idx = 0;
            SentenceNode* cur = node, *prev = NULL;
            char *original_remainder = NULL;
            int remainder_len = 0;
            
            // Save the remainder of the original sentence before we modify it
            if (sentence && insert < len) {
                remainder_len = len - insert;
                original_remainder = malloc(remainder_len + 1);
                memcpy(original_remainder, sentence + insert, remainder_len);
                original_remainder[remainder_len] = '\0';
            }
            
            for (int i = 0 ; i < words_len ; i++) {
                if (words[i] == '.' || words[i] == '?' || words[i] == '!') {
                    if (first_time) {
                        char *newbuf = malloc(insert + i + 2);
                        memcpy(newbuf, sentence, insert);
                        if (insert == len - 1) {
                            newbuf[insert] = ' ';
                            memcpy(newbuf + insert + 1, words, i + 1);
                            newbuf[insert + 1 + i + 1] = '\0';
                        }
                        else {
                            memcpy(newbuf + insert, words, i + 1);
                            newbuf[insert + i + 1] = '\0';
                        }

                        free(cur->text);
                        cur->text = newbuf;

                        first_time = false;
                        prev_idx = i+1;

                        prev = cur;
                        cur = cur->next;
                    }
                    else {
                        if (cur) {
                            SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                            pthread_rwlock_init(&new_node->lock, NULL);
                            
                            new_node->next = cur;
                            prev->next = new_node;

                            char* newbuf = malloc(i - prev_idx + 2); 
                            memcpy(newbuf, words + prev_idx, i - prev_idx + 1);
                            newbuf[i - prev_idx + 1] = '\0';
                            // no need to concatenate anything as it is a whole sentence in itself

                            prev_idx = i+1;

                            new_node->text = newbuf;
                            prev = new_node;
                            cur = new_node->next;
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
                int rest_len = original_remainder ? strlen(original_remainder) : 0;

                char *merged = malloc(tail_len + rest_len + 1);
                memcpy(merged, words + prev_idx, tail_len);
                if (original_remainder) {
                    memcpy(merged + tail_len, original_remainder, rest_len);
                    merged[tail_len + rest_len] = '\0';
                } else {
                    merged[tail_len] = '\0';
                }

                if (cur) {
                    SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                    pthread_rwlock_init(&new_node->lock, NULL);
                    new_node->text = merged;
                    new_node->next = cur;
                    prev->next = new_node;
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

                    if (original_remainder && original_remainder[0] != '\0') {
                        int rest_len = strlen(original_remainder);
                        char *rest = malloc(rest_len + 1);
                        memcpy(rest, original_remainder, rest_len);
                        rest[rest_len] = '\0';
                        SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                        pthread_rwlock_init(&new_node->lock, NULL);
                        new_node->text = rest;
                        new_node->next = cur;
                        prev->next = new_node;
                    }
                }

                else {
                    if (original_remainder && original_remainder[0] != '\0') {
                        int rest_len = strlen(original_remainder);
                        char *rest = malloc(rest_len + 1);
                        memcpy(rest, original_remainder, rest_len);
                        rest[rest_len] = '\0';

                        SentenceNode *new_node = calloc(1, sizeof(SentenceNode));
                        pthread_rwlock_init(&new_node->lock, NULL);
                        new_node->text = rest;
                        new_node->next = NULL;
                        prev->next = new_node;
                    }
                }
            }
            
            if (original_remainder) {
                free(original_remainder);
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
void print_file(FileModel *fm){

    SentenceNode *it = fm->head;
    int i =0;
    while(it){
        printf("%d %s\n",i,it->text);
        it = it->next;
        i++;
    }
}
void delete_file(char *filename){

    extern FileModel *global_models[];
    extern int global_model_count;
    extern  pthread_mutex_t global_models_lock;

    extern FileModel *prev_models[];
    extern int global_prev_count;

    int i = 0;
    pthread_mutex_lock(&global_models_lock);
    while (i < global_model_count){
        if(strcmp(global_models[i]->filename,filename)==0)
            break;
        i++;
    }
    if(i == global_model_count){
        pthread_mutex_unlock(&global_models_lock);
        return;
    }
    FileModel *curr_file = global_models[i];
    pthread_rwlock_wrlock(&curr_file->for_delete);
    while(i+1<global_model_count){
        global_models[i] = global_models[i+1];
        prev_models[i] = prev_models[i+1];
        i++;
    }
    global_model_count--;
    global_prev_count--;
    pthread_mutex_unlock(&global_models_lock);
    SentenceNode *it =  curr_file->head,*prev=NULL;

    while(it){
        prev = it;
        it = it->next;
        pthread_rwlock_destroy(&prev->lock);
        if(prev->text)
            free(prev->text);
        free(prev);
    }
    pthread_mutex_destroy(&curr_file->list_lock);
    pthread_mutex_destroy(&curr_file->writer_count_lock);
    pthread_rwlock_unlock(&curr_file->for_delete);
    pthread_rwlock_destroy(&curr_file->for_delete);
    free(curr_file);

    curr_file = prev_models[i];
    prev = NULL;
    while(it){
        prev = it;
        it = it->next;
        pthread_rwlock_destroy(&prev->lock);
        if(prev->text)
            free(prev->text);
        free(prev);
    }
    pthread_mutex_destroy(&curr_file->list_lock);
    pthread_mutex_destroy(&curr_file->writer_count_lock);


    printf("Removed the file ram successfully\n");
}

SentenceNode* clone_list(SentenceNode *head) {
    if (!head) return NULL;
    if (!head->text) return NULL;
    SentenceNode *new_head = NULL;
    SentenceNode *tail = NULL;

    while (head && head->text) {
        SentenceNode *node = malloc(sizeof(SentenceNode));
        node->text = strdup(head->text);
        pthread_rwlock_init(&node->lock, NULL);
        node->next = NULL;

        if (!new_head)
            new_head = node;
        else
            tail->next = node;

        tail = node;
        head = head->next;
    }

    return new_head;
}

void free_list(SentenceNode *head) {
    while (head) {
        SentenceNode *next = head->next;
        pthread_rwlock_destroy(&head->lock);
        free(head->text);
        free(head);
        head = next;
    }
}

void copy_LL(FileModel* src, FileModel* dst) {
    pthread_mutex_lock(&src->list_lock);
    pthread_mutex_lock(&dst->list_lock);

    // Free old list in dst
    free_list(dst->head);
    // Deep clone from src
    dst->head = clone_list(src->head);
    pthread_mutex_unlock(&dst->list_lock);
    pthread_mutex_unlock(&src->list_lock);
}
