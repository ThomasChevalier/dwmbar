#ifndef BLOCK_HEADER_TCHEV
#define BLOCK_HEADER_TCHEV

#include <pthread.h>

typedef struct {
    char  *icon;
    char  *text;
    char  *color;
} BlockData;

typedef struct {
    void* (*listener)(void*);
    BlockData data;
    char *string;
    int new;
    pthread_mutex_t mutex_data;
} Block;

#define BLOCK_DEF(listener) {listener, {NULL, NULL, NULL}, NULL, 0, PTHREAD_MUTEX_INITIALIZER}


#endif // BLOCK_HEADER_TCHEV