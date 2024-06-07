#include "player.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <lorproto/coretypes.h>
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
    struct tf_header_t* seq;    /* decoded sequence file metadata header */
    struct frame_pump_s* pump;  /* frame pump for reading/queueing frame data */
    struct sleep_coll_s* scoll; /* sleep collector for frame rate control */
    struct ctable_s* ctable;    /* computed+cached channel map lookup table */
};

/// @brief Frees dynamic allocated structures referenced by the player runtime data.
/// `rtd` itself is not freed by this function.
/// @param rtd player runtime data to free
static void Player_free(struct player_rtd_s* rtd) {
    assert(rtd != NULL);

    free(rtd->seq);
    FP_free(rtd->pump);
    free(rtd->scoll);
    CT_free(rtd->ctable);
}

/// @brief Populates the player runtime data with dynamically allocated
/// structures before initializing each subsystem.
/// @param player player configuration
/// @param rtd player runtime data to populate
/// @return 0 on success, a negative error code on failure
static int Player_init(const struct player_s* player,
                       struct player_rtd_s* rtd) {
    assert(player != NULL);
    assert(rtd != NULL);
    assert(rtd->seq != NULL);

    int err;

    // initialize the sleep collector for frame rate control
    if ((err = Sleep_init(&rtd->scoll))) goto ret;

    // initialize the channel map lookup table
    if ((err = CT_init(player->cmap, rtd->seq->channelCount, &rtd->ctable)))
        goto ret;

    // initialize the frame pump for reading/queueing frame data
    if ((err = FP_init(player->fc, rtd->seq, &rtd->pump))) goto ret;

ret:
    if (err) Player_free(rtd);

    return err;
}

/// @brief Prints a log message summarizing the player's current state.
/// @param rtd player runtime data to log
static void Player_log(struct player_rtd_s* rtd) {
    assert(rtd != NULL);

    const double ms = (double) Sleep_average(rtd->scoll) / 1e6;
    const double fps = ms > 0 ? 1000 / ms : 0;

    const long seconds = PU_secondsRemaining(rtd->nextFrame, rtd->seq);
    const int frames = FP_framesRemaining(rtd->pump);

    printf("remaining: %02ldm %02lds\tdt: %.4fms (%.2f fps)\tpump: %5d\n",
           seconds / 60, seconds % 60, ms, fps, frames);
}

/// @brief Increments the current frame index and writes the minified frame data
/// to the serial output. This function drives the core functionality of the player.
/// @param rtd player runtime data to write the next frame from
/// @return 0 on success, a negative error code on failure
static int Player_writeFrame(struct player_rtd_s* rtd) {
    assert(rtd != NULL);
    assert(rtd->nextFrame < rtd->seq->frameCount);

    const uint32_t frameSize = rtd->seq->channelCount;
    const uint32_t frameId = rtd->nextFrame++;

    uint8_t* frameData = NULL; /* frame data buffer */
    LorBuffer* msg = NULL;     /* shared outbound message buffer */

    int err = FP_EOK;

    if ((err = FP_checkPreload(rtd->pump, frameId))) goto ret;
    if ((err = FP_nextFrame(rtd->pump, &frameData))) goto ret;

    // update the cell table with latest frame data
    for (uint32_t i = 0; i < frameSize; i++)
        CT_change(rtd->ctable, i, frameData[i]);

    if ((msg = LB_alloc()) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    // write the effect data for each matching channel group
    for (uint32_t i = 0; i < rtd->seq->channelCount; i++) {
        struct ctgroup_s group;
        if (!CT_groupof(rtd->ctable, i, &group)) continue;
        if ((err = PU_writeEffect(&group, msg))) goto ret;

        LB_rewind(msg);
    }

    // wait for serial to drain outbound
    // this creates back pressure that results in fps loss if the serial can't keep up
    Serial_drain();

ret:
    free(frameData);
    free(msg);

    return err;
}

/// @brief Main loop of the player that drives the playback of the sequence.
/// This function will block until the sequence is complete, writing frame data
/// to the serial output and logging the player's current state. A heartbeat
/// message is sent every ~500ms to ensure the connection is maintained.
/// @param rtd initialized player runtime data
/// @return 0 on success, a negative error code on failure
static int Player_loop(struct player_rtd_s* rtd) {
    assert(rtd != NULL);

    int err;

    while (rtd->nextFrame < rtd->seq->frameCount) {
        Sleep_do(rtd->scoll, rtd->seq->frameStepTimeMillis);

        // send heartbeat every ~500ms, or sooner if the fps doesn't divide evenly
        if (rtd->nextFrame % (500 / rtd->seq->frameStepTimeMillis) == 0)
            if ((err = PU_writeHeartbeat())) return err;

        if ((err = Player_writeFrame(rtd))) return err;

        // only print every second (using the current frame rate as a timer)
        if (!((rtd->nextFrame - 1) % (1000 / rtd->seq->frameStepTimeMillis)))
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

    struct player_rtd_s rtd = {0}; /* player runtime data */

    // open, read and configure environment for the sequence provided
    if ((err = Seq_open(player->fc, &rtd.seq))) goto ret;

    // initialize runtime data for the player
    if ((err = Player_init(player, &rtd))) goto ret;

    // sleep/wait for connection if requested
    if ((err = PU_wait(player->waitsec))) goto ret;

    // play audio if available
    // TODO: print err for audio, but ignore
    if ((err = PU_playFirstAudio(player->audiofp, player->fc, rtd.seq)))
        goto ret;

    // begin the main loop of the player
    if ((err = Player_loop(&rtd))) goto ret;

ret:
    Player_free(&rtd);

    return err;
}
