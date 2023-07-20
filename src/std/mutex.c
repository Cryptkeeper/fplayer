#include "mutex.h"

#ifndef ENABLE_PTHREAD
#undef NDEBUG
#include <assert.h>
#endif

void fileMutexInit(FileMutex *mutex, FILE *file) {
    *mutex = (FileMutex){
            .file = file,
#ifdef ENABLE_PTHREAD
            .mutex = PTHREAD_MUTEX_INITIALIZER,
#else
            .rc = 0,
#endif
    };
}

inline void fileMutexLock(FileMutex *const mutex, FILE **file) {
#ifdef ENABLE_PTHREAD
    pthread_mutex_lock(&mutex->mutex);
#else
    assert(mutex->rc++ == 0);
#endif

    *file = mutex->file;
}

inline void fileMutexUnlock(FileMutex *const mutex, FILE **file) {
#ifdef ENABLE_PTHREAD
    pthread_mutex_unlock(&mutex->mutex);
#else
    assert(--mutex->rc == 0);
#endif

    *file = NULL;
}

void fileMutexClose(FileMutex *mutex) {
#ifdef ENABLE_PTHREAD
    pthread_mutex_lock(&mutex->mutex);
#else
    assert(mutex->rc++ == 0);
#endif

    if (mutex->file != NULL) fclose(mutex->file);

    mutex->file = NULL;

#ifdef ENABLE_PTHREAD
    pthread_mutex_unlock(&mutex->mutex);
#else
    assert(--mutex->rc == 0);
#endif
}