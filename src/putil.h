#ifndef FPLAYER_PUTIL_H
#define FPLAYER_PUTIL_H

#include <stdint.h>

struct serialdev_s;

/// @brief Waits for the given number of seconds by blocking the current thread.
/// LOR heartbeat messages will intentionally be sent during this time. This
/// function is used to ensure the LOR hardware is connected to the player before
/// sending playback commands.
/// @param sdev serial device to write the heartbeat messages to
/// @param seconds number of seconds to wait
/// @return 0 on success, a negative error code on failure
int PU_wait(struct serialdev_s* sdev, unsigned int seconds);

/// @brief Turns off all lights by sending a set off effect to all LOR units.
/// @param sdev serial device to write the command to
/// @return 0 on success, a negative error code on failure
int PU_lightsOff(struct serialdev_s* sdev);

struct tf_header_t;

/// @brief Returns the seconds remaining in the sequence based on the current
/// frame and sequence configuration provided by the caller.
/// @param frame current frame in the sequence
/// @param seq sequence header for playback configuration
/// @return seconds remaining in the sequence, or 0 if the sequence is complete
long PU_secondsRemaining(uint32_t frame, const struct tf_header_t* seq);

/// @brief Encodes and writes a LOR heartbeat message to the serial port.
/// @param sdev serial device to write the heartbeat message to
/// @return 0 on success, a negative error code on failure
int PU_writeHeartbeat(struct serialdev_s* sdev);

struct ctgroup_s;

struct LorBuffer;

/// @brief Encodes the given channel group state update to the provided message
/// buffer as a LOR effect. The number of bytes written to the message buffer
/// will be added to the optional accumulator parameter.
/// @param sdev serial device to write the effect to
/// @param group channel group state to encode
/// @param msg message buffer to encode the effect to
/// @param accum optional accumulator to store the number of bytes written
/// @return 0 on success, a negative error code on failure
int PU_writeEffect(struct serialdev_s* sdev,
                   const struct ctgroup_s* group,
                   struct LorBuffer* msg,
                   uint32_t* accum);

struct FC;

/// @brief If audiofp is not NULL, this function will attempt to play the audio
/// file at the given path. If audiofp is NULL, this function will attempt to
/// lookup the `mf` (media file) variable within the file controller and play
/// the audio file at the path stored in the variable.
/// @param audiofp suggested audio file path to play, or NULL to lookup from fc
/// @param fc file controller to read a fallback audio file from
/// @param seq sequence header for file layout information
/// @return 0 on success, a negative error code on failure
int PU_playFirstAudio(const char* audiofp,
                      struct FC* fc,
                      const struct tf_header_t* seq);

#endif//FPLAYER_PUTIL_H
