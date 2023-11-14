#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include "sds.h"

typedef struct player_opts_t {
    uint8_t frameStepTimeOverrideMs;
    uint8_t connectionWaitS;
    bool precomputeFades;
} PlayerOpts;

void playerRun(sds sequenceFilePath,
               sds audioOverrideFilePath,
               PlayerOpts opts);

#endif//FPLAYER_PLAYER_H
