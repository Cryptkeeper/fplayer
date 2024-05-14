#undef NDEBUG
#include <assert.h>
#include <string.h>

#include "crmap.h"
#include "transform/cell.h"

#define ISIZE 16

// via `default_channels.json`
#define UNITID 20

/// @brief Sets the intensity value of all cells in the table to the given value.
/// @param table table to set
/// @param intensity intensity value to set
static void Pop_setAll(struct ctable_s* table, const uint8_t intensity) {
    for (int i = 0; i < ISIZE; i++) CT_set(table, i, intensity);
}

static void Test_setAll(struct ctable_s* table, const uint8_t target) {
    /// This configures the entire table with the same intensity value. The table
    /// is then linked, and the first group is extracted. The group should contain
    /// the entire table, with the matching intensity value, expected number of
    /// results, and correctly encoded network protocol data. No further groups
    /// should be available after the first.
    {
        Pop_setAll(table, target);
        CT_linkall(table);

        uint32_t from = 0;
        struct ctgroup_s group;
        assert(CT_nextgroup(table, &from, &group) == 1);
        assert(group.size == ISIZE);
        assert(group.unit == UNITID);
        assert(group.cs.offset == 0);
        assert(group.cs.channelBits == 0xFFFF);
        assert(group.intensity == target);

        assert(CT_nextgroup(table, &from, &group) == 0);
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
        CT_set(table, i, intensity);
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
    {
        Pop_halfAndHalf(table, low, high);
        CT_linkall(table);

        uint32_t from = 0;
        struct ctgroup_s group;
        assert(CT_nextgroup(table, &from, &group) == 1);
        assert(group.size == ISIZE / 2);
        assert(group.unit == UNITID);
        assert(group.cs.offset == 0);
        assert(group.cs.channelBits == 0x00FF);
        assert(group.intensity == low);

        assert(CT_nextgroup(table, &from, &group) == 1);
        assert(group.size == ISIZE / 2);
        assert(group.unit == UNITID);
        assert(group.cs.offset == 0);
        assert(group.cs.channelBits == 0xFF00);
        assert(group.intensity == high);

        assert(CT_nextgroup(table, &from, &group) == 0);
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

    CT_free(table);
    CMap_free(cr);

    return 0;
}
