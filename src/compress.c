#include "compress.h"

#ifdef ENABLE_ZSTD
#include <string.h>

#include <zstd.h>

#include "std/mem.h"
#endif

#include "std/err.h"

static uint32_t sequenceGetComBlockPos(const Sequence *seq, int comBlockIndex) {
    uint32_t offset = 0;

    // add the sizes of all previous compression block entries
    for (int i = 0; i < comBlockIndex; i++)
        offset += seq->compressionBlocks[i].size;

    return seq->header.channelDataOffset + offset;
}

#ifdef ENABLE_ZSTD
static void decompressBlockZstd(Sequence *seq,
                                int comBlockIndex,
                                uint8_t **frameData,
                                uint32_t *size) {
    const size_t dInSize = seq->compressionBlocks[comBlockIndex].size;
    void *dIn = mustMalloc(dInSize);

    const size_t dOutSize = ZSTD_DStreamOutSize();
    void *dOut = mustMalloc(dOutSize);

    ZSTD_DCtx *ctx = ZSTD_createDCtx();
    if (ctx == NULL) fatalf(E_ALLOC_FAIL, NULL);

    FILE *f = seq->openFile;

    // seek to start position of this compression block's frame data
    if (fseek(f, sequenceGetComBlockPos(seq, comBlockIndex), SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    if (fread(dIn, 1, dInSize, f) != dInSize) fatalf(E_FILE_IO, NULL);

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

void decompressBlock(Sequence *seq,
                     int comBlockIndex,
                     uint8_t **frameData,
                     uint32_t *size) {
    switch (seq->header.compressionType) {
        case TF_COMPRESSION_ZSTD: {
#ifdef ENABLE_ZSTD
            decompressBlockZstd(seq, comBlockIndex, frameData, size);
#else
            fatalf(E_FATAL, "zstd support disabled at build time\n");
#endif
            break;
        }

        default:
            fatalf(E_FATAL, "cannot decompress type: %d\n",
                   seq->header.compressionType);
    }
}
