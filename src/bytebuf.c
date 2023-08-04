#include "bytebuf.h"

#include "serial.h"
#include "std/err.h"

static const int BufHeadDefault = 1;

static unsigned char gWriteBuf[32];
static int gBufHead = BufHeadDefault;

void bbWrite(unsigned char b) {
    gWriteBuf[gBufHead++] = b;

    // keep space for a final stop byte
    if (gBufHead >= sizeof(gWriteBuf))
        fatalf(E_FATAL, "write buffer is full, have you called `bbFlush()`?\n");
}

int bbFlush(void) {
    if (gBufHead == BufHeadDefault) return 0;

    // push a 0-byte stop value to the end of the stack
    // a prefix 0-byte stop value is prepended via `BufHeadDefault`
    gWriteBuf[gBufHead++] = 0;

    serialWrite(gWriteBuf, gBufHead);

    const int written = gBufHead;

    // consumes the value to reset the buffer
    gBufHead = BufHeadDefault;

    return written;
}
