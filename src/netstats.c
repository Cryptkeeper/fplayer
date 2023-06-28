#include "netstats.h"

#include <stdio.h>
#include <string.h>

static struct netstats_update_t gLastSecond;
static struct netstats_update_t gSum;

static inline void nsUpdateAdd(struct netstats_update_t *a,
                               struct netstats_update_t b) {
    a->packets += b.packets;
    a->saved += b.saved;
    a->size += b.size;
}

void nsRecord(struct netstats_update_t update) {
    nsUpdateAdd(&gLastSecond, update);
    nsUpdateAdd(&gSum, update);
}

static float nsGetCompressionRatio(const struct netstats_update_t *src) {
    const float saved = (float) src->saved;

    return saved / (saved + (float) src->size);
}

sds nsGetStatus(void) {
    const float cr = nsGetCompressionRatio(&gLastSecond);

    sds str = sdscatprintf(sdsempty(), "%dB/s\tpackets: %d\tcompressed: %.02f",
                           gLastSecond.size, gLastSecond.packets, cr);

    memset(&gLastSecond, 0, sizeof(gLastSecond));

    return str;
}

void nsPrintSummary(void) {
    const float cr = nsGetCompressionRatio(&gSum);

    printf("transferred %d bytes via %d packets, saved %d bytes (%.0f%%)\n",
           gSum.size, gSum.packets, gSum.saved, cr * 100);
}
