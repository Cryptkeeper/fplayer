#include "fade.h"

#include <assert.h>
#include <string.h>

#include <stb_ds.h>

#include "mem.h"

struct frame_fades_t {
    uint32_t frame;
    Fade **fades;
    int nFades;
};

struct frame_kv_t {
    uint32_t key;
    struct frame_fades_t value;
};

static struct frame_kv_t *gFrameKV;

static struct frame_fades_t *fadeGetFrame(const uint32_t frame,
                                          const bool insert) {
    struct frame_kv_t *const existing = hmgetp_null(gFrameKV, frame);
    if (existing != NULL) return &existing->value;

    if (!insert) return NULL;

    struct frame_kv_t pair = (struct frame_kv_t){
            .key = frame,
            .value = (struct frame_fades_t){0},
    };

    hmputs(gFrameKV, pair);

    struct frame_kv_t *const put = hmgetp_null(gFrameKV, frame);
    return put != NULL ? &put->value : NULL;
}

void fadePush(uint32_t startFrame, Fade fade) {
    // multiple frames may store a pointer to a single Fade struct
    // alloc a central copy of the parameter
    Fade *const newFade = mustMalloc(sizeof(Fade));

    memcpy(newFade, &fade, sizeof(Fade));

    for (uint32_t frame = startFrame; frame < startFrame + fade.frames;
         frame++) {
        struct frame_fades_t *const fades = fadeGetFrame(frame, true);

        // re-alloc its pointer list and insert the new central value
        const int newIdx = fades->nFades++;

        fades->fades = mustRealloc(
                fades->fades, sizeof(struct frame_fades_t *) * fades->nFades);

        fades->fades[newIdx] = newFade;

        // increment reference counter
        // used by `fadeFrameFree` to know when all references to a fade are
        // unused and it can be freed
        newFade->rc++;
    }
}

void fadeFrameFree(uint32_t frame) {
    struct frame_fades_t *const fades = fadeGetFrame(frame, false);

    if (fades == NULL) return;

    for (int i = 0; i < fades->nFades; i++) {
        Fade *fade = fades->fades[i];

        // previously freed reference
        // pointer entry is NULL'd to avoid realloc'ing the pointer list
        if (fade == NULL) continue;

        // see `fadePush` for matching reference counter increment
        const int rc = --fade->rc;

        assert(rc >= 0);

        if (rc == 0) {
            freeAndNull((void **) &fade);

            // freeAndNull only NULLs the local `fade` variable
            fades->fades[i] = NULL;
        }
    }

    freeAndNull((void **) &fades->fades);

    fades->nFades = 0;
}

void fadeGetStatus(uint32_t frame,
                   uint32_t id,
                   Fade **started,
                   bool *const finishing) {
    *started = NULL;
    *finishing = false;

    struct frame_fades_t *const fades = fadeGetFrame(frame, false);

    if (fades == NULL) return;

    for (int i = 0; i < fades->nFades; i++) {
        Fade *const match = fades->fades[i];

        if (match->id != id) continue;

        // a newly started effect was found, pass a copy to the caller
        const bool startedNew = match != NULL && match->startFrame == frame;

        if (startedNew) {
            *started = match;
            *finishing = false;

            break;
        }

        // a fade effect is active, but may not have been started this frame (i.e. a fade tail)
        *finishing = true;
    }
}
