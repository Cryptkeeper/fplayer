#ifndef FPLAYER_NETSTATS_H
#define FPLAYER_NETSTATS_H

#include <inttypes.h>

#define PRInetstat PRIu64

typedef uint64_t netstat_t;

extern netstat_t gNSPackets; /* # of LOR packets sent last sec. (incl. fades) */
extern netstat_t gNSFades;   /* # of LOR fade packets sent last second */
extern netstat_t gNSSaved; /* # of bytes saved by protocol minifier last sec. */
extern netstat_t gNSWritten; /* # of LOR protocol bytes written last second */

char *nsGetStatus(void);

char *nsGetSummary(void);

#endif//FPLAYER_NETSTATS_H
