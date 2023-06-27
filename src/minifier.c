#include "minifier.h"

#include <assert.h>
#include <string.h>

#include <lightorama/easy.h>
#include <lightorama/lightorama.h>

#include "cmap.h"
#include "lor.h"

// this magic value is captured here to make its usage more obvious.
//
// it is derived from the 16-bit channel masks the LOR hardware protocol uses,
// 16-bit masks corresponds to 16 individual channels, each with its own 1-byte
// intensity state.
//
// this value doesn't change without LOR protocol support for increased channel bitmask sizes
// and adjusting the uint16_t based bitmask logic to match the new width.
#define N 16

#define CIRCUIT_BIT(i) ((uint16_t) (1 << (i)))

struct encoding_request_t {
    uint8_t unit;
    uint8_t groupOffset;
    uint16_t circuits;
    uint8_t nCircuits;
    uint8_t intensity;
};

static void minifyEncodeRequest(struct encoding_request_t request,
                                minify_write_fn_t write) {
    assert(request.circuits > 0);
    assert(request.nCircuits > 0);

    const struct lor_effect_setintensity_t setEffect = {
            .intensity = lor_intensity_curve_vendor(
                    (float) (request.intensity / 255.0)),
    };

    if (request.nCircuits == 1) {
        assert(request.groupOffset == 0);

        bufadv(lor_write_channel_effect(LOR_EFFECT_SET_INTENSITY, &setEffect,
                                        request.circuits - 1, request.unit,
                                        bufhead()));
    } else {
        bufadv(lor_write_channelset_effect(
                LOR_EFFECT_SET_INTENSITY, &setEffect,
                (lor_channelset_t){
                        .offset = request.groupOffset,
                        .channels = request.circuits,
                },
                request.unit, bufhead()));
    }

    bufflush(false, write);
}

struct encoding_change_t {
    uint8_t circuit;
    uint8_t newIntensity;
    uint8_t oldIntensity;
};

typedef struct encoding_stack_t {
    struct encoding_change_t changes[N];
    uint8_t size;
} Stack;

static bool stackPush(Stack *stack, struct encoding_change_t change) {
    const int idx = stack->size++;

    assert(idx >= 0 && idx < N);

    stack->changes[idx] = change;

    // return true when stack is full and should be written
    return stack->size >= N;
}

static void stackAlign(const Stack *src, int offset, Stack *low, Stack *high) {
    memset(low->changes, 0, sizeof(struct encoding_change_t) * N);
    memset(high->changes, 0, sizeof(struct encoding_change_t) * N);

    // aligns a 16-byte (N) stack with a given offset (0,15) between two N-byte stacks
    //
    // [*low                                          ] [*high                                         ]
    // [ 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31]
    //                                                  N
    //                                         [8 size `src` example]
    //                                         ^ offset = 13

    // slots remaining in low stack
    const int lremaining = N - offset;

    // copyable count for low stack
    const int lc = src->size < lremaining ? src->size : lremaining;

    memcpy(&low->changes[offset], src->changes,
           sizeof(struct encoding_change_t) * lc);

    low->size = lc;

    // copyable count for high stack
    const int hc = src->size - lc;

    assert(hc >= 0);

    if (hc > 0)
        memcpy(&high->changes[0], &src->changes[lc],
               sizeof(struct encoding_change_t) * hc);

    high->size = hc;
}

static void minifyWrite16Aligned(uint8_t unit,
                                 uint8_t groupOffset,
                                 Stack *stack,
                                 minify_write_fn_t write);

// map each circuit stack value to its intensity stack value
// if the map result is not a multiple of 16 (i.e. not aligned to the 16-bit LOR protocol window),
// it is written into a double-sized buffer, and each half is written individually as needed
static void stackFlush(uint8_t unit, Stack *stack, minify_write_fn_t write) {
    const struct encoding_change_t change = stack->changes[0];

    const int firstCircuit = change.circuit - 1;

    const uint8_t groupOffset = (uint8_t) (firstCircuit / N);
    const int alignOffset = firstCircuit % N;

    if (alignOffset == 0) {
        minifyWrite16Aligned(unit, groupOffset, stack, write);
    } else {
        static Stack gLow, gHigh;

        // first circuit ID isn't boundary aligned
        // manually construct up to two frames to re-align the data within
        stackAlign(stack, alignOffset, &gLow, &gHigh);

        minifyWrite16Aligned(unit, groupOffset, &gLow, write);

        if (gHigh.size > 0)
            minifyWrite16Aligned(unit, groupOffset, &gHigh, write);
    }

    stack->size = 0;
}

static inline uint16_t stackGetMatches(const Stack *stack, uint8_t intensity) {
    uint16_t matches = 0;

    for (int i = 0; i < stack->size; i++) {
        const struct encoding_change_t change = stack->changes[i];

        if (change.newIntensity == intensity) matches |= CIRCUIT_BIT(i);
    }

    return matches;
}

// "explodes" a series of up to 16 brightness values (uint8_t) into a series of
// update packets (either as individual channels, multichannel bitmasks, or a mixture
// of both) via the `write` function
static void minifyWrite16Aligned(uint8_t unit,
                                 uint8_t groupOffset,
                                 Stack *stack,
                                 minify_write_fn_t write) {
    assert(stack->size > 0 && stack->size <= N);

    uint16_t consumed = 0;

    for (uint8_t i = 0; i < stack->size; i++) {
        // check if circuit has already been accounted for
        // i.e. the intensity value matched another and the update was bulked
        if (consumed & CIRCUIT_BIT(i)) continue;

        const struct encoding_change_t change = stack->changes[i];

        // the intensity did not change, do not attempt to create a bulk update
        // another circuit with the same intensity will still be updated, with
        // itself as the root circuit
        if (change.newIntensity == change.oldIntensity) continue;

        const uint16_t matches = stackGetMatches(stack, change.newIntensity);

        // mark all matched circuits as consumed for a bulk update
        consumed |= matches;

        const int popcount = __builtin_popcount(matches);

        minifyEncodeRequest(
                (struct encoding_request_t){
                        .unit = unit,
                        .groupOffset = popcount == 1 ? 0 : groupOffset,
                        .circuits = popcount == 1 ? change.circuit : matches,
                        .nCircuits = popcount,
                        .intensity = change.newIntensity,
                },
                write);

        // detect when all circuits are handled and break early
        if (consumed == 0xFFFFu) break;
    }
}

void minifyStream(const uint8_t *frameData,
                  const uint8_t *lastFrameData,
                  uint32_t size,
                  minify_write_fn_t write) {
    uint8_t prevUnit = 0;

    Stack stack = {0};

    for (uint32_t id = 0; id < size; id++) {
        uint8_t unit;
        uint16_t circuit;

        if (!channelMapFind(id, &unit, &circuit)) continue;

        const bool unitIdChanged = stack.size > 0 && prevUnit != unit;

        if (unitIdChanged) stackFlush(prevUnit, &stack, write);

        // old stack is flushed, update context to use new unit value
        prevUnit = unit;

        // test if circuits are too far apart for minifying
        // flush the previous stack and process the request in the reset stack
        const bool outOfRange =
                stack.size > 0 && circuit >= stack.changes[0].circuit + N;

        if (outOfRange) stackFlush(unit, &stack, write);

        // record values onto (possibly fresh) stack
        // flush when stack is full
        const bool stackFull =
                stackPush(&stack, (struct encoding_change_t){
                                          .circuit = circuit,
                                          .newIntensity = frameData[id],
                                          .oldIntensity = lastFrameData[id],
                                  });

        if (stackFull) stackFlush(unit, &stack, write);
    }

    // flush any pending data from the last iteration
    if (stack.size > 0) stackFlush(prevUnit, &stack, write);
}
