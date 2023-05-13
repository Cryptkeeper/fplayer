#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdbool.h>

void sleepGetDrift(char *b, int c);

typedef bool (*sleep_fn_t)(void);

void sleepTimerLoop(sleep_fn_t sleep, long millis);

#endif//FPLAYER_SLEEP_H
