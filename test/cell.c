#undef NDEBUG
#include <assert.h>
#include <string.h>

#include "cell.h"
#include "crmap.h"

#define ISIZE 16

// via `default_channels.json`
#define UNITID 20

/// @brief Sets the intensity value of all cells in the table to the given value.
/// @param table table to set
/// @param intensity intensity value to set
static void Pop_setAll(struct ctable_s* table, const uint8_t intensity) {
    for (int i = 0; i < ISIZE; i++) CT_set(table, i, intensity, false);
}

static void Test_setAll(struct ctable_s* table, const uint8_t target) {
    /// This configures the entire table with the same intensity value. The table
    /// is then linked, and the first group is extracted. The group should contain
    /// the entire table, with the matching intensity value, expected number of
    /// results, and correctly encoded network protocol data. No further groups
    /// should be available after the first.
    Pop_setAll(table, target);

    struct ctgroup_s group;

    for (uint32_t at = 0; at < ISIZE; at++) {
        if (at == 0) {
            assert(CT_groupof(table, at, &group) == 1);
            assert(group.size == ISIZE);
            assert(group.unit == UNITID);
            assert(group.offset == 0);
            assert(group.cs == 0xFFFF);
            assert(group.intensity == target);
        } else {
            assert(CT_groupof(table, at, &group) == 0);
        }
    }
}

/// @brief Generates a half-and-half intensity table, where the first half of
/// the table is set to the low intensity value, and the second half is set to
/// the high intensity value.
/// @param table table to set
/// @param low low intensity value
/// @param high high intensity value
static void
Pop_halfAndHalf(struct ctable_s* table, const uint8_t low, const uint8_t high) {
    for (int i = 0; i < ISIZE; i++) {
        const uint8_t intensity = i < (ISIZE / 2) ? low : high;
        CT_set(table, i, intensity, false);
    }
}

static void Test_halfAndHalf(struct ctable_s* table,
                             const uint8_t low,
                             const uint8_t high) {
    /// This configures half the channel range with one value, and the other half
    /// with another value. The table is then linked, and the first group is
    /// extracted. The group should contain the first half of the table, with the
    /// matching intensity value, expected number of results, and correctly
    /// encoded network protocol data. This is repeated for the second half of the
    /// table. No further groups should be available after the two.
    Pop_halfAndHalf(table, low, high);

    struct ctgroup_s group;

    for (uint32_t at = 0; at < ISIZE; at++) {
        if (at == 0) {
            assert(CT_groupof(table, at, &group) == 1);
            assert(group.size == ISIZE / 2);
            assert(group.unit == UNITID);
            assert(group.offset == 0);
            assert(group.cs == 0x00FF);
            assert(group.intensity == low);
        } else if (at == ISIZE / 2) {
            assert(CT_groupof(table, at, &group) == 1);
            assert(group.size == ISIZE / 2);
            assert(group.unit == UNITID);
            assert(group.offset == 0);
            assert(group.cs == 0xFF00);
            assert(group.intensity == high);
        } else {
            assert(CT_groupof(table, at, &group) == 0);
        }
    }
}

/// @brief Generates an alternating intensity table, where the intensity value
/// alternates between the low and high values for each channel.
/// @param table table to set
/// @param low low intensity value
/// @param high high intensity value
static void
Pop_alternating(struct ctable_s* table, const uint8_t low, const uint8_t high) {
    for (int i = 0; i < ISIZE; i++) {
        const uint8_t intensity = i % 2 == 0 ? low : high;
        CT_set(table, i, intensity, false);
    }
}

static void Test_alternating(struct ctable_s* table,
                             const uint8_t low,
                             const uint8_t high) {
    // This configures the table with an alternating intensity value. The table is
    // then linked, and the first group is extracted. The group should contain the
    // half the table corresponding to the low intensity value, with the matching
    // channel bitmask layout. This is repeated for the high intensity value. No
    // further groups should be available after the two.
    Pop_alternating(table, low, high);

    struct ctgroup_s group;

    for (uint32_t at = 0; at < ISIZE; at++) {
        if (at == 0) {
            assert(CT_groupof(table, at, &group) == 1);
            assert(group.size == ISIZE / 2);
            assert(group.unit == UNITID);
            assert(group.offset == 0);
            assert(group.cs == 0x5555);
            assert(group.intensity == low);
        } else if (at == 1) {
            assert(CT_groupof(table, at, &group) == 1);
            assert(group.size == ISIZE / 2);
            assert(group.unit == UNITID);
            assert(group.offset == 0);
            assert(group.cs == 0xAAAA);
            assert(group.intensity == high);
        } else {
            assert(CT_groupof(table, at, &group) == 0);
        }
    }
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    struct cr_s* cr = NULL;
    assert(CMap_read("../test/default_channels.json", &cr) == 0);

    struct ctable_s* table = NULL;
    assert(CT_init(cr, ISIZE, &table) == 0);

    Test_setAll(table, 0);
    Test_setAll(table, 255);

    Test_halfAndHalf(table, 0, 0xFF);
    Test_halfAndHalf(table, 0xFF, 0x00);

    Test_alternating(table, 0, 0xFF);
    Test_alternating(table, 0xFF, 0x00);

    CT_free(table);
    CMap_free(cr);

    return 0;
}
