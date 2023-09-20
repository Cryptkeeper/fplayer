#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdbool.h>
#include <stdint.h>

#include "sds.h"

sds sleepGetStatus(void);

struct sleep_loop_config_t {
    long intervalMillis;
    bool (*sleep)(void);
};

void sleepTimerLoop(struct sleep_loop_config_t config);

#endif//FPLAYER_SLEEP_H
