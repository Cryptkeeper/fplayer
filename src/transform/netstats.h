#ifndef FPLAYER_NETSTATS_H
#define FPLAYER_NETSTATS_H

#include "sds.h"

typedef uint64_t netstat_t;

extern netstat_t gNSPackets; /* # of LOR packets sent last sec. (incl. fades) */
extern netstat_t gNSFades;   /* # of LOR fade packets sent last second */
extern netstat_t gNSSaved; /* # of bytes saved by protocol minifier last sec. */
extern netstat_t gNSWritten; /* # of LOR protocol bytes written last second */

sds nsGetStatus(void);

sds nsGetSummary(void);

#endif//FPLAYER_NETSTATS_H
