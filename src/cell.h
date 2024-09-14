/// @file cell.h
/// @brief Cell table for tracking output states.
#ifndef FPLAYER_CELL_H
#define FPLAYER_CELL_H

#include <stdint.h>

/// @struct ctable_s
/// @brief Represents a table of cells that map raw FSEQ sequence indexes to a
/// known unit and channel number.
struct ctable_s;

struct cr_s;

/// @brief Initializes table that maps the raw fseq sequence indexes to a
/// known LOR unit and channel number using the provided channel map. The lookup
/// is cached into the table for faster access. The table is dynamically
/// allocated and must be freed with `CT_free`.
/// @param cmap channel map to use for lookup
/// @param size number of indexes to map
/// @param table pointer to store the table
/// @return 0 on success, or a negative error code on failure
int CT_init(const struct cr_s* cmap, uint32_t size, struct ctable_s** table);

/// @brief Sets the output intensity for the cell at the given index. This marks
/// the cell as modified, regardless if the new output intensity is the same as
/// the current value.
/// @param table table to set the output on
/// @param index index of the cell to set
/// @param output intensity to set
void CT_set(struct ctable_s* table, uint32_t index, uint8_t output);

/// @brief Changes the output intensity for the cell at the given index. This
/// only marks the cell as modified if the new output intensity is different
/// from the current value.
/// @param table table to change the output on
/// @param index index of the cell to change
/// @param output intensity to change to
void CT_change(struct ctable_s* table, uint32_t index, uint8_t output);

/// @struct ctgroup_s
/// @brief Represents a group of linked cells that share the same unit number,
/// channel selection bitmask, and output intensity value.
struct ctgroup_s {
    uint8_t unit;      ///< Unit number shared by all channels
    uint8_t offset;    ///< Channel selection offset
    uint16_t cs;       ///< Channel selection bitmask
    uint8_t intensity; ///< Intensity output value for all channels
    int size;          ///< The number of active channels
};

/// @brief Returns a group of linked cells starting at the given index. The
/// group is identified by the unit number, channel section, and output
/// intensity value. Any cells that have not been modified, or do not match, are
/// excluded from the grouping. Assuming the cell at `at` is valid and modified,
/// `group` should always contain at least one cell.
/// @param table table to search
/// @param at index to start the group search
/// @param group pointer to store the group
/// @return 1 if a group was found, 0 if no group was found
int CT_groupof(struct ctable_s* table, uint32_t at, struct ctgroup_s* group);

/// @brief Frees the table and any held resources.
/// @param table table to free
void CT_free(struct ctable_s* table);

#endif//FPLAYER_CELL_H
