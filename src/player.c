#include "player.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <lightorama/heartbeat.h>

#include "audio.h"
#include "pump.h"
#include "seq.h"
#include "serial.h"
#include "std/mem.h"
#include "std/sleep.h"
#include "std/time.h"
#include "transform/fade.h"
#include "transform/netstats.h"
#include "transform/precompute.h"

void playerOptsFree(PlayerOpts *opts) {
    freeAndNull((void **) &opts->sequenceFilePath);
    freeAndNull((void **) &opts->channelMapFilePath);
    freeAndNull((void **) &opts->audioOverrideFilePath);
}

static FramePump gFramePump;

static uint32_t gNextFrame;

sds playerGetRemaining(void) {
    const uint32_t framesRemaining = sequenceGet(SI_FRAME_SIZE) - gNextFrame;
    const long seconds = framesRemaining / sequenceGet(SI_FPS);

    return sdscatprintf(sdsempty(), "%02ldm %02lds", seconds / 60,
                        seconds % 60);
}

static void playerLogStatus(void) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();

    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    sds remaining = playerGetRemaining();
    sds sleep = sleepGetStatus();
    sds netstats = nsGetStatus();

    const uint32_t frameSize = sequenceGet(SI_FRAME_SIZE);

    printf("remaining: %s\tdt: %s\tpump: %4d\t%s\n", remaining, sleep,
           (gFramePump.size - gFramePump.readIdx) / frameSize, netstats);

    sdsfree(remaining);
    sdsfree(sleep);
    sdsfree(netstats);
}

static uint8_t *gLastFrameData;

static void playerFree(void) {
    freeAndNull((void **) &gLastFrameData);
}

static bool playerHandleNextFrame(void) {
    if (gNextFrame >= sequenceGet(SI_FRAME_COUNT)) return false;

    const uint32_t frameSize = sequenceGet(SI_FRAME_SIZE);

    if (gLastFrameData == NULL) {
        gLastFrameData = mustMalloc(frameSize);

        // zero out the array to represent all existing intensity values as off
        memset(gLastFrameData, 0, frameSize);
    }

    const uint32_t frame = gNextFrame++;

    // fetch the current frame data
    uint8_t *frameData = NULL;

    if (!framePumpGet(&gFramePump, frame, &frameData)) return false;

    serialWriteFrame(frameData, gLastFrameData, frameSize, frame);

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameData, frameSize);

    playerLogStatus();

    return true;
}

static void playerPlayFirstAudioFile(const PlayerOpts opts,
                                     const char *seqAudioFilePath) {
    // select the override, if set, otherwise fallback to the sequence's hint
    const char *const audioFilePath = opts.audioOverrideFilePath != NULL
                                              ? opts.audioOverrideFilePath
                                              : seqAudioFilePath;

    if (audioFilePath != NULL) {
        printf("preparing to play %s\n", audioFilePath);

        audioPlayFile(audioFilePath);
    } else {
        printf("no audio file detected using override or via sequence\n");
    }
}

// LOR hardware may require several heartbeat messages are sent
// before it considers itself connected to the player
// This artificially waits prior to starting playback to ensure the device is
// considered connected and ready for frame data
static void playerWaitForConnection(PlayerOpts opts) {
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

static void playerOverrunSkipFrames(const int64_t ns) {
    const long millis = ns / 1000000;

    const uint8_t frameTimeMs = sequenceData()->frameStepTimeMillis;

    if (millis <= frameTimeMs) return;

    const int skippedFrames =
            (int) ceil((double) (millis - frameTimeMs) / (double) frameTimeMs);

    if (skippedFrames == 0) return;

    int64_t newFrame = gNextFrame + skippedFrames;

    const uint32_t max = sequenceGet(SI_FRAME_COUNT);

    if (newFrame >= max) newFrame = max;

    gNextFrame = (uint32_t) newFrame;

    printf("warning: skipping %d frames\n", skippedFrames);
}

static void playerStartPlayback(PlayerOpts opts, const char *seqAudioFilePath) {
    // optionally override the sequence's playback rate with the CLI's value
    if (opts.frameStepTimeOverrideMs > 0)
        sequenceData()->frameStepTimeMillis = opts.frameStepTimeOverrideMs;

    playerPlayFirstAudioFile(opts, seqAudioFilePath);

    freeAndNull((void **) &seqAudioFilePath);

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

void playerRun(PlayerOpts opts) {
    playerWaitForConnection(opts);

    const char *audioFilePath = NULL;

    sequenceOpen(opts.sequenceFilePath, &audioFilePath);

    if (opts.precomputeFades) precomputeStart();

    playerStartPlayback(opts, audioFilePath);

    fadeFree();

    sequenceFree();

    framePumpFree(&gFramePump);

    playerFree();
}
