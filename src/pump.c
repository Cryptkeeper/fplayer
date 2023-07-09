#include "pump.h"

#include <assert.h>
#include <string.h>

#include <pthread.h>

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
                              const uint32_t currentFrame,
                              const bool preload) {
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

    printf("%s %d frames in %.4fms\n", preload ? "pre-loaded" : "loaded",
           (int) arrlen(frames), chargeTimeMs);
}

struct frame_pump_thread_args_t {
    uint32_t startFrame;
    int16_t consumedComBlocks;
};

static void *framePumpThread(void *pargs) {
    const struct frame_pump_thread_args_t args =
            *((struct frame_pump_thread_args_t *) pargs);

    FramePump *const framePump = mustMalloc(sizeof(FramePump));

    *framePump = (FramePump){
            .consumedComBlocks = args.consumedComBlocks,
    };

    framePumpRecharge(framePump, args.startFrame, true);

    pthread_exit(framePump);
}

// https://www.austingroupbugs.net/view.php?id=599
#define PTHREAD_NULL ((pthread_t) NULL)

static pthread_t gPumpThread = PTHREAD_NULL;
static struct frame_pump_thread_args_t gThreadArgs;

static void framePumpHintPreload(const uint32_t startFrame,
                                 const int16_t consumedComBlocks) {
    if (gPumpThread != PTHREAD_NULL) return;

    gThreadArgs.startFrame = startFrame;
    gThreadArgs.consumedComBlocks = consumedComBlocks;

    int err;
    if ((err = pthread_create(&gPumpThread, NULL, framePumpThread,
                              &gThreadArgs)) != 0)
        fatalf(E_FATAL, "error creating pthread: %d\n", err);
}

static FramePump *framePumpPreloadGet(void) {
    if (gPumpThread == PTHREAD_NULL) return NULL;

    void *args = NULL;

    int err;
    if ((err = pthread_join(gPumpThread, &args)) != 0)
        fatalf(E_FATAL, "error joining pthread: %d\n", err);

    gPumpThread = PTHREAD_NULL;

    return (FramePump *) args;
}

const uint8_t *framePumpGet(FramePump *const pump,
                            const uint32_t currentFrame,
                            const bool recharge) {
    if (recharge) {
        const uint32_t remaining = framePumpGetRemaining(pump);

        const uint32_t threshold = sequenceFPS() / 5;

        // once we hit the 10 frames remaining warning, hint at starting a
        // job thread for pre-processing the next frame pump chunk
        if (remaining > 0 && remaining <= threshold) {
            const uint32_t startFrame = currentFrame + remaining;

            if (startFrame < sequenceData()->frameCount)
                framePumpHintPreload(startFrame, pump->consumedComBlocks);
        }
    }

    if (framePumpGetRemaining(pump) == 0) {
        FramePump *const nextPump = framePumpPreloadGet();

        // attempt to swap to the preloaded frame pump, if any
        // otherwise block the playback loop while the next frames are loaded
        if (nextPump != NULL) {
            framePumpFree(pump);

            memcpy(pump, nextPump, sizeof(FramePump));

            free(nextPump);
        } else {
            framePumpRecharge(pump, currentFrame, false);
        }
    }

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

    freeAndNull((void **) &pump->buffer);
}
