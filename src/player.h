#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#include <sds.h>

typedef struct player_opts_t {
    char *sequenceFilePath;
    char *audioOverrideFilePath;
    uint8_t frameStepTimeOverrideMs;
    char *channelMapFilePath;
    uint8_t connectionWaitS;
    bool precomputeFades;
} PlayerOpts;

void playerOptsFree(PlayerOpts *opts);

sds playerGetRemaining(void);

void playerRun(PlayerOpts opts);

#endif//FPLAYER_PLAYER_H
