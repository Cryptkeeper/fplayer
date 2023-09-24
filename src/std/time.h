#ifndef FPLAYER_TIME_H
#define FPLAYER_TIME_H

#include <stdint.h>

#include <sds.h>

#ifdef _WIN32

// provide a portable stub with a custom prefix to avoid collisions
typedef struct fplayer_timespec {
    long tv_sec;
    long tv_nsec;
} timeInstant;

#else

#include <time.h>

typedef struct timespec timeInstant;

#endif

timeInstant timeGetNow(void);

int64_t timeElapsedNs(timeInstant start, timeInstant end);

sds timeElapsedString(timeInstant start, timeInstant end);

#endif//FPLAYER_TIME_H
