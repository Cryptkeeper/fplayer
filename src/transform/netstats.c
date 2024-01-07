#include "netstats.h"

#include <inttypes.h>

#include "std/string.h"

struct netstats_t netstats; /* exposed interface */

static struct netstats_t sum; /* internal sum counter copy */

char *nsGetStatus(void) {
    const float kb = (float) netstats.written / 1024.0F;
    const float cr = (float) netstats.saved /
                     ((float) netstats.saved + (float) netstats.written);

    char *const msg = dsprintf("%.03f KB/s\tfades: %" PRIu64
                               "\tpackets: %" PRIu64 "\tcompressed: %.02f",
                               kb, netstats.saved, netstats.packets, cr);

    sum.packets += netstats.packets;
    sum.fades += netstats.fades;
    sum.saved += netstats.saved;
    sum.written += netstats.written;

    netstats = (struct netstats_t){0};

    return msg;
}

char *nsGetSummary(void) {
    const float cr =
            (float) sum.saved / ((float) sum.saved + (float) sum.written);

    return dsprintf("transferred %" PRIu64 " bytes via %" PRIu64
                    " packets, saved %" PRIu64 " bytes (%.0f%%)",
                    sum.written, sum.packets, sum.saved, cr * 100);
}
