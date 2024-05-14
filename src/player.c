#include "player.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <lorproto/coretypes.h>
#include <lorproto/easy.h>
#include <lorproto/effect.h>
#include <lorproto/heartbeat.h>
#include <lorproto/uid.h>
#include <tinyfseq.h>

#include "audio.h"
#include "lor/protowriter.h"
#include "pump.h"
#include "seq.h"
#include "serial.h"
#include "sleep.h"
#include "std2/errcode.h"
#include "std2/string.h"
#include "std2/time.h"
#include "transform/cell.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

struct player_rtd_s {
    struct frame_pump_s* pump; /* frame pump for reading/queueing frame data */
    uint32_t nextFrame;        /* index of the next frame to be played */
    struct sleep_coll_s scoll; /* sleep collector for frame rate control */
    struct ctable_s* ctable;   /* computed+cached channel map lookup table */
};

static void Player_freeRTD(struct player_rtd_s* rtd) {
    if (rtd == NULL) return;
    FP_free(rtd->pump);
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

/// @brief Compensates for the player to connect to the LOR hardware by sending
/// heartbeat messages for the given number of seconds.
/// @param seconds number of seconds to wait for connection
/// @return 0 on success, a negative error code on failure
static int Player_wait(const unsigned int seconds) {
    // LOR hardware may require several heartbeat messages are sent
    // before it considers itself connected to the player
    // This artificially waits prior to starting playback to ensure the device is
    // considered connected and ready for frame data
    if (seconds == 0) return FP_EOK;

    printf("waiting %u seconds for connection...\n", seconds);

    // generate a single heartbeat message to repeatedly send
    LorBuffer* msg = LB_alloc();
    if (msg == NULL) return -FP_ENOMEM;

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

    LB_free(msg);

    return FP_EOK;
}

static char* Player_timeRemaining(struct player_rtd_s* rtd) {
    const uint32_t framesRemaining = curSequence->frameCount - rtd->nextFrame;
    const long seconds =
            framesRemaining / (1000 / curSequence->frameStepTimeMillis);

    return dsprintf("%02ldm %02lds", seconds / 60, seconds % 60);
}

static void Player_log(struct player_rtd_s* rtd) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();
    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    const double ms = (double) Sleep_average(&rtd->scoll) / 1e6;
    const double fps = ms > 0 ? 1000 / ms : 0;
    char* const sleep = dsprintf("%.4fms (%.2f fps)", ms, fps);

    char* const remaining = Player_timeRemaining(rtd);

    const int frames = FP_framesRemaining(rtd->pump);

    printf("remaining: %s\tdt: %s\tpump: %4d\n", remaining, sleep, frames);

    free(remaining);
    free(sleep);
}

static int Player_nextFrame(struct player_rtd_s* rtd) {
    assert(rtd != NULL);
    assert(rtd->nextFrame < curSequence->frameCount);

    const uint32_t frameSize = curSequence->channelCount;

    rtd->nextFrame++;

    // send a heartbeat if it has been >500ms
    static timeInstant lastHeartbeat;

    if (timeElapsedNs(lastHeartbeat, timeGetNow()) > LOR_HEARTBEAT_DELAY_NS) {
        lastHeartbeat = timeGetNow();

        LorBuffer* msg = LB_alloc();
        if (msg == NULL) return -FP_ENOMEM;

        lorAppendHeartbeat(msg);
        Serial_write(msg->buffer, msg->offset);

        LB_free(msg);
    }

    // fetch the current frame data
    uint8_t* frameData = NULL;

    int err;
    if ((err = FP_nextFrame(rtd->pump, &frameData))) return err;

    for (uint32_t i = 0; i < frameSize; i++)
        CT_set(rtd->ctable, i, frameData[i]);
    CT_linkall(rtd->ctable);

    uint32_t pos = 0;
    struct ctgroup_s group;

    LorBuffer* msg = LB_alloc();
    if (msg == NULL) return -FP_ENOMEM;// FIXME: may leak frameData

    while (CT_nextgroup(rtd->ctable, &pos, &group)) {
        // TODO
    }

    // wait for serial to drain outbound
    // this creates back pressure that results in fps loss if the serial can't keep up
    Serial_drain();

    free(frameData);

    return FP_EOK;
}

static int Player_lightsOff(void) {
    LorBuffer* msg;
    if ((msg = LB_alloc()) == NULL) return -FP_ENOMEM;

    for (uint8_t u = LOR_UNIT_MIN; u <= LOR_UNIT_MAX; u++) {
        lorAppendUnitEffect(msg, LOR_EFFECT_SET_OFF, NULL, u);
        Serial_write(msg->buffer, msg->offset);
        LB_rewind(msg);
    }

    LB_free(msg);
    return FP_EOK;
}

static int Player_loop(struct player_rtd_s* rtd) {
    int err;

    while (rtd->nextFrame < curSequence->frameCount) {
        Sleep_do(&rtd->scoll, curSequence->frameStepTimeMillis);

        if ((err = Player_nextFrame(rtd))) return err;

        Player_log(rtd);
    }

    printf("turning off lights, waiting for end of audio...\n");

    if ((err = Player_lightsOff())) return err;

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
    if ((err = Player_wait(player->waitsec))) goto ret;

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
