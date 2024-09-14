/// @file comblock.h
/// @brief FSEQ compression block loading API.
#ifndef FPLAYER_COMBLOCK_H
#define FPLAYER_COMBLOCK_H

struct FC;

struct tf_header_t;

struct fd_list_s;

/// @brief Reads the given compression block (by index) from the given file
/// controller and decompresses it (if supported).
/// @param fc target file controller instance
/// @param seq sequence file for file layout information
/// @param index index of the compression block to read
/// @param list pointer to the list for storing the decompressed frame data
/// @return 0 on success, a negative error code on failure
int ComBlock_read(struct FC* fc,
                  const struct tf_header_t* seq,
                  int index,
                  struct fd_list_s* list);

/// @brief Determines the number of compression blocks available within the
/// given sequence file. The FSEQ file header already contains a field,
/// `compressionBlockCount`, that specifies the number of compression blocks,
/// but other encoding programs are known to write additional zero-sized
/// compression blocks to align write operations. This results in a discrepancy
/// between the header field and the actual number of playable blocks in the
/// sequence file.
/// @param fc target file controller instance
/// @param seq sequence file for file layout information
/// @return the number of compression blocks on success or a negative error code
/// on failure
int ComBlock_count(struct FC* fc, const struct tf_header_t* seq);

#endif//FPLAYER_COMBLOCK_H
