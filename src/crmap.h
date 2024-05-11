#ifndef FPLAYER_CRMAP_H
#define FPLAYER_CRMAP_H

#include <stdint.h>

struct cr_s;

/// @brief Reads a channel range map from the given file path. Parsing is done
/// via `CR_parse` and the result is dynamically allocated. The caller is
/// responsible for freeing the result with `CR_free`.
/// @param fp file path to read from
/// @param cr pointer to write the channel range map to
/// @return 0 on success, or a negative error code on failure
int CR_read(const char* fp, struct cr_s** cr);

/// @brief Frees the given channel range map by walking the linked list and
/// freeing each node.
/// @param cr channel range map to free
void CR_free(struct cr_s* cr);

/// @brief Remaps the given sequence channel index to a unit and circuit number
/// using the channel range mapping. The result is written to the given `unit`
/// and `circuit` pointers.
/// @param cr channel range map to use for remapping
/// @param id sequence channel index to remap
/// @param unit pointer to write the unit number to
/// @param circuit pointer to write the circuit number to
/// @return non-zero on success, zero on failure
int CR_remap(const struct cr_s* cr,
             uint32_t id,
             uint8_t* unit,
             uint16_t* circuit);

#endif//FPLAYER_CRMAP_H
