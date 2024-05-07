#ifndef FPLAYER_NETSTATS_H
#define FPLAYER_NETSTATS_H

#include <stdint.h>

struct netstats_t {
    uint64_t packets; /* # of LOR packets sent last sec. (incl. fades) */
    uint64_t fades;   /* # of LOR fade packets sent last second */
    uint64_t saved;   /* # of bytes saved by protocol minifier last sec. */
    uint64_t written; /* # of LOR protocol bytes written last second */
};

extern struct netstats_t netstats; /* last second stats tracker */

char* nsGetStatus(void);

char* nsGetSummary(void);

#endif//FPLAYER_NETSTATS_H
