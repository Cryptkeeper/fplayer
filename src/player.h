#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

struct player_s {
    struct FC* fc;        /* file controller instance */
    char* audiofp;        /* audio file, if NULL attempts to load from fc */
    unsigned int waitsec; /* wait time in seconds before starting */
    struct cr_s* cmap;    /* channel map for hardware output addressing */
};

/// @brief Initializes and starts playback of the given player configuration.
/// Execution will block until the sequence is complete, including audio playback.
/// @param player player configuration to execute
/// @return 0 on success, a negative error code on failure
int Player_exec(struct player_s* player);

#endif//FPLAYER_PLAYER_H
