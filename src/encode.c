#include "encode.h"

#include <assert.h>
#include <string.h>

void encodeStackPush(EncodeStack *stack, EncodeChange change) {
    const int idx = stack->nChanges++;

    assert(idx >= 0 && idx < encodeStackCapacity());

    stack->changes[idx] = change;
}

inline bool encodeStackFull(const EncodeStack *const stack) {
    return stack->nChanges >= encodeStackCapacity();
}

void encodeStackAlign(const EncodeStack *src,
                      int offset,
                      EncodeStack *low,
                      EncodeStack *high) {
    const int capacity = encodeStackCapacity();

    memset(low->changes, 0, sizeof(EncodeChange) * capacity);
    memset(high->changes, 0, sizeof(EncodeChange) * capacity);

    // aligns a 16-byte (N) stack with a given offset (0,15) between two N-byte stacks
    //
    // [*low                                          ] [*high                                         ]
    // [ 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31]
    //                                                  N
    //                                         [8 size `src` example]
    //                                         ^ offset = 13

    // slots remaining in low stack
    const int lremaining = capacity - offset;

    // copyable count for low stack
    const int lc = src->nChanges < lremaining ? src->nChanges : lremaining;

    memcpy(&low->changes[offset], src->changes, sizeof(EncodeChange) * lc);

    low->nChanges = lc;

    // copyable count for high stack
    const int hc = src->nChanges - lc;

    assert(hc >= 0);

    if (hc > 0)
        memcpy(&high->changes[0], &src->changes[lc], sizeof(EncodeChange) * hc);

    high->nChanges = hc;
}

#define CIRCUIT_BIT(i) ((uint16_t) (1 << (i)))

uint16_t encodeStackGetMatches(const EncodeStack *const stack,
                               const EncodeChange compare) {
    uint16_t matches = 0;

    for (int i = 0; i < stack->nChanges; i++) {
        const EncodeChange change = stack->changes[i];

        if (compare.fade) {
            if (change.oldIntensity == compare.oldIntensity &&
                change.newIntensity == compare.newIntensity && change.fade)
                matches |= CIRCUIT_BIT(i);
        } else {
            if (change.newIntensity == compare.newIntensity)
                matches |= CIRCUIT_BIT(i);
        }
    }

    return matches;
}
