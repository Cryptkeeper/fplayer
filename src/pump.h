#ifndef FPLAYER_PUMP_H
#define FPLAYER_PUMP_H

#include <stdbool.h>
#include <stdint.h>

#include "std/fc.h"

typedef struct frame_pump_t {
    uint8_t **frames;
    uint32_t head;
    uint8_t *buffer;
    int16_t consumedComBlocks;
} FramePump;

uint32_t framePumpGetRemaining(const FramePump *pump);

const uint8_t *framePumpGet(struct FC* fc,
                            FramePump *pump,
                            uint32_t currentFrame,
                            bool canHintPreload);

void framePumpFree(FramePump *pump);

#endif//FPLAYER_PUMP_H
