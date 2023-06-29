#ifndef FPLAYER_ENCODE_H
#define FPLAYER_ENCODE_H

#include <stdbool.h>
#include <stdint.h>

#include "fade.h"

typedef struct encode_change_t {
    uint16_t circuit;
    uint8_t oldIntensity;
    uint8_t newIntensity;
    bool fadeFinishing;
    Fade *fadeStarted;
} EncodeChange;

#define encodeStackCapacity() 16

typedef struct encode_stack_t {
    EncodeChange changes[16];
    int nChanges;
} EncodeStack;

void encodeStackPush(EncodeStack *stack, EncodeChange change);

bool encodeStackFull(const EncodeStack *stack);

void encodeStackAlign(const EncodeStack *src,
                      int offset,
                      EncodeStack *low,
                      EncodeStack *high);

uint16_t encodeStackGetMatches(const EncodeStack *stack, EncodeChange compare);

#endif//FPLAYER_ENCODE_H
