#include "pump.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"

void framePumpInit(FramePump *pump) {
    memset(pump, 0, sizeof(FramePump));

    pump->comBlockIndex = -1;
}

static void framePumpChargeSequentialRead(FramePump *pump, Sequence *seq) {
    const int fps = 1000 / seq->header.frameStepTimeMillis;

    const uint32_t frameSize = sequenceGetFrameSize(seq);

    // generates a frame data buffer of 5 seconds worth of playback
    // FIXME: this doesn't respect possible frame rate override
    const uint32_t reqFrameCount = fps * 5;
    const uint32_t reqFrameDataSize = reqFrameCount * frameSize;

    if (pump->frameData == NULL) pump->frameData = malloc(reqFrameDataSize);

    assert(pump->frameData != NULL);

    FILE *f;
    assert((f = seq->openFile) != NULL);

    fseek(f, seq->currentFrame * frameSize, SEEK_SET);

    const unsigned long size = fread(pump->frameData, 1, reqFrameDataSize, f);

    // ensure whatever amount of data was read is divisible into frames
    assert(size % frameSize == 0);

    pump->framePos = 0;
    pump->frameEnd = size / frameSize;
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
                framePumpChargeSequentialRead(pump, seq);
                break;

            case TF_COMPRESSION_ZLIB:
            case TF_COMPRESSION_ZSTD:
                if (framePumpChargeCompressionBlock(pump, seq)) return false;
                break;
        }

        assert(!framePumpIsEmpty(pump));
    }

    *frameDataHead = &pump->frameData[pump->framePos];

    pump->framePos += 1;

    return true;
}

void framePumpFree(FramePump *pump) {
    free(pump->frameData);
    pump->frameData = NULL;
}
