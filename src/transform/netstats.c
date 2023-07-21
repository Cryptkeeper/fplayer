#include "netstats.h"

#include <inttypes.h>
#include <stddef.h>

netstat_t gNSPackets = 0;
netstat_t gNSFades = 0;
netstat_t gNSSaved = 0;
netstat_t gNSWritten = 0;

static netstat_t gNSPacketsSum;
static netstat_t gNSSavedSum;
static netstat_t gNSWrittenSum;

#define gStatsCount 4

// a table with each entry being a [0: netstat_t *, 1: netstat_t *] pair of a
// tracked stat and its matching sum accumulator variable
static netstat_t *gStats[gStatsCount][2] = {
        {&gNSPackets, &gNSPacketsSum},
        {&gNSFades, NULL},
        {&gNSSaved, &gNSSavedSum},
        {&gNSWritten, &gNSWrittenSum},
};

static float nsGetCompressionRatio(const netstat_t saved,
                                   const netstat_t size) {
    return (float) saved / (float) (saved + size);
}

sds nsGetStatus(void) {
    const float kb = (float) gNSWritten / 1024.0F;

    const float cr = nsGetCompressionRatio(gNSSaved, gNSWritten);

    sds str = sdscatprintf(sdsempty(),
                           "%.03f KB/s\tfades: %" PRInetstat
                           "\tpackets: %" PRInetstat "\tcompressed: %.02f",
                           kb, gNSFades, gNSPackets, cr);

    for (int i = 0; i < gStatsCount; i++) {
        netstat_t *const last = gStats[i][0];
        netstat_t *const sum = gStats[i][1];

        if (sum != NULL) *sum += *last;

        *last = 0;
    }

    return str;
}

sds nsGetSummary(void) {
    const float cr = nsGetCompressionRatio(gNSSavedSum, gNSWrittenSum);

    return sdscatprintf(sdsempty(),
                        "transferred %" PRInetstat " bytes via %" PRInetstat
                        " packets, saved %llu "
                        "bytes (%.0f%%)",
                        gNSWrittenSum, gNSPacketsSum, gNSSavedSum, cr * 100);
}
