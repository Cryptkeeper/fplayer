#ifndef FPLAYER_TIME_H
#define FPLAYER_TIME_H

#include <stdint.h>
#include <time.h>

typedef struct timespec timeInstant;

timeInstant timeGetNow(void);

int64_t timeElapsedNs(timeInstant start, timeInstant end);

char *timeElapsedString(timeInstant start, timeInstant end);

#endif//FPLAYER_TIME_H
