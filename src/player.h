#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "std/fc.h"

typedef struct player_opts_t {
    uint8_t frameStepTimeOverrideMs;
    uint8_t connectionWaitS;
    bool precomputeFades;
} PlayerOpts;

void playerRun(FCHandle fc, const char *audioOverrideFilePath, PlayerOpts opts);

#endif//FPLAYER_PLAYER_H
