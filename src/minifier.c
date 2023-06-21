#include "minifier.h"

#include <assert.h>

#include <lightorama/easy.h>
#include <lightorama/lightorama.h>

#include "cmap.h"
#include "lor.h"

#define CIRCUIT_BIT(i) ((uint16_t) (1 << i))

static uint16_t minifyGetMatches(int nCircuits,
                                 const uint8_t frameData[16],
                                 uint8_t intensity) {
    assert(nCircuits > 0 && nCircuits <= 16);

    uint16_t matches = 0;

    for (int i = 0; i < nCircuits; i++) {
        if (frameData[i] == intensity) matches |= CIRCUIT_BIT(i);
    }

    return matches;
}

static void minifySetSingle(uint8_t unit,
                            int circuit,
                            uint8_t intensity,
                            minify_write_fn_t write) {
    uint8_t encodeBuf[LOR_PACKET_BUFFER];

    const struct lor_effect_setintensity_t setEffect = {
            .intensity =
                    lor_intensity_curve_vendor((float) (intensity / 255.0)),
    };

    const int written = lor_write_channel_effect(
            LOR_EFFECT_SET_INTENSITY, &setEffect, circuit - 1, unit, encodeBuf);

    assert(written <= LOR_PACKET_BUFFER);

    write(encodeBuf, written);
}

static void minifySetMask(uint8_t unit,
                          uint8_t groupOffset,
                          uint16_t circuits,
                          uint8_t intensity,
                          minify_write_fn_t write) {
    uint8_t encodeBuf[LOR_PACKET_BUFFER];

    const struct lor_effect_setintensity_t setEffect = {
            .intensity =
                    lor_intensity_curve_vendor((float) (intensity / 255.0)),
    };

    const int written =
            lor_write_channelset_effect(LOR_EFFECT_SET_INTENSITY, &setEffect,
                                        (lor_channelset_t){
                                                .offset = groupOffset,
                                                .channels = circuits,
                                        },
                                        unit, encodeBuf);

    assert(written <= LOR_PACKET_BUFFER);

    write(encodeBuf, written);
}

static void minifyStreamChunk(uint8_t unit,
                              uint8_t groupOffset,
                              uint8_t nCircuits,
                              const uint8_t frameData[16],
                              minify_write_fn_t write) {
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
            const int absoluteCircuit = (groupOffset * 16) + circuit;

            minifySetSingle(unit, absoluteCircuit, intensity, write);
        } else {
            minifySetMask(unit, groupOffset, matches, intensity, write);
        }

        // detect when all circuits are handled and break early
        if (consumed == 0xFFFFu) break;
    }
}

static void minifyFlushChunk(uint8_t unit,
                             int *nStack,
                             uint8_t circuitStack[16],
                             uint8_t powerStack[16]) {
    // TODO

    *nStack = 0;
}

void minifyStream(const uint8_t *frameData,
                  uint32_t size,
                  minify_write_fn_t write) {
    uint8_t circuitStack[16] = {0};
    uint8_t powerStack[16] = {0};

    int nStack = 0;

    uint8_t prevUnit = 0;

    for (uint32_t id = 0; id < size; id++) {
        uint8_t unit;
        uint16_t circuit;

        if (!channelMapFind(id, &unit, &circuit)) continue;

        if (prevUnit > 0 && prevUnit != unit)
            minifyFlushChunk(unit, &nStack, circuitStack, powerStack);

        prevUnit = unit;

        // make sure circuit values are sequential
        if (nStack > 0 && circuitStack[nStack - 1] != circuit - 1)
            minifyFlushChunk(unit, &nStack, circuitStack, powerStack);

        // push the circuit+output value onto a minifier stack
        // this is the pending flush queue for sequential circuits
        circuitStack[nStack] = circuit;
        powerStack[nStack] = frameData[id];

        nStack++;

        if (nStack == 16) {
            minifyFlushChunk(unit, &nStack, circuitStack, powerStack);

            // reset to avoid wasteful false trip when next chunk arrives
            prevUnit = 0;
        }
    }

    // flush any pending data from the last iteration
    if (nStack > 0)
        minifyFlushChunk(prevUnit, &nStack, circuitStack, powerStack);
}
