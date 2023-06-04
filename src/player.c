#include "player.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <lightorama/heartbeat.h>

#include "audio.h"
#include "mem.h"
#include "pump.h"
#include "seq.h"
#include "serial.h"
#include "sleep.h"
#include "time.h"

void playerOptsFree(PlayerOpts *opts) {
    freeAndNull((void **) &opts->sequenceFilePath);
    freeAndNull((void **) &opts->channelMapFilePath);
    freeAndNull((void **) &opts->audioOverrideFilePath);
}

static Sequence gPlaying;
static FramePump gFramePump;

static void playerLogStatus(void) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();

    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    static char bDuration[64];
    sequenceGetDuration(&gPlaying, bDuration, sizeof(bDuration));

    static char bDrift[64];
    sleepGetDrift(bDrift, sizeof(bDrift));

    printf("remaining: %s\t\tdt: %s\t\tpump: %4d\n", bDuration, bDrift,
           gFramePump.frameEnd - gFramePump.framePos);
}

static uint8_t *gLastFrameData;

static void playerFree(void) {
    freeAndNull((void **) &gLastFrameData);

    memset(&gPlaying, 0, sizeof(Sequence));
    memset(&gFramePump, 0, sizeof(FramePump));
}

static bool playerHandleNextFrame(void) {
    if (!sequenceNextFrame(&gPlaying)) return false;

    // maintain a copy of the previous frame to use for detecting differences
    const uint32_t frameSize = sequenceGetFrameSize(&gPlaying);

    if (gLastFrameData == NULL) gLastFrameData = mustMalloc(frameSize);

    // fetch the current frame data
    uint8_t *frameDataHead = NULL;

    if (!framePumpGet(&gFramePump, &gPlaying, &frameDataHead)) return false;

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameDataHead, frameSize);

    serialWriteFrame(frameDataHead, gLastFrameData, frameSize);

    playerLogStatus();

    return true;
}

static char *playerGetFirstAudioFile(PlayerOpts opts) {
    if (opts.audioOverrideFilePath != NULL) {
        return opts.audioOverrideFilePath;
    } else if (gPlaying.audioFilePath != NULL) {
        return gPlaying.audioFilePath;
    } else {
        return NULL;
    }
}

static void playerPlayFirstAudioFile(PlayerOpts opts) {
    // select best audio file and play
    const char *audioFilePath = playerGetFirstAudioFile(opts);

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

static void playerCheckSkippedFrames(int64_t ns) {
    const long millis = ns / 1000000;

    if (millis <= gPlaying.header.frameStepTimeMillis) return;

    const int skippedFrames =
            (int) ceil((double) (millis - gPlaying.header.frameStepTimeMillis) /
                       (double) gPlaying.header.frameStepTimeMillis);

    if (skippedFrames == 0) return;

    int64_t newFrame = gPlaying.currentFrame + skippedFrames;

    if (newFrame > gPlaying.header.frameCount)
        newFrame = gPlaying.header.frameCount;

    gPlaying.currentFrame = newFrame;

    printf("warning: skipping %d frames\n", skippedFrames);
}

void playerInit(PlayerOpts opts) {
    playerWaitForConnection(opts);

    // read and parse sequence file data
    sequenceInit(&gPlaying);

    framePumpInit(&gFramePump);

    sequenceOpen(opts.sequenceFilePath, &gPlaying);

    // optionally override the sequence's playback rate with the CLI's value
    if (opts.frameStepTimeOverrideMs > 0)
        gPlaying.header.frameStepTimeMillis = opts.frameStepTimeOverrideMs;

    playerPlayFirstAudioFile(opts);

    // start sequence timer loop
    sleepTimerLoop(playerHandleNextFrame, gPlaying.header.frameStepTimeMillis,
                   playerCheckSkippedFrames);

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (audioCheckPlaying())
        ;

    printf("end of sequence!\n");

    // cleanup resources
    audioStop();

    sequenceFree(&gPlaying);

    framePumpFree(&gFramePump);

    playerFree();
}
