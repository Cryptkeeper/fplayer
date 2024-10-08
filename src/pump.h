/// @file pump.h
/// @brief Frame data loading interface.
#ifndef FPLAYER_PUMP_H
#define FPLAYER_PUMP_H

#include <stdint.h>

struct FC;

struct tf_header_t;

/// @struct frame_pump_s
/// @brief Frame pump state controller for loading/tracking frame data.
struct frame_pump_s;

/// @brief Initializes a frame pump with the provided file controller. The pump
/// will read frames from the file controller and store them in an internal
/// buffer for playback. The pump will also preload the next frame set
/// asynchronously in a separate thread to ensure smooth playback. The caller is
/// responsible for freeing the pump with `FP_free`.
/// @param fc file controller to read frames from
/// @param seq sequence file for file layout information
/// @param pump pointer to store the initialized frame pump in
/// @return 0 on success, a negative error code on failure
int FP_init(struct FC* fc,
            const struct tf_header_t* seq,
            struct frame_pump_s** pump);

/// @brief Checks if the pump's internal buffer is low, and if so, preloads the
/// next frame set from the file controller asynchronously in a separate thread.
/// The preloaded data is stored within the pump, allowing it to be read once
/// the pre-existing primary buffer is empty.
/// @param pump pump to check
/// @param frame current frame index for aligning read position
/// @return 0 on success, a negative error code on failure
int FP_checkPreload(struct frame_pump_s* pump, uint32_t frame);

/// @brief Copies the next frame of data from the pump to the provided frame
/// data buffer. If the pump's internal buffer is empty, the pump will attempt
/// to read more frames from the file controller provided during initialization.
/// @param pump pump to copy from
/// @param fd frame data pointer to return the next frame in
/// @return 0 on success, a negative error code on failure, or 1 if the pump has
/// reached the end of the sequence
int FP_nextFrame(struct frame_pump_s* pump, uint8_t** fd);

/// @brief Returns the number of frames remaining in the pump's internal buffer.
/// @param pump pump to check
/// @return number of frames remaining in the pump's internal buffer
int FP_framesRemaining(struct frame_pump_s* pump);

/// @brief Frees the resources associated with the provided frame pump.
/// @param pump pump to free
void FP_free(struct frame_pump_s* pump);

#endif//FPLAYER_PUMP_H
