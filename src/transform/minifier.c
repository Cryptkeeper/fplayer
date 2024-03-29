#include "minifier.h"

#include <assert.h>
#include <string.h>

#include "lorproto/easy.h"
#include "lorproto/intensity.h"
#include "stb_ds.h"

#include "../cmap.h"
#include "../protowriter.h"
#include "../seq.h"
#include "encode.h"
#include "fade.h"
#include "netstats.h"

#define CIRCUIT_BIT(i) ((uint16_t) (1 << (i)))

struct encoding_request_t {
    uint8_t unit;
    uint8_t groupOffset;
    uint16_t circuits;
    uint8_t nCircuits;
    LorEffect effect;
    union LorEffectArgs *args;
    uint16_t nFrames;
};

static void minifyEncodeRequest(const struct encoding_request_t request) {
    assert(request.circuits > 0);
    assert(request.nCircuits > 0);

    netstats.fades += request.nFrames > 1 ? 1 : 0;// only fades are >1 frame

    LorBuffer *msg = protowriter.checkout_msg();

    if (request.nCircuits == 1) {
        assert(request.groupOffset == 0);

        lorAppendChannelEffect(msg, request.effect, request.args,
                               request.circuits - 1, request.unit);

        const size_t written = protowriter.return_msg(msg);

        if (request.effect == LOR_EFFECT_FADE) {
            // 4 bytes per individual set normally
            // +2 to written size since it doesn't include padding yet
            netstats.saved += request.nFrames * 6 - (written + 2);
        }
    } else {
        lorAppendChannelSetEffect(msg, request.effect, request.args,
                                  (LorChannelSet){
                                          .offset = request.groupOffset,
                                          .channelBits = request.circuits,
                                  },
                                  request.unit);

        const size_t written = protowriter.return_msg(msg);

        // N bytes per individual set/fade normally + 2 bytes padding each
        const int ungroupedSize =
                (request.effect == LOR_EFFECT_FADE ? 7 : 4) + 2;

        // if the effect is sent once, mark the individual step frames as saved
        // +2 to written size since it doesn't include padding yet
        netstats.saved += request.nFrames * request.nCircuits * ungroupedSize -
                          (written + 2);
    }
}

static LorIntensity minifyEncodeIntensity(const uint8_t abs) {
    return LorIntensityCurveVendor(abs / 255.0);
}

static uint16_t minifyGetFadeDuration(const Fade fade) {
    const uint64_t ms = curSequence.frameStepTimeMillis * fade.frames;
    assert(ms / 100 <= UINT16_MAX);
    return ms / 100;
}

static void minifyEncodeStack(const uint8_t unit, EncodeChange *const stack) {
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
            struct encoding_request_t request = {
                    .unit = unit,
                    .groupOffset = popcount == 1 ? 0 : groupOffset,
                    .circuits = popcount == 1 ? change.circuit : matches,
                    .nCircuits = popcount,
                    .args = NULL,
            };

            Fade fade;
            bool hasFade = false;

            if (change.fadeStarted >= 0) {
                hasFade = fadeGet(change.fadeStarted, &fade);
            }

            // FIXME: allow non-null `request.args` value in downstream encoding
            //  or allocate a non-static instance each time for true pointers
            static union LorEffectArgs gEffectArgs;

            if (hasFade) {
                gEffectArgs = (union LorEffectArgs){
                        .fade = {
                                .startIntensity =
                                        minifyEncodeIntensity(fade.from),
                                .endIntensity = minifyEncodeIntensity(fade.to),
                                .deciseconds = minifyGetFadeDuration(fade),
                        }};

                request.effect = LOR_EFFECT_FADE;
                request.args = &gEffectArgs;
            } else {
                gEffectArgs = (union LorEffectArgs){
                        .setIntensity = {
                                .intensity = minifyEncodeIntensity(
                                        change.newIntensity),
                        }};

                request.effect = LOR_EFFECT_SET_INTENSITY;
                request.args = &gEffectArgs;
            }

            request.nFrames = hasFade ? fade.frames : 1;

            minifyEncodeRequest(request);

            // detect when all circuits are handled and break early
            if (consumed == 0xFFFFu) break;
        }

        arrdeln(stack, 0, count);
    } while (arrlen(stack) > 0);
}

void minifyStream(const uint8_t *const frameData,
                  const uint8_t *const lastFrameData,
                  const uint32_t size,
                  const uint32_t frame) {
    uint8_t prevUnit = 0;

    EncodeChange *stack = NULL;

    arrsetcap(stack, 16);

    for (uint32_t id = 0; id < size; id++) {
        uint8_t unit;
        uint16_t circuit;

        if (!channelMapFind(id, &unit, &circuit)) continue;

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
                .oldIntensity = lastFrameData[id],
                .newIntensity = frameData[id],
        };

        fadeGetChange(frame, id, &change.fadeStarted, &change.fadeFinishing);

        arrput(stack, change);

        if (arrlen(stack) >= 16) minifyEncodeStack(unit, stack);
    }

    // flush any pending data from the last iteration
    if (arrlen(stack) > 0) minifyEncodeStack(prevUnit, stack);

    assert(arrlen(stack) == 0);

    arrfree(stack);

    // allow fade data to be progressively freed
    fadeFrameFree(frame);
}
