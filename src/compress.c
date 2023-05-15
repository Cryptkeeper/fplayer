#include "compress.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <zstd.h>

#include "err.h"

#define zstdPrintError(err, msg)                                               \
    do {                                                                       \
        if (ZSTD_isError(err)) {                                               \
            fprintf(stderr, "zstd error (version %s)\n",                       \
                    ZSTD_versionString());                                     \
            fprintf(stderr, "%s (%zu)\n", ZSTD_getErrorName(err), err);        \
                                                                               \
            errPrintTrace(msg);                                                \
        }                                                                      \
    } while (0)

static uint32_t sequenceGetComBlockPos(const Sequence *seq, int comBlockIndex) {
    uint32_t offset = 0;

    // add the sizes of all previous compression block entries
    for (int i = 0; i < comBlockIndex; i++)
        offset += seq->compressionBlocks[i].size;

    return seq->header.channelDataOffset + offset;
}

static bool decompressBlockZstd(Sequence *seq, int comBlockIndex,
                                uint8_t **frameData, uint32_t *size) {
    bool err = false;

    const size_t dInSize = seq->compressionBlocks[comBlockIndex].size;
    void *dIn;

    assert((dIn = malloc(dInSize)) != NULL);

    const size_t dOutSize = ZSTD_DStreamOutSize();
    void *dOut;

    assert((dOut = malloc(dOutSize)) != NULL);

    ZSTD_DCtx *ctx;
    assert((ctx = ZSTD_createDCtx()) != NULL);

    FILE *f;
    assert((f = seq->openFile) != NULL);

    // seek to start position of this compression block's frame data
    fseek(f, sequenceGetComBlockPos(seq, comBlockIndex), SEEK_SET);

    if (fread(dIn, 1, dInSize, f) != dInSize) {
        perror("error when reading compression block frame data");

        err = true;
        goto free_and_return;
    }

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

        const size_t zstdErr = ZSTD_decompressStream(ctx, &out, &in);

        if (ZSTD_isError(zstdErr)) {
            zstdPrintError(zstdErr,
                           "error when decompressing zstd stream section");

            err = true;
            goto free_and_return;
        }

        // append the decompressed data chunk to the full array
        const size_t head = *size;
        *size += out.pos;
        *frameData = reallocf(*frameData, *size);

        memcpy(&(*frameData)[head], dOut, out.pos);
    }

free_and_return:
    ZSTD_freeDCtx(ctx);

    free(dIn);
    free(dOut);

    return err;
}

bool decompressBlock(Sequence *seq, int comBlockIndex, uint8_t **frameData,
                     uint32_t *size) {
    switch (seq->header.compressionType) {
        case TF_COMPRESSION_NONE:
            fprintf(stderr, "cannot decompress non-compressed block\n");
            return true;
        case TF_COMPRESSION_ZSTD:
            return decompressBlockZstd(seq, comBlockIndex, frameData, size);
        case TF_COMPRESSION_ZLIB:
            fprintf(stderr, "cannot decompress unsupported zlib\n");
            return true;
    }

    return false;
}
