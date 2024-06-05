#include "putil.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <lorproto/coretypes.h>
#include <lorproto/easy.h>
#include <lorproto/effect.h>
#include <lorproto/heartbeat.h>
#include <lorproto/uid.h>
#include <tinyfseq.h>

#include "audio.h"
#include "cell.h"
#include "seq.h"
#include "serial.h"
#include <lor/buf.h>
#include <std2/errcode.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

int PU_wait(const unsigned int seconds) {
    // LOR hardware may require several heartbeat messages are sent
    // before it considers itself connected to the player
    // This artificially waits prior to starting playback to ensure the device is
    // considered connected and ready for frame data
    if (seconds == 0) return FP_EOK;

    printf("waiting %u seconds for connection...\n", seconds);

    // generate a single heartbeat message to repeatedly send
    LorBuffer* msg;
    if ((msg = LB_alloc()) == NULL) return -FP_ENOMEM;

    lorAppendHeartbeat(msg);

    // assumes 2 heartbeat messages per second (500ms delay)
    for (unsigned int toSend = seconds * 2; toSend > 0; toSend--) {
        Serial_write(msg->buffer, msg->offset);

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

    free(msg);

    return FP_EOK;
}

int PU_lightsOff(void) {
    LorBuffer* msg;
    if ((msg = LB_alloc()) == NULL) return -FP_ENOMEM;

    for (uint8_t u = LOR_UNIT_MIN; u <= LOR_UNIT_MAX; u++) {
        lorAppendUnitEffect(msg, LOR_EFFECT_SET_OFF, NULL, u);
        Serial_write(msg->buffer, msg->offset);
        LB_rewind(msg);
    }

    free(msg);

    return FP_EOK;
}

long PU_secondsRemaining(const uint32_t frame, const struct tf_header_t* seq) {
    if (seq->frameCount < frame) return 0;

    const uint32_t framesRemaining = seq->frameCount - frame;
    return framesRemaining / (1000 / seq->frameStepTimeMillis);
}

int PU_writeHeartbeat(void) {
    LorBuffer* msg;
    if ((msg = LB_alloc()) == NULL) return -FP_ENOMEM;
    lorAppendHeartbeat(msg);
    Serial_write(msg->buffer, msg->offset);
    free(msg);
    return FP_EOK;
}

int PU_writeEffect(const struct ctgroup_s* group, struct LorBuffer* msg) {
    assert(group != NULL);
    assert(group->size > 0);

    const LorEffect effect = LOR_EFFECT_SET_INTENSITY;
    const union LorEffectArgs effectArgs = {
            .setIntensity = {.intensity = group->intensity}};

    if (group->size > 1) {
        const LorChannelSet cs = {
                .offset = group->offset,
                .channelBits = group->cs,
        };

        lorAppendChannelSetEffect(msg, effect, &effectArgs, cs, group->unit);
    } else {
        assert(__builtin_popcount(group->cs) == 1);
        const uint16_t channel = __builtin_ctz(group->cs) + group->offset;
        lorAppendChannelEffect(msg, effect, &effectArgs, channel, group->unit);
    }

    Serial_write(msg->buffer, msg->offset);

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
