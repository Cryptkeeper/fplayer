#ifndef FPLAYER_PUMP_H
#define FPLAYER_PUMP_H

#include <stdint.h>

struct FC;

struct frame_pump_s;

/// @brief Initializes a frame pump with the provided file controller.
/// @param fc file controller to read frames from
/// @return initialized frame pump, NULL on failure
struct frame_pump_s* FP_init(struct FC* fc);

/// @brief Copies the next frame of data from the pump to the provided frame
/// data buffer. If the pump's internal buffer is empty, the pump will attempt
/// to read more frames from the file controller provided during initialization.
/// @param pump pump to copy from
/// @param fd frame data pointer to return the next frame in
/// @return 0 on success, a negative error code on failure, or `FP_ESEQEND`
/// if the pump has reached the end of the sequence
int FP_nextFrame(struct frame_pump_s* pump, uint8_t** fd);

/// @brief Returns the number of frames remaining in the pump's internal buffer.
/// @param pump pump to check
/// @return number of frames remaining in the pump's internal buffer
int FP_framesRemaining(struct frame_pump_s* pump);

/// @brief Frees the resources associated with the provided frame pump.
/// @param pump pump to free
void FP_free(struct frame_pump_s* pump);

#endif//FPLAYER_PUMP_H
