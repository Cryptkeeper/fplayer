#include "protowriter.h"

#include "serial.h"
#include "transform/netstats.h"

static LorBuffer *ProtoWriter_checkoutMsg(void) {
    static uint8_t b[64] = {0};
    static LorBuffer buffer = {
            .buffer = &b[0],
            .size = sizeof(b),
    };

    // reset write index
    // caller should never increment offset without modifying b to reset previous state
    buffer.offset = 0;

    return &buffer;
}

static size_t ProtoWriter_returnMsg(const LorBuffer *const b) {
    if (b->offset == 0) return 0;

    const size_t written = b->offset;

    netstats.written += written;
    netstats.packets += 1;

    serialWrite(b->buffer, written);

    return written;
}

struct protowriter_t protowriter = {
        .checkout_msg = ProtoWriter_checkoutMsg,
        .return_msg = ProtoWriter_returnMsg,
};
