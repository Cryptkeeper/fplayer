#include "minifier.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lorproto/coretypes.h>
#include <lorproto/easy.h>
#include <lorproto/intensity.h>
#include <stb_ds.h>

#include "../crmap.h"
#include "../serial.h"
#include "encode.h"
#include "lor/protowriter.h"

#define CIRCUIT_BIT(i) ((uint16_t) (1 << (i)))

struct encoding_request_t {
    uint8_t unit;
    uint8_t groupOffset;
    uint16_t circuits;
    uint8_t nCircuits;
    LorIntensity setIntensity;
};

static void minifyEncodeRequest(const struct encoding_request_t request) {
    assert(request.circuits > 0);
    assert(request.nCircuits > 0);

    // TODO: fix error handling, make this buffer static?
    LorBuffer* msg = LB_alloc();
    if (msg == NULL) exit(1);

    const union LorEffectArgs args = {
            .setIntensity = {request.setIntensity},
    };

    if (request.nCircuits == 1) {
        assert(request.groupOffset == 0);

        lorAppendChannelEffect(msg, LOR_EFFECT_SET_INTENSITY, &args,
                               request.circuits - 1, request.unit);
    } else {
        lorAppendChannelSetEffect(msg, LOR_EFFECT_SET_INTENSITY, &args,
                                  (LorChannelSet){
                                          .offset = request.groupOffset,
                                          .channelBits = request.circuits,
                                  },
                                  request.unit);
    }

    if (msg->offset > 0) Serial_write(msg->buffer, msg->offset);

    LB_free(msg);
}

static LorIntensity minifyEncodeIntensity(const uint8_t abs) {
    return LorIntensityCurveVendor(abs / 255.0);
}

static void minifyEncodeStack(const uint8_t unit, EncodeChange* const stack) {
    assert(arrlen(stack) > 0);

    const int firstCircuit = stack[0].circuit - 1;
    const uint8_t groupOffset = firstCircuit / 16;

    int alignOffset = firstCircuit % 16;

    do {
        uint16_t consumed = 0;

        const int count = arrlen(stack);

        // consume alignment offset after the first iteration
        // writing enough data to overflow into multiple chunking iterations
        // ensures that any >0 iteration starts at a new 16-bit boundary
        alignOffset = 0;

        for (uint8_t i = alignOffset; i < alignOffset + count; i++) {
            // check if circuit has already been accounted for
            // i.e. the intensity value matched another and the update was bulked
            if (consumed & CIRCUIT_BIT(i)) continue;

            const EncodeChange change = stack[i];

            // filter matches to only matches that have not yet been consumed
            // bonus: minifier can compress the 16-bit channel set if either 8-bit block
            //  is unused, so we benefit more later from minimizing the active bits
            const uint16_t bits = encodeStackGetMatches(stack, change);
            const uint16_t matches = ~consumed & bits;

            if (matches == 0) continue;

            // mark all matched circuits as consumed for a bulk update
            consumed |= matches;

            const int popcount = __builtin_popcount(matches);

            // build encoding request
            // this serves as a generic layer specifying the intended data,
            // which is processed by the encoder and written as LOR protocol data
            minifyEncodeRequest((struct encoding_request_t){
                    .unit = unit,
                    .groupOffset = popcount == 1 ? 0 : groupOffset,
                    .circuits = popcount == 1 ? change.circuit : matches,
                    .nCircuits = popcount,
                    .setIntensity = minifyEncodeIntensity(change.newIntensity),
            });

            // detect when all circuits are handled and break early
            if (consumed == 0xFFFFu) break;
        }

        arrdeln(stack, 0, count);
    } while (arrlen(stack) > 0);
}

void minifyStream(const struct cr_s* cr,
                  const uint8_t* const frameData,
                  const uint8_t* const lastFrameData,
                  const uint32_t size) {
    uint8_t prevUnit = 0;

    EncodeChange* stack = NULL;

    arrsetcap(stack, 16);

    for (uint32_t id = 0; id < size; id++) {
        uint8_t unit;
        uint16_t circuit;

        if (!CMap_remap(cr, id, &unit, &circuit)) continue;

        const bool unitIdChanged = arrlen(stack) > 0 && prevUnit != unit;

        if (unitIdChanged) minifyEncodeStack(prevUnit, stack);

        // old stack is flushed, update context to use new unit value
        prevUnit = unit;

        // test if circuits are too far apart for minifying
        // flush the previous stack and process the request in the reset stack
        const bool outOfRange =
                arrlen(stack) > 0 && circuit >= stack[0].circuit + 16;

        if (outOfRange) minifyEncodeStack(unit, stack);

        // record values onto (possibly fresh) stack
        // flush when stack is full
        EncodeChange change = (EncodeChange){
                .circuit = circuit,
                .oldIntensity = lastFrameData == NULL ? 0 : lastFrameData[id],
                .newIntensity = frameData[id],
        };

        arrput(stack, change);

        if (arrlen(stack) >= 16) minifyEncodeStack(unit, stack);
    }

    // flush any pending data from the last iteration
    if (arrlen(stack) > 0) minifyEncodeStack(prevUnit, stack);

    assert(arrlen(stack) == 0);

    arrfree(stack);
}
