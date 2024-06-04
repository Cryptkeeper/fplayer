#ifndef FPLAYER_PUTIL_H
#define FPLAYER_PUTIL_H

#include <stdint.h>

/// @brief Waits for the given number of seconds by blocking the current thread.
/// LOR heartbeat messages will intentionally be sent during this time. This
/// function is used to ensure the LOR hardware is connected to the player before
/// sending playback commands.
/// @param seconds number of seconds to wait
/// @return 0 on success, a negative error code on failure
int PU_wait(unsigned int seconds);

/// @brief Turns off all lights by sending a set off effect to all LOR units.
/// @return 0 on success, a negative error code on failure
int PU_lightsOff(void);

/// @brief Returns the seconds remaining in the sequence based on the current
/// frame provided by the caller.
/// @param frame current frame in the sequence
/// @return seconds remaining in the sequence, or 0 if the sequence is complete
long PU_secondsRemaining(uint32_t frame);

/// @brief Encodes and writes a LOR heartbeat message to the serial port.
/// @return 0 on success, a negative error code on failure
int PU_writeHeartbeat(void);

struct ctgroup_s;

struct LorBuffer;

/// @brief Encodes the given channel group state update to the provided message
/// buffer as a LOR effect.
/// @param group channel group state to encode
/// @param msg message buffer to encode the effect to
/// @return 0 on success, a negative error code on failure
int PU_writeEffect(const struct ctgroup_s* group, struct LorBuffer* msg);

struct FC;

/// @brief If audiofp is not NULL, this function will attempt to play the audio
/// file at the given path. If audiofp is NULL, this function will attempt to
/// lookup the `mf` (media file) variable within the file controller and play
/// the audio file at the path stored in the variable.
/// @param audiofp suggested audio file path to play, or NULL to lookup from fc
/// @param fc file controller to read a fallback audio file from
/// @return 0 on success, a negative error code on failure
int PU_playFirstAudio(char* audiofp, struct FC* fc);

#endif//FPLAYER_PUTIL_H
