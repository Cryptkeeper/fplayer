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

static lor_time_t minifyGetFrameTime(void) {
    const float ms = (float) playerGetPlaying()->header.frameStepTimeMillis;

    return lor_seconds_to_time(ms / 1000.0F);
}

// "explodes" a series of up to 16 brightness values (uint8_t) into a series of
// update packets (either as individual channels, multichannel bitmasks, or a mixture
// of both) via the `write` function
static uint16_t minifyWriteStack(const uint8_t unit,
                                 const uint8_t groupOffset,
                                 const bool fade,
                                 const EncodeStack *const stack,
                                 uint16_t consumed,
                                 const minify_write_fn_t write) {
    assert(stack->nChanges > 0 && stack->nChanges <= encodeStackCapacity());

    for (uint8_t i = 0; i < stack->nChanges; i++) {
        // check if circuit has already been accounted for
        // i.e. the intensity value matched another and the update was bulked
        if (consumed & CIRCUIT_BIT(i)) continue;

        const EncodeChange change = stack->changes[i];

        // the intensity did not change, do not attempt to create a bulk update
        // another circuit with the same intensity will still be updated, with
        // itself as the root circuit
        if (change.newIntensity == change.oldIntensity) continue;

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

        if (fade) {
            request.effect = LOR_EFFECT_FADE;
            request.effectData = (union lor_effect_any_t){
                    .fade = {
                            .startIntensity =
                                    minifyEncodeIntensity(change.oldIntensity),
                            .endIntensity =
                                    minifyEncodeIntensity(change.newIntensity),
                            .duration = minifyGetFrameTime(),
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

    return consumed;
}

static void minifyWrite16Aligned(uint8_t unit,
                                 uint8_t groupOffset,
                                 const EncodeStack *const stack,
                                 minify_write_fn_t write) {
    uint16_t consumed = 0;

    // first pass, attempt matching changing in intensities as fade effects
    // consumed is a bitset for the circuits in the stack that were "updated"
    // anything left over should be handled as "set" updates, and grouped if possible
    consumed =
            minifyWriteStack(unit, groupOffset, true, stack, consumed, write);

    if (consumed < 0xFFFFu)
        minifyWriteStack(unit, groupOffset, false, stack, consumed, write);
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

void minifyStream(const uint8_t *frameData,
                  const uint8_t *lastFrameData,
                  uint32_t size,
                  minify_write_fn_t write) {
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

        // fade module tracks intensity history and if a consistent slope is found,
        // may opt to enable LOR hardware fading instead of setting the intensity directly
        const bool fade = fadeApplySmoothing(id, oldIntensity, newIntensity);

        // record values onto (possibly fresh) stack
        // flush when stack is full
        encodeStackPush(&gStack, (EncodeChange){
                                         .circuit = circuit,
                                         .oldIntensity = oldIntensity,
                                         .newIntensity = newIntensity,
                                         .fade = fade,
                                 });

        if (encodeStackFull(&gStack))
            minifyWriteUnaligned(unit, &gStack, write);
    }

    // flush any pending data from the last iteration
    if (gStack.nChanges > 0) minifyWriteUnaligned(prevUnit, &gStack, write);
}
