#include "shared_mutex.h"

shared_mutex_t lock;

int destory() {
    int res1 = pthread_mutex_destroy(&lock.exclusive_lock);
    int res2 = pthread_mutex_destroy(&lock.shared_lock);
    if (res1 == 0 && res2 == 0) {
        return 0;
    }

    return 1;
}

void lock() {
    // this is for the exclusive lock we need to check if the shared lock is not able to be locked
    // I think the shared lock must be an infinite semaphore right now since we can technically have infinite readers (although most likely not the case)
    // maybe I should cap it to like int64 or something but that is a design 
    // i think for now I will make it non infinite semaphore but we can convert it later
    if (pthread_mutex_trylock(&k))

}

void unlock() {
    pthread_mutex_unlock(&lock.exclusive_lock)
}

void lock_shared() {
    // the condition we can acquire this lock if no one has acquired the exclusive lock and as many people can get the shared lock
     

}