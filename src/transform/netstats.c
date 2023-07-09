#include "netstats.h"

#include <stddef.h>

int gNSPackets = 0;
int gNSFades = 0;
int gNSSaved = 0;
int gNSSize = 0;

static int gNSPacketsSum;
static int gNSSavedSum;
static int gNSSizeSum;

#define gStatsCount 4

// a table with each entry being a [0: int *, 1: int *] pair of a tracked stat
// and its matching sum accumulator variable
static int *gStats[gStatsCount][2] = {
        {&gNSPackets, &gNSPacketsSum},
        {&gNSFades, NULL},
        {&gNSSaved, &gNSSavedSum},
        {&gNSSize, &gNSSizeSum},
};

static float nsGetCompressionRatio(const int saved, const int size) {
    return (float) saved / (float) (saved + size);
}

sds nsGetStatus(void) {
    const float kb = (float) gNSSize / 1024.0F;

    const float cr = nsGetCompressionRatio(gNSSaved, gNSSize);

    sds str = sdscatprintf(
            sdsempty(), "%.03f KB/s\tfades: %d\tpackets: %d\tcompressed: %.02f",
            kb, gNSFades, gNSPackets, cr);

    for (int i = 0; i < gStatsCount; i++) {
        int *const last = gStats[i][0];
        int *const sum = gStats[i][1];

        if (sum != NULL) *sum += *last;

        *last = 0;
    }

    return str;
}

sds nsGetSummary(void) {
    const float cr = nsGetCompressionRatio(gNSSavedSum, gNSSizeSum);

    return sdscatprintf(
            sdsempty(),
            "transferred %d bytes via %d packets, saved %d bytes (%.0f%%)",
            gNSSizeSum, gNSPacketsSum, gNSSavedSum, cr * 100);
}
