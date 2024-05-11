#include "pump.h"

#include <assert.h>
#include <stdlib.h>

#include <tinyfseq.h>

#include "fseq/comblock.h"
#include "fseq/fd.h"
#include "seq.h"
#include "std2/errcode.h"

struct frame_pump_s {
    struct FC* fc;          /* file controller to read frames from */
    struct fd_node_s* curr; /* current frame set to read from */
    struct fd_node_s* next; /* preloaded frame set to read from next */
    int cbidx;              /* current compression block index */
};

struct frame_pump_s* FP_init(struct FC* fc) {
    assert(fc != NULL);

    struct frame_pump_s* pump = calloc(1, sizeof(*pump));
    if (pump == NULL) return NULL;
    pump->fc = fc;
    return pump;
}

static int FP_readZstd(struct frame_pump_s* pump, struct fd_node_s** fn) {
    assert(pump != NULL);
    assert(fn != NULL);

    if (pump->cbidx >= curSequence->compressionBlockCount) return FP_ESEQEND;
    return ComBlock_read(pump->fc, pump->cbidx++, fn);
}

static int FP_read(struct frame_pump_s* pump, struct fd_node_s** fn) {
    assert(pump != NULL);
    assert(fn != NULL);

    switch (curSequence->compressionType) {
        case TF_COMPRESSION_ZSTD:
            return FP_readZstd(pump, fn);
        default:
            return -FP_ENOSUP;
    }
}

int FP_copy(struct frame_pump_s* pump, uint8_t** fd) {
    assert(pump != NULL);
    assert(fd != NULL);

    // pump is empty
    // check if a preloaded frame set is available for instant consumption,
    // otherwise block the playback and read the next frame set immediately
    if (pump->curr == NULL) {
        // immediately read from source if a preload is not available
        if (pump->next == NULL) {
            int err;
            if ((err = FP_read(pump, &pump->next))) {
                FD_free(pump->next), pump->next = NULL;
                return err;
            }
        }

        // handle the swap to the new frame set
        if (pump->next != NULL) {
            FD_free(pump->curr);
            pump->curr = pump->next, pump->next = NULL;
        }
    }

    // copy the next frame from the current frame set
    struct fd_node_s* node = FD_shift(&pump->curr);
    if (node == NULL) return FP_ESEQEND;
    *fd = node->frame, free(node);

    return FP_EOK;
}

void FP_free(struct frame_pump_s* pump) {
    if (pump == NULL) return;
    FD_free(pump->curr);
    FD_free(pump->next);
    free(pump);
}
