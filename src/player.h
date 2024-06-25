#ifndef FPLAYER_PLAYER_H
#define FPLAYER_PLAYER_H

struct qentry_s;

struct serialdev_s;

/// @brief Initializes and starts playback of the given playback configuration.
/// Execution will block until the sequence is complete, including audio
/// playback; unless an error occurs. The caller is responsible for freeing the
/// resources provided in the playback request.
/// @param req play request to execute
/// @param sdev serial device to use for playback
/// @return 0 on success, a negative error code on failure
int Player_exec(struct qentry_s* req, struct serialdev_s* sdev);

#endif//FPLAYER_PLAYER_H
