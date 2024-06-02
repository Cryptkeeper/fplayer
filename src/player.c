#include "player.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <lorproto/coretypes.h>
#include <lorproto/easy.h>
#include <lorproto/heartbeat.h>
#include <tinyfseq.h>

#include "audio.h"
#include "cell.h"
#include "pump.h"
#include "putil.h"
#include "seq.h"
#include "serial.h"
#include "sleep.h"
#include <lor/buf.h>
#include <std2/errcode.h>

struct player_rtd_s {
    uint32_t nextFrame;         /* index of the next frame to be played */
    struct frame_pump_s* pump;  /* frame pump for reading/queueing frame data */
    struct sleep_coll_s* scoll; /* sleep collector for frame rate control */
    struct ctable_s* ctable;    /* computed+cached channel map lookup table */
};

static void Player_freeRTD(struct player_rtd_s* rtd) {
    if (rtd == NULL) return;
    FP_free(rtd->pump);
    free(rtd->scoll);
    CT_free(rtd->ctable);
    free(rtd);
}

int Player_newRTD(const struct player_s* player, struct player_rtd_s** rtdp) {
    assert(player != NULL);
    assert(rtdp != NULL);
    assert(curSequence != NULL);

    struct player_rtd_s* rtd = calloc(1, sizeof(*rtd));
    if (rtd == NULL) return -FP_ENOMEM;

    int err;

    if ((rtd->scoll = calloc(1, sizeof(*rtd->scoll))) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    // initialize the channel map lookup table
    if ((err = CT_init(player->cmap, curSequence->channelCount, &rtd->ctable)))
        goto ret;

    // initialize the frame pump for reading/queueing frame data
    if ((rtd->pump = FP_init(player->fc)) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

ret:
    if (err) Player_freeRTD(rtd), rtd = NULL;
    *rtdp = rtd;

    return err;
}

static int Player_heartbeat(struct player_rtd_s* rtd) {
    // send heartbeat every ~500ms, or sooner if the fps doesn't divide evenly
    if (rtd->nextFrame % (500 / curSequence->frameStepTimeMillis) != 0)
        return FP_EOK;

    LorBuffer* msg = LB_alloc();
    if (msg == NULL) return -FP_ENOMEM;

    lorAppendHeartbeat(msg);
    Serial_write(msg->buffer, msg->offset);
    LB_free(msg);

    return FP_EOK;
}

static void Player_log(struct player_rtd_s* rtd) {
    // only print every second (using the current frame rate as a timer)
    if ((rtd->nextFrame - 1) % (1000 / curSequence->frameStepTimeMillis) != 0)
        return;

    const double ms = (double) Sleep_average(rtd->scoll) / 1e6;
    const double fps = ms > 0 ? 1000 / ms : 0;

    const long seconds = PU_secondsRemaining(rtd->nextFrame);
    const int frames = FP_framesRemaining(rtd->pump);

    printf("remaining: %02ldm %02lds\tdt: %.4fms (%.2f fps)\tpump: %5d\n",
           seconds / 60, seconds % 60, ms, fps, frames);
}

static void Player_write(const struct ctgroup_s* group, LorBuffer* msg) {
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
}

static int Player_nextFrame(struct player_rtd_s* rtd) {
    assert(rtd != NULL);
    assert(rtd->nextFrame < curSequence->frameCount);

    const uint32_t frameSize = curSequence->channelCount;
    const uint32_t frameId = rtd->nextFrame++;

    uint8_t* frameData = NULL; /* frame data buffer */
    LorBuffer* msg = NULL;     /* shared outbound message buffer */

    int err = FP_EOK;

    if ((err = FP_checkPreload(rtd->pump, frameId))) goto ret;
    if ((err = FP_nextFrame(rtd->pump, &frameData))) goto ret;

    for (uint32_t i = 0; i < frameSize; i++)
        CT_change(rtd->ctable, i, frameData[i]);

    struct ctgroup_s group;
    for (uint32_t i = 0; i < curSequence->channelCount; i++) {
        if (CT_groupof(rtd->ctable, i, &group)) {
            if (msg == NULL && (msg = LB_alloc()) == NULL) {
                err = -FP_ENOMEM;
                goto ret;
            }

            Player_write(&group, msg);
            LB_rewind(msg);
        }
    }

    // wait for serial to drain outbound
    // this creates back pressure that results in fps loss if the serial can't keep up
    Serial_drain();

ret:
    free(frameData);
    LB_free(msg);

    return err;
}

static int Player_loop(struct player_rtd_s* rtd) {
    int err;
    while (rtd->nextFrame < curSequence->frameCount) {
        Sleep_do(rtd->scoll, curSequence->frameStepTimeMillis);

        if ((err = Player_heartbeat(rtd))) return err;
        if ((err = Player_nextFrame(rtd))) return err;

        Player_log(rtd);
    }

    printf("turning off lights, waiting for end of audio...\n");
    if ((err = PU_lightsOff())) return err;

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (Audio_isPlaying())
        ;

    printf("end of sequence!\n");

    return FP_EOK;
}

int Player_exec(struct player_s* player) {
    assert(player != NULL);

    int err = FP_EOK;

    struct player_rtd_s* rtd = NULL;   /* player runtime data */
    char* playAudio = player->audiofp; /* audio to play */
    char* readAudio = NULL;            /* audio fp read from sequence */

    // open, read and configure environment for the sequence provided
    if ((err = Seq_open(player->fc))) goto ret;

    if (playAudio == NULL) {// audio fp not provided, read from sequence
        if (!(err = Seq_getMediaFile(player->fc, &readAudio))) {
            playAudio = readAudio;
        } else {
            goto ret;
        }
    }

    // initialize runtime data for the player
    if ((err = Player_newRTD(player, &rtd))) goto ret;

    // sleep/wait for connection if requested
    if ((err = PU_wait(player->waitsec))) goto ret;

    // play audio if available
    // TODO: print err for audio, but ignore
    if (playAudio != NULL) Audio_play(playAudio);

    // begin the main loop of the player
    if ((err = Player_loop(rtd))) goto ret;

ret:
    free(readAudio);
    Player_freeRTD(rtd);
    Seq_close();

    return err;
}
