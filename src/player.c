/// @file player.c
/// @brief Playback execution function implementation.
#include "player.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tinyfseq.h"
#include "tinylor.h"

#include "audio.h"
#include "cell.h"
#include "crmap.h"
#include "fseq/seq.h"
#include "pump.h"
#include "putil.h"
#include "queue.h"
#include "serial.h"
#include "sleep.h"
#include "std2/errcode.h"
#include "std2/fc.h"

/// @struct player_rtd_s
/// @brief Player runtime data structure.
struct player_rtd_s {
    uint32_t nextFrame;         ///< Index of the next frame to be played
    struct tf_header_t* seq;    ///< Decoded sequence file metadata header
    struct frame_pump_s* pump;  ///< Frame pump for reading/queueing frame data
    struct sleep_coll_s* scoll; ///< Sleep collector for frame rate control
    struct ctable_s* ctable;    ///< Computed+cached channel map lookup table
    uint32_t written;           ///< Network bytes written in the last second
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
/// @param fc sequence file controller to read from
/// @param cmap channel map to use for index lookups
/// @param rtd player runtime data to populate
/// @return 0 on success, a negative error code on failure
static int
Player_init(struct FC* fc, struct cr_s* cmap, struct player_rtd_s* rtd) {
    assert(fc != NULL);
    assert(cmap != NULL);
    assert(rtd != NULL);
    assert(rtd->seq != NULL);

    int err;

    // initialize the sleep collector for frame rate control
    if ((err = Sleep_init(&rtd->scoll))) goto ret;

    // initialize the channel map lookup table
    if ((err = CT_init(cmap, rtd->seq->channelCount, &rtd->ctable))) goto ret;

    // initialize the frame pump for reading/queueing frame data
    if ((err = FP_init(fc, rtd->seq, &rtd->pump))) goto ret;

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

    const double kbps = rtd->written / 1024.0;
    rtd->written = 0;

    printf("remaining: %02ldm %02lds\tdt: %.4fms (%.2f fps)\tpump: "
           "%5d\t\tkbps: "
           "%.2f\n",
           seconds / 60, seconds % 60, ms, fps, frames, kbps);
}

/// @brief Increments the current frame index and writes the minified frame data
/// to the serial output. This function drives the core functionality of the player.
/// @param rtd player runtime data to write the next frame from
/// @param sdev serial device to write the frame data to
/// @return 0 on success, a negative error code on failure
static int Player_writeFrame(struct player_rtd_s* rtd,
                             struct serialdev_s* sdev) {
    assert(rtd != NULL);
    assert(rtd->nextFrame < rtd->seq->frameCount);
    assert(sdev != NULL);

    const uint32_t frameSize = rtd->seq->channelCount;
    const uint32_t frameId = rtd->nextFrame++;

    uint8_t* frameData = NULL; /* frame data buffer */

    int err = FP_EOK;

    if ((err = FP_checkPreload(rtd->pump, frameId))) goto ret;
    if ((err = FP_nextFrame(rtd->pump, &frameData))) goto ret;

    // update the cell table with latest frame data
    for (uint32_t i = 0; i < frameSize; i++)
        CT_change(rtd->ctable, i, frameData[i]);

    // write the effect data for each matching channel group
    for (uint32_t i = 0; i < rtd->seq->channelCount; i++) {
        struct ctgroup_s group;
        if (!CT_groupof(rtd->ctable, i, &group)) continue;
        if ((err = PU_writeEffect(sdev, &group, &rtd->written))) goto ret;
    }

    // wait for serial to drain outbound
    // this creates back pressure that results in fps loss if the serial can't keep up
    Serial_drain(sdev);

ret:
    free(frameData);

    return err;
}

/// @brief Main loop of the player that drives the playback of the sequence.
/// This function will block until the sequence is complete, writing frame data
/// to the serial output and logging the player's current state. A heartbeat
/// message is sent every ~500ms to ensure the connection is maintained.
/// @param rtd initialized player runtime data
/// @param sdev serial device to write frame data to
/// @return 0 on success, a negative error code on failure
static int Player_loop(struct player_rtd_s* rtd, struct serialdev_s* sdev) {
    assert(rtd != NULL);
    assert(sdev != NULL);

    int err;

    while (rtd->nextFrame < rtd->seq->frameCount) {
        Sleep_do(rtd->scoll, rtd->seq->frameStepTimeMillis);

        // send heartbeat every ~500ms, or sooner if the fps doesn't divide evenly
        if (rtd->nextFrame % (500 / rtd->seq->frameStepTimeMillis) == 0)
            if ((err = PU_writeHeartbeat(sdev))) return err;

        if ((err = Player_writeFrame(rtd, sdev))) return err;

        // only print every second (using the current frame rate as a timer)
        if (!((rtd->nextFrame - 1) % (1000 / rtd->seq->frameStepTimeMillis)))
            Player_log(rtd);
    }

    printf("turning off lights, waiting for end of audio...\n");
    if ((err = PU_lightsOff(sdev))) return err;

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (Audio_isPlaying())
        ;

    printf("end of sequence!\n");

    return FP_EOK;
}

int Player_exec(struct qentry_s* req, struct serialdev_s* sdev) {
    assert(req != NULL);
    assert(sdev != NULL);

    struct FC* fc = NULL;          /* sequence file controller */
    struct cr_s* cmap = NULL;      /* channel map file data */
    struct player_rtd_s rtd = {0}; /* player runtime data */

    int err = FP_EOK;

    // open the sequence file
    if ((fc = FC_open(req->seqfp, FC_MODE_READ)) == NULL) {
        err = -FP_ESYSCALL;
        goto ret;
    }

    // open the channel map file
    if ((err = CMap_read(req->cmapfp, &cmap))) {
        fprintf(stderr, "failed to read/parse channel map file `%s`: %s %d\n",
                req->cmapfp, FP_strerror(err), err);
        goto ret;
    }

    // open, read and configure environment for the sequence provided
    if ((err = Seq_open(fc, &rtd.seq))) goto ret;

    // initialize runtime data for the player
    if ((err = Player_init(fc, cmap, &rtd))) goto ret;

    // sleep/wait for connection if requested
    if ((err = PU_wait(sdev, req->waitsec))) goto ret;

    // play audio if available
    // TODO: print err for audio, but ignore
    if ((err = PU_playFirstAudio(req->audiofp, fc, rtd.seq))) goto ret;

    // begin the main loop of the player
    if ((err = Player_loop(&rtd, sdev))) goto ret;

ret:
    Player_free(&rtd);
    CMap_free(cmap);
    FC_close(fc);

    return err;
}
