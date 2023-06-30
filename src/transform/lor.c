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

static struct lor_buffer_t gBuffer;

uint8_t *bufhead(void) {
    // general bounds checking before returning pointer
    // increment `writeIdx` to add a padding stop byte prefix
    // `bufadv` appends trailing stop byte and offsets the final size
    assert(gBuffer.writeIdx >= 0 && gBuffer.writeIdx + 1 < STACK_SIZE);

    return &gBuffer.stack[gBuffer.writeIdx + 1];
}

void bufadv(int size) {
    assert(size > 0);

    // increment size to add a trailing stop byte and offset prefix stop byte
    // see `bufhead`
    size += 2;

    assert(gBuffer.writeIdx + size <= STACK_SIZE);

    gBuffer.writeIdx += size;
    gBuffer.blocks++;
}

static bool bufchkflush(bool force) {
    // force flush buffer when it contains any writes
    if (force) return gBuffer.blocks > 0;

    // flush when buffer is >=N% full
    // this is a randomly selected threshold, tuning it may improve performance
    return gBuffer.writeIdx >= (int) (STACK_SIZE * 0.8);
}

static void bufreset(void) {
    memset(&gBuffer, 0, sizeof(gBuffer));
}

void bufflush(bool force, buf_transfer_t transfer) {
    if (!bufchkflush(force)) return;

    transfer(gBuffer.stack, gBuffer.writeIdx);

    bufreset();
}
