#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fseq/fd.h>

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    struct fd_node_s* head = NULL;

    // scan depth handles NULL/empty arrays
    assert(FD_scanDepth(head, 0) == 0);
    assert(FD_scanDepth(head, 1) == 0);

    // shifting an empty list returns NULL
    assert(FD_shift(&head) == NULL);
    assert(head == NULL);

    // appending a frame to an empty list allocates a new node
    uint8_t* frame1 = calloc(16, 1);
    assert(frame1 != NULL);
    assert(FD_append(&head, frame1) == 0);
    assert(head != NULL);         // has item 1
    assert(head->frame == frame1);// item 1 correct
    assert(head->next == NULL);   // no item 2 (expected)

    // scan depth handles single-element arrays
    assert(FD_scanDepth(head, 0) == 1);// full scan of 1
    assert(FD_scanDepth(head, 1) == 1);// limited scan of 1
    assert(FD_scanDepth(head, 2) == 1);// limited scan of 2

    // appending a frame to a non-empty list allocates a new node
    uint8_t* frame2 = calloc(16, 1);
    assert(frame2 != NULL);
    assert(FD_append(&head, frame2) == 0);
    assert(head != NULL);               // has item 1
    assert(head->frame == frame1);      // item 1 correct
    assert(head->next != NULL);         // has item 2
    assert(head->next->frame == frame2);// item 2 correct
    assert(head->next->next == NULL);   // no item 3 (expected)

    // scan depth handles multi-element arrays
    assert(FD_scanDepth(head, 0) == 2);// full scan of 2
    assert(FD_scanDepth(head, 1) == 1);// limited scan of 1
    assert(FD_scanDepth(head, 2) == 2);// limited scan of 2
    assert(FD_scanDepth(head, 3) == 2);// limited scan of 3

    struct fd_node_s* shifted;

    // shift removes the first element and returns it
    const struct fd_node_s* first = head;
    shifted = FD_shift(&head);

    assert(shifted == first);     // first element returned
    assert(shifted->next == NULL);// link listed data removed
    assert(head != NULL);         // has item 2
    assert(head->next == NULL);   // no item 3 (expected)

    FD_free(shifted);

    // scan depth operates correctly after shifting
    assert(FD_scanDepth(head, 0) == 1);// full scan of 1
    assert(FD_scanDepth(head, 1) == 1);// limited scan of 1
    assert(FD_scanDepth(head, 2) == 1);// limited scan of 2

    // shift removes the second element and returns it
    const struct fd_node_s* second = head;
    shifted = FD_shift(&head);

    assert(shifted == second);    // second element returned
    assert(shifted->next == NULL);// link listed data removed
    assert(head == NULL);         // no items left

    FD_free(shifted);

    return 0;
}
