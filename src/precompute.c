#include "precompute.h"

#include <string.h>

#include "fade.h"
#include "mem.h"
#include "time.h"

struct intensity_history_t {
    uint32_t id;
    uint32_t startFrame;
    uint8_t firstIntensity;
    uint8_t lastIntensity;
    int frames;
    int slope;
};

static struct intensity_history_t *gHistories;
static int gSize;

static struct intensity_history_t *intensityHistoryGet(const uint32_t id,
                                                       bool insert) {
    for (int i = 0; i < gSize; i++)
        if (gHistories[i].id == id) return &gHistories[i];

    if (!insert) return NULL;

    // expand allocation and insert new record
    // TODO: hashmap options? larger array reallocs?
    const int newIdx = gSize;

    gHistories = mustRealloc(gHistories,
                             sizeof(struct intensity_history_t) * ++gSize);

    gHistories[newIdx] = (struct intensity_history_t){
            .id = id,
            .startFrame = 0,
            .firstIntensity = 0,
            .lastIntensity = 0,
            .frames = 0,
            .slope = 0,
    };

    return &gHistories[newIdx];
}

static void intensityHistoryReset(struct intensity_history_t *const history) {
    history->startFrame = 0;
    history->firstIntensity = 0;
    history->lastIntensity = 0;
    history->frames = 0;
    history->slope = 0;
}

static void intensityHistoryFlush(const uint32_t id,
                                  struct intensity_history_t *const history,
                                  const char *why) {
    if (history->frames >= 2)
        printf("%d %d breaking %d: %s\n", history->startFrame, id,
               history->frames, why);

    // require at least two repeat frames of the slope to be considered a fade
    // otherwise it is a static change between two intensity levels
    if (history->frames >= 2)
        fadePush(history->startFrame, (Fade){
                                              .id = id,
                                              .from = history->firstIntensity,
                                              .to = history->lastIntensity,
                                              .startFrame = history->startFrame,
                                              .frames = history->frames,
                                      });

    intensityHistoryReset(history);
}

static bool intensityHistorySlopeAligned(const int slope, const int dt) {
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
    struct intensity_history_t *const history = intensityHistoryGet(id, true);

    const int dt = (int) newIntensity - (int) oldIntensity;

    if (dt == 0) {
        intensityHistoryFlush(id, history, "dt == 0");

        // early return, no value storing invalid slope data for next iteration
        return;
    }

    if (history->frames > 0 &&
        !intensityHistorySlopeAligned(history->slope, dt)) {
        sds msg = sdscatprintf(sdsempty(), "slope change: %d -> %d",
                               history->slope, dt);
        intensityHistoryFlush(id, history, msg);
        sdsfree(msg);
    }

    history->slope = dt;
    history->lastIntensity = newIntensity;

    // only set value when updating with first frame
    if (history->frames++ == 0) {
        history->startFrame = frame;
        history->firstIntensity = oldIntensity;
    }
}

static void intensityHistoryFree(void) {
    freeAndNull((void **) &gHistories);

    gSize = 0;
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

void precomputeStart(FramePump *const pump, Sequence *const seq) {
    printf("precomputing fades...\n");

    const timeInstant now = timeGetNow();

    while (precomputeHandleNextFrame(pump, seq))
        ;

    // flush any pending fades that end on the last frame
    for (uint32_t id = 0; id < sequenceGetFrameSize(seq); id++) {
        struct intensity_history_t *const history =
                intensityHistoryGet(id, false);

        if (history != NULL && history->frames >= 2)
            intensityHistoryFlush(id, history, "EOF");
    }

    intensityHistoryFree();

    const int ms = (int) (timeElapsedNs(now, timeGetNow()) / 1000000);

    printf("took %dms\n", ms);

    precomputeFree();

    // reset playback state, see `sequenceInit`
    seq->currentFrame = -1;

    framePumpFree(pump);
    framePumpInit(pump);
}
