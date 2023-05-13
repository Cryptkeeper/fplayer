#include "player.h"

#include <stdio.h>

#include "audio.h"
#include "seq.h"
#include "serial.h"
#include "sleep.h"

static Sequence gPlaying;

static void playerLogStatus(void) {
    static char bDuration[64];
    sequenceGetDuration(&gPlaying, bDuration, sizeof(bDuration));

    static char bDrift[64];
    sleepGetDrift(bDrift, sizeof(bDrift));

    static char bStatus[256];
    snprintf(bStatus, sizeof(bStatus), "remaining: %s\t\ttime drift: %s",
             bDuration, bDrift);

    printf("\r%s", bStatus);
    fflush(stdout);
}

static bool playerHandleNextFrame(void) {
    if (!sequenceNextFrame(&gPlaying)) return false;

    if (serialWriteFrame(gPlaying.currentFrameData, gPlaying.channelCount))
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

    return gPlaying.frameStepTimeMillis;
}

bool playerInit(PlayerOpts opts) {
    // read and parse sequence file data
    sequenceInit(&gPlaying);

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

    return false;
}
