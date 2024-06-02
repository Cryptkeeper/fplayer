#include "buf.h"

#include <assert.h>
#include <stdlib.h>

#include <lorproto/coretypes.h>

#define LOR_BUFFER_SIZE 256

struct LorBuffer* LB_alloc(void) {
    struct LorBuffer* lb = calloc(1, sizeof(struct LorBuffer));
    if (lb == NULL) return NULL;
    if ((lb->buffer = calloc(LOR_BUFFER_SIZE, 1)) == NULL) {
        LB_free(lb);
        return NULL;
    }
    lb->size = LOR_BUFFER_SIZE;
    lb->offset = 0;
    return lb;
}

void LB_free(struct LorBuffer* lb) {
    if (lb == NULL) return;
    free(lb->buffer);
    free(lb);
}

void LB_rewind(struct LorBuffer* lb) {
    assert(lb != NULL);
    lb->offset = 0;
}
