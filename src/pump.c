#include "pump.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "stb_ds.h"

#include "comblock.h"
#include "seq.h"
#include "std/err.h"
#include "std/time.h"

uint32_t framePumpGetRemaining(const FramePump *pump) {
    const size_t remaining = arrlenu(pump->frames);

    if (pump->head >= remaining) return 0;

    return remaining - pump->head;
}

static uint8_t **framePumpChargeSequentialRead(FCHandle fc,
                                               const uint32_t currentFrame) {
    const uint32_t frameSize = curSequence.channelCount;

    // generates a frame data buffer of 5 seconds worth of playback
    const uint32_t reqFrameCount = 1000 / curSequence.frameStepTimeMillis * 5;

    uint8_t *const frameData = mustMalloc(frameSize * reqFrameCount);

    const uint32_t framesRead =
            Seq_readFrames(fc,
                           (struct seq_read_args_t){
                                   .startFrame = currentFrame,
                                   .frameSize = frameSize,
                                   .frameCount = reqFrameCount,
                           },
                           frameData);

    if (framesRead < 1) fatalf(E_APP, "unexpected end of frame data\n");

    // most of this is a modified copy of how `comblock.c` handles generating
    // the frames list of all individually free-able frames
    uint8_t **frames = NULL;

    arrsetcap(frames, framesRead);

    for (uint32_t i = 0; i < framesRead; i++) {
        uint8_t *const frame = mustMalloc(frameSize);

        memcpy(frame, &frameData[i * frameSize], frameSize);

        arrput(frames, frame);
    }

    free(frameData);

    return frames;
}

static uint8_t **framePumpChargeCompressionBlock(FCHandle fc,
                                                 FramePump *const pump) {
    if (pump->consumedComBlocks >= curSequence.compressionBlockCount)
        return NULL;

    return comBlockGet(fc, pump->consumedComBlocks++);
}

static void framePumpFreeFrames(FramePump *const pump) {
    // by the time a pump is freed, all frames should have already
    // been consumed and freed by `framePumpGet` calls
    for (size_t i = 0; i < arrlenu(pump->frames); i++)
        assert(pump->frames[i] == NULL);

    arrfree(pump->frames);
}

static void framePumpRecharge(FCHandle fc,
                              FramePump *const pump,
                              const uint32_t currentFrame,
                              const bool preload) {
    const timeInstant start = timeGetNow();

    uint8_t **frames = NULL;

    // recharge pump depending on the compression type
    switch (curSequence.compressionType) {
        case TF_COMPRESSION_NONE:
            frames = framePumpChargeSequentialRead(fc, currentFrame);
            break;
        case TF_COMPRESSION_ZLIB:
        case TF_COMPRESSION_ZSTD:
            frames = framePumpChargeCompressionBlock(fc, pump);
            break;
    }

    if (frames == NULL || arrlen(frames) == 0)
        fatalf(E_APP, "unexpected end of frame pump\n");

    if (pump->frames != NULL) framePumpFreeFrames(pump);

    pump->frames = frames;
    pump->head = 0;

    // check for performance issues after reading
    char *const time = timeElapsedString(start, timeGetNow());

    printf("%s %d frames in %s\n", preload ? "pre-loaded" : "loaded",
           (int) arrlen(frames), time);

    free(time);
}

struct frame_pump_thread_args_t {
    FCHandle fc;
    uint32_t startFrame;
    int16_t consumedComBlocks;
};

static void *framePumpThread(void *pargs) {
    const struct frame_pump_thread_args_t args =
            *(struct frame_pump_thread_args_t *) pargs;

    FramePump *const framePump = mustMalloc(sizeof(FramePump));

    memset(framePump, 0, sizeof(FramePump));

    *framePump = (FramePump){
            .consumedComBlocks = args.consumedComBlocks,
    };

    framePumpRecharge(args.fc, framePump, args.startFrame, true);

    return framePump;
}

// https://www.austingroupbugs.net/view.php?id=599
#define PTHREAD_NULL ((pthread_t) NULL)

static pthread_t gPumpThread = PTHREAD_NULL;
static struct frame_pump_thread_args_t gThreadArgs;

static void framePumpHintPreload(FCHandle fc,
                                 const uint32_t startFrame,
                                 const int16_t consumedComBlocks) {
    if (gPumpThread != PTHREAD_NULL) return;

    gThreadArgs.fc = fc;
    gThreadArgs.startFrame = startFrame;
    gThreadArgs.consumedComBlocks = consumedComBlocks;

    int err;
    if ((err = pthread_create(&gPumpThread, NULL, framePumpThread,
                              &gThreadArgs)) != 0)
        fatalf(E_SYS, "error creating pthread: %d\n", err);
}

static FramePump *framePumpPreloadGet(void) {
    if (gPumpThread == PTHREAD_NULL) return NULL;

    void *args = NULL;

    int err;
    if ((err = pthread_join(gPumpThread, &args)) != 0)
        fatalf(E_SYS, "error joining pthread: %d\n", err);

    gPumpThread = PTHREAD_NULL;

    return args;
}

static bool framePumpSwapPreload(FramePump *const pump) {
    FramePump *nextPump = framePumpPreloadGet();

    if (nextPump == NULL) return false;

    // frees old pump's internal allocations, but not `pump` itself
    framePumpFree(pump);

    *pump = *nextPump;

    free(nextPump);

    return true;
}

const uint8_t *framePumpGet(FCHandle fc,
                            FramePump *const pump,
                            const uint32_t currentFrame,
                            const bool canHintPreload) {
    if (canHintPreload) {
        const uint32_t remaining = framePumpGetRemaining(pump);

        const uint32_t threshold = 1000 / curSequence.frameStepTimeMillis * 3;

        // once we hit the `threshold` frames remaining warning, hint at starting
        // a job thread for pre-loading the next frame pump chunk
        if (remaining > 0 && remaining <= threshold) {
            const uint32_t startFrame = currentFrame + remaining;

            if (startFrame < curSequence.frameCount)
                framePumpHintPreload(fc, startFrame, pump->consumedComBlocks);
        }
    }

    if (framePumpGetRemaining(pump) == 0) {
        // attempt to swap to the preloaded frame pump, if any
        // otherwise block the playback loop while the next frames are loaded
        if (!framePumpSwapPreload(pump))
            framePumpRecharge(fc, pump, currentFrame, false);
    }

    const uint32_t frameSize = curSequence.channelCount;

    // copy the frame data entry to a central buffer that is exposed
    // this enables us to internally free the frame allocation without another callback
    if (pump->buffer == NULL) pump->buffer = mustMalloc(frameSize);

    memcpy(pump->buffer, pump->frames[pump->head], frameSize);

    const uint32_t index = pump->head++;

    // free previous frame data, not needed once copied
    free(pump->frames[index]);

    pump->frames[index] = NULL;

    return pump->buffer;
}

void framePumpFree(FramePump *const pump) {
    framePumpFreeFrames(pump);

    free(pump->buffer);

    pump->head = 0;
    pump->buffer = NULL;
    pump->consumedComBlocks = 0;
}
