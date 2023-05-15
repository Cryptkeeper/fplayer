#include "pump.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"

void framePumpInit(FramePump *pump) {
    memset(pump, 0, sizeof(FramePump));

    pump->comBlockIndex = -1;
}

static bool framePumpChargeSequentialRead(FramePump *pump, Sequence *seq) {
    const uint32_t frameSize = sequenceGetFrameSize(seq);

    if (pump->frameData == NULL) pump->frameData = malloc(frameSize);

    assert(pump->frameData != NULL);

    FILE *f;
    assert((f = seq->openFile) != NULL);

    const uint32_t frameReadIdx = seq->currentFrame * frameSize;

    if (fseek(f, frameReadIdx, SEEK_SET) != 0 ||
        fread(pump->frameData, frameSize, 1, f) != 1) {
        fprintf(stderr,
                "error when seeking to next frame read position: %d %d\n",
                ferror(f), feof(f));

        return true;
    }

    pump->framePos = 0;
    pump->frameEnd = 1;

    return false;
}

static bool framePumpChargeCompressionBlock(FramePump *pump, Sequence *seq) {
    if (pump->comBlockIndex >= seq->header.compressionBlockCount) return true;

    pump->comBlockIndex += 1;

    uint8_t *frameData = NULL;
    uint32_t size = 0;

    if (decompressBlock(seq, pump->comBlockIndex, &frameData, &size))
        return true;

    const uint32_t frameSize = sequenceGetFrameSize(seq);

    // the decompressed size should be a product of the frameSize
    // otherwise the data decompressed incorrectly
    assert(size % frameSize == 0);

    // free previously decompressed block prior to overwriting reference
    free(pump->frameData);

    pump->frameData = frameData;
    pump->framePos = 0;
    pump->frameEnd = size / frameSize;

    return false;
}

static bool framePumpIsEmpty(const FramePump *pump) {
    return pump->framePos >= pump->frameEnd;
}

bool framePumpGet(FramePump *pump, Sequence *seq, uint8_t **frameDataHead) {
    if (framePumpIsEmpty(pump)) {
        // recharge pump depending on the compression type
        switch (seq->header.compressionType) {
            case TF_COMPRESSION_NONE:
                if (framePumpChargeSequentialRead(pump, seq)) return false;
                break;
            case TF_COMPRESSION_ZLIB:
            case TF_COMPRESSION_ZSTD:
                if (framePumpChargeCompressionBlock(pump, seq)) return false;
                break;
        }

        if (framePumpIsEmpty(pump)) return false;
    }

    *frameDataHead = &pump->frameData[pump->framePos];

    pump->framePos += 1;

    return true;
}

void framePumpFree(FramePump *pump) {
    free(pump->frameData);
    pump->frameData = NULL;
}
