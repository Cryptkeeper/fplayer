#include "pump.h"

#include "compress.h"
#include "seq.h"
#include "std/err.h"
#include "std/mem.h"
#include "std/time.h"

static void framePumpChargeSequentialRead(FramePump *const pump,
                                          const uint32_t currentFrame) {
    const uint32_t frameSize = sequenceGet(SI_FRAME_SIZE);

    // generates a frame data buffer of 5 seconds worth of playback
    const uint32_t reqFrameCount = sequenceGet(SI_FPS) * 5;

    if (pump->frameData == NULL)
        pump->frameData = mustMalloc(frameSize * reqFrameCount);

    pthread_mutex_lock(&gFileMutex);

    if (fseek(gFile, currentFrame * frameSize, SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    const unsigned long framesRead =
            fread(pump->frameData, frameSize, reqFrameCount, gFile);

    pthread_mutex_unlock(&gFileMutex);

    if (framesRead < 1) fatalf(E_FILE_IO, "unexpected end of frame data\n");

    pump->readIdx = 0;
    pump->size = framesRead * frameSize;
}

static bool framePumpChargeCompressionBlock(FramePump *const pump) {
    if (pump->consumedComBlocks >= sequenceData()->compressionBlockCount)
        return true;

    const int comBlockIndex = pump->consumedComBlocks++;

    uint8_t *frameData = NULL;
    uint32_t size = 0;

    decompressBlock(comBlockIndex, &frameData, &size);

    const uint32_t frameSize = sequenceGet(SI_FRAME_SIZE);

    // the decompressed size should be a product of the frameSize
    // otherwise the data decompressed incorrectly
    if (size % frameSize != 0)
        fatalf(E_FATAL,
               "decompressed frame data size (%d) is not multiple of frame "
               "size (%d)\n",
               size, frameSize);

    // free previously decompressed block prior to overwriting reference
    freeAndNull((void **) &pump->frameData);

    pump->frameData = frameData;
    pump->readIdx = 0;
    pump->size = size;

    return false;
}

static bool framePumpIsEmpty(const FramePump *const pump) {
    return pump->readIdx >= pump->size;
}

static bool framePumpRecharge(FramePump *const pump,
                              const uint32_t currentFrame) {
    const timeInstant start = timeGetNow();

    // recharge pump depending on the compression type
    switch (sequenceData()->compressionType) {
        case TF_COMPRESSION_NONE:
            framePumpChargeSequentialRead(pump, currentFrame);
            break;

        case TF_COMPRESSION_ZLIB:
        case TF_COMPRESSION_ZSTD:
            if (framePumpChargeCompressionBlock(pump)) return false;
            break;
    }

    if (framePumpIsEmpty(pump))
        fatalf(E_FATAL, "unexpected end of frame pump\n");

    // check for performance issues after reading
    const double chargeTimeMs =
            (double) timeElapsedNs(start, timeGetNow()) / 1000000.0;

    const uint32_t frameSize = sequenceGet(SI_FRAME_SIZE);

    printf("loaded %d frames in %.4fms\n", pump->size / frameSize,
           chargeTimeMs);

    return true;
}

bool framePumpGet(FramePump *const pump,
                  const uint32_t currentFrame,
                  uint8_t **const frameData) {
    if (framePumpIsEmpty(pump) && !framePumpRecharge(pump, currentFrame))
        return false;

    *frameData = &pump->frameData[pump->readIdx];

    pump->readIdx += sequenceGet(SI_FRAME_SIZE);

    return true;
}

void framePumpFree(FramePump *pump) {
    freeAndNull((void **) &pump->frameData);
}
