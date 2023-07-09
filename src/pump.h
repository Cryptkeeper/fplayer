#ifndef FPLAYER_PUMP_H
#define FPLAYER_PUMP_H

#include <stdint.h>

typedef struct frame_pump_t {
    uint8_t **frames;
    uint32_t head;
    uint8_t *buffer;
    int16_t consumedComBlocks;
} FramePump;

const uint8_t *framePumpGet(FramePump *pump, uint32_t currentFrame);

void framePumpFree(FramePump *pump);

#endif//FPLAYER_PUMP_H
