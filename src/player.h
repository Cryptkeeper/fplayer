#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

struct player_s {
    struct FC* fc;        /* file controller instance */
    char* audiofp;        /* audio file, if NULL attempts to load from fc */
    unsigned int waitsec; /* wait time in seconds before starting */
    struct cr_s* cmap;    /* channel map for hardware output addressing */
};

int Player_exec(struct player_s* player);

#endif//FPLAYER_PLAYER_H
