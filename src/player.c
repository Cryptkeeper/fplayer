#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lorproto/heartbeat.h"

#include "audio.h"
#include "comblock.h"
#include "pump.h"
#include "seq.h"
#include "serial.h"
#include "std/err.h"
#include "std/sleep.h"
#include "std/string.h"
#include "std/time.h"
#include "transform/netstats.h"
#include "transform/precompute.h"

#ifdef _WIN32
    #include <windows.h>
#endif

static FramePump gFramePump;

static uint32_t gNextFrame;

static uint8_t *gLastFrameData;

// LOR hardware may require several heartbeat messages are sent
// before it considers itself connected to the player
// This artificially waits prior to starting playback to ensure the device is
// considered connected and ready for frame data
static void playerWaitForConnection(const unsigned int seconds) {
    if (seconds == 0) return;

    printf("waiting %u seconds for connection...\n", seconds);

    // assumes 2 heartbeat messages per second (500ms delay)
    for (unsigned int toSend = seconds * 2; toSend > 0; toSend--) {
        serialWriteHeartbeat();

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
}

static void playerPlayFirstAudioFile(const char *const override,
                                     const char *const sequence) {
    // select the override, if set, otherwise fallback to the sequence's hint
    const char *const priority = override != NULL ? override : sequence;

    if (priority != NULL) {
        printf("preparing to play %s\n", priority);

        audioPlayFile(priority);
    } else {
        printf("no audio file detected using override or via sequence\n");
    }
}

static char *playerGetRemaining(void) {
    const uint32_t framesRemaining = curSequence.frameCount - gNextFrame;
    const long seconds =
            framesRemaining / (1000 / curSequence.frameStepTimeMillis);

    return dsprintf("%02ldm %02lds", seconds / 60, seconds % 60);
}

static void playerLogStatus(void) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();

    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    char *const remaining = playerGetRemaining();
    char *const sleep = Sleep_status();
    char *const netstats = nsGetStatus();

    printf("remaining: %s\tdt: %s\tpump: %4d\t%s\n", remaining, sleep,
           framePumpGetRemaining(&gFramePump), netstats);

    free(remaining);
    free(sleep);
    free(netstats);
}

static void playerHandleNextFrame(struct sleep_loop_t *const loop) {
    if (gNextFrame >= curSequence.frameCount) {
        Sleep_halt(loop, "out of frames");
        return;
    }

    const uint32_t frameSize = curSequence.channelCount;

    if (gLastFrameData == NULL) {
        gLastFrameData = mustMalloc(frameSize);

        // zero out the array to represent all existing intensity values as off
        memset(gLastFrameData, 0, frameSize);
    }

    const uint32_t frame = gNextFrame++;

    // fetch the current frame data
    const uint8_t *const frameData =
            framePumpGet(loop->args, &gFramePump, frame, true);

    serialWriteFrame(frameData, gLastFrameData, frameSize, frame);

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameData, frameSize);

    playerLogStatus();
}

static void playerStartPlayback(FCHandle fc) {
    struct sleep_loop_t loop = {
            .args = fc,
            .fn = playerHandleNextFrame,
    };

    // start sequence timer loop
    // this call blocks until playback is completed
    Sleep_loop(&loop);

    printf("sequence stopped: %s\n", loop.msg);
    printf("turning off lights, waiting for end of audio...\n");

    serialWriteAllOff();

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (audioCheckPlaying())
        ;

    printf("end of sequence!\n");

    // print closing remarks
    char *const netstats = nsGetSummary();

    printf("%s\n", netstats);
    free(netstats);
}

static void playerFree(void) {
    free(gLastFrameData);

    gLastFrameData = NULL;
    gNextFrame = 0;
}

void playerRun(FCHandle fc,
               const char *const audioOverrideFilePath,
               const PlayerOpts opts) {
    Seq_initHeader(fc);

    comBlocksInit(fc);

    if (opts.precomputeFades) {
        char *const cacheFilePath = dsprintf("%s.pcf", FC_filepath(fc));

        // load existing data or precompute and save new data
        precomputeRun(cacheFilePath, fc);

        free(cacheFilePath);
    }

    playerWaitForConnection(opts.connectionWaitS);

    char *audioFilePath = Seq_getMediaFile(fc);
    playerPlayFirstAudioFile(audioOverrideFilePath, audioFilePath);
    free(audioFilePath);// only needed to init playback

    // optionally override the sequence's playback rate with the CLI's value
    if (opts.frameStepTimeOverrideMs > 0)
        curSequence.frameStepTimeMillis = opts.frameStepTimeOverrideMs;

    playerStartPlayback(fc);

    // playback finished, free resources and exit cleanly
    precomputeFree();
    comBlocksFree();

    framePumpFree(&gFramePump);

    playerFree();
}
