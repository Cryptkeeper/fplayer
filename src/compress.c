#include "compress.h"

#ifdef ENABLE_ZSTD
#include <string.h>

#include <zstd.h>

#include "std/mem.h"
#endif

#include "seq.h"
#include "std/err.h"

static uint32_t sequenceGetComBlockPos(const int comBlockIndex) {
    uint32_t offset = 0;

    // add the sizes of all previous compression block entries
    for (int i = 0; i < comBlockIndex; i++)
        offset += sequenceCompressionBlockSize(i);

    return sequenceData()->channelDataOffset + offset;
}

#ifdef ENABLE_ZSTD
static void decompressBlockZstd(const uint32_t comBlockIndex,
                                uint8_t **const frameData,
                                uint32_t *const size) {
    const size_t dInSize = sequenceCompressionBlockSize(comBlockIndex);
    void *dIn = mustMalloc(dInSize);

    const size_t dOutSize = ZSTD_DStreamOutSize();
    void *dOut = mustMalloc(dOutSize);

    ZSTD_DCtx *ctx = ZSTD_createDCtx();
    if (ctx == NULL) fatalf(E_ALLOC_FAIL, NULL);

    pthread_mutex_lock(&gFileMutex);

    // seek to start position of this compression block's frame data
    if (fseek(gFile, sequenceGetComBlockPos(comBlockIndex), SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    if (fread(dIn, 1, dInSize, gFile) != dInSize) fatalf(E_FILE_IO, NULL);

    pthread_mutex_unlock(&gFileMutex);

    ZSTD_inBuffer in = {
            .src = dIn,
            .size = dInSize,
            .pos = 0,
    };

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

        // append the decompressed data chunk to the full array
        const size_t head = *size;

        *size += out.pos;
        *frameData = mustRealloc(*frameData, *size);

        memcpy(&(*frameData)[head], dOut, out.pos);
    }

    freeAndNullWith(&ctx, ZSTD_freeDCtx);

    freeAndNull(&dIn);
    freeAndNull(&dOut);
}
#endif

void decompressBlock(const uint32_t comBlockIndex,
                     uint8_t **const frameData,
                     uint32_t *const size) {
    switch (sequenceData()->compressionType) {
        case TF_COMPRESSION_ZSTD: {
#ifdef ENABLE_ZSTD
            decompressBlockZstd(comBlockIndex, frameData, size);
#else
            fatalf(E_FATAL, "zstd support disabled at build time\n");
#endif
            break;
        }

        default:
            fatalf(E_FATAL, "cannot decompress type: %d\n",
                   sequenceData()->compressionType);
    }
}
