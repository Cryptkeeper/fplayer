/// @file time.h
/// @brief Basic time tracking utility functions.
#ifndef FPLAYER_TIME_H
#define FPLAYER_TIME_H

#include <stdint.h>
#include <time.h>

/// @struct timeInstant
/// @brief Represents an instant moment in time.
/// @note Wrapper for \p timespec struct to simplify cross platform time tracking.
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
