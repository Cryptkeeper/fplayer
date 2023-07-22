#include "pump.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_PTHREAD
#include <pthread.h>
#endif

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

    const uint32_t framesRead = sequenceReadFrames(
            (struct seq_read_args_t){
                    .startFrame = currentFrame,
                    .frameSize = frameSize,
                    .frameCount = reqFrameCount,
            },
            frameData);

    if (framesRead < 1) fatalf(E_FILE_IO, "unexpected end of frame data\n");

    // most of this is a modified copy of how `comblock.c` handles generating
    // the frames list of all individually free-able frames
    uint8_t **frames = NULL;

    arrsetcap(frames, framesRead);

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

static void framePumpFreeFrames(FramePump *const pump) {
    // by the time a pump is freed, all frames should have already
    // been consumed and freed by `framePumpGet` calls
    for (int i = 0; i < arrlen(pump->frames); i++)
        assert(pump->frames[i] == NULL);

    arrfree(pump->frames);
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

    if (pump->frames != NULL) framePumpFreeFrames(pump);

    pump->frames = frames;
    pump->head = 0;

    // check for performance issues after reading
    sds time = timeElapsedString(start, timeGetNow());

    printf("%s %d frames in %s\n", preload ? "pre-loaded" : "loaded",
           (int) arrlen(frames), time);

    sdsfree(time);
}

#ifdef ENABLE_PTHREAD
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

static bool framePumpSwapPreload(FramePump *const pump) {
    FramePump *nextPump = framePumpPreloadGet();

    if (nextPump == NULL) return false;

    // frees old pump's internal allocations, but not `pump` itself
    framePumpFree(pump);

    *pump = *nextPump;

    freeAndNull((void **) &nextPump);

    return true;
}
#endif

const uint8_t *framePumpGet(FramePump *const pump,
                            const uint32_t currentFrame,
                            const bool recharge) {
    if (recharge) {
#ifdef ENABLE_PTHREAD
        const uint32_t remaining = framePumpGetRemaining(pump);

        const uint32_t threshold = sequenceFPS() / 5;

        // once we hit the `threshold` frames remaining warning, hint at starting
        // a job thread for pre-loading the next frame pump chunk
        if (remaining > 0 && remaining <= threshold) {
            const uint32_t startFrame = currentFrame + remaining;

            if (startFrame < sequenceData()->frameCount)
                framePumpHintPreload(startFrame, pump->consumedComBlocks);
        }
#endif
    }

    if (framePumpGetRemaining(pump) == 0) {
#ifdef ENABLE_PTHREAD
        // attempt to swap to the preloaded frame pump, if any
        // otherwise block the playback loop while the next frames are loaded
        if (!framePumpSwapPreload(pump))
            framePumpRecharge(pump, currentFrame, false);
#else
        framePumpRecharge(pump, currentFrame, false);
#endif
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
    framePumpFreeFrames(pump);

    freeAndNull((void **) &pump->buffer);
}
