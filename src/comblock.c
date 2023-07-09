#include "comblock.h"

#include <stb_ds.h>

#ifdef ENABLE_ZSTD
#include <zstd.h>
#endif

#include "seq.h"
#include "std/err.h"
#include "std/mem.h"

#define COMPRESSION_BLOCK_SIZE 8

typedef struct com_block_t {
    uint32_t addr;
    uint32_t size;
} ComBlock;

static ComBlock *gBlocks;

static void comBlocksLoadAddrs(void) {
    const uint8_t nBlocks = sequenceData()->compressionBlockCount;

    if (nBlocks == 0) return;

    pthread_mutex_lock(&gFileMutex);

    // fseq header is fixed to 32 bytes, followed by compression block array
    if (fseek(gFile, 32, SEEK_SET) < 0) fatalf(E_FILE_IO, NULL);

    const int size = nBlocks * COMPRESSION_BLOCK_SIZE;

    uint8_t *b = mustMalloc(size);

    memset(b, 0, size);

    if (fread(b, COMPRESSION_BLOCK_SIZE, nBlocks, gFile) != nBlocks)
        fatalf(E_FILE_IO, "unexpected end of compression blocks\n");

    pthread_mutex_unlock(&gFileMutex);

    // individually parse and validate each block
    // append to a std_ds.h array to avoid managing the resizing
    arrsetcap(gBlocks, nBlocks);

    uint8_t *head = b;

    uint32_t offset = sequenceData()->channelDataOffset;

    for (int i = 0; i < nBlocks; i++) {
        enum tf_err_t err;
        struct tf_compression_block_t block;

        if ((err = tf_read_compression_block(
                     head, size - (i * COMPRESSION_BLOCK_SIZE), &block,
                     &head)) != TF_OK)
            fatalf(E_FATAL, "error parsing compression block: %s\n",
                   tf_err_str(err));

        // a fseq file may include multiple empty compression blocks for padding purposes
        // these will appear with a 0 size value, trailing previously valid blocks
        if (block.size == 0) break;

        ComBlock comBlock = (ComBlock){
                .addr = offset,
                .size = block.size,
        };

        arrput(gBlocks, comBlock);

        offset += block.size;
    }

    freeAndNull((void **) &b);
}

#ifdef ENABLE_ZSTD
static uint8_t **comBlockGetZstd(const int index) {
    const ComBlock comBlock = gBlocks[index];

    const size_t dInSize = comBlock.size;
    void *dIn = mustMalloc(dInSize);

    const size_t dOutSize = ZSTD_DStreamOutSize();
    void *dOut = mustMalloc(dOutSize);

    ZSTD_DCtx *ctx = ZSTD_createDCtx();
    if (ctx == NULL) fatalf(E_ALLOC_FAIL, NULL);

    pthread_mutex_lock(&gFileMutex);

    // seek to start position of this compression block's frame data
    if (fseek(gFile, comBlock.addr, SEEK_SET) < 0) fatalf(E_FILE_IO, NULL);

    if (fread(dIn, 1, dInSize, gFile) != dInSize) fatalf(E_FILE_IO, NULL);

    pthread_mutex_unlock(&gFileMutex);

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
            fatalf(E_FILE_IO,
                   "error while decompressing zstd stream: %s (%zu)\n",
                   ZSTD_getErrorName(err), err);

        const uint32_t frameSize = sequenceData()->channelCount;

        // the decompressed size should be a product of the frameSize
        // otherwise the data (is most likely) decompressed incorrectly
        if (out.pos % frameSize != 0)
            fatalf(E_FATAL,
                   "decompressed frame data size (%d) is not multiple of frame "
                   "size (%d)\n",
                   out.pos, frameSize);

        // break each chunk into its own allocation
        // they are appended to a central, ordered table for playback
        // this enables fplayer to free decompressed frame blocks as they are played
        for (uint32_t i = 0; i < out.pos / frameSize; i++) {
            uint8_t *const frame = mustMalloc(frameSize);
            uint8_t *const src = &((uint8_t *) dOut)[i * frameSize];

            memcpy(frame, src, frameSize);

            arrput(frames, frame);
        }
    }

    freeAndNullWith(&ctx, ZSTD_freeDCtx);

    freeAndNull(&dIn);
    freeAndNull(&dOut);

    return frames;
}
#endif

void comBlocksInit(void) {
    comBlocksLoadAddrs();
}

uint8_t **comBlockGet(const int index) {
    const enum tf_ctype_t compression = sequenceData()->compressionType;

    switch (compression) {
#ifdef ENABLE_ZSTD
        case TF_COMPRESSION_ZSTD:
            return comBlockGetZstd(index);
#endif
        default:
            fatalf(E_FATAL, "cannot decompress type: %d\n", compression);
            return NULL;
    }
}

void comBlocksFree(void) {
    arrfree(gBlocks);
}