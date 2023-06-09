#ifndef FPLAYER_ENCODE_H
#define FPLAYER_ENCODE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct encode_change_t {
    uint16_t circuit;
    uint8_t oldIntensity;
    uint8_t newIntensity;
    int fadeStarted;
    bool fadeFinishing;
} EncodeChange;

uint16_t encodeStackGetMatches(const EncodeChange *stack, EncodeChange compare);

#endif//FPLAYER_ENCODE_H
