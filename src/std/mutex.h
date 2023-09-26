#ifndef FPLAYER_MUTEX_H
#define FPLAYER_MUTEX_H

#ifdef ENABLE_PTHREAD

    #include <pthread.h>

typedef pthread_mutex_t file_mutex_t;

    #define file_mutex_lock(mutex)   pthread_mutex_lock(mutex)
    #define file_mutex_unlock(mutex) pthread_mutex_unlock(mutex)
    #define file_mutex_init()        ((file_mutex_t) PTHREAD_MUTEX_INITIALIZER)

#else

    #include <assert.h>

typedef int file_mutex_t;

    #define file_mutex_lock(mutex)   assert((*mutex)++ == 0)
    #define file_mutex_unlock(mutex) assert(--(*mutex) == 0)
    #define file_mutex_init()        ((file_mutex_t) 0)

#endif

#endif//FPLAYER_MUTEX_H
