#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdint.h>

struct FC;

typedef struct player_opts_t {
    uint8_t frameStepTimeOverrideMs;
    uint8_t connectionWaitS;
} PlayerOpts;

void playerRun(struct FC* fc,
               const char* audioOverrideFilePath,
               PlayerOpts opts);

#endif//FPLAYER_PLAYER_H
