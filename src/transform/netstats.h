#ifndef FPLAYER_NETSTATS_H
#define FPLAYER_NETSTATS_H

#include "sds.h"

extern int gNSPackets;
extern int gNSFades;
extern int gNSSaved;
extern int gNSSize;

sds nsGetStatus(void);

sds nsGetSummary(void);

#endif//FPLAYER_NETSTATS_H
