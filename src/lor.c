#include "lor.h"

#undef NDEBUG
#include <assert.h>

#include <string.h>

#define STACK_SIZE 2048

struct lor_buffer_t {
    uint8_t stack[STACK_SIZE];
    int writeIdx;
    int blocks;
};

static struct lor_buffer_t gBuffer = {
        .stack = {0},
        .writeIdx = 1,// add initial empty stop byte for LOR protocol
        .blocks = 0,
};

uint8_t *bufhead(void) {
    // general bounds checking before returning pointer
    assert(gBuffer.writeIdx >= 0 && gBuffer.writeIdx < STACK_SIZE);

    return &gBuffer.stack[gBuffer.writeIdx];
}

void bufadv(int size) {
    assert(size > 0);

    // increment size to add a trailing stop byte
    size++;

    assert(gBuffer.writeIdx + size <= STACK_SIZE);

    gBuffer.writeIdx += size;
    gBuffer.blocks++;
}

static bool bufflush(bool force) {
    // force flush buffer when it contains any writes
    if (force) return gBuffer.blocks > 0;

    // flush when buffer is >=N% full
    // this is a randomly selected threshold, tuning it may improve performance
    return gBuffer.writeIdx >= (int) (STACK_SIZE * 0.8);
}

static void bufreset(void) {
    memset(gBuffer.stack, 0, STACK_SIZE);

    gBuffer.writeIdx = 1;// add initial empty stop byte for LOR protocol
    gBuffer.blocks = 0;
}

void bufwrite(bool force, buf_transfer_t transfer) {
    if (!bufflush(force)) return;

    transfer(gBuffer.stack, gBuffer.writeIdx);

    bufreset();
}
