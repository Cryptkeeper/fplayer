#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdint.h>

#include "seq.h"

typedef struct player_opts_t {
    char *sequenceFilePath;
    char *audioOverrideFilePath;
    uint8_t frameStepTimeOverrideMs;
    char *channelMapFilePath;
    uint8_t connectionWaitS;
} PlayerOpts;

void playerOptsFree(PlayerOpts *opts);

void playerRun(PlayerOpts opts);

Sequence *playerGetPlaying(void);

#endif//FPLAYER_PLAYER_H
