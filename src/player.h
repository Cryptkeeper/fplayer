#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct player_opts_t {
    char *sequenceFilePath;
    char *audioOverrideFilePath;
    uint8_t frameStepTimeOverrideMillis;
    char *channelMapFilePath;
} PlayerOpts;

bool playerInit(PlayerOpts opts);

#endif//FPLAYER_PLAYER_H
