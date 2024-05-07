#include "encode.h"

#include <assert.h>
#include <stdbool.h>

#include "stb_ds.h"

#define CIRCUIT_BIT(i) ((uint16_t) (1 << (i)))

uint16_t encodeStackGetMatches(const EncodeChange* const stack,
                               const EncodeChange compare) {
    uint16_t matches = 0;

    const int len = arrlen(stack);
    assert(len > 0);

    const int max = len < 16 ? len : 16;

    for (int i = 0; i < max; i++) {
        const EncodeChange change = stack[i];

        // the intensity did not change, do not attempt to create a bulk update
        // another circuit with the same intensity will still be updated, with
        // itself as the root circuit
        const bool didChangeIntensity =
                change.newIntensity != change.oldIntensity;

        if (didChangeIntensity && change.newIntensity == compare.newIntensity)
            matches |= CIRCUIT_BIT(i);
    }

    return matches;
}
