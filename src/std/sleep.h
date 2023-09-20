#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdbool.h>
#include <stdint.h>

#include "sds.h"

sds sleepGetStatus(void);

void sleepTimerLoop(long intervalMillis, bool (*sleep)(void));

#endif//FPLAYER_SLEEP_H
