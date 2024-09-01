#include "putil.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "tinyfseq.h"
#include "tinylor.h"

#include "audio.h"
#include "cell.h"
#include "fseq/seq.h"
#include "serial.h"
#include "std2/errcode.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

int PU_wait(struct serialdev_s* sdev, const unsigned int seconds) {
    assert(sdev != NULL);

    // LOR hardware may require several heartbeat messages are sent
    // before it considers itself connected to the player
    // This artificially waits prior to starting playback to ensure the device is
    // considered connected and ready for frame data
    if (seconds == 0) return FP_EOK;

    printf("waiting %u seconds for connection...\n", seconds);

    // assumes 2 heartbeat messages per second (500ms delay)
    for (unsigned int toSend = seconds * 2; toSend > 0; toSend--) {
        Serial_write(sdev, LOR_HEARTBEAT_BYTES, LOR_HEARTBEAT_SIZE);

#ifdef _WIN32
        Sleep(LOR_HEARTBEAT_DELAY_MS);
#else
        const struct timespec itrSleep = {
                .tv_sec = 0,
                .tv_nsec = LOR_HEARTBEAT_DELAY_NS,
        };
        nanosleep(&itrSleep, NULL);
#endif
    }

    return FP_EOK;
}

int PU_lightsOff(struct serialdev_s* sdev) {
    assert(sdev != NULL);

    lor_req_s req = {0};
    lor_set_unit(&req, 0xFF);// broadcast to all units
    lor_set_effect(&req, LOR_SET_OFF, NULL);

    unsigned char b[32] = {0};
    const size_t w = lor_write(b, sizeof(b), &req, 1);

    Serial_write(sdev, b, w);
    Serial_drain(sdev);

    return FP_EOK;
}

long PU_secondsRemaining(const uint32_t frame, const struct tf_header_t* seq) {
    if (seq->frameCount < frame) return 0;

    const uint32_t framesRemaining = seq->frameCount - frame;
    return framesRemaining / (1000 / seq->frameStepTimeMillis);
}

int PU_writeHeartbeat(struct serialdev_s* sdev) {
    assert(sdev != NULL);

    Serial_write(sdev, LOR_HEARTBEAT_BYTES, LOR_HEARTBEAT_SIZE);

    return FP_EOK;
}

int PU_writeEffect(struct serialdev_s* sdev,
                   const struct ctgroup_s* group,
                   uint32_t* accum) {
    assert(sdev != NULL);
    assert(group != NULL);
    assert(group->size > 0);

    lor_req_s req = {0};

    lor_set_unit(&req, group->unit);
    lor_set_intensity(&req, lor_get_intensity(group->intensity));

    if (group->size > 1) {
        req.cset.offset = group->offset;// values already aligned, set directly
        req.cset.cbits = group->cs;
    } else {
        assert(__builtin_popcount(group->cs) == 1);
        const uint16_t channel = __builtin_ctz(group->cs) + group->offset;
        lor_set_channel(&req, channel);
    }

    unsigned char b[32] = {0};
    const size_t w = lor_write(b, sizeof(b), &req, 1);

    Serial_write(sdev, b, w);

    if (accum != NULL) *accum += w;

    return FP_EOK;
}

int PU_playFirstAudio(const char* audiofp,
                      struct FC* fc,
                      const struct tf_header_t* seq) {
    assert(fc != NULL);
    assert(seq != NULL);

    if (audiofp != NULL) return Audio_play(audiofp);

    char* lookup = NULL;

    // attempt to read file path variable from sequence
    int err;
    if ((err = Seq_getMediaFile(fc, seq, &lookup))) return err;
    if (lookup == NULL) return FP_EOK;// nothing to play

    err = Audio_play(lookup);
    free(lookup);// unused once playback is started

    return err;
}
