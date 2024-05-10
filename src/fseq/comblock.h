#ifndef FPLAYER_COMBLOCK_H
#define FPLAYER_COMBLOCK_H

#include <stdint.h>

struct FC;

struct fd_node_s;

/// @brief Reads the given compression block (by index) from the given file
/// controller and decompresses it (if supported).
/// @param fc target file controller instance
/// @param index index of the compression block to read
/// @param fn out pointer to the decompressed block data (array of frames) or
/// NULL on failure
/// @return 0 on success, a negative error code on failure
int ComBlock_read(struct FC* fc, int index, struct fd_node_s** fn);

#endif//FPLAYER_COMBLOCK_H
