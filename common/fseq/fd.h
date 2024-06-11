#ifndef FPLAYER_FD_H
#define FPLAYER_FD_H

#include <stdint.h>

struct fd_list_s {
    struct fd_node_s* head;
    struct fd_node_s* tail;
    int count;
};

struct fd_node_s {
    uint8_t* frame;
    struct fd_node_s* next;
};

/// @brief Frees all entry nodes in the list, but does not free the list itself.
/// @param list pointer to the list structure to free
void FD_free(struct fd_list_s* list);

/// @brief Returns the first node in the list and removes it for consumption by
/// the caller. The caller is responsible for freeing the returned node.
/// @param list pointer to the list structure, must be non-NULL and zero initialized
/// @return the first node in the list, or NULL if the list is empty
struct fd_node_s* FD_shift(struct fd_list_s* list);

/// @brief Appends a new frame data block to the end of the list. The frame data
/// provided is not copied, so the caller is responsible for ensuring the memory
/// remains valid until the frame data block is freed.
/// @param list pointer to the list structure, must be non-NULL and zero initialized
/// @param frame frame data to append
/// @return 0 on success, a negative error code on failure
int FD_append(struct fd_list_s* list, uint8_t* frame);

#endif//FPLAYER_FD_H
