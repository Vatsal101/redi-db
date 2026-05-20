#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

typedef struct {
    pthread_mutex_t exclusive_lock; 
    pthread_mutex_t shared_lock; 
} shared_mutex_t;


#endif SHARED_H