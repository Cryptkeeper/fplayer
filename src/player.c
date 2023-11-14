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
#include "std/mem.h"
#include "std/sleep.h"
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
static void playerWaitForConnection(const PlayerOpts opts) {
    if (opts.connectionWaitS == 0) return;

    printf("waiting %d seconds for connection...\n", opts.connectionWaitS);

    // assumes 2 heartbeat messages per second (500ms delay)
    for (int toSend = opts.connectionWaitS * 2; toSend > 0; toSend--) {
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

static void playerPlayFirstAudioFile(const sds override, const sds sequence) {
    // select the override, if set, otherwise fallback to the sequence's hint
    const sds audioFilePath = override != NULL ? override : sequence;

    if (audioFilePath != NULL) {
        printf("preparing to play %s\n", audioFilePath);

        audioPlayFile(audioFilePath);
    } else {
        printf("no audio file detected using override or via sequence\n");
    }
}

static void playerLogStatus(void) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();

    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    const sds remaining = playerGetRemaining();
    const sds sleep = sleepGetStatus();
    const sds netstats = nsGetStatus();

    printf("remaining: %s\tdt: %s\tpump: %4d\t%s\n", remaining, sleep,
           framePumpGetRemaining(&gFramePump), netstats);

    sdsfree(remaining);
    sdsfree(sleep);
    sdsfree(netstats);
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
    const sds netstats = nsGetSummary();

    printf("%s\n", netstats);

    sdsfree(netstats);
}

static void playerFree(void) {
    freeAndNull(gLastFrameData);
    gNextFrame = 0;
}

void playerRun(const sds sequenceFilePath,
               const sds audioOverrideFilePath,
               const PlayerOpts opts) {
    sds audioFilePath = NULL;
    sequenceOpen(sequenceFilePath, &audioFilePath);

    comBlocksInit();

    if (opts.precomputeFades) {
        const sds cacheFilePath =
                sdscatprintf(sdsempty(), "%s.pcf", sequenceFilePath);

        // load existing data or precompute and save new data
        precomputeRun(cacheFilePath);

        sdsfree(cacheFilePath);
    }

    playerWaitForConnection(opts);
    playerPlayFirstAudioFile(audioOverrideFilePath, audioFilePath);

    sdsfree(audioFilePath);// only needed to init playback

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

sds playerGetRemaining(void) {
    const uint32_t framesRemaining = sequenceData()->frameCount - gNextFrame;
    const long seconds = framesRemaining / sequenceFPS();

    return sdscatprintf(sdsempty(), "%02ldm %02lds", seconds / 60,
                        seconds % 60);
}
