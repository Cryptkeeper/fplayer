#ifndef FPLAYER_PUMP_H
#define FPLAYER_PUMP_H

#include <stdbool.h>
#include <stdint.h>

#include "seq.h"

typedef struct frame_pump_t {
    uint8_t *frameData;
    uint32_t framePos;
    uint32_t frameEnd;
    int16_t comBlockIndex;
} FramePump;

void framePumpInit(FramePump *pump);

extern int64_t framePumpLastChargeTime;

bool framePumpGet(FramePump *pump, Sequence *seq, uint8_t **frameDataHead);

void framePumpFree(FramePump *pump);

#endif//FPLAYER_PUMP_H
