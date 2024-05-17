#include "cell.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "crmap.h"
#include <std2/errcode.h>

struct cell_s {
    _Bool valid : 1;    /* cell is configured and valid */
    _Bool modified : 1; /* cell has been modified since last groupof */
    uint8_t unit;       /* hardware unit id for routing */
    uint8_t section;    /* circuit id / 16 for 16-bit proto alignment */
    uint8_t offset;     /* circuit id % 16 for 16-bit proto alignment */
    uint8_t intensity;  /* current output intensity */
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
        c->modified = 1;
        c->section = (channel - 1) / 16;
        c->offset = (channel - 1) % 16;

        confd++;
    }

    printf("configured %u/%u indexes\n", confd, size);

    return FP_EOK;
}

void CT_set(struct ctable_s* table,
            const uint32_t index,
            const uint8_t output,
            const bool diff) {
    assert(table != NULL);
    assert(index < table->size);

    struct cell_s* c = &table->cells[index];
    if (!c->valid || (diff && c->intensity == output)) return;
    c->modified = 1;
    c->intensity = output;
}

/// @brief Checks if two cells match, which indicates they are addressed to the
/// same hardware controller (unit+section), and have a matching output intensity.
/// @param a first cell to compare
/// @param b second cell to compare
/// @return true if the cells match, false otherwise
static inline bool CT_matches(const struct cell_s* a, const struct cell_s* b) {
    assert(a != NULL);
    assert(b != NULL);

    return a->unit == b->unit && a->section == b->section &&
           a->intensity == b->intensity;
}

#define MAX_MATCHES 16

/// @brief Finds all cells in the table that match the provided reference cell.
/// The reference cell must be valid and modified. The matching cells are stored
/// in the provided array, up to a maximum of `MAX_MATCHES`.
/// @param table table to search
/// @param start index to start searching from
/// @param cmp reference cell to match against
/// @param matches array to store matching cells
/// @return the number of matching cells found
static int CT_findMatches(struct ctable_s* table,
                          const uint32_t start,
                          const struct cell_s* cmp,
                          struct cell_s* matches[MAX_MATCHES]) {
    assert(table != NULL);
    assert(cmp != NULL);
    assert(cmp->valid);
    assert(cmp->modified);
    assert(matches != NULL);

    int pos = 0;
    for (uint32_t i = start; i < table->size; i++) {
        struct cell_s* c = &table->cells[i];
        if (c->valid && c->modified && CT_matches(c, cmp)) matches[pos++] = c;
        if (pos >= MAX_MATCHES) break;
    }
    return pos;
}

#define CHANNEL_BIT(i) (1 << (i))

int CT_groupof(struct ctable_s* table, uint32_t at, struct ctgroup_s* group) {
    assert(table != NULL);
    assert(group != NULL);

    *group = (struct ctgroup_s){0};

    struct cell_s* cmp = &table->cells[at];
    if (!cmp->valid || !cmp->modified) return 0;

    struct cell_s* matches[MAX_MATCHES] = {NULL};
    const int mc = CT_findMatches(table, at, cmp, matches);

    assert(mc > 0);// should always find at least one match (the initial input)
    assert(mc <= MAX_MATCHES);

    // group the cells together
    for (int i = 0; i < mc; i++) {
        struct cell_s* m = matches[i];
        assert(m->valid);
        assert(m->modified);

        group->cs |= CHANNEL_BIT(m->offset);

        // first cell in the group, use its data as the group's shared data
        // (this is already known to match via the hash checking previously)
        if (group->size++ == 0) {
            group->offset = m->section;
            group->unit = m->unit;
            group->intensity = m->intensity;
        }

        m->modified = 0;// consume the hash value to prevent re-matching
    }

    return 1;
}

void CT_free(struct ctable_s* table) {
    if (table == NULL) return;
    free(table->cells);
    free(table);
}
