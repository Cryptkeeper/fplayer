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
#include "std2/fc.h"

struct frame_pump_s {
    struct FC* fc;          /* file controller to read frames from */
    struct fd_node_s* curr; /* current frame set to read from */
    struct fd_node_s* next; /* preloaded frame set to read from next */
    bool preloading;        /* preloading/preloaded state flag */
    pthread_t thread;       /* preload thread */
    union {
        uint32_t frame; /* current frame index */
        int cb;         /* current compression block index */
    } pos;              /* current position data */
};

struct frame_pump_s* FP_init(struct FC* fc) {
    assert(fc != NULL);

    struct frame_pump_s* pump = calloc(1, sizeof(*pump));
    if (pump == NULL) return NULL;
    pump->fc = fc;
    return pump;
}

/// @brief Reads the next frame set from the file controller and stores it in the
/// provided frame data node pointer. This function is used when the sequence is
/// not compressed and read sequentially from the file controller.
/// @param pump frame pump to read from
/// @param frame frame index to read
/// @param fn frame data node pointer to store the next frame set in
/// @return 0 on success, a negative error code on failure, or `FP_ESEQEND` if
/// the pump has reached the end of the sequence
static int FP_readSeq(struct frame_pump_s* pump,
                      const uint32_t frame,
                      struct fd_node_s** fn) {
    assert(pump != NULL);
    assert(fn != NULL);

    // attempt to read 10 seconds of frame data at a time
    const int frames = 10000 / curSequence->frameStepTimeMillis;
    const uint32_t pos = curSequence->channelDataOffset +
                         (frame * curSequence->channelCount);

    // allocate a single block for the full frame set
    uint8_t* b = calloc(frames, curSequence->channelCount);
    if (b == NULL) return -FP_ENOMEM;

    const uint32_t read =
            FC_readto(pump->fc, pos, curSequence->channelCount, frames, b);

    if (read == 0) {// EOF
        free(b);
        return FP_ESEQEND;
    }

    // copy into frame data linked list
    // TODO: this is a slow operation given the memory is *already* structured
    int err = FP_EOK;
    for (uint32_t i = 0; i < read; i++) {
        if ((err = FD_append(fn, &b[i * curSequence->channelCount])) < 0) {
            FD_free(*fn), *fn = NULL;
            free(b);
            return err;
        }
    }

    return err;
}

/// @brief Reads the next frame set from the file controller and stores it in the
/// provided frame data node pointer
/// @param pump frame pump to read from
/// @param frame frame index to read
/// @param fn frame data node pointer to store the next frame set in
/// @return 0 on success, a negative error code on failure, or `FP_ESEQEND` if
/// the pump has reached the end of the sequence
static int FP_read(struct frame_pump_s* pump,
                   const uint32_t frame,
                   struct fd_node_s** fn) {
    assert(pump != NULL);
    assert(fn != NULL);

    switch (curSequence->compressionType) {
        case TF_COMPRESSION_ZSTD:
            if (pump->pos.cb >= curSequence->compressionBlockCount)
                return FP_ESEQEND;
            return ComBlock_read(pump->fc, pump->pos.cb++, fn);
        case TF_COMPRESSION_NONE:
            return FP_readSeq(pump, frame, fn);
        default:
            return -FP_ENOSUP;
    }
}

static void* FP_thread(void* pargs) {
    assert(pargs != NULL);

    struct frame_pump_s* pump = pargs;
    struct fd_node_s* fn = NULL;

    int err;
    if ((err = FP_read(pump, pump->pos.frame, &fn))) {
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
/// @param frame current frame index
/// @return 0 on success, a negative error code on failure
static int FP_testPreload(struct frame_pump_s* pump, const uint32_t frame) {
    assert(pump != NULL);

    if (pump->curr == NULL) return false; /* empty, will sync read */
    if (pump->preloading) return false;   /* already busy */

    // require at least N seconds of frames to be available for playback
    const int reqd = (1000 / curSequence->frameStepTimeMillis) * 3;
    if (FD_scanDepth(pump->curr, 0) >= reqd) return FP_EOK;

    pump->preloading = true;
    pump->pos.frame = frame + reqd;// preload starting relative to curr position

    if (pthread_create(&pump->thread, NULL, FP_thread, pump))
        return -FP_EPTHREAD;

    return FP_EOK;
}

int FP_nextFrame(struct frame_pump_s* pump, uint32_t frame, uint8_t** fd) {
    assert(pump != NULL);
    assert(fd != NULL);

    int err;
    if ((err = FP_testPreload(pump, frame))) return err;

    // pump is empty
    // check if a preloaded frame set is available for instant consumption,
    // otherwise block the playback and read the next frame set immediately
    if (pump->curr == NULL) {
        // attempt to pull from a potentially pre-existing preload thread
        if (pump->preloading) {
            if (pthread_join(pump->thread, (void**) &pump->next))
                return -FP_EPTHREAD;
            pump->preloading = false;
        }

        if (pump->next == NULL) {
            // immediately read from source if a preload is not available
            if ((err = FP_read(pump, frame, &pump->next))) {
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

    // destroy any lingering preload thread that may have been triggered,
    // loaded no data, and was therefore not joined via swapping frame sets
    if (pump->preloading) pthread_detach(pump->thread);

    FD_free(pump->curr);
    FD_free(pump->next);
    free(pump);
}
