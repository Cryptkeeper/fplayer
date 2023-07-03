#ifndef FPLAYER_PUMP_H
#define FPLAYER_PUMP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct frame_pump_t {
    uint8_t *frameData;
    uint32_t readIdx;
    uint32_t size;
    int16_t consumedComBlocks;
} FramePump;

bool framePumpGet(FramePump *pump, uint32_t currentFrame, uint8_t **frameData);

void framePumpFree(FramePump *pump);

#endif//FPLAYER_PUMP_H
