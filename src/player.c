#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lightorama/heartbeat.h"

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
    const uint32_t framesRemaining = sequenceData()->frameCount - gNextFrame;
    const long seconds = framesRemaining / sequenceFPS();

    return dsprintf("%02ldm %02lds", seconds / 60, seconds % 60);
}

static void playerLogStatus(void) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();

    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    char *const remaining = playerGetRemaining();
    char *const sleep = sleepGetStatus();
    char *const netstats = nsGetStatus();

    printf("remaining: %s\tdt: %s\tpump: %4d\t%s\n", remaining, sleep,
           framePumpGetRemaining(&gFramePump), netstats);

    free(remaining);
    free(sleep);
    free(netstats);
}

static bool playerHandleNextFrame(void) {
    if (gNextFrame >= sequenceData()->frameCount) return false;

    const uint32_t frameSize = sequenceData()->channelCount;

    if (gLastFrameData == NULL) {
        gLastFrameData = mustMalloc(frameSize);

        // zero out the array to represent all existing intensity values as off
        memset(gLastFrameData, 0, frameSize);
    }

    const uint32_t frame = gNextFrame++;

    // fetch the current frame data
    const uint8_t *const frameData = framePumpGet(&gFramePump, frame, true);

    serialWriteFrame(frameData, gLastFrameData, frameSize, frame);

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameData, frameSize);

    playerLogStatus();

    return true;
}

static void playerStartPlayback(void) {
    // start sequence timer loop
    // this call blocks until playback is completed
    sleepTimerLoop(sequenceData()->frameStepTimeMillis, playerHandleNextFrame);

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

void playerRun(const char *const sequenceFilePath,
               const char *const audioOverrideFilePath,
               const PlayerOpts opts) {
    char *audioFilePath = NULL;
    sequenceOpen(sequenceFilePath, &audioFilePath);

    comBlocksInit();

    if (opts.precomputeFades) {
        char *const cacheFilePath = dsprintf("%s.pcf", sequenceFilePath);

        // load existing data or precompute and save new data
        precomputeRun(cacheFilePath);

        free(cacheFilePath);
    }

    playerWaitForConnection(opts.connectionWaitS);
    playerPlayFirstAudioFile(audioOverrideFilePath, audioFilePath);

    free(audioFilePath);// only needed to init playback

    // optionally override the sequence's playback rate with the CLI's value
    if (opts.frameStepTimeOverrideMs > 0)
        sequenceData()->frameStepTimeMillis = opts.frameStepTimeOverrideMs;

    playerStartPlayback();

    // playback finished, free resources and exit cleanly
    precomputeFree();
    comBlocksFree();
    sequenceFree();

    framePumpFree(&gFramePump);

    playerFree();
}
