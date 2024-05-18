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

/// @brief Returns a dynamically allocated string representing the time
/// remaining in the sequence. The string is formatted as "mm:ss". The string
/// must be freed by the caller when no longer needed.
/// @param frame current frame in the sequence
/// @return string representing the time remaining in the sequence
char* PU_timeRemaining(uint32_t frame);

#endif//FPLAYER_PUTIL_H
