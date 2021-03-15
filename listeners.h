#ifndef LISTENERS_HEADER_TCHEV
#define LISTENERS_HEADER_TCHEV

#include "block.h"

void file_listener(Block* blk, const char* file, void (*callback)(Block*), pthread_cond_t* update_cond);
void aligned_time_listener(time_t align, time_t interval, Block* blk, void (*callback)(Block*), pthread_cond_t* update_cond);
void time_listener(time_t interval, Block* blk, void (*callback)(Block*), pthread_cond_t* update_cond);

void safe_callback(Block* blk, void (*callback)(Block*), pthread_cond_t* update_cond);

#endif // LISTENERS_HEADER_TCHEV