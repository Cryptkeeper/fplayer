#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include <sds.h>

typedef struct player_opts_t {
    sds sequenceFilePath;
    sds channelMapFilePath;
    sds audioOverrideFilePath;
    uint8_t frameStepTimeOverrideMs;
    uint8_t connectionWaitS;
    bool precomputeFades;
} PlayerOpts;

void playerOptsFree(PlayerOpts *opts);

void playerRun(PlayerOpts opts);

sds playerGetRemaining(void);

#endif//FPLAYER_PLAYER_H
