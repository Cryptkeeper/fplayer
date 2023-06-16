#include "minifier.h"

#include <assert.h>

#include <lightorama/easy.h>
#include <lightorama/lightorama.h>

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

// guidance size for a stack-based Light-O-Rama packet encoding buffer
// no LOR packet emitted by this code should exceed half this size
#define LOR_PACKET_BUFFER 32

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
                          uint16_t circuits,
                          uint8_t intensity,
                          minify_write_fn_t write) {
    uint8_t encodeBuf[LOR_PACKET_BUFFER];

    const struct lor_effect_setintensity_t setEffect = {
            .intensity =
                    lor_intensity_curve_vendor((float) (intensity / 255.0)),
    };

    // TODO: this needs to support base offsets (i.e. 0-15 relative to n) for
    // expanded channel ranges. I know LOR supports this, but I think the lib is
    // missing support. Need to dig into old research.
    const int written =
            lor_write_channelset_effect(LOR_EFFECT_SET_INTENSITY, &setEffect,
                                        (lor_channelset_t){
                                                .offset = 0,
                                                .channels = circuits,
                                        },
                                        unit, encodeBuf);

    assert(written <= LOR_PACKET_BUFFER);

    write(encodeBuf, written);
}

void minifyStream(uint8_t unit,
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
        if (popcount == 1) minifySetSingle(unit, circuit, intensity, write);
        else
            minifySetMask(unit, matches, intensity, write);

        // detect when all circuits are handled and break early
        if (consumed == 0xFFFFu) break;
    }
}
