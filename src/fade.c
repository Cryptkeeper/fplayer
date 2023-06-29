#include "fade.h"

#include <assert.h>
#include <string.h>

#include "mem.h"

struct frame_fades_t {
    uint32_t frame;
    Fade **fades;
    int nFades;
};

static struct frame_fades_t *gFrameFades;
static int gFrames;

static struct frame_fades_t *fadeGetFrame(const uint32_t frame,
                                          const bool insert) {
    for (int i = 0; i < gFrames; i++)
        if (gFrameFades[i].frame == frame) return &gFrameFades[i];

    if (!insert) return NULL;

    // realloc and insert new struct
    // TODO: optimize realloc behavior/hashmap opts?
    const int newIdx = gFrames++;

    gFrameFades =
            mustRealloc(gFrameFades, sizeof(struct frame_fades_t) * gFrames);

    gFrameFades[newIdx] = (struct frame_fades_t){
            .frame = frame,
            .fades = NULL,
            .nFades = 0,
    };

    return &gFrameFades[newIdx];
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

#include <stdio.h>

void fadeDump(void) {
    for (int i = 0; i < gFrames; i++) {
        const struct frame_fades_t *const fades = &gFrameFades[i];

        bool printed = false;

        for (int j = 0; j < fades->nFades; j++) {
            const Fade *const fade = fades->fades[j];

            if (fade->startFrame == fades->frame) {
                if (!printed) {
                    printed = true;
                    printf("frame %d\n", fades->frame);
                }

                printf("\t%d: %d -> %d over %d\n", fade->id, fade->from,
                       fade->to, fade->frames);
            }
        }
    }
}
