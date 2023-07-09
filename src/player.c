#include "player.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <lightorama/heartbeat.h>
#include <stb_ds.h>

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

void playerOptsFree(PlayerOpts *const opts) {
    freeAndNull((void **) &opts->sequenceFilePath);
    freeAndNull((void **) &opts->channelMapFilePath);
    freeAndNull((void **) &opts->audioOverrideFilePath);
}

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

        const struct timespec itrSleep = {
                .tv_sec = 0,
                .tv_nsec = LOR_HEARTBEAT_DELAY_NS,
        };

        nanosleep(&itrSleep, NULL);
    }
}

static void playerPlayFirstAudioFile(const char *const override,
                                     const char *const sequence) {
    // select the override, if set, otherwise fallback to the sequence's hint
    const char *const audioFilePath = override != NULL ? override : sequence;

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

    sds remaining = playerGetRemaining();
    sds sleep = sleepGetStatus();
    sds netstats = nsGetStatus();

    printf("remaining: %s\tdt: %s\tpump: %4d\t%s\n", remaining, sleep,
           (int) (arrlen(gFramePump.frames) - gFramePump.head), netstats);

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
    const uint8_t *const frameData = framePumpGet(&gFramePump, frame);

    serialWriteFrame(frameData, gLastFrameData, frameSize, frame);

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameData, frameSize);

    playerLogStatus();

    return true;
}

static void playerOverrunSkipFrames(const int64_t ns) {
    const long millis = ns / 1000000;
    const uint8_t frameTimeMs = sequenceData()->frameStepTimeMillis;

    if (millis <= frameTimeMs) return;

    const int skippedFrames =
            (int) ceil((double) (millis - frameTimeMs) / (double) frameTimeMs);

    if (skippedFrames == 0) return;

    const uint32_t max = sequenceData()->frameCount;
    const int64_t newFrame = gNextFrame + skippedFrames;

    gNextFrame = newFrame >= max ? max : (uint32_t) newFrame;

    printf("warning: skipping %d frames\n", skippedFrames);
}

static void playerStartPlayback(const PlayerOpts opts,
                                const char *mediaFilePath) {
    // optionally override the sequence's playback rate with the CLI's value
    if (opts.frameStepTimeOverrideMs > 0)
        sequenceData()->frameStepTimeMillis = opts.frameStepTimeOverrideMs;

    playerPlayFirstAudioFile(opts.audioOverrideFilePath, mediaFilePath);

    freeAndNull((void **) &mediaFilePath);

    // start sequence timer loop
    sleepTimerLoop(playerHandleNextFrame, sequenceData()->frameStepTimeMillis,
                   playerOverrunSkipFrames);

    printf("turning off lights, waiting for end of audio...\n");

    serialWriteAllOff();

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (audioCheckPlaying())
        ;

    printf("end of sequence!\n");

    // print closing remarks
    sds netstats = nsGetSummary();

    printf("%s\n", netstats);

    sdsfree(netstats);

    // cleanup resources
    audioStop();
}

static void playerFree(void) {
    freeAndNull((void **) &gLastFrameData);
}

void playerRun(const PlayerOpts opts) {
    playerWaitForConnection(opts);

    const char *audioFilePath = NULL;

    sequenceOpen(opts.sequenceFilePath, &audioFilePath);

    comBlocksInit();

    if (opts.precomputeFades) precomputeRun();

    playerStartPlayback(opts, audioFilePath);

    // used by `playerRun`, don't free until player has finished
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
