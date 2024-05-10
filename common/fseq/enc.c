#include "enc.h"

#include <assert.h>
#include <stddef.h>

void encode_uint16(uint8_t* b, uint16_t v) {
    assert(b != NULL);
    b[0] = (v >> 8) & 0xFF;
    b[1] = v & 0xFF;
}

void encode_uint32(uint8_t* b, uint32_t v) {
    assert(b != NULL);
    b[0] = (v >> 24) & 0xFF;
    b[1] = (v >> 16) & 0xFF;
    b[2] = (v >> 8) & 0xFF;
    b[3] = v & 0xFF;
}

void encode_uint64(uint8_t* b, uint64_t v) {
    assert(b != NULL);
    b[0] = (v >> 56) & 0xFF;
    b[1] = (v >> 48) & 0xFF;
    b[2] = (v >> 40) & 0xFF;
    b[3] = (v >> 32) & 0xFF;
    b[4] = (v >> 24) & 0xFF;
    b[5] = (v >> 16) & 0xFF;
    b[6] = (v >> 8) & 0xFF;
    b[7] = v & 0xFF;
}
