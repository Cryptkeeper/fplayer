#include "fade.h"

#include <assert.h>

#include "stb_ds.h"

#include "../pcf/pcf.h"
#include "../std/mem.h"

struct frame_fade_kvp_t {
    uint32_t key;// circuit id
    int value;   // fade handle
};

struct frame_data_t {
    struct frame_fade_kvp_t *fades;
};

struct frame_data_kvp_t {
    uint32_t key;// frame id
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
           a.frames == b.frames && a.type == b.type;
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
    for (uint32_t frame = startFrame; frame < startFrame + fade.frames;
         frame++) {
        struct frame_data_t *const data = fadeGetInsertFrameData(frame);

        struct frame_fade_kvp_t kvp = (struct frame_fade_kvp_t){
                .key = id,
                // call each iteration so the internal reference counter is incremented
                .value = fadeCreateHandleRef(fade),
        };

        hmputs(data->fades, kvp);
    }
}

int fadeTableSize(void) {
    return (int) hmlen(gFades);
}

struct handle_remap_t {
    int key;        /* handle */
    uint32_t value; /* index */
};

bool fadeTableCache(const char *const fp) {
    pcf_file_t file = {NULL};

    struct handle_remap_t *remaps = NULL;

    for (uint32_t i = 0; i < hmlen(gFrames); i++) {
        const struct frame_data_kvp_t data = gFrames[i];

        pcf_event_t *events = NULL;

        arrsetcap(events, hmlen(data.value.fades));

        for (uint32_t j = 0; j < hmlen(data.value.fades); j++) {
            // map of (uint32_t circuit) to (int handle)
            const struct frame_fade_kvp_t frame = data.value.fades[j];

            // lookup fade reference
            const struct fade_handle_kvp_t *fade =
                    hmgetp_null(gFades, frame.value);

            assert(fade != NULL);

            // only export fades at their start frame points, not trailing effects
            if (fade->value.fade.startFrame != data.key) continue;

            // remap the handle to the deduplicated, flat index
            uint32_t index = 0;

            // check if fade handle has already been deduplicated
            struct handle_remap_t *const remap =
                    hmgetp_null(remaps, frame.value);

            if (remap != NULL) {
                index = remap->value;
            } else {
                bool didMatch = false;

                // check if the underlying fade matches another handle's fade
                // this is the primary dedupe mechanism
                for (uint32_t k = 0; k < arrlen(file.fades); k++) {
                    const pcf_fade_t prevFade = file.fades[k];

                    if (prevFade.from == fade->value.fade.from &&
                        prevFade.to == fade->value.fade.to &&
                        prevFade.frames == fade->value.fade.frames) {
                        index = k;
                        didMatch = true;

                        break;
                    }
                }

                // insert new fade for reference
                // used by auto deduping above to generate a dictionary
                if (!didMatch) {
                    index = arrlen(file.fades);

                    const pcf_fade_t newFade = (pcf_fade_t){
                            .from = fade->value.fade.from,
                            .to = fade->value.fade.to,
                            .frames = fade->value.fade.frames,
                    };

                    arrput(file.fades, newFade);
                }

                // insert remap instruction for future usages of the handle
                hmput(remaps, frame.value, index);
            }

            const pcf_event_t event = (pcf_event_t){
                    .circuit = frame.key,
                    .fade = index,
            };

            arrput(events, event);
        }

        if (arrlen(events) > 0) {
            arrput(file.events, events);

            const pcf_frame_t frame = (pcf_frame_t){
                    .frame = data.key,
                    .nEvents = arrlen(events),
            };

            arrput(file.frames, frame);

            // safety check, mismatched array size returns could cause OOB access
            assert(arrlen(file.frames) == arrlen(file.events));
        } else {
            arrfree(events);
        }
    }

    hmfree(remaps);

    const bool ok = pcfSave(fp, &file);

    pcfFree(&file);

    return ok;
}

bool fadeTableLoadCache(const char *const fp) {
    // TODO
    return false;
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

bool fadeGet(const int handle, Fade *const fade) {
    const struct fade_handle_kvp_t *const kvp = hmgetp_null(gFades, handle);

    if (kvp == NULL) return false;

    *fade = kvp->value.fade;

    return true;
}

void fadeGetChange(const uint32_t frame,
                   const uint32_t id,
                   int *const started,
                   bool *const finishing) {
    *started = -1;
    *finishing = false;

    struct frame_data_kvp_t *const data = hmgetp_null(gFrames, frame);

    if (data == NULL) return;

    const struct frame_fade_kvp_t *const fade =
            hmgetp_null(data->value.fades, id);

    if (fade == NULL) return;

    const int key = fade->value;
    const struct fade_handle_t handle = hmget(gFades, key);

    if (handle.fade.startFrame == frame) {
        // a newly started effect was found, pass a copy to the caller
        *started = key;
    } else {
        // a fade effect is active, but was not started this frame
        // this is distinct to allow fplayer to ignore duplicate updates to circuits
        // when they are known to be actively fading
        *finishing = true;
    }
}
