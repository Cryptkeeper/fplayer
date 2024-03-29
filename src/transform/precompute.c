#include "precompute.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "stb_ds.h"

#include "../pump.h"
#include "../seq.h"
#include "../std/time.h"
#include "fade.h"
#include "std/err.h"

struct precompute_history_t {
    uint32_t startFrame;
    uint8_t firstIntensity;
    uint8_t lastIntensity;
    uint16_t frames;
    int slope;
};

struct precompute_history_kv_t {
    uint32_t key;
    struct precompute_history_t value;
};

static struct precompute_history_kv_t *gHistory;

static struct precompute_history_t *
precomputeHistoryGetInsert(const uint32_t id) {
    struct precompute_history_kv_t *const existing = hmgetp_null(gHistory, id);

    if (existing != NULL) return &existing->value;

    hmput(gHistory, id, (struct precompute_history_t){0});

    struct precompute_history_kv_t *const put = hmgetp_null(gHistory, id);

    return put != NULL ? &put->value : NULL;
}

static void precomputeHistoryReset(struct precompute_history_t *const history) {
    *history = (struct precompute_history_t){0};
}

static int gFadesGenerated;

static void precomputeHistoryFlush(const uint32_t id,
                                   struct precompute_history_t *const history) {
    // require at least N repeat frames of the slope to be considered a fade
    // otherwise it is a static change between two intensity levels
    if (history->frames < 2) goto reset;

    gFadesGenerated++;

    fadePush(id, (Fade){
                         .from = history->firstIntensity,
                         .to = history->lastIntensity,
                         .startFrame = history->startFrame,
                         .frames = history->frames,
                 });

reset:
    precomputeHistoryReset(history);
}

static bool precomputeHistoryAligned(const int slope, const int dt) {
    // returns whether `dt` (a delta between two intensity values) is considered
    // align with a previous delta, `slope`
    // this controls when fading detects "shifts" and interrupts the fade state
    const int r = 1;
    return dt >= slope - r && dt <= slope + r;
}

static void precomputeHistoryPush(const uint32_t id,
                                  const uint32_t frame,
                                  const uint8_t oldIntensity,
                                  const uint8_t newIntensity) {
    assert(frame > 0);

    struct precompute_history_t *const history = precomputeHistoryGetInsert(id);

    const int dt = (int) newIntensity - (int) oldIntensity;

    if (dt == 0) {
        precomputeHistoryFlush(id, history);

        // early return, no value storing invalid slope data for next iteration
        return;
    }

    if (history->frames > 0) {
        if (!precomputeHistoryAligned(history->slope, dt))
            precomputeHistoryFlush(id, history);
    }

    history->slope = dt;
    history->lastIntensity = newIntensity;

    // only set value when updating with first frame
    if (history->frames++ == 0) {
        // -1 since it started with the frame id which owns oldIntensity, not newIntensity
        history->startFrame = frame - 1;
        history->firstIntensity = oldIntensity;
    }
}

static void precomputeHistoryFree(void) {
    hmfree(gHistory);
}

static uint8_t *gLastFrameData;

static int64_t gNextFrame;

static bool precomputeHandleNextFrame(FCHandle fc, FramePump *const pump) {
    if (gNextFrame >= curSequence.frameCount) return false;

    const uint32_t frameSize = curSequence.channelCount;

    const bool hasPrevFrame = gLastFrameData != NULL;

    if (gLastFrameData == NULL) {
        gLastFrameData = mustMalloc(frameSize);

        // zero out the array to represent all existing intensity values as off
        memset(gLastFrameData, 0, frameSize);
    }

    const uint32_t frame = gNextFrame++;

    // fetch the current frame data
    const uint8_t *const frameData = framePumpGet(fc, pump, frame, false);

    if (hasPrevFrame) {
        for (uint32_t circuit = 0; circuit < frameSize; circuit++) {
            const uint8_t oldIntensity = gLastFrameData[circuit];
            const uint8_t newIntensity = frameData[circuit];

            precomputeHistoryPush(circuit, frame, oldIntensity, newIntensity);
        }
    }

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameData, frameSize);

    return true;
}

static void precomputeFlush(void) {
    // flush any pending fades that end on the last frame
    for (int i = 0; i < hmlen(gHistory); i++) {
        struct precompute_history_kv_t *const kv = &gHistory[i];

        if (kv != NULL && kv->value.frames >= 2)
            precomputeHistoryFlush(kv->key, &kv->value);
    }
}

void precomputeRun(const char *const fp, FCHandle fc) {
    const timeInstant start = timeGetNow();

    if (fadeTableLoadCache(fp)) {
        char *const time = timeElapsedString(start, timeGetNow());

        printf("loaded precomputed cache file: %s in %s\n", fp, time);
        free(time);

        return;
    }

    // no cache, start a fresh precompute
    // step through each frame via a temporary frame pump and compare state changes
    // all data MUST be freed prior to returning to playback init
    printf("precomputing fades...\n");

    FramePump pump = {0};

    while (precomputeHandleNextFrame(fc, &pump))
        ;

    precomputeFlush();

    precomputeHistoryFree();

    free(gLastFrameData);

    framePumpFree(&pump);

    char *const time = timeElapsedString(start, timeGetNow());

    printf("identified %d fade events (%d variants) in %s\n", gFadesGenerated,
           fadeTableSize(), time);

    free(time);

    if (fadeTableCache(fp)) {
        printf("saved precompute cache: %s\n", fp);
    } else {
        fprintf(stderr, "failed to save precompute cache: %s\n", fp);
    }
}

void precomputeFree(void) {
    fadeFree();
}
