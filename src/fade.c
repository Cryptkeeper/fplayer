#include "fade.h"

#include <assert.h>

#include <stb_ds.h>

#include "mem.h"

struct frame_data_t {
    int *handles;
};

struct frame_kv_t {
    uint32_t key;
    struct frame_data_t value;
};

static struct frame_kv_t *gFrames;

static struct frame_data_t *fadeGetFrameData(const uint32_t frame,
                                             const bool insert) {
    struct frame_kv_t *const existing = hmgetp_null(gFrames, frame);

    if (existing != NULL) return &existing->value;

    if (!insert) return NULL;

    hmput(gFrames, frame, (struct frame_data_t){0});

    struct frame_kv_t *const put = hmgetp_null(gFrames, frame);

    return put != NULL ? &put->value : NULL;
}

static Fade *gFades;

static int fadeGetSharedHandle(const Fade fade) {
    for (int i = 0; i < arrlen(gFades); i++) {
        const Fade f = gFades[i];

        // match to a pre-existing Fade entry
        // this ignores `rc` with is used for live reference counting
        if (f.id == fade.id && f.from == fade.from && f.to == fade.to &&
            f.startFrame == fade.startFrame && f.frames == fade.frames)
            return i;
    }

    const int idx = arrlen(gFades);

    arrput(gFades, fade);

    return idx;
}

void fadePush(uint32_t startFrame, const Fade fade) {
    const int handle = fadeGetSharedHandle(fade);

    for (uint32_t frame = startFrame; frame < startFrame + fade.frames;
         frame++) {
        struct frame_data_t *const data = fadeGetFrameData(frame, true);

        arrput(data->handles, handle);

        // increment reference counter
        // used by `fadeFrameFree` to know when all references to a fade are
        // unused and it can be freed
        (&gFades[handle])->rc++;
    }
}

void fadeFrameFree(uint32_t frame) {
    struct frame_data_t *const data = fadeGetFrameData(frame, false);

    if (data == NULL) return;

    for (int i = 0; i < arrlen(data->handles); i++) {
        const int handle = data->handles[i];

        Fade *const fade = &gFades[handle];

        // see `fadePush` for matching reference counter increment
        const int rc = --fade->rc;

        assert(rc >= 0);

        if (rc == 0) {
            // TODO: free fade? seems like removing from array will cause large mem shift
        }
    }

    arrfree(data->handles);

    // remove frame from lookup map
    hmdel(gFrames, frame);
}

void fadeGetStatus(uint32_t frame,
                   uint32_t id,
                   Fade **started,
                   bool *const finishing) {
    *started = NULL;
    *finishing = false;

    struct frame_data_t *const data = fadeGetFrameData(frame, false);

    if (data == NULL) return;

    for (int i = 0; i < arrlen(data->handles); i++) {
        Fade *const fade = &gFades[i];

        if (fade->id != id) continue;

        // a newly started effect was found, pass a copy to the caller
        const bool startedNew = fade != NULL && fade->startFrame == frame;

        if (startedNew) {
            *started = fade;
            *finishing = false;

            break;
        }

        // a fade effect is active, but may not have been started this frame (i.e. a fade tail)
        // there's no reason why multiple Fades should be in the same frame with the same circuit ID,
        // but this loop ensures it selects a newly started fade first, then a finishing fade
        *finishing = true;
    }
}
