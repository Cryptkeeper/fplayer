#ifndef FPLAYER_NETSTATS_H
#define FPLAYER_NETSTATS_H

#include <sds.h>

struct netstats_update_t {
    int packets;
    int fades;
    int saved;
    int size;
};

void nsRecord(struct netstats_update_t update);

sds nsGetStatus(void);

void nsPrintSummary(void);

#endif//FPLAYER_NETSTATS_H
