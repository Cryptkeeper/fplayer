#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdbool.h>

char *sleepGetStatus(void);

void sleepTimerLoop(long intervalMillis, bool (*sleep)(void));

#endif//FPLAYER_SLEEP_H
