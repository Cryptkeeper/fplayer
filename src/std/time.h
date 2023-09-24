#ifndef FPLAYER_TIME_H
#define FPLAYER_TIME_H

#include <stdint.h>

#include <sds.h>

#ifdef _WIN32
struct timespec {
    long tv_sec;
    long tv_nsec;
};
#else
    #include <time.h>
#endif

typedef struct timespec timeInstant;

timeInstant timeGetNow(void);

int64_t timeElapsedNs(timeInstant start, timeInstant end);

sds timeElapsedString(timeInstant start, timeInstant end);

#endif//FPLAYER_TIME_H
