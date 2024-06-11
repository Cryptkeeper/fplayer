#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fseq/fd.h>

static void free_node(struct fd_node_s* node) {
    if (node == NULL) return;
    free(node->frame);
    free(node);
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    struct fd_list_s list = {0};

    // shifting an empty list returns NULL
    assert(FD_shift(&list) == NULL);
    assert(list.head == NULL);
    assert(list.count == 0);

    // appending a frame to an empty list allocates a new node
    uint8_t* frame1 = calloc(16, 1);
    assert(frame1 != NULL);
    assert(FD_append(&list, frame1) == 0);
    assert(list.head != NULL);         // has item 1
    assert(list.tail != NULL);         // has tail
    assert(list.count == 1);           // has 1 item
    assert(list.head->frame == frame1);// item 1 correct
    assert(list.tail->frame == frame1);// item 1 correct
    assert(list.head->next == NULL);   // no item 2 (expected)

    // appending a frame to a non-empty list allocates a new node
    uint8_t* frame2 = calloc(16, 1);
    assert(frame2 != NULL);
    assert(FD_append(&list, frame2) == 0);
    assert(list.head != NULL);               // has item 1
    assert(list.head != list.tail);          // has tail
    assert(list.count == 2);                 // has 2 items total
    assert(list.head->frame == frame1);      // item 1 correct
    assert(list.head->next != NULL);         // has item 2
    assert(list.head->next->frame == frame2);// item 2 correct
    assert(list.head->next->next == NULL);   // no item 3 (expected)

    struct fd_node_s* shifted;

    // shift removes the first element and returns it
    const struct fd_node_s* first = list.head;
    shifted = FD_shift(&list);

    assert(shifted == first);       // first element returned
    assert(shifted->next == NULL);  // link listed data removed
    assert(list.head != NULL);      // has item 2
    assert(list.head == list.tail); // has tail
    assert(list.head->next == NULL);// no item 3 (expected)
    assert(list.count == 1);        // has 1 item remaining

    free_node(shifted);

    // shift removes the second element and returns it
    const struct fd_node_s* second = list.head;
    shifted = FD_shift(&list);

    assert(first != second);      // first and second are different
    assert(shifted == second);    // second element returned
    assert(shifted->next == NULL);// link listed data removed
    assert(list.head == NULL);    // no items left
    assert(list.tail == NULL);    // no tail
    assert(list.count == 0);      // no items remaining

    free_node(shifted);

    return 0;
}
