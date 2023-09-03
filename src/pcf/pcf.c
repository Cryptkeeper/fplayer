#include "pcf.h"

#include <assert.h>
#include <stdio.h>

#include "stb_ds.h"

#include "../std/mem.h"

// (p)re-computed (c)ache (f)file v(1)
// OR
// (p)re-(c)omputed (f)ades v(1)
#define pcfMagicSig                                                            \
    { 'P', 'C', 'F', '1' }

#define pcfMagicSig4 ((unsigned char[4]) pcfMagicSig)

bool pcfOpen(const char *const fp, pcf_file_t *const file) {
    FILE *f = fopen(fp, "rb");

    if (f == NULL) return false;

    pcf_directory_t dir = {0};
    if (fread(&dir, sizeof(dir), 1, f) != 1) goto fail;

    if (memcmp(dir.magic, pcfMagicSig4, sizeof(pcfMagicSig4)) != 0) goto fail;

    pcf_file_t open = {NULL};

    arrsetcap(open.fades, dir.nFades);

    for (uint32_t i = 0; i < dir.nFades; i++) {
        pcf_fade_t fade = {0};
        if (fread(&fade, sizeof(fade), 1, f) != 1) goto fail;

        arrput(open.fades, fade);
    }

    arrsetcap(open.frames, dir.nFrames);
    arrsetcap(open.events, dir.nFrames);

    for (uint32_t i = 0; i < dir.nFrames; i++) {
        pcf_frame_t frame = {0};
        if (fread(&frame, sizeof(frame), 1, f) != 1) goto fail;

        arrput(open.frames, frame);

        // fplayer doesn't encode frames with zero length events, but that doesn't
        // mean others might — this helps avoid zero length array allocations that
        // would otherwise require additional NULL checking
        assert(frame.nEvents > 0);

        pcf_event_t *events = NULL;

        arrsetcap(events, frame.nEvents);

        if (fread(events, sizeof(pcf_event_t), frame.nEvents, f) !=
            frame.nEvents)
            goto fail;


        arrput(open.events, events);
    }

    *file = open;

    freeAndNullWith(f, fclose);

    return true;

fail:
    freeAndNullWith(f, fclose);

    // caller isn't responsible for freeing resources in fail return
    pcfFree(file);

    return false;
}

bool pcfSave(const char *const fp, const pcf_file_t *const file) {
    FILE *f = fopen(fp, "wb");

    if (f == NULL) return false;

    const pcf_directory_t dir = (pcf_directory_t){
            .magic = pcfMagicSig,
            .nFades = arrlen(file->fades),
            .nFrames = arrlen(file->frames),
    };

    if (fwrite(&dir, sizeof(dir), 1, f) != 1) goto fail;

    if (fwrite(&file->fades, sizeof(pcf_fade_t), dir.nFades, f) != dir.nFades)
        goto fail;

    assert(arrlen(file->frames) == arrlen(file->events));

    for (uint32_t i = 0; i < dir.nFrames; i++) {
        const pcf_frame_t frame = file->frames[i];

        if (fwrite(&frame, sizeof(frame), 1, f) != 1) goto fail;

        if (fwrite(&file->events[i], sizeof(pcf_event_t), frame.nEvents, f) !=
            frame.nEvents)
            goto fail;
    }

    freeAndNullWith(f, fclose);

    return true;

fail:
    freeAndNullWith(f, fclose);

    // quick attempt at trying to delete corrupt file
    remove(fp);

    return false;
}

void pcfFree(pcf_file_t *const file) {
    arrfree(file->fades);
    arrfree(file->frames);

    for (uint32_t i = 0; i < arrlen(file->events); i++)
        arrfree(file->events[i]);

    arrfree(file->events);
}