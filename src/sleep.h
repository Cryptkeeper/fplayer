#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdbool.h>
#include <time.h>

void timeGetNow(struct timespec *spec);

long timeMillisElapsed(const struct timespec *start,
                       const struct timespec *end);

void timeSetMillis(struct timespec *spec, long millis);

typedef bool (*sleep_fn_t)(void);

void sleepTimerLoop(sleep_fn_t sleep, long millis);

#endif//FPLAYER_SLEEP_H
