#include "minifier.h"

#include <assert.h>
#include <string.h>

#include <lightorama/easy.h>
#include <lightorama/lightorama.h>

#include "cmap.h"
#include "lor.h"

#define CIRCUIT_BIT(i) ((uint16_t) (1 << i))

static inline uint16_t minifyGetMatches(int nCircuits,
                                        const uint8_t frameData[16],
                                        uint8_t intensity) {
    assert(nCircuits > 0 && nCircuits <= 16);

    uint16_t matches = 0;

    for (int i = 0; i < nCircuits; i++) {
        if (frameData[i] == intensity) matches |= CIRCUIT_BIT(i);
    }

    return matches;
}

typedef struct encoding_ctx_t {
    minify_write_fn_t write;
    uint8_t unit;
    uint8_t groupOffset;
} Ctx;

static void minifyWriteUpdate(Ctx *ctx, int circuit, uint8_t intensity) {
    uint8_t encodeBuf[LOR_PACKET_BUFFER];

    const struct lor_effect_setintensity_t setEffect = {
            .intensity =
                    lor_intensity_curve_vendor((float) (intensity / 255.0)),
    };

    const int written =
            lor_write_channel_effect(LOR_EFFECT_SET_INTENSITY, &setEffect,
                                     circuit - 1, ctx->unit, encodeBuf);

    assert(written <= LOR_PACKET_BUFFER);

    ctx->write(encodeBuf, written);
}

static void
minifyWriteMultiUpdate(Ctx *ctx, uint16_t circuits, uint8_t intensity) {
    uint8_t encodeBuf[LOR_PACKET_BUFFER];

    const struct lor_effect_setintensity_t setEffect = {
            .intensity =
                    lor_intensity_curve_vendor((float) (intensity / 255.0)),
    };

    const int written =
            lor_write_channelset_effect(LOR_EFFECT_SET_INTENSITY, &setEffect,
                                        (lor_channelset_t){
                                                .offset = ctx->groupOffset,
                                                .channels = circuits,
                                        },
                                        ctx->unit, encodeBuf);

    assert(written <= LOR_PACKET_BUFFER);

    ctx->write(encodeBuf, written);
}

// "explodes" a series of up to 16 brightness values (uint8_t) into a series of
// update packets (either as individual channels, multichannel bitmasks, or a mixture
// of both) via the ctx->write function
static void
minifyWrite16Aligned(Ctx *ctx, uint8_t nCircuits, const uint8_t frameData[16]) {
    assert(nCircuits > 0 && nCircuits <= 16);

    uint16_t consumed = 0;

    for (uint8_t circuit = 0; circuit < nCircuits; circuit++) {
        // check if circuit has already been accounted for
        // i.e. the intensity value matched another and the update was bulked
        if (consumed & CIRCUIT_BIT(circuit)) continue;

        const uint8_t intensity = frameData[circuit];

        const uint16_t matches =
                minifyGetMatches(nCircuits, frameData, intensity);

        // mark all matched circuits as consumed for a bulk update
        consumed |= matches;

        const int popcount = __builtin_popcount(matches);

        // use individual encode operations (optimizes bandwidth usage) depending
        // on the amount of matched circuits being updated via popcount
        if (popcount == 1) {
            // drop offset and calculate absolute circuit ID
            // minifyStream operates on 16 byte chunks to better align with the
            // window size for how LOR addresses circuit groups
            const int absoluteCircuit = (ctx->groupOffset * 16) + circuit + 1;

            minifyWriteUpdate(ctx, absoluteCircuit, intensity);
        } else {
            minifyWriteMultiUpdate(ctx, matches, intensity);
        }

        // detect when all circuits are handled and break early
        if (consumed == 0xFFFFu) break;
    }
}

typedef struct encoding_stack_t {
    uint8_t circuits[16];
    uint8_t intensities[16];
    uint8_t size;
} Stack;

static bool stackPush(Stack *stack, uint8_t circuit, uint8_t intensity) {
    const int idx = stack->size++;

    assert(idx >= 0 && idx < 16);

    stack->circuits[idx] = circuit;
    stack->intensities[idx] = intensity;

    // return true when stack is full and should be written
    return stack->size >= 16;
}

// map each circuit stack value to its intensity stack value
// if the map result is not a multiple of 16 (i.e. not aligned to the 16-bit LOR protocol window),
// it is written into a double-sized buffer, and each half is written individually as needed
static void stackFlush(Stack *stack, Ctx *ctx) {
    const int firstCircuit = stack->circuits[0] - 1;

    ctx->groupOffset = firstCircuit / 16;

    const int alignOffset = firstCircuit % 16;

    if (alignOffset == 0) {
        minifyWrite16Aligned(ctx, stack->size, stack->intensities);
    } else {
        // circuits aren't boundary aligned
        // manually construct up to two frames to re-align the data within
        uint8_t doubleChunk[32] = {0};

        memcpy(&doubleChunk[alignOffset], (const uint8_t *) stack->intensities,
               stack->size);

        minifyWrite16Aligned(ctx, 16, &doubleChunk[0]);

        const int chunkSize = alignOffset + stack->size;

        if (chunkSize > 16) minifyWrite16Aligned(ctx, 16, &doubleChunk[16]);
    }

    stack->size = 0;
}

void minifyStream(const uint8_t *frameData,
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
                stack.size > 0 && circuit >= stack.circuits[0] + 16;

        if (outOfRange) stackFlush(&stack, ctx);

        // record values onto (possibly fresh) stack
        // flush when stack is full
        const bool stackFull = stackPush(&stack, circuit, frameData[id]);

        if (stackFull) stackFlush(&stack, ctx);
    }

    // flush any pending data from the last iteration
    if (stack.size > 0) stackFlush(&stack, ctx);
}
