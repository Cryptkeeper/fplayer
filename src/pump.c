#include "pump.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"
#include "mem.h"
#include "time.h"

void framePumpInit(FramePump *pump) {
    memset(pump, 0, sizeof(FramePump));

    pump->comBlockIndex = -1;
}

static void framePumpChargeSequentialRead(FramePump *pump, Sequence *seq) {
    const uint32_t frameSize = sequenceGetFrameSize(seq);

    // generates a frame data buffer of 5 seconds worth of playback
    // FIXME: this doesn't respect possible frame rate override
    const uint32_t reqFrameCount = sequenceGetFPS(seq) * 5;
    const uint32_t reqFrameDataSize = reqFrameCount * frameSize;

    if (pump->frameData == NULL) pump->frameData = malloc(reqFrameDataSize);

    assert(pump->frameData != NULL);

    FILE *f;
    assert((f = seq->openFile) != NULL);

    fseek(f, seq->currentFrame * frameSize, SEEK_SET);

    unsigned long size = fread(pump->frameData, 1, reqFrameDataSize, f);

    // ensure whatever amount of data was read is divisible into frames
    size -= (size % frameSize);

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

static void framePumpCorrectLongChargeTime(const FramePump *pump,
                                           Sequence *seq,
                                           int64_t chargeTimeNs) {
    const double chargeTimeMs = (double) chargeTimeNs / 1000000.0;

    printf("loaded %d frames in %.4fms\n", pump->frameEnd, chargeTimeMs);

    if (chargeTimeMs <= seq->header.frameStepTimeMillis) return;

    const int skippedFrames =
            (int) ceil((chargeTimeMs - seq->header.frameStepTimeMillis) /
                       (double) seq->header.frameStepTimeMillis);

    int64_t newFrame = seq->currentFrame + skippedFrames;
    if (newFrame > seq->header.frameCount) newFrame = seq->header.frameCount;

    seq->currentFrame = newFrame;

    printf("warning: skipping %d frames\n", skippedFrames);
}

bool framePumpGet(FramePump *pump, Sequence *seq, uint8_t **frameDataHead) {
    if (framePumpIsEmpty(pump)) {
        const timeInstant start = timeGetNow();

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

        // check for performance issues after reading
        framePumpCorrectLongChargeTime(pump, seq,
                                       timeElapsedNs(start, timeGetNow()));
    }

    *frameDataHead = &pump->frameData[pump->framePos];

    pump->framePos += 1;

    return true;
}

void framePumpFree(FramePump *pump) {
    freeAndNull((void **) &pump->frameData);
}
