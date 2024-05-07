#ifndef FPLAYER_COMBLOCK_H
#define FPLAYER_COMBLOCK_H

#include <stdint.h>

#include "std/fc.h"

/// \brief Reads the given compression block (by index) from the given file controller and decompresses it (if supported).
/// \param fc target file controller instance
/// \param index index of the compression block to read, must be >= 0 and < `curSequence.compressionBlockCount`
/// \return A decompressed copy of the block's contained frame data, or NULL if an error occurred
uint8_t **ComBlock_read(struct FC* fc, int index);

#endif//FPLAYER_COMBLOCK_H
