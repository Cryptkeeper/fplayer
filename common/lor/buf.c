#include "buf.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <lorproto/coretypes.h>

#define LOR_BUFFER_SIZE 64

/// @brief Conforms to the LorBuffer interface, but uses a fixed-size internal
/// buffer for storage to avoid a second dynamic allocation. This allows for
/// callers to free the buffer with a single call to free().
struct lor_buffer_fixed_s {
    uint8_t* buffer;
    uint32_t size;
    uint32_t offset;
    uint8_t stack[LOR_BUFFER_SIZE];
};

struct LorBuffer* LB_alloc(void) {
    struct lor_buffer_fixed_s* lb = calloc(1, sizeof(*lb));
    if (lb == NULL) return NULL;
    lb->buffer = lb->stack;// point write head to internal buffer
    lb->size = LOR_BUFFER_SIZE;
    return (struct LorBuffer*) lb;
}

void LB_rewind(struct LorBuffer* lb) {
    assert(lb != NULL);
    lb->offset = 0;
}
