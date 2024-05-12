#include "pump.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
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
    bool preloading;        /* preload ready/running flag */
    pthread_t plthread;     /* preloading thread */
};

struct frame_pump_s* FP_init(struct FC* fc) {
    assert(fc != NULL);

    struct frame_pump_s* pump = calloc(1, sizeof(*pump));
    if (pump == NULL) return NULL;
    pump->fc = fc;
    return pump;
}

/// @brief Reads the next frame set from the file controller and stores it in the
/// provided frame data node pointer
/// @param pump frame pump to read from
/// @param fn frame data node pointer to store the next frame set in
/// @return 0 on success, a negative error code on failure
static int FP_read(struct frame_pump_s* pump, struct fd_node_s** fn) {
    assert(pump != NULL);
    assert(fn != NULL);

    switch (curSequence->compressionType) {
        case TF_COMPRESSION_ZSTD:
            if (pump->cbidx >= curSequence->compressionBlockCount)
                return FP_ESEQEND;
            return ComBlock_read(pump->fc, pump->cbidx++, fn);
        default:
            return -FP_ENOSUP;
    }
}

static void* FP_thread(void* pargs) {
    assert(pargs != NULL);

    struct frame_pump_s* pump = pargs;
    struct fd_node_s* fn = NULL;

    int err;
    if ((err = FP_read(pump, &fn))) {
        FD_free(fn), fn = NULL;
        if (err < 0)
            fprintf(stderr, "failed to preload next frame set: %d\n", err);
    }

    return fn;
}

/// @brief Checks if the current frame set is running low and triggers a preload
/// of the next frame set if necessary. An empty frame pump is NOT considered
/// low, as it will immediately read the next frame set from the file controller.
/// @param pump frame pump to check
/// @return true if the current frame set is running low, false otherwise
static bool FP_testPreload(struct frame_pump_s* pump) {
    assert(pump != NULL);

    if (pump->curr == NULL) return false; /* empty, will sync read */
    if (pump->preloading) return false;   /* already busy */

    // require at least N seconds of frames to be available for playback
    const int reqd = (1000 / curSequence->frameStepTimeMillis) * 3;
    return FD_scanDepth(pump->curr, reqd) < reqd;
}

int FP_nextFrame(struct frame_pump_s* pump, uint8_t** fd) {
    assert(pump != NULL);
    assert(fd != NULL);

    if (FP_testPreload(pump)) {
        pump->preloading = true;

        if (pthread_create(&pump->plthread, NULL, FP_thread, pump))
            return -FP_EPTHREAD;
    }

    // pump is empty
    // check if a preloaded frame set is available for instant consumption,
    // otherwise block the playback and read the next frame set immediately
    if (pump->curr == NULL) {
        // attempt to pull from a potentially pre-existing preload thread
        if (pump->preloading) {
            if (pthread_join(pump->plthread, (void**) &pump->next))
                return -FP_EPTHREAD;

            pump->preloading = false;
        }

        if (pump->next == NULL) {
            // immediately read from source if a preload is not available
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

int FP_framesRemaining(struct frame_pump_s* pump) {
    assert(pump != NULL);
    return FD_scanDepth(pump->curr, 0);
}

void FP_free(struct frame_pump_s* pump) {
    if (pump == NULL) return;

    FD_free(pump->curr);
    FD_free(pump->next);
    free(pump);
}
