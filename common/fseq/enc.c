/// @file enc.c
/// @brief Binary encoding utility function implementations.
#include "enc.h"

#include <assert.h>
#include <stddef.h>

void enc_uint16_le(uint8_t* b, uint16_t v) {
    assert(b != NULL);
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
}

void enc_uint32_le(uint8_t* b, uint32_t v) {
    assert(b != NULL);
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
    b[2] = (v >> 16) & 0xFF;
    b[3] = (v >> 24) & 0xFF;
}

void enc_uint64_le(uint8_t* b, uint64_t v) {
    assert(b != NULL);
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
    b[2] = (v >> 16) & 0xFF;
    b[3] = (v >> 24) & 0xFF;
    b[4] = (v >> 32) & 0xFF;
    b[5] = (v >> 40) & 0xFF;
    b[6] = (v >> 48) & 0xFF;
    b[7] = (v >> 56) & 0xFF;
}
