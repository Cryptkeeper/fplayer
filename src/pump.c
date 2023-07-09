#include "pump.h"

#include <assert.h>
#include <string.h>

#include <stb_ds.h>

#include "comblock.h"
#include "seq.h"
#include "std/err.h"
#include "std/mem.h"
#include "std/time.h"

uint32_t framePumpGetRemaining(const FramePump *pump) {
    return arrlen(pump->frames) - pump->head;
}

static uint8_t **framePumpChargeSequentialRead(const uint32_t currentFrame) {
    const uint32_t frameSize = sequenceData()->channelCount;

    // generates a frame data buffer of 5 seconds worth of playback
    const uint32_t reqFrameCount = sequenceFPS() * 5;

    uint8_t *frameData = mustMalloc(frameSize * reqFrameCount);

    pthread_mutex_lock(&gFileMutex);

    if (fseek(gFile, currentFrame * frameSize, SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    const unsigned long framesRead =
            fread(frameData, frameSize, reqFrameCount, gFile);

    pthread_mutex_unlock(&gFileMutex);

    if (framesRead < 1) fatalf(E_FILE_IO, "unexpected end of frame data\n");

    // most of this is a modified copy of how `comblock.c` handles generating
    // the frames list of all individually free-able frames
    uint8_t **frames = NULL;

    for (uint32_t i = 0; i < framesRead; i++) {
        uint8_t *const frame = mustMalloc(frameSize);

        memcpy(frame, &frameData[i * frameSize], frameSize);

        arrput(frames, frame);
    }

    freeAndNull((void **) &frameData);

    return frames;
}

static uint8_t **framePumpChargeCompressionBlock(FramePump *const pump) {
    if (pump->consumedComBlocks >= sequenceData()->compressionBlockCount)
        return NULL;

    return comBlockGet(pump->consumedComBlocks++);
}

static void framePumpRecharge(FramePump *const pump,
                              const uint32_t currentFrame) {
    const timeInstant start = timeGetNow();

    uint8_t **frames = NULL;

    // recharge pump depending on the compression type
    switch (sequenceData()->compressionType) {
        case TF_COMPRESSION_NONE:
            frames = framePumpChargeSequentialRead(currentFrame);
            break;
        case TF_COMPRESSION_ZLIB:
        case TF_COMPRESSION_ZSTD:
            frames = framePumpChargeCompressionBlock(pump);
            break;
    }

    if (frames == NULL || arrlen(frames) == 0)
        fatalf(E_FATAL, "unexpected end of frame pump\n");

    pump->frames = frames;
    pump->head = 0;

    // check for performance issues after reading
    const double chargeTimeMs =
            (double) timeElapsedNs(start, timeGetNow()) / 1000000.0;

    printf("loaded %d frames in %.4fms\n", (int) arrlen(frames), chargeTimeMs);
}

const uint8_t *framePumpGet(FramePump *const pump,
                            const uint32_t currentFrame) {
    if (framePumpGetRemaining(pump) == 0) framePumpRecharge(pump, currentFrame);

    const uint32_t frameSize = sequenceData()->channelCount;

    // copy the frame data entry to a central buffer that is exposed
    // this enables us to internally free the frame allocation without another callback
    if (pump->buffer == NULL) pump->buffer = mustMalloc(frameSize);

    memcpy(pump->buffer, pump->frames[pump->head], frameSize);

    // free previous frame data, not needed once copied
    freeAndNull((void **) &pump->frames[pump->head++]);

    return pump->buffer;
}

void framePumpFree(FramePump *const pump) {
    // by the time a pump is freed, all frames should have already
    // been consumed and freed by `framePumpGet` calls
    for (int i = 0; i < arrlen(pump->frames); i++)
        assert(pump->frames[i] == NULL);

    arrfree(pump->frames);

    freeAndNull((void **) pump->buffer);
}
