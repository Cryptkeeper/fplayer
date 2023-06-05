#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdbool.h>
#include <stdint.h>

void sleepGetDrift(char *b, int c);

typedef bool (*sleep_fn_t)(void);

typedef void (*overrun_fn_t)(int64_t ns);

void sleepTimerLoop(sleep_fn_t sleep, long millis, overrun_fn_t overrun);

#endif//FPLAYER_SLEEP_H
