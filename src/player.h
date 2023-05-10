#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

#include <stdbool.h>

typedef struct player_opts_t {
    char *sequenceFilePath;
    char *audioOverrideFilePath;
} PlayerOpts;

bool playerInit(PlayerOpts opts);

#endif//FPLAYER_PLAYER_H
