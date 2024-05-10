#include "player.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <lorproto/easy.h>
#include <lorproto/heartbeat.h>
#include <tinyfseq.h>

#include "audio.h"
#include "cmap.h"
#include "lor/protowriter.h"
#include "pump.h"
#include "seq.h"
#include "serial.h"
#include "std/err.h"
#include "std2/errcode.h"
#include "std2/sleep.h"
#include "std2/string.h"
#include "std2/time.h"
#include "transform/minifier.h"

#ifdef _WIN32
    #include <windows.h>
#endif

struct player_rtd_s {
    struct frame_pump_s* pump;
    uint32_t nextFrame;
    uint8_t* lastFrameData;
    struct sleep_coll_s scoll;
};

/// @brief Compensates for the player to connect to the LOR hardware by sending
/// heartbeat messages for the given number of seconds.
/// @param seconds number of seconds to wait for connection
/// @return 0 on success, a negative error code on failure
static int playerWaitForConnection(const unsigned int seconds) {
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

static char* playerGetRemaining(struct player_rtd_s* rtd) {
    const uint32_t framesRemaining = curSequence->frameCount - rtd->nextFrame;
    const long seconds =
            framesRemaining / (1000 / curSequence->frameStepTimeMillis);

    return dsprintf("%02ldm %02lds", seconds / 60, seconds % 60);
}

static void playerLogStatus(struct player_rtd_s* rtd) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();
    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    const double ms = (double) Sleep_average(&rtd->scoll) / 1e6;
    const double fps = ms > 0 ? 1000 / ms : 0;
    char* const sleep = dsprintf("%.4fms (%.2f fps)", ms, fps);

    char* const remaining = playerGetRemaining(rtd);

    printf("remaining: %s\tdt: %s\tpump: %4d\n", remaining, sleep, 0);

    free(remaining);
    free(sleep);
}

static int playerHandleNextFrame(struct player_rtd_s* rtd) {
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
    if ((err = FP_copy(rtd->pump, &frameData))) return err;

    minifyStream(frameData, rtd->lastFrameData, frameSize);

    // wait for serial to drain outbound
    // this creates back pressure that results in fps loss if the serial can't keep up
    Serial_drain();

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    free(rtd->lastFrameData), rtd->lastFrameData = frameData;

    return FP_EOK;
}

static int playerTurnOffAllLights(void) {
    int err = FP_EOK;

    uint8_t* uids = NULL;  /* array of all unit ids */
    LorBuffer* msg = NULL; /* disable lights message */

    if ((uids = channelMapGetUids()) == NULL || (msg = LB_alloc()) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    for (size_t i = 0; uids != NULL && uids[i] != 0; i++) {
        lorAppendUnitEffect(msg, LOR_EFFECT_SET_OFF, NULL, uids[i]);
        Serial_write(msg->buffer, msg->offset);
        LB_rewind(msg);
    }

ret:
    free(uids);
    LB_free(msg);
    return err;
}

static int FP_playdo(struct player_rtd_s* rtd) {
    int err;

    while (rtd->nextFrame < curSequence->frameCount) {
        Sleep_do(&rtd->scoll, curSequence->frameStepTimeMillis);

        if ((err = playerHandleNextFrame(rtd))) return err;

        playerLogStatus(rtd);
    }

    printf("turning off lights, waiting for end of audio...\n");

    if ((err = playerTurnOffAllLights())) return err;

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (audioCheckPlaying())
        ;

    printf("end of sequence!\n");

    return FP_EOK;
}

static void PL_freertd(struct player_rtd_s* rtd) {
    if (rtd == NULL) return;
    FP_free(rtd->pump);
    free(rtd->lastFrameData);
    free(rtd);
}

int PL_play(struct player_s* player) {
    assert(player != NULL);

    int err = FP_EOK;

    // allocate a runtime data instance
    struct player_rtd_s* rtd = calloc(1, sizeof(*rtd));
    if (rtd == NULL) return -FP_ENOMEM;

    char* audiofp = player->audiofp;

    if ((err = Seq_open(player->fc))) goto ret;

    if (audiofp == NULL)// attempt to load audio file from sequence
        if ((err = Seq_getMediaFile(player->fc, &audiofp))) goto ret;

    if ((rtd->pump = FP_init(player->fc)) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    if ((err = playerWaitForConnection(player->wait_s))) goto ret;

    // TODO: print err for audio, but ignore
    audioPlayFile(audiofp);

    if ((err = FP_playdo(rtd))) goto ret;

ret:
    free(audiofp);
    PL_freertd(rtd);
    Seq_close();

    return err;
}
