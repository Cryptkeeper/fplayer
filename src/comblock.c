#include "comblock.h"

#include <assert.h>
#include <stdio.h>

#include <zstd.h>

#include "stb_ds.h"

#include "seq.h"
#include "std/err.h"

#define COMPRESSION_BLOCK_SIZE 8

static bool ComBlock_findAbsoluteAddr(FCHandle fc,
                                      const int index,
                                      uint32_t *const cbAddr,
                                      uint32_t *const cbSize) {
    if (index < 0 || index >= curSequence.compressionBlockCount) return false;

    // read each table entry up to and including index
    // the leading entries are used to calculate the absolute address of a block
    // additional leading entries may exist but aren't required to calculate the address and are ignored
    const int tableSize = (index + 1) * COMPRESSION_BLOCK_SIZE;
    uint8_t *const table = mustMalloc(tableSize);

    // header is 32 bytes, followed by compression block table
    FC_read(fc, 32, tableSize, table);

    uint8_t *head = table;

    // base absolute address of the first compression block
    *cbAddr = curSequence.channelDataOffset;
    *cbSize = 0;

    // sum size of each leading compression block to get the absolute address
    // also read the final block at index to return its size
    int i = 0;
    for (; i <= index; i++) {
        const int remaining = tableSize - i * COMPRESSION_BLOCK_SIZE;

        TFError err;
        TFCompressionBlock block;
        if ((err = TFCompressionBlock_read(head, remaining, &block, &head))) {
            fprintf(stderr, "error parsing compression block: %s\n",
                    TFError_string(err));

            free(table);

            return false;
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

    free(table);

    // only return success if at least one block was read
    // this avoids a 0-size block @ 0 from being considered valid
    return i > 0;
}

static uint8_t **ComBlock_readZstd(FCHandle fc, const int index) {
    // attempt to read the address and size of the compression block
    uint32_t cbAddr = 0, cbSize = 0;
    if (!ComBlock_findAbsoluteAddr(fc, index, &cbAddr, &cbSize))
        fatalf(E_APP, "error looking up compression block: %d\n", index);

    assert(cbAddr >= curSequence.channelDataOffset);
    assert(cbSize > 0);

    const size_t dInSize = cbSize;
    void *dIn = mustMalloc(dInSize);

    const size_t dOutSize = ZSTD_DStreamOutSize();
    void *dOut = mustMalloc(dOutSize);

    ZSTD_DCtx *ctx = ZSTD_createDCtx();
    if (ctx == NULL) fatalf(E_SYS, NULL);

    // read full compression block entry
    FC_read(fc, cbAddr, dInSize, dIn);

    ZSTD_inBuffer in = {
            .src = dIn,
            .size = dInSize,
            .pos = 0,
    };

    uint8_t **frames = NULL;

    while (in.pos < in.size) {
        ZSTD_outBuffer out = {
                .dst = dOut,
                .size = dOutSize,
                .pos = 0,
        };

        const size_t err = ZSTD_decompressStream(ctx, &out, &in);

        if (ZSTD_isError(err))
            fatalf(E_APP, "error while decompressing zstd stream: %s (%zu)\n",
                   ZSTD_getErrorName(err), err);

        const uint32_t frameSize = curSequence.channelCount;

        // the decompressed size should be a product of the frameSize
        // otherwise the data (is most likely) decompressed incorrectly
        if (out.pos % frameSize != 0)
            fatalf(E_APP,
                   "decompressed frame data size (%d) is not multiple of "
                   "frame size (%d)\n",
                   out.pos, frameSize);

        // break each chunk into its own allocation
        // they are appended to a central, ordered table for playback
        // this enables fplayer to free decompressed frame blocks as they are played
        for (uint32_t i = 0; i < out.pos / frameSize; i++) {
            uint8_t *const frame = mustMalloc(frameSize);
            const uint8_t *const src = &((uint8_t *) dOut)[i * frameSize];

            memcpy(frame, src, frameSize);

            arrput(frames, frame);
        }
    }

    ZSTD_freeDCtx(ctx);

    free(dIn);
    free(dOut);

    return frames;
}

uint8_t **ComBlock_read(FCHandle fc, const int index) {
    const TFCompressionType compression = curSequence.compressionType;

    switch (compression) {
        case TF_COMPRESSION_ZSTD:
            return ComBlock_readZstd(fc, index);
        default:
            fatalf(E_APP, "cannot decompress type: %d\n", compression);
            return NULL;
    }
}
