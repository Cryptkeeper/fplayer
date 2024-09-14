/// @file sleep.h
/// @brief Adaptive sleep time controller for frame rate control.
#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdint.h>

struct sleep_coll_s;

/// @brief Allocates and initializes a new sleep collector with default values.
/// @param coll target sleep collector
/// @return 0 on success, a negative error code on failure
int Sleep_init(struct sleep_coll_s** coll);

/// @brief Returns the average sleep time in nanoseconds from the given sleep
/// collector. This is calculated by summing all sleep times in the collector
/// and dividing by the number of samples.
/// @param coll target sleep collector
/// @return the average sleep time in nanoseconds
int64_t Sleep_average(const struct sleep_coll_s* coll);

/// @brief Sleeps for the given number of milliseconds using incremental sleep
/// calls that estimate the time remaining to sleep using historical sleep data.
/// The remaining sleep time is performed using a primitive spin lock to ensure
/// the executed sleep time is accurate.
/// @param coll target sleep collector for recording historical sleep data
/// @param ms number of milliseconds to sleep
void Sleep_do(struct sleep_coll_s* coll, uint32_t ms);

#endif//FPLAYER_SLEEP_H
