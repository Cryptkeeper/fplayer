#ifndef FPLAYER_COMBLOCK_H
#define FPLAYER_COMBLOCK_H

struct FC;

struct tf_header_t;

struct fd_node_s;

/// @brief Reads the given compression block (by index) from the given file
/// controller and decompresses it (if supported).
/// @param fc target file controller instance
/// @param seq sequence file for file layout information
/// @param index index of the compression block to read
/// @param fn out pointer to the decompressed block data (array of frames) or
/// NULL on failure
/// @return 0 on success, a negative error code on failure
int ComBlock_read(struct FC* fc,
                  const struct tf_header_t* seq,
                  int index,
                  struct fd_node_s** fn);

#endif//FPLAYER_COMBLOCK_H
