#ifndef FPLAYER_FD_H
#define FPLAYER_FD_H

#include <stdint.h>

struct fd_node_s {
    uint8_t* frame;
    struct fd_node_s* next;
};

/// @brief Frees the given frame data block instance and all of its children.
/// @param node pointer to the first frame data to free
void FD_free(struct fd_node_s* node);

/// @brief Returns the first node in the list and updates the head pointer to
/// the next node, effectively removing the first node from the linked list. The
/// caller is responsible for freeing the returned node.
/// @param head pointer to the head of the list
/// @return the first node in the list, or NULL if the list is empty
struct fd_node_s* FD_shift(struct fd_node_s** head);

/// @brief Appends a new frame data block to the end of the linked list. If the
/// head pointer is NULL, a new list is created with the new frame data block as
/// the head. The frame data provided is not copied, so the caller is responsible
/// for ensuring the memory remains valid until the frame data block is freed.
/// @param head return pointer to the head of the list, or a previously allocated
/// pointer to the head of the list
/// @param frame frame data to append
/// @return 0 on success, a negative error code on failure
int FD_append(struct fd_node_s** head, uint8_t* frame);

#endif//FPLAYER_FD_H
