#include "fd.h"

#include <assert.h>
#include <stdlib.h>

#include "../std2/errcode.h"

void FD_free(struct fd_node_s* node) {
    if (node == NULL) return;
    struct fd_node_s* next = node->next;
    free(node->frame);
    free(node);
    FD_free(next);
}

struct fd_node_s* FD_shift(struct fd_node_s** head) {
    if (head == NULL) return NULL;
    struct fd_node_s* node = *head;
    if (node == NULL) return NULL;
    *head = node->next;
    node->next = NULL;
    return node;
}

int FD_scanDepth(struct fd_node_s* head, int max) {
    if (head == NULL) return 0;
    int depth = 0;
    while (head != NULL && (max <= 0 || depth < max))
        head = head->next, depth++;
    return depth;
}

int FD_append(struct fd_node_s** head, uint8_t* frame) {
    assert(head != NULL);
    assert(frame != NULL);

    struct fd_node_s* node = calloc(1, sizeof(struct fd_node_s));
    if (node == NULL) return -FP_ENOMEM;

    node->frame = frame;

    if (*head == NULL) {
        *head = node;
    } else {
        // find last node in tree
        // TODO: this is O(n) and could be optimized
        struct fd_node_s* last = *head;
        while (last->next != NULL) last = last->next;
        last->next = node;
    }

    return FP_EOK;
}
