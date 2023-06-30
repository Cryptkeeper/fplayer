#include "fade.h"

#include <stb_ds.h>

#include "mem.h"

struct frame_event_kv_t {
    uint32_t key;// circuit id
    int value;   // fade handle
};

struct frame_data_t {
    struct frame_event_kv_t *events;
};

struct frame_kv_t {
    uint32_t key;
    struct frame_data_t value;
};

static struct frame_kv_t *gFrames;

static struct frame_data_t *fadeGetInsertFrameData(const uint32_t frame) {
    struct frame_kv_t *const existing = hmgetp_null(gFrames, frame);

    if (existing != NULL) return &existing->value;

    hmput(gFrames, frame, (struct frame_data_t){0});

    struct frame_kv_t *const put = hmgetp_null(gFrames, frame);

    return put != NULL ? &put->value : NULL;
}

static Fade *gFades;

static int fadeGetSharedHandle(const Fade fade) {
    for (int i = 0; i < arrlen(gFades); i++) {
        const Fade f = gFades[i];

        // match to a pre-existing Fade entry
        if (f.from == fade.from && f.to == fade.to &&
            f.startFrame == fade.startFrame && f.frames == fade.frames)
            return i;
    }

    const int idx = arrlen(gFades);

    arrput(gFades, fade);

    return idx;
}

void fadePush(const uint32_t startFrame, const uint32_t id, const Fade fade) {
    const int handle = fadeGetSharedHandle(fade);

    for (uint32_t frame = startFrame; frame < startFrame + fade.frames;
         frame++) {
        struct frame_data_t *const data = fadeGetInsertFrameData(frame);

        struct frame_event_kv_t event = (struct frame_event_kv_t){
                .key = id,
                .value = handle,
        };

        hmputs(data->events, event);
    }
}

void fadeFrameFree(uint32_t frame) {
    struct frame_kv_t *const kv = hmgetp_null(gFrames, frame);

    if (kv == NULL) return;

    hmfree(kv->value.events);

    // remove frame from lookup map
    hmdel(gFrames, frame);
}

void fadeFree(void) {
    hmfree(gFrames);
    arrfree(gFades);
}

void fadeGetStatus(const uint32_t frame,
                   const uint32_t id,
                   Fade **started,
                   bool *const finishing) {
    *started = NULL;
    *finishing = false;

    struct frame_kv_t *const kv = hmgetp_null(gFrames, frame);

    if (kv == NULL) return;

    const struct frame_event_kv_t *const event =
            hmgetp_null(kv->value.events, id);

    if (event == NULL) return;

    Fade *const fade = &gFades[event->value];

    if (fade->startFrame == frame) {
        // a newly started effect was found, pass a copy to the caller
        *started = fade;
    } else {
        // a fade effect is active, but was not started this frame
        // this is distinct to allow fplayer to ignore duplicate updates to circuits
        // when they are known to be actively fading
        *finishing = true;
    }
}
