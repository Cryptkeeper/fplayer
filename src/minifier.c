#include "minifier.h"

#include <assert.h>
#include <string.h>

#include <lightorama/easy.h>
#include <lightorama/lightorama.h>
#include <stb_ds.h>

#include "cmap.h"
#include "encode.h"
#include "fade.h"
#include "lor.h"
#include "netstats.h"
#include "player.h"

static inline lor_intensity_t minifyEncodeIntensity(uint8_t abs) {
    return lor_intensity_curve_vendor((float) (abs / 255.0));
}

#define CIRCUIT_BIT(i) ((uint16_t) (1 << (i)))

struct encoding_request_t {
    uint8_t unit;
    uint8_t groupOffset;
    uint16_t circuits;
    uint8_t nCircuits;
    lor_effect_t effect;
    union lor_effect_any_t effectData;
    uint16_t nFrames;
};

static void minifyEncodeRequest(struct encoding_request_t request,
                                minify_write_fn_t write) {
    assert(request.circuits > 0);
    assert(request.nCircuits > 0);

    struct netstats_update_t update = {
            .packets = 1,
            .fades = request.effect == LOR_EFFECT_FADE,
    };

    if (request.nCircuits == 1) {
        assert(request.groupOffset == 0);

        const int size = lor_write_channel_effect(
                request.effect, &request.effectData, request.circuits - 1,
                request.unit, bufhead());

        bufadv(size);

        if (request.effect == LOR_EFFECT_FADE) {
            // 4 bytes per individual set normally + 2 bytes padding each
            // +2 to written size since it doesn't include padding yet
            update.saved = request.nFrames * 6 - (size + 2);
        }
    } else {
        const int size = lor_write_channelset_effect(
                request.effect, &request.effectData,
                (lor_channelset_t){
                        .offset = request.groupOffset,
                        .channels = request.circuits,
                },
                request.unit, bufhead());

        bufadv(size);

        // N bytes per individual set/fade normally + 2 bytes padding each
        const int ungroupedSize =
                (request.effect == LOR_EFFECT_FADE ? 7 : 4) + 2;

        // if the effect is sent once, mark the individual step frames as saved
        // +2 to written size since it doesn't include padding yet
        update.saved = (request.nFrames * request.nCircuits * ungroupedSize) -
                       (size + 2);
    }

    nsRecord(update);

    bufflush(false, write);
}

static inline lor_time_t minifyGetFadeDuration(const Fade *const fade) {
    const uint64_t ms =
            playerGetPlaying()->header.frameStepTimeMillis * fade->frames;

    return lor_seconds_to_time((float) ms / 1000.0F);
}

static void minifyEncodeLoopOffset(const uint8_t unit,
                                   const uint8_t groupOffset,
                                   uint8_t alignOffset,
                                   EncodeChange *const stack,
                                   const minify_write_fn_t write) {
    assert(stack->nChanges > 0);

    do {
        uint16_t consumed = 0;

        const int end = alignOffset + (int) arrlen(stack);
        const int max = end < 16 ? end : 16;

        // consume alignment offset after the first iteration
        // writing enough data to overflow into multiple chunking iterations
        // ensures that any >0 iteration starts at a new 16-bit boundary
        alignOffset = 0;

        for (uint8_t i = alignOffset; i < max; i++) {
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
            struct encoding_request_t request;

            memset(&request, 0, sizeof(request));

            request.unit = unit;
            request.groupOffset = popcount == 1 ? 0 : groupOffset;
            request.circuits = popcount == 1 ? change.circuit : matches;
            request.nCircuits = popcount;

            if (change.fadeStarted != NULL) {
                request.effect = LOR_EFFECT_FADE;
                request.effectData = (union lor_effect_any_t){
                        .fade = {
                                .startIntensity = minifyEncodeIntensity(
                                        change.fadeStarted->from),
                                .endIntensity = minifyEncodeIntensity(
                                        change.fadeStarted->to),
                                .duration = minifyGetFadeDuration(
                                        change.fadeStarted),
                        }};
            } else {
                request.effect = LOR_EFFECT_SET_INTENSITY;
                request.effectData = (union lor_effect_any_t){
                        .setIntensity = {
                                .intensity = minifyEncodeIntensity(
                                        change.newIntensity),
                        }};
            }

            request.nFrames =
                    change.fadeStarted != NULL ? change.fadeStarted->frames : 1;

            minifyEncodeRequest(request, write);

            // detect when all circuits are handled and break early
            if (consumed == 0xFFFFu) break;
        }

        arrdeln(stack, 0, max);
    } while (arrlen(stack) > 0);
}

static void minifyEncodeLoop(const uint8_t unit,
                             EncodeChange *const stack,
                             const minify_write_fn_t write) {
    const int firstCircuit = stack[0].circuit - 1;

    const uint8_t groupOffset = (uint8_t) (firstCircuit / 16);
    const int alignOffset = firstCircuit % 16;

    minifyEncodeLoopOffset(unit, groupOffset, alignOffset, stack, write);
}

void minifyStream(const uint8_t *const frameData,
                  const uint8_t *const lastFrameData,
                  const uint32_t size,
                  const uint32_t frame,
                  const minify_write_fn_t write) {
    uint8_t prevUnit = 0;

    EncodeChange *stack = NULL;

    arrsetcap(stack, 16);

    for (uint32_t id = 0; id < size; id++) {
        uint8_t unit;
        uint16_t circuit;

        if (!channelMapFind(id, &unit, &circuit)) continue;

        const bool unitIdChanged = arrlen(stack) > 0 && prevUnit != unit;

        if (unitIdChanged) minifyEncodeLoop(prevUnit, stack, write);

        // old stack is flushed, update context to use new unit value
        prevUnit = unit;

        // test if circuits are too far apart for minifying
        // flush the previous stack and process the request in the reset stack
        // FIXME: is this still needed?
        const bool outOfRange =
                arrlen(stack) > 0 && circuit >= stack[0].circuit + 16;

        if (outOfRange) minifyEncodeLoop(unit, stack, write);

        const uint8_t oldIntensity = lastFrameData[id];
        const uint8_t newIntensity = frameData[id];

        Fade *fadeStarted;
        bool fadeFinishing;

        fadeGetStatus(frame, id, &fadeStarted, &fadeFinishing);

        // record values onto (possibly fresh) stack
        // flush when stack is full
        EncodeChange change = (EncodeChange){
                .circuit = circuit,
                .oldIntensity = oldIntensity,
                .newIntensity = newIntensity,
                .fadeStarted = fadeStarted,
                .fadeFinishing = fadeFinishing,
        };

        arrput(stack, change);

        if (arrlen(stack) >= 16) minifyEncodeLoop(unit, stack, write);
    }

    // flush any pending data from the last iteration
    if (arrlen(stack) > 0) minifyEncodeLoop(prevUnit, stack, write);

    // allow fade data to be progressively freed
    fadeFrameFree(frame);
}
