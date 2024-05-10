#ifndef FPLAYER_TIME_H
#define FPLAYER_TIME_H

#include <stdint.h>
#include <time.h>

typedef struct timespec timeInstant;

/// @brief Get the current system time.
/// @return current time
timeInstant timeGetNow(void);

/// @brief Return the time elapsed from `end` since the start time in nanoseconds.
/// @param start start time
/// @param end end time
/// @return time elapsed in nanoseconds
int64_t timeElapsedNs(timeInstant start, timeInstant end);

#endif//FPLAYER_TIME_H
