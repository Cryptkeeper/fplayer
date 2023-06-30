#include "fade.h"

#include <stb_ds.h>

#include "mem.h"

struct frame_fade_kvp_t {
    uint32_t key;// circuit id
    int value;   // fade handle
};

struct frame_data_t {
    struct frame_fade_kvp_t *fades;
};

struct frame_data_kvp_t {
    uint32_t key;
    struct frame_data_t value;
};

static struct frame_data_kvp_t *gFrames;

static struct frame_data_t *fadeGetInsertFrameData(const uint32_t frame) {
    struct frame_data_kvp_t *const existing = hmgetp_null(gFrames, frame);

    if (existing != NULL) return &existing->value;

    hmput(gFrames, frame, (struct frame_data_t){0});

    struct frame_data_kvp_t *const put = hmgetp_null(gFrames, frame);

    return put != NULL ? &put->value : NULL;
}

struct fade_handle_t {
    int rc;
    Fade fade;
};

struct fade_handle_kvp_t {
    int key;                   // auto incrementing unique id
    struct fade_handle_t value;// associated fade with reference counter
};

static struct fade_handle_kvp_t *gFades;

static inline bool fadeEqual(const Fade a, const Fade b) {
    return a.from == b.from && a.to == b.to && a.startFrame == b.startFrame &&
           a.frames == b.frames;
}

static int fadeCreateHandleRef(const Fade fade) {
    for (int i = 0; i < hmlen(gFades); i++) {
        struct fade_handle_kvp_t *const kv = &gFades[i];

        if (fadeEqual(fade, kv->value.fade)) {
            // matched to a pre-existing Fade entry
            // increment reference counter and return the handle
            kv->value.rc++;

            return kv->key;
        }
    }

    static int gNextHandle = 0;

    const int key = gNextHandle++;

    struct fade_handle_t handle = (struct fade_handle_t){
            .rc = 1,
            .fade = fade,
    };

    hmput(gFades, key, handle);

    return key;
}

void fadePush(const uint32_t startFrame, const uint32_t id, const Fade fade) {
    const int handle = fadeCreateHandleRef(fade);

    for (uint32_t frame = startFrame; frame < startFrame + fade.frames;
         frame++) {
        struct frame_data_t *const data = fadeGetInsertFrameData(frame);

        struct frame_fade_kvp_t kvp = (struct frame_fade_kvp_t){
                .key = id,
                .value = handle,
        };

        hmputs(data->fades, kvp);
    }
}

int fadeTableSize(void) {
    return (int) hmlen(gFades);
}

void fadeFrameFree(const uint32_t frame) {
    struct frame_data_kvp_t *const data = hmgetp_null(gFrames, frame);

    if (data == NULL) return;

    for (int i = 0; i < hmlen(data->value.fades); i++) {
        struct frame_fade_kvp_t fade = data->value.fades[i];

        struct fade_handle_kvp_t *const handle =
                hmgetp_null(gFades, fade.value);

        if (--handle->value.rc == 0) hmdel(gFades, handle->key);
    }

    hmfree(data->value.fades);

    // remove frame from lookup map
    hmdel(gFrames, frame);
}

void fadeFree(void) {
    hmfree(gFrames);
    hmfree(gFades);
}

void fadeGetStatus(const uint32_t frame,
                   const uint32_t id,
                   Fade **started,
                   bool *const finishing) {
    *started = NULL;
    *finishing = false;

    struct frame_data_kvp_t *const data = hmgetp_null(gFrames, frame);

    if (data == NULL) return;

    const struct frame_fade_kvp_t *const fade =
            hmgetp_null(data->value.fades, id);

    if (fade == NULL) return;

    struct fade_handle_kvp_t *const handle = hmgetp_null(gFades, fade->value);

    Fade *const f = &handle->value.fade;

    if (f->startFrame == frame) {
        // a newly started effect was found, pass a copy to the caller
        // FIXME: warning about `f` pointer escaping local var, false positive?
        *started = f;
    } else {
        // a fade effect is active, but was not started this frame
        // this is distinct to allow fplayer to ignore duplicate updates to circuits
        // when they are known to be actively fading
        *finishing = true;
    }
}
