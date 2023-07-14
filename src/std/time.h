#ifndef FPLAYER_TIME_H
#define FPLAYER_TIME_H

#include <stdint.h>
#include <time.h>

#include <sds.h>

typedef struct timespec timeInstant;

timeInstant timeGetNow(void);

int64_t timeElapsedNs(timeInstant start, timeInstant end);

sds timeElapsedString(timeInstant start, timeInstant end);

#endif//FPLAYER_TIME_H
