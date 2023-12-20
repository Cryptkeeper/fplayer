#include "comblock.h"

#include <assert.h>

#include <zstd.h>

#include "stb_ds.h"

#include "seq.h"
#include "std/err.h"

#define COMPRESSION_BLOCK_SIZE 8

typedef struct com_block_t {
    uint32_t addr;
    uint32_t size;
} ComBlock;

static ComBlock *gBlocks;

static void comBlocksLoadAddrs(void) {
    const uint8_t nBlocks = sequenceData()->compressionBlockCount;

    if (nBlocks == 0) return;

    const int size = nBlocks * COMPRESSION_BLOCK_SIZE;

    uint8_t *const b = mustMalloc(size);

    // fseq header is fixed to 32 bytes, followed by compression block array
    sequenceRead(32, COMPRESSION_BLOCK_SIZE * nBlocks, b);

    // individually parse and validate each block
    // append to a std_ds.h array to avoid managing the resizing
    arrsetcap(gBlocks, nBlocks);

    uint8_t *head = b;

    uint32_t offset = sequenceData()->channelDataOffset;

    for (int i = 0; i < nBlocks; i++) {
        TFError err;
        TFCompressionBlock block;
        if ((err = TFCompressionBlock_read(
                     head, size - i * COMPRESSION_BLOCK_SIZE, &block, &head)))
            fatalf(E_APP, "error parsing compression block: %s\n",
                   TFError_string(err));

        // a fseq file may include multiple empty compression blocks for padding purposes
        // these will appear with a 0 size value, trailing previously valid blocks
        if (block.size == 0) break;

        const ComBlock comBlock = (ComBlock){
                .addr = offset,
                .size = block.size,
        };

        arrput(gBlocks, comBlock);

        offset += block.size;
    }

    free(b);
}

static uint8_t **comBlockGetZstd(const int index) {
    assert(index >= 0 && index < arrlen(gBlocks));

    const ComBlock comBlock = gBlocks[index];

    const size_t dInSize = comBlock.size;
    void *dIn = mustMalloc(dInSize);

    const size_t dOutSize = ZSTD_DStreamOutSize();
    void *dOut = mustMalloc(dOutSize);

    ZSTD_DCtx *ctx = ZSTD_createDCtx();
    if (ctx == NULL) fatalf(E_SYS, NULL);

    // read full compression block entry
    sequenceRead(comBlock.addr, dInSize, dIn);

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

        const uint32_t frameSize = sequenceData()->channelCount;

        // the decompressed size should be a product of the frameSize
        // otherwise the data (is most likely) decompressed incorrectly
        if (out.pos % frameSize != 0)
            fatalf(E_APP,
                   "decompressed frame data size (%d) is not multiple of frame "
                   "size (%d)\n",
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

void comBlocksInit(void) {
    comBlocksLoadAddrs();
}

uint8_t **comBlockGet(const int index) {
    const TFCompressionType compression = sequenceData()->compressionType;

    switch (compression) {
        case TF_COMPRESSION_ZSTD:
            return comBlockGetZstd(index);
        default:
            fatalf(E_APP, "cannot decompress type: %d\n", compression);
            return NULL;
    }
}

void comBlocksFree(void) {
    arrfree(gBlocks);
}
