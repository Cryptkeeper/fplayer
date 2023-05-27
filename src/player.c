#include "player.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "pump.h"
#include "seq.h"
#include "serial.h"
#include "sleep.h"

void playerOptsFree(PlayerOpts *opts) {
    free(opts->sequenceFilePath);
    opts->sequenceFilePath = NULL;

    free(opts->channelMapFilePath);
    opts->channelMapFilePath = NULL;

    free(opts->audioOverrideFilePath);
    opts->audioOverrideFilePath = NULL;
}

static Sequence gPlaying;
static FramePump gFramePump;

static void playerLogStatus(void) {
    static char bDuration[64];
    sequenceGetDuration(&gPlaying, bDuration, sizeof(bDuration));

    static char bDrift[64];
    sleepGetDrift(bDrift, sizeof(bDrift));

    static char bStatus[256];
    snprintf(bStatus, sizeof(bStatus),
             "remaining: %s\t\tdt: %s\t\tpump: %4d\t\tcharge time: %.3fms",
             bDuration, bDrift, gFramePump.frameEnd - gFramePump.framePos,
             framePumpLastChargeTime / 1000000.0);

    printf("\r%s", bStatus);
    fflush(stdout);
}

static uint8_t *gLastFrameData;

static bool playerHandleNextFrame(void) {
    if (!sequenceNextFrame(&gPlaying)) return false;

    // maintain a copy of the previous frame to use for detecting differences
    const uint32_t frameSize = sequenceGetFrameSize(&gPlaying);

    if (gLastFrameData == NULL) gLastFrameData = malloc(frameSize);

    assert(gLastFrameData != NULL);

    // fetch the current frame data
    uint8_t *frameDataHead = NULL;

    if (!framePumpGet(&gFramePump, &gPlaying, &frameDataHead)) return false;

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameDataHead, frameSize);

    if (serialWriteFrame(frameDataHead, gLastFrameData, frameSize))
        return false;

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

static long playerGetFrameStepTime(PlayerOpts opts) {
    if (opts.frameStepTimeOverrideMillis > 0)
        return opts.frameStepTimeOverrideMillis;

    return gPlaying.header.frameStepTimeMillis;
}

bool playerInit(PlayerOpts opts) {
    // read and parse sequence file data
    sequenceInit(&gPlaying);

    framePumpInit(&gFramePump);

    if (sequenceOpen(opts.sequenceFilePath, &gPlaying)) return true;

    playerPlayFirstAudioFile(opts);

    // start sequence timer loop
    sleepTimerLoop(playerHandleNextFrame, playerGetFrameStepTime(opts));

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (audioCheckPlaying())
        ;

    // cleanup resources
    audioStop();

    sequenceFree(&gPlaying);

    framePumpFree(&gFramePump);

    free(gLastFrameData);
    gLastFrameData = NULL;

    return false;
}
