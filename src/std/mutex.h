#ifndef FPLAYER_MUTEX_H
#define FPLAYER_MUTEX_H

#include <stdbool.h>
#include <stdio.h>

#ifdef ENABLE_PTHREAD
#include <pthread.h>
#endif

typedef struct file_mutex_t {
    FILE *file;
#ifdef ENABLE_PTHREAD
    pthread_mutex_t mutex;
#else
    int rc;
#endif
} FileMutex;

void fileMutexInit(FileMutex *mutex, FILE *file);

void fileMutexLock(FileMutex *mutex, FILE **file);

void fileMutexUnlock(FileMutex *mutex, FILE **file);

void fileMutexClose(FileMutex *mutex);

#endif//FPLAYER_MUTEX_H
