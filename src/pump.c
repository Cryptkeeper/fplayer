/// @file pump.c
/// @brief Frame data loading implementation.
#include "pump.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyfseq.h"

#include "fseq/comblock.h"
#include "fseq/fd.h"
#include "std2/errcode.h"
#include "std2/fc.h"

struct frame_pump_s {
    struct FC* fc;                 ///< File controller to read from
    const struct tf_header_t* seq; ///< Sequence file metadata header
    struct fd_list_s curr;         ///< Current frame set to read from
    struct fd_list_s next;         ///< Preloaded frame set to read from next
    bool preloading;               ///< Preloading/preloaded state flag
    pthread_t thread;              ///< Preload thread
    union {
        uint32_t frame; ///< Target frame index
        int cb;         ///< Target compression block index
    } pos;              ///< Read position data
};

int FP_init(struct FC* fc,
            const struct tf_header_t* seq,
            struct frame_pump_s** pump) {
    assert(fc != NULL);
    assert(seq != NULL);
    assert(pump != NULL);
    if ((*pump = calloc(1, sizeof(struct frame_pump_s))) == NULL)
        return -FP_ENOMEM;
    (*pump)->fc = fc;
    (*pump)->seq = seq;
    return FP_EOK;
}

/// @brief Reads the next frame set from the file controller and stores it in the
/// provided frame data node pointer. This function is used when the sequence is
/// not compressed and read sequentially from the file controller.
/// @param fc file controller to read from
/// @param seq sequence header for playback configuration
/// @param frame frame index to read
/// @param list frame data list pointer to store the next frame set in
/// @return 0 on success, a negative error code on failure, or `FP_ESEQEND` if
/// the pump has reached the end of the sequence
static int FP_readSeq(struct FC* fc,
                      const struct tf_header_t* seq,
                      const uint32_t frame,
                      struct fd_list_s* list) {
    assert(fc != NULL);
    assert(seq != NULL);
    assert(list != NULL);

    // attempt to read 10 seconds of frame data at a time
    const int frames = 10000 / seq->frameStepTimeMillis;
    const uint32_t pos = seq->channelDataOffset + (frame * seq->channelCount);

    // allocate a single block for the full frame set
    uint8_t* b = calloc(frames, seq->channelCount);
    if (b == NULL) return -FP_ENOMEM;

    const uint32_t read = FC_readto(fc, pos, seq->channelCount, frames, b);

    if (read == 0) {// EOF
        free(b);
        return 1; /* end of sequence */
    }

    int err = FP_EOK;

    // copy into frame data linked list
    // TODO: this is a slow operation given the memory is *already* structured
    for (uint32_t i = 0; i < read; i++) {
        uint8_t* fd;
        if ((fd = malloc(seq->channelCount)) == NULL) {
            err = -FP_ENOMEM;
            goto ret;
        }

        memcpy(fd, &b[i * seq->channelCount], seq->channelCount);

        if ((err = FD_append(list, fd)) < 0) goto ret;
    }

ret:
    if (err) FD_free(list);
    free(b);

    return err;
}

/// @brief Reads the next frame set from the file controller and stores it in the
/// provided frame data node pointer. The pump should have its `pos` union
/// updated with read request position information before calling this function.
/// @param pump frame pump to read from
/// @param list frame data list pointer to store the next frame set in
/// @return 0 on success, a negative error code on failure, or `FP_ESEQEND` if
/// the pump has reached the end of the sequence
static int FP_read(struct frame_pump_s* pump, struct fd_list_s* list) {
    assert(pump != NULL);
    assert(list != NULL);

    switch (pump->seq->compressionType) {
        case TF_COMPRESSION_ZSTD:
            if (pump->pos.cb >= pump->seq->compressionBlockCount)
                return 1; /* end of sequence */
            return ComBlock_read(pump->fc, pump->seq, pump->pos.cb, list);
        case TF_COMPRESSION_NONE:
            if (pump->pos.frame >= pump->seq->frameCount)
                return 1; /* end of sequence */
            return FP_readSeq(pump->fc, pump->seq, pump->pos.frame, list);
        default:
            return -FP_ERANGE;
    }
}

static void* FP_thread(void* pargs) {
    assert(pargs != NULL);

    struct frame_pump_s* pump = pargs;

    int err;
    if ((err = FP_read(pump, &pump->next))) {
        FD_free(&pump->next);// free list contents

        if (err < 0)
            fprintf(stderr, "failed to preload next frame set: %s %d\n",
                    FP_strerror(err), err);
    }

    return NULL;
}

int FP_checkPreload(struct frame_pump_s* pump, const uint32_t frame) {
    assert(pump != NULL);

    if (pump->curr.count == 0) return false; /* empty, will sync read */
    if (pump->preloading) return false;      /* already busy */

    // require at least N seconds of frames to be available for playback
    const int reqd = (1000 / pump->seq->frameStepTimeMillis) * 3;
    if (pump->curr.count >= reqd) return FP_EOK;

    pump->preloading = true;

    // customize the preload request given the compression mode of the sequence
    // if compressed, request the next block (in order)
    // otherwise, read frame data sequentially starting at the end of the
    // currently available frame data
    switch (pump->seq->compressionType) {
        case TF_COMPRESSION_ZSTD:
            pump->pos.cb++;
            break;
        case TF_COMPRESSION_NONE:
            pump->pos.frame = frame + reqd;
            break;
        default:
            return -FP_ERANGE;
    }

    if (pthread_create(&pump->thread, NULL, FP_thread, pump))
        return -FP_EPTHREAD;

    return FP_EOK;
}

int FP_nextFrame(struct frame_pump_s* pump, uint8_t** fd) {
    assert(pump != NULL);
    assert(fd != NULL);

    // pump is empty
    // check if a preloaded frame set is available for instant consumption,
    // otherwise block the playback and read the next frame set immediately
    if (pump->curr.count == 0) {
        // attempt to pull from a potentially pre-existing preload thread
        if (pump->preloading) {
            if (pthread_join(pump->thread, NULL)) return -FP_EPTHREAD;
            pump->preloading = false;
        }

        if (pump->next.count == 0) {
            // immediately read from source if a preload is not available
            int err;
            if ((err = FP_read(pump, &pump->next))) {
                FD_free(&pump->next);
                return err;
            }
        }

        // handle the swap to the new frame set
        if (pump->next.count > 0) {
            pump->curr = pump->next;
            memset(&pump->next, 0, sizeof(pump->next));
        }
    }

    // copy the next frame from the current frame set
    struct fd_node_s* node = FD_shift(&pump->curr);
    if (node == NULL) return 1; /* end of sequence */
    *fd = node->frame, free(node);

    return FP_EOK;
}

int FP_framesRemaining(struct frame_pump_s* pump) {
    assert(pump != NULL);
    return pump->curr.count;
}

void FP_free(struct frame_pump_s* pump) {
    if (pump == NULL) return;

    // destroy any lingering preload thread that may have been triggered,
    // loaded no data, and was therefore not joined via swapping frame sets
    if (pump->preloading) pthread_detach(pump->thread);

    FD_free(&pump->curr);
    FD_free(&pump->next);
    free(pump);
}
