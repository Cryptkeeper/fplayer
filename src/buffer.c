#include "buffer.h"

#include "serial.h"

static uint8_t gBufferMemory[512]; /* a safe min. for one msg is 32 bytes */

LorBuffer gWriteBuffer = {
        .buffer = &gBufferMemory[0],
        .size = sizeof(gBufferMemory),
        .offset = 0,
};

size_t writeBufferFlush(void) {
    const size_t written = gWriteBuffer.offset;
    if (written == 0) return 0;

    serialWrite(gWriteBuffer.buffer, written);

    gWriteBuffer.offset = 0;

    return written;
}
