#include "comblock.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <tinyfseq.h>
#include <zstd.h>

#include "../seq.h"
#include <fseq/fd.h>
#include <std2/errcode.h>
#include <std2/fc.h>

#define COMBLOCK_SIZE 8

/// @brief Calculates the absolute address of the given compression block index
/// and its encoded size.
/// @param fc target file controller instance
/// @param index index of the compression block to look up
/// @param cbAddr out pointer to the absolute address of the compression block
/// @param cbSize out pointer to the size of the compression block
/// @return 0 on success, a negative error code on failure
static int ComBlock_findAbsoluteAddr(struct FC* fc,
                                     const int index,
                                     uint32_t* const cbAddr,
                                     uint32_t* const cbSize) {
    assert(fc != NULL);
    assert(cbAddr != NULL);
    assert(cbSize != NULL);

    if (index < 0 || index >= curSequence->compressionBlockCount)
        return -FP_ERANGE;

    // read each table entry up to and including index
    // the leading entries are used to calculate the absolute address of a block
    // additional leading entries may exist but aren't required to calculate the address and are ignored
    const int tableSize = (index + 1) * COMBLOCK_SIZE;

    uint8_t* const table = malloc(tableSize);
    if (table == NULL) return -FP_ENOMEM;

    int err = FP_EOK;

    // header is 32 bytes, followed by compression block table
    if (FC_read(fc, 32, tableSize, table) != (uint32_t) tableSize) {
        err = -FP_ESYSCALL;
        goto ret;
    }

    uint8_t* head = table;

    // base absolute address of the first compression block
    *cbAddr = curSequence->channelDataOffset;
    *cbSize = 0;

    // sum size of each leading compression block to get the absolute address
    // also read the final block at index to return its size
    int i = 0;
    for (; i <= index; i++) {
        const int remaining = tableSize - i * COMBLOCK_SIZE;
        assert(remaining > 0);

        TFCompressionBlock block;
        if (TFCompressionBlock_read(head, remaining, &block, &head)) {
            err = -FP_EDECODE;
            goto ret;
        }

        // a fseq file may include multiple empty compression blocks for padding purposes
        // these will appear with a 0 size value, trailing previously valid blocks
        if (block.size == 0) break;

        // always update to the "last valid" size, even if it isn't the final block
        *cbSize = block.size;

        // if this isn't the final block, continuing summing the relative offsets
        // into an absolute position
        if (i < index) *cbAddr += block.size;
    }

    // only return success if at least one block was read
    // this avoids a 0-size block @ 0 from being considered valid
    if (i == 0) err = -FP_ERANGE;

ret:
    free(table);
    return err;
}

/// @brief Reads the given compression block (by index) from the given file
/// controller and decompresses it using zstd.
/// @param fc target file controller instance
/// @param index index of the compression block to read
/// @param fn out pointer to the decompressed block data (array of frames) or
/// NULL on failure
/// @return 0 on success, a negative error code on failure
static int
ComBlock_readZstd(struct FC* fc, const int index, struct fd_node_s** fn) {
    assert(fc != NULL);
    assert(index >= 0);
    assert(fn != NULL);

    int err = FP_EOK;

    // attempt to read the address and size of the compression block
    uint32_t cbAddr = 0, cbSize = 0;
    if ((err = ComBlock_findAbsoluteAddr(fc, index, &cbAddr, &cbSize)))
        return err;

    assert(cbAddr >= curSequence->channelDataOffset);
    assert(cbSize > 0);

    void* dIn = NULL;                /* compressed data input buffer */
    void* dOut = NULL;               /* decompressed data output buffer */
    ZSTD_DCtx* ctx = NULL;           /* zstd decompression context */
    struct fd_node_s* cbData = NULL; /* decoded frame data */

    const size_t dOutSize = ZSTD_DStreamOutSize();

    // allocate buffers for decompression
    if ((dIn = malloc(cbSize)) == NULL || (dOut = malloc(dOutSize)) == NULL ||
        (ctx = ZSTD_createDCtx()) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    // read full compression block entry
    if (FC_read(fc, cbAddr, cbSize, dIn) < cbSize) {
        err = -FP_ESYSCALL;
        goto ret;
    }

    ZSTD_inBuffer in = {.src = dIn, .size = cbSize, .pos = 0};

    while (in.pos < in.size) {
        ZSTD_outBuffer out = {.dst = dOut, .size = dOutSize, .pos = 0};

        if (ZSTD_isError(ZSTD_decompressStream(ctx, &out, &in))) {
            err = -FP_EZSTD;
            goto ret;
        }

        const uint32_t frameSize = curSequence->channelCount;

        // the decompressed size should be a product of the frameSize
        // otherwise the data (is most likely) decompressed incorrectly
        if (out.pos % frameSize != 0) {
            err = -FP_EDECODE;
            goto ret;
        }

        // break each chunk into its own allocation
        // they are appended to a central, ordered table for playback
        // this enables fplayer to free decompressed frame blocks as they are played
        for (uint32_t i = 0; i < out.pos / frameSize; i++) {
            uint8_t* const frame = malloc(frameSize);
            if (frame == NULL) {
                err = -FP_ENOMEM;
                goto ret;
            }

            const uint8_t* const src = &((uint8_t*) dOut)[i * frameSize];
            memcpy(frame, src, frameSize);

            if ((err = FD_append(&cbData, frame))) {
                free(frame);
                goto ret;
            }
        }
    }

ret:
    free(dIn);
    free(dOut);
    ZSTD_freeDCtx(ctx);

    if (err) {
        FD_free(cbData);
        *fn = NULL;
    } else {
        *fn = cbData;
    }

    return err;
}

int ComBlock_read(struct FC* fc, const int index, struct fd_node_s** fn) {
    assert(fc != NULL);
    assert(fn != NULL);

    if (index < 0 || index >= curSequence->compressionBlockCount)
        return -FP_ERANGE;

    switch (curSequence->compressionType) {
        case TF_COMPRESSION_ZSTD:
            return ComBlock_readZstd(fc, index, fn);
        default:
            return -FP_ENOSUP;
    }
}
