#include "fade.h"

#include "mem.h"

struct intensity_history_t {
    uint32_t id;
    int frames;
    int slope;
};

static struct intensity_history_t *gHistories;
static int gSize;

static struct intensity_history_t *fadeGetHistory(uint32_t id) {
    for (int i = 0; i < gSize; i++)
        if (gHistories[i].id == id) return &gHistories[i];

    // expand allocation and insert new record
    // TODO: hashmap options? larger array reallocs?
    const int newIdx = gSize;

    gHistories = mustRealloc(gHistories,
                             sizeof(struct intensity_history_t) * ++gSize);

    gHistories[newIdx] = (struct intensity_history_t){
            .id = id,
            .frames = 0,
            .slope = 0,
    };

    return &gHistories[newIdx];
}

static void fadeResetHistory(struct intensity_history_t *history) {
    history->frames = 0;
    history->slope = 0;
}

bool fadeApplySmoothing(uint32_t id,
                        uint8_t oldIntensity,
                        uint8_t newIntensity) {
    struct intensity_history_t *history = fadeGetHistory(id);

    const int dt = (int) newIntensity - (int) oldIntensity;

    bool fade = false;

    if (history->frames > 0) {
        if (history->slope == dt) {
            fade = true;
        } else {
            fadeResetHistory(history);
        }
    }

    history->frames++;
    history->slope = dt;

    return fade;
}
