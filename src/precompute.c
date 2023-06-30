#include "precompute.h"

#include <string.h>

#include <stb_ds.h>

#include "fade.h"
#include "mem.h"
#include "time.h"

struct intensity_history_t {
    uint32_t startFrame;
    uint8_t firstIntensity;
    uint8_t lastIntensity;
    uint16_t frames;
    int slope;
};

struct intensity_history_kv_t {
    uint32_t key;
    struct intensity_history_t value;
};

static struct intensity_history_kv_t *gHistory;

static struct intensity_history_t *
intensityHistoryGetInsert(const uint32_t id) {
    struct intensity_history_kv_t *const existing = hmgetp_null(gHistory, id);

    if (existing != NULL) return &existing->value;

    hmput(gHistory, id, (struct intensity_history_t){0});

    struct intensity_history_kv_t *const put = hmgetp_null(gHistory, id);

    return put != NULL ? &put->value : NULL;
}

static inline void
intensityHistoryReset(struct intensity_history_t *const history) {
    memset(history, 0, sizeof(struct intensity_history_t));
}

static int gFadesGenerated;

static void intensityHistoryFlush(const uint32_t id,
                                  struct intensity_history_t *const history) {
    // require at least two repeat frames of the slope to be considered a fade
    // otherwise it is a static change between two intensity levels
    if (history->frames >= 2) {
        gFadesGenerated++;

        fadePush(history->startFrame, id,
                 (Fade){
                         .from = history->firstIntensity,
                         .to = history->lastIntensity,
                         .startFrame = history->startFrame,
                         .frames = history->frames,
                 });
    }

    intensityHistoryReset(history);
}

static inline bool intensityHistorySlopeAligned(const int slope, const int dt) {
    // returns whether `dt` (a delta between two intensity values) is considered
    // align with a previous delta, `slope`
    // this controls when fading detects "shifts" and interrupts the fade state
    const int r = 1;
    return dt >= slope - r && dt <= slope + r;
}

static void intensityHistoryPush(const uint32_t id,
                                 const uint32_t frame,
                                 uint8_t oldIntensity,
                                 uint8_t newIntensity) {
    struct intensity_history_t *const history = intensityHistoryGetInsert(id);

    const int dt = (int) newIntensity - (int) oldIntensity;

    if (dt == 0) {
        intensityHistoryFlush(id, history);

        // early return, no value storing invalid slope data for next iteration
        return;
    }

    if (history->frames > 0 &&
        !intensityHistorySlopeAligned(history->slope, dt))
        intensityHistoryFlush(id, history);

    history->slope = dt;
    history->lastIntensity = newIntensity;

    // only set value when updating with first frame
    if (history->frames++ == 0) {
        history->startFrame = frame;
        history->firstIntensity = oldIntensity;
    }
}

static void intensityHistoryFree(void) {
    hmfree(gHistory);
}

static uint8_t *gLastFrameData;

static void precomputeFree(void) {
    freeAndNull((void **) &gLastFrameData);
}

static bool precomputeHandleNextFrame(FramePump *const pump,
                                      Sequence *const seq) {
    if (!sequenceNextFrame(seq)) return false;

    // maintain a copy of the previous frame to use for detecting differences
    const uint32_t frameSize = sequenceGetFrameSize(seq);

    static bool hasPrevFrame = false;

    if (gLastFrameData == NULL) {
        gLastFrameData = mustMalloc(frameSize);

        // zero out the array to represent all existing intensity values as off
        memset(gLastFrameData, 0, frameSize);
    }

    // fetch the current frame data
    uint8_t *frameData = NULL;

    if (!framePumpGet(pump, seq, &frameData)) return false;

    if (hasPrevFrame) {
        for (uint32_t circuit = 0; circuit < frameSize; circuit++) {
            const uint32_t frame = sequenceGetFrame(seq);

            const uint8_t oldIntensity = gLastFrameData[circuit];
            const uint8_t newIntensity = frameData[circuit];

            intensityHistoryPush(circuit, frame, oldIntensity, newIntensity);
        }
    }

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameData, frameSize);

    hasPrevFrame = true;

    return true;
}

static void precomputeFlush(void) {
    // flush any pending fades that end on the last frame
    for (int i = 0; i < hmlen(gHistory); i++) {
        struct intensity_history_kv_t *const kv = &gHistory[i];

        if (kv != NULL && kv->value.frames >= 2)
            intensityHistoryFlush(kv->key, &kv->value);
    }
}

void precomputeStart(FramePump *const pump, Sequence *const seq) {
    printf("precomputing fades...\n");

    const timeInstant now = timeGetNow();

    while (precomputeHandleNextFrame(pump, seq))
        ;

    precomputeFlush();
    precomputeFree();

    intensityHistoryFree();

    const int ms = (int) (timeElapsedNs(now, timeGetNow()) / 1000000);

    printf("identified %d fade events (%d variants) in %dms\n", gFadesGenerated,
           fadeTableSize(), ms);

    // reset playback state, see `sequenceInit`
    seq->currentFrame = -1;

    framePumpFree(pump);
    framePumpInit(pump);
}
