#ifndef FPLAYER_WRITER_H
#define FPLAYER_WRITER_H

#include <stdint.h>

struct FC;

struct tf_header_t;
struct tf_compression_block_t;

/// @brief Encodes the given file header and writes it to the file at the
/// current stream position.
/// @param fc target file controller
/// @param header header to encode and write
/// @return 0 on success, a negative error code on failure
int fseqWriteHeader(struct FC* fc, const struct tf_header_t* header);

/// @brief Encodes the given compression blocks and writes them to the file at
/// the current stream position.
/// @param fc target file controller
/// @param blocks compression blocks to encode and write
/// @param count number of blocks in the array
/// @return 0 on success, a negative error code on failure
int fseqWriteCompressionBlocks(struct FC* fc,
                               const struct tf_compression_block_t* blocks,
                               long count);

struct fseq_var_s {
    unsigned char id[2]; /* two char id for identifying the variable */
    uint16_t size;       /* size of the variable data `value` */
    char* value;         /* binary variable data */
};

/// @brief Encodes the given variables and writes them to the file at the
/// current stream position.
/// @param fc target file controller
/// @param header header to account for
/// @param vars variables to encode and write
/// @param count number of variables in the array
/// @return 0 on success, a negative error code on failure
int fseqWriteVars(struct FC* fc,
                  const struct tf_header_t* header,
                  const struct fseq_var_s* vars,
                  long count);

/// @brief Re-aligns the offsets in the given header to account for the length
/// of the variable data that will be written to the file.
/// @param header header to update
/// @param vars variables to account for
/// @param count number of variables in the array
/// @return 0 on success, a negative error code on failure
int fseqRealignHeaderOffsets(struct tf_header_t* header,
                             const struct fseq_var_s* vars,
                             long count);

#endif//FPLAYER_WRITER_H
