#include "cell.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <lorproto/intensity.h>

#include <std2/errcode.h>

#include "../crmap.h"

struct cell_s {
    _Bool valid : 1;        /* cell is configured and valid */
    _Bool linked : 1;       /* cell is linked to the neighboring cell */
    _Bool outdated : 1;     /* output has changed since last frame */
    uint8_t unit;           /* hardware unit id for routing */
    uint8_t section;        /* circuit id / 16 for 16-bit proto alignment */
    uint8_t offset;         /* circuit id % 16 for 16-bit proto alignment */
    LorIntensity intensity; /* current output intensity */
};

struct ctable_s {
    struct cell_s* cells;
    size_t size;
};

int CT_init(const struct cr_s* cmap,
            const uint32_t size,
            struct ctable_s** table) {
    assert(cmap != NULL);
    assert(size > 0);
    assert(table != NULL);

    struct ctable_s* t;
    if ((t = calloc(1, sizeof(struct ctable_s))) == NULL) return -FP_ENOMEM;

    t->size = size;
    if ((t->cells = calloc(size, sizeof(struct cell_s))) == NULL) {
        free(t);
        return -FP_ENOMEM;
    }

    *table = t;

    uint32_t confd = 0; /* number of configured cells */

    for (uint32_t i = 0; i < size; i++) {
        struct cell_s* c = &t->cells[i];

        // attempt to map raw index to known device
        uint16_t channel;
        if (!CMap_lookup(cmap, i, &c->unit, &channel)) {
            fprintf(stderr, "channel mapping does not cover index %u\n", i);
            continue;
        }

        assert(channel > 0);

        c->valid = 1;
        c->section = (channel - 1) / 16;
        c->offset = (channel - 1) % 16;
        c->intensity = LOR_INTENSITY_MIN;
        confd++;
    }

    printf("configured %u/%u indexes\n", confd, size);

    return FP_EOK;
}

void CT_set(struct ctable_s* table,
            const uint32_t index,
            const uint8_t output) {
    assert(table != NULL);
    assert(index < table->size);

    struct cell_s* c = &table->cells[index];
    if (c->intensity == output) return;
    c->intensity = output;
    c->outdated = 1;
}

/// @brief Determines if the two cells could be merged. This checks compatibility
/// between the two cells, ensuring they are valid, on the same unit, within
/// the same 16-bit aligned channel bank definition, and set to the same output.
/// @param a first cell to compare
/// @param b second cell to compare
/// @return true if the cells are mergeable, false otherwise
static _Bool CT_linkable(const struct cell_s* a, const struct cell_s* b) {
    return a->valid && b->valid && a->unit == b->unit &&
           a->section == b->section && a->intensity == b->intensity;
}

void CT_linkall(struct ctable_s* table) {
    assert(table != NULL);

    for (uint32_t i = 0; i < table->size; i++) {
        struct cell_s* c = &table->cells[i];

        c->linked = c->valid && i < table->size - 1 &&
                    CT_linkable(c, &table->cells[i + 1]);
    }
}

/// @brief Finds the next valid cell in the table starting from the given index.
/// @param table table to search
/// @param from pointer to the current index to start searching from, returns the
/// next valid index if successful, otherwise the value is undefined
/// @return non-zero if a valid cell was found, zero otherwise
static int CT_findnext(const struct ctable_s* table, uint32_t* from) {
    assert(table != NULL);
    assert(from != NULL);

    for (; *from < table->size && !table->cells[*from].valid; (*from)++)
        ;
    return *from < table->size;
}

#define CHANNEL_BIT(i) (1 << (i))

int CT_nextgroup(struct ctable_s* table,
                 uint32_t* from,
                 struct ctgroup_s* group) {
    assert(table != NULL);
    assert(from != NULL);
    assert(group != NULL);

    *group = (struct ctgroup_s){0};

    // find the first/next valid cell within the table to handle
    if (!CT_findnext(table, from)) return 0;

    do {
        const struct cell_s* c = &table->cells[*from];
        assert(c->valid);// should not be possible to reach an invalid cell

        group->cs.channelBits |= CHANNEL_BIT(c->offset);

        // first cell in the group, copy the data that is shared across the
        // linked cells into the group
        if (group->size++ == 0) {
            group->cs.offset = c->section;
            group->unit = c->unit;
            group->intensity = c->intensity;
        }

        (*from)++;

        // TODO: use outdated bit to determine write importance for more compression
    } while (*from < table->size && table->cells[*from - 1].linked);

    return 1;
}

void CT_free(struct ctable_s* table) {
    if (table == NULL) return;
    free(table->cells);
    free(table);
}
