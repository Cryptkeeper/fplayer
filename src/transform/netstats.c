#include "netstats.h"

static struct netstats_update_t gLastSecond;
static struct netstats_update_t gSum;

static inline void nsUpdateAdd(struct netstats_update_t *const a,
                               const struct netstats_update_t b) {
    a->packets += b.packets;
    a->fades += b.fades;
    a->saved += b.saved;
    a->size += b.size;
}

inline void nsRecord(const struct netstats_update_t update) {
    nsUpdateAdd(&gLastSecond, update);
    nsUpdateAdd(&gSum, update);
}

static float nsGetCompressionRatio(const struct netstats_update_t *const src) {
    const float saved = (float) src->saved;

    return saved / (saved + (float) src->size);
}

sds nsGetStatus(void) {
    const float kb = (float) gLastSecond.size / 1024.0F;

    const float cr = nsGetCompressionRatio(&gLastSecond);

    sds str = sdscatprintf(
            sdsempty(), "%.03f KB/s\tfades: %d\tpackets: %d\tcompressed: %.02f",
            kb, gLastSecond.fades, gLastSecond.packets, cr);

    gLastSecond = (struct netstats_update_t){0};

    return str;
}

sds nsGetSummary(void) {
    const float cr = nsGetCompressionRatio(&gSum);

    return sdscatprintf(
            sdsempty(),
            "transferred %d bytes via %d packets, saved %d bytes (%.0f%%)",
            gSum.size, gSum.packets, gSum.saved, cr * 100);
}
