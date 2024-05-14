#ifndef FPLAYER_CELL_H
#define FPLAYER_CELL_H

#include <stdint.h>

#include <lorproto/coretypes.h>

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

/// @brief Sets the output intensity for the cell at the given index.
/// @param table table to set the output on
/// @param index index of the cell to set
/// @param output intensity to set
void CT_set(struct ctable_s* table, uint32_t index, LorIntensity output);

/// @brief Iterates the full table and links matching neighboring cells together.
/// This de-duplicates non-unique cells, enabling the table to be used as a
/// compressed, pre-grouped linked list by the network serializer.
/// This should be called after all relevant cells have had their output state
/// initially configured, or recently updated via `CT_set`.
/// @param table table to link
void CT_linkall(struct ctable_s* table);

struct ctgroup_s {
    LorUnit unit;           /* unit number shared by all channels */
    LorChannelSet cs;       /* channel selection bitmask + offset */
    LorIntensity intensity; /* intensity output value for all channels */
    int size;               /* the number of active channels */
};

/// @brief Gets the next group of linked cells that share the same output
/// intensity. This should be used after `CT_linkall` as been invoked, otherwise
/// the results will not be de-duplicated. The `from` pointer should be
/// initialized to 0 before the first call to this function. The value is updated
/// during the function call to enable a caller to continue iterating groups
/// as long as the return value is successful.
/// @param table table to get the next group from
/// @param from pointer to the current group's starting index
/// @param group pointer to store the group's data, always zeroed before use
/// @return 1 if a group was found, 0 if no more groups are available
int CT_nextgroup(struct ctable_s* table,
                 uint32_t* from,
                 struct ctgroup_s* group);

/// @brief Frees the table and any held resources.
/// @param table table to free
void CT_free(struct ctable_s* table);

#endif//FPLAYER_CELL_H
