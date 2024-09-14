/// @file fd.c
/// @brief Frame data block list implementation.
#include "fd.h"

#include <assert.h>
#include <stdlib.h>

#include "std2/errcode.h"

void FD_free(struct fd_list_s* list) {
    for (struct fd_node_s* h = list->head; h != NULL;) {
        struct fd_node_s* next = h->next;
        free(h->frame);
        free(h);
        h = next;
    }
    list->head = list->tail = NULL;
    list->count = 0;
}

struct fd_node_s* FD_shift(struct fd_list_s* list) {
    assert(list != NULL);

    struct fd_node_s* head = list->head;
    if (head == NULL) return NULL;
    list->head = head->next;
    if (--list->count == 0) list->tail = NULL;
    head->next = NULL;// remove linkage once detached from list
    return head;
}

int FD_append(struct fd_list_s* list, uint8_t* frame) {
    assert(list != NULL);
    assert(frame != NULL);

    struct fd_node_s* node = calloc(1, sizeof(*node));
    if (node == NULL) return -FP_ENOMEM;
    node->frame = frame;

    list->count++;
    if (list->tail == NULL) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    return FP_EOK;
}
