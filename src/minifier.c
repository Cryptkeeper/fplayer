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

typedef struct encoding_ctx_t {
    minify_write_fn_t write;
    uint8_t unit;
    uint8_t groupOffset;
} Ctx;

static void minifyWriteUpdate(Ctx *ctx, uint16_t circuit, uint8_t intensity) {
    const struct lor_effect_setintensity_t setEffect = {
            .intensity =
                    lor_intensity_curve_vendor((float) (intensity / 255.0)),
    };

    bufadv(lor_write_channel_effect(LOR_EFFECT_SET_INTENSITY, &setEffect,
                                    circuit - 1, ctx->unit, bufhead()));

    bufflush(false, ctx->write);
}

static void
minifyWriteMultiUpdate(Ctx *ctx, uint16_t circuits, uint8_t intensity) {
    const struct lor_effect_setintensity_t setEffect = {
            .intensity =
                    lor_intensity_curve_vendor((float) (intensity / 255.0)),
    };

    bufadv(lor_write_channelset_effect(LOR_EFFECT_SET_INTENSITY, &setEffect,
                                       (lor_channelset_t){
                                               .offset = ctx->groupOffset,
                                               .channels = circuits,
                                       },
                                       ctx->unit, bufhead()));

    bufflush(false, ctx->write);
}

#define CIRCUIT_BIT(i) ((uint16_t) (1 << (i)))

struct encoding_entry_t {
    uint8_t circuit;
    uint8_t newIntensity;
    uint8_t oldIntensity;
};

typedef struct encoding_stack_t {
    struct encoding_entry_t entries[N];
    uint8_t size;
} Stack;

static inline uint16_t minifyGetMatches(Stack *stack, uint8_t intensity) {
    uint16_t matches = 0;

    for (int i = 0; i < stack->size; i++) {
        const struct encoding_entry_t entry = stack->entries[i];

        // exclude frames that have not changed intensity
        // hardware maintains the state, do not need to resend current values
        if (entry.newIntensity == entry.oldIntensity) continue;

        if (entry.newIntensity == intensity) matches |= CIRCUIT_BIT(i);
    }

    return matches;
}

// "explodes" a series of up to 16 brightness values (uint8_t) into a series of
// update packets (either as individual channels, multichannel bitmasks, or a mixture
// of both) via the ctx->write function
static void minifyWrite16Aligned(Ctx *ctx, Stack *stack) {
    assert(stack->size > 0 && stack->size <= N);

    uint16_t consumed = 0;

    for (uint8_t i = 0; i < stack->size; i++) {
        // check if circuit has already been accounted for
        // i.e. the intensity value matched another and the update was bulked
        if (consumed & CIRCUIT_BIT(i)) continue;

        const struct encoding_entry_t entry = stack->entries[i];

        const uint16_t matches = minifyGetMatches(stack, entry.newIntensity);

        // zero matches (i.e. circuit intensity doesn't match itself) indicates
        // the intensity did not change between the two frames and should be
        // ignored from the grouping mechanism
        if (matches == 0) continue;

        // mark all matched circuits as consumed for a bulk update
        consumed |= matches;

        const int popcount = __builtin_popcount(matches);

        // use individual encode operations (optimizes bandwidth usage) depending
        // on the amount of matched circuits being updated via popcount
        if (popcount == 1) {
            minifyWriteUpdate(ctx, entry.circuit, entry.newIntensity);
        } else {
            minifyWriteMultiUpdate(ctx, matches, entry.newIntensity);
        }

        // detect when all circuits are handled and break early
        if (consumed == 0xFFFFu) break;
    }
}

static bool stackPush(Stack *stack, struct encoding_entry_t entry) {
    const int idx = stack->size++;

    assert(idx >= 0 && idx < N);

    stack->entries[idx] = entry;

    // return true when stack is full and should be written
    return stack->size >= N;
}

static void stackAlign(const Stack *src, int offset, Stack *low, Stack *high) {
    memset(low->entries, 0, sizeof(struct encoding_entry_t) * N);
    memset(high->entries, 0, sizeof(struct encoding_entry_t) * N);

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

    memcpy(&low->entries[offset], src->entries,
           sizeof(struct encoding_entry_t) * lc);

    low->size = lc;

    // copyable count for high stack
    const int hc = src->size - lc;

    assert(hc >= 0);

    if (hc > 0)
        memcpy(&high->entries[0], &src->entries[lc],
               sizeof(struct encoding_entry_t) * hc);

    high->size = hc;
}

// map each circuit stack value to its intensity stack value
// if the map result is not a multiple of 16 (i.e. not aligned to the 16-bit LOR protocol window),
// it is written into a double-sized buffer, and each half is written individually as needed
static void stackFlush(Stack *stack, Ctx *ctx) {
    const struct encoding_entry_t entry = stack->entries[0];

    const int firstCircuit = entry.circuit - 1;

    ctx->groupOffset = firstCircuit / N;

    const int alignOffset = firstCircuit % N;

    if (alignOffset == 0) {
        minifyWrite16Aligned(ctx, stack);
    } else {
        static Stack gLow, gHigh;

        // first circuit ID isn't boundary aligned
        // manually construct up to two frames to re-align the data within
        stackAlign(stack, alignOffset, &gLow, &gHigh);

        minifyWrite16Aligned(ctx, &gLow);

        if (gHigh.size > 0) minifyWrite16Aligned(ctx, &gHigh);
    }

    stack->size = 0;
}

void minifyStream(const uint8_t *frameData,
                  const uint8_t *lastFrameData,
                  uint32_t size,
                  minify_write_fn_t write) {
    Ctx *ctx = &(Ctx){
            .write = write,
    };

    Stack stack = {0};

    for (uint32_t id = 0; id < size; id++) {
        uint8_t unit;
        uint16_t circuit;

        if (!channelMapFind(id, &unit, &circuit)) continue;

        const bool unitIdChanged = stack.size > 0 && ctx->unit != unit;

        if (unitIdChanged) stackFlush(&stack, ctx);

        // old stack is flushed, update context to use new unit value
        ctx->unit = unit;

        // test if circuits are too far apart for minifying
        // flush the previous stack and process the request in the reset stack
        const bool outOfRange =
                stack.size > 0 && circuit >= stack.entries[0].circuit + N;

        if (outOfRange) stackFlush(&stack, ctx);

        // record values onto (possibly fresh) stack
        // flush when stack is full
        const bool stackFull =
                stackPush(&stack, (struct encoding_entry_t){
                                          .circuit = circuit,
                                          .newIntensity = frameData[id],
                                          .oldIntensity = lastFrameData[id],
                                  });

        if (stackFull) stackFlush(&stack, ctx);
    }

    // flush any pending data from the last iteration
    if (stack.size > 0) stackFlush(&stack, ctx);
}
