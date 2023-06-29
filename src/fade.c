#include "fade.h"

#include <string.h>

#include "mem.h"

struct intensity_history_t {
    uint16_t id;
    int frames;
    float slope;
};

static struct intensity_history_t *gHistories;
static int gSize;

static struct intensity_history_t *fadeGetCircuitHistory(uint16_t id) {
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

static bool fadeFollowsSlope(const struct intensity_history_t *history,
                             uint8_t prev,
                             uint8_t new) {
    const float dt = (float) new - (float) prev;
    const float allowance = 0.1F;

    return dt >= history->slope * (1 - allowance) &&
           dt <= history->slope * (1 + allowance);
}

static bool fadeApplySmoothing(uint16_t circuit, );
