#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

struct FC;

struct player_s {
    struct FC* fc;       /* file controller instance */
    char* audiofp;       /* audio file, if NULL attempts to load from fc */
    unsigned int wait_s; /* wait time in seconds before starting */
};

int PL_play(struct player_s* player);

#endif//FPLAYER_PLAYER_H
