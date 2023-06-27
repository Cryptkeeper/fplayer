#include "pump.h"

#include <stdlib.h>
#include <string.h>

#include "compress.h"
#include "err.h"
#include "mem.h"
#include "time.h"

void framePumpInit(FramePump *pump) {
    memset(pump, 0, sizeof(FramePump));

    pump->comBlockIndex = -1;
}

static void framePumpChargeSequentialRead(FramePump *pump, Sequence *seq) {
    const uint32_t frameSize = sequenceGetFrameSize(seq);

    // generates a frame data buffer of 5 seconds worth of playback
    const uint32_t reqFrameCount = sequenceGetFPS(seq) * 5;

    if (pump->frameData == NULL)
        pump->frameData = mustMalloc(frameSize * reqFrameCount);

    FILE *f = seq->openFile;

    if (fseek(f, seq->currentFrame * frameSize, SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    const unsigned long framesRead =
            fread(pump->frameData, frameSize, reqFrameCount, f);

    if (framesRead < 1) fatalf(E_FILE_IO, "unexpected end of frame data\n");

    pump->readIdx = 0;
    pump->size = framesRead * frameSize;
}

static bool framePumpChargeCompressionBlock(FramePump *pump, Sequence *seq) {
    if (pump->comBlockIndex >= seq->header.compressionBlockCount) return true;

    pump->comBlockIndex += 1;

    uint8_t *frameData = NULL;
    uint32_t size = 0;

    decompressBlock(seq, pump->comBlockIndex, &frameData, &size);

    const uint32_t frameSize = sequenceGetFrameSize(seq);

    // the decompressed size should be a product of the frameSize
    // otherwise the data decompressed incorrectly
    if (size % frameSize != 0)
        fatalf(E_FATAL,
               "decompressed frame data size (%d) is not multiple of frame "
               "size (%d)\n",
               size, frameSize);

    // free previously decompressed block prior to overwriting reference
    free(pump->frameData);

    pump->frameData = frameData;
    pump->readIdx = 0;
    pump->size = size;

    return false;
}

static bool framePumpIsEmpty(const FramePump *pump) {
    return pump->readIdx >= pump->size;
}

bool framePumpGet(FramePump *pump, Sequence *seq, uint8_t **frameData) {
    const uint32_t frameSize = sequenceGetFrameSize(seq);

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

        if (framePumpIsEmpty(pump))
            fatalf(E_FATAL, "unexpected end of frame pump\n");

        // check for performance issues after reading
        const double chargeTimeMs =
                (double) timeElapsedNs(start, timeGetNow()) / 1000000.0;

        printf("loaded %d frames in %.4fms\n", pump->size / frameSize,
               chargeTimeMs);
    }

    *frameData = &pump->frameData[pump->readIdx];

    pump->readIdx += frameSize;

    return true;
}

void framePumpFree(FramePump *pump) {
    freeAndNull((void **) &pump->frameData);
}
