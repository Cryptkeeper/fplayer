#include "minifier.h"

#include <assert.h>
#include <string.h>

#include <lightorama/easy.h>
#include <lightorama/lightorama.h>

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

        bufadv(lor_write_channel_effect(request.effect, &request.effectData,
                                        request.circuits - 1, request.unit,
                                        bufhead()));
    } else {
        const int size = lor_write_channelset_effect(
                request.effect, &request.effectData,
                (lor_channelset_t){
                        .offset = request.groupOffset,
                        .channels = request.circuits,
                },
                request.unit, bufhead());

        bufadv(size);

        // 4 bytes per individual set normally + 2 bytes padding each
        // +2 to written size since it doesn't include padding yet
        update.saved = (request.nCircuits * 6) - (size + 2);
    }

    nsRecord(update);

    bufflush(false, write);
}

static inline lor_time_t minifyGetFadeDuration(const Fade *const fade) {
    const int ms =
            playerGetPlaying()->header.frameStepTimeMillis * fade->frames;

    return lor_seconds_to_time((float) ms / 1000.0F);
}

// "explodes" a series of up to 16 brightness values (uint8_t) into a series of
// update packets (either as individual channels, multichannel bitmasks, or a mixture
// of both) via the `write` function
static void minifyWrite16Aligned(const uint8_t unit,
                                 const uint8_t groupOffset,
                                 const EncodeStack *const stack,
                                 const minify_write_fn_t write) {
    assert(stack->nChanges > 0 && stack->nChanges <= encodeStackCapacity());

    uint16_t consumed = 0;

    for (uint8_t i = 0; i < stack->nChanges; i++) {
        // check if circuit has already been accounted for
        // i.e. the intensity value matched another and the update was bulked
        if (consumed & CIRCUIT_BIT(i)) continue;

        const EncodeChange change = stack->changes[i];

        // XOR to avoid matches including any previously consumed circuits
        // this ensures all data is unique updates and not a reset of a previous state
        // bonus: minifier can compress the 16-bit channel set if either 8-bit block
        //  is unused, so we benefit from minimizing the active bits
        const uint16_t matches =
                consumed ^ encodeStackGetMatches(stack, change);

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
                            .duration =
                                    minifyGetFadeDuration(change.fadeStarted),
                    }};
        } else {
            request.effect = LOR_EFFECT_SET_INTENSITY;
            request.effectData = (union lor_effect_any_t){
                    .setIntensity = {
                            .intensity =
                                    minifyEncodeIntensity(change.newIntensity),
                    }};
        }

        minifyEncodeRequest(request, write);

        // detect when all circuits are handled and break early
        if (consumed == 0xFFFFu) break;
    }
}

// map each circuit stack value to its intensity stack value
// if the map result is not a multiple of 16 (i.e. not aligned to the 16-bit LOR protocol window),
// it is written into a double-sized buffer, and each half is written individually as needed
static void minifyWriteUnaligned(const uint8_t unit,
                                 EncodeStack *const stack,
                                 const minify_write_fn_t write) {
    const int capacity = encodeStackCapacity();

    const EncodeChange change = stack->changes[0];

    const int firstCircuit = change.circuit - 1;

    const uint8_t groupOffset = (uint8_t) (firstCircuit / capacity);
    const int alignOffset = firstCircuit % capacity;

    if (alignOffset == 0) {
        minifyWrite16Aligned(unit, groupOffset, stack, write);
    } else {
        static EncodeStack gLow, gHigh;

        // first circuit ID isn't boundary aligned
        // manually construct up to two frames to re-align the data within
        encodeStackAlign(stack, alignOffset, &gLow, &gHigh);

        minifyWrite16Aligned(unit, groupOffset, &gLow, write);

        if (gHigh.nChanges > 0)
            minifyWrite16Aligned(unit, groupOffset, &gHigh, write);
    }

    stack->nChanges = 0;
}

void minifyStream(const uint8_t *const frameData,
                  const uint8_t *const lastFrameData,
                  const uint32_t size,
                  const uint32_t frame,
                  const minify_write_fn_t write) {
    uint8_t prevUnit = 0;

    static EncodeStack gStack;

    for (uint32_t id = 0; id < size; id++) {
        uint8_t unit;
        uint16_t circuit;

        if (!channelMapFind(id, &unit, &circuit)) continue;

        const bool unitIdChanged = gStack.nChanges > 0 && prevUnit != unit;

        if (unitIdChanged) minifyWriteUnaligned(prevUnit, &gStack, write);

        // old stack is flushed, update context to use new unit value
        prevUnit = unit;

        // test if circuits are too far apart for minifying
        // flush the previous stack and process the request in the reset stack
        const bool outOfRange =
                gStack.nChanges > 0 &&
                circuit >= gStack.changes[0].circuit + encodeStackCapacity();

        if (outOfRange) minifyWriteUnaligned(unit, &gStack, write);

        const uint8_t oldIntensity = lastFrameData[id];
        const uint8_t newIntensity = frameData[id];

        Fade *fadeStarted;
        bool fadeFinishing;

        fadeGetStatus(frame, id, &fadeStarted, &fadeFinishing);

        // record values onto (possibly fresh) stack
        // flush when stack is full
        encodeStackPush(&gStack, (EncodeChange){
                                         .circuit = circuit,
                                         .oldIntensity = oldIntensity,
                                         .newIntensity = newIntensity,
                                         .fadeStarted = fadeStarted,
                                         .fadeFinishing = fadeFinishing,
                                 });

        if (encodeStackFull(&gStack))
            minifyWriteUnaligned(unit, &gStack, write);
    }

    // flush any pending data from the last iteration
    if (gStack.nChanges > 0) minifyWriteUnaligned(prevUnit, &gStack, write);

    // allow fade data to be progressively freed
    fadeFrameFree(frame);
}
