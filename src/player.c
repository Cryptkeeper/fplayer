#include "player.h"

#include <stdio.h>

#include "audio.h"
#include "seq.h"
#include "serial.h"
#include "sleep.h"

static char *playerGetAudioFile(PlayerOpts opts, Sequence *seq) {
    if (opts.audioOverrideFilePath != NULL) {
        return opts.audioOverrideFilePath;
    } else if (seq->audioFilePath != NULL) {
        return seq->audioFilePath;
    } else {
        return NULL;
    }
}

static Sequence gPlaying;

static bool playerHandleNextFrame(void) {
    if (!sequenceNextFrame(&gPlaying)) return false;

    static char gDurationBuf[64];
    sequenceGetDuration(&gPlaying, gDurationBuf, sizeof(gDurationBuf));

    static char gLatencyBuf[64];
    sleepGetDrift(gLatencyBuf, sizeof(gLatencyBuf));

    static char gStatusBuf[256];
    snprintf(gStatusBuf, sizeof(gStatusBuf), "remaining: %s\t\ttime drift: %s",
             gDurationBuf, gLatencyBuf);

    printf("\r%s", gStatusBuf);
    fflush(stdout);

    if (serialWriteFrame(gPlaying.currentFrameData, gPlaying.channelCount))
        return false;

    // ...?

    return true;
}

bool playerInit(PlayerOpts opts) {
    // read and parse sequence file data
    sequenceInit(&gPlaying);

    if (sequenceOpen(opts.sequenceFilePath, &gPlaying)) return true;

    // select best audio file and play
    const char *audioFilePath = playerGetAudioFile(opts, &gPlaying);

    if (audioFilePath != NULL) {
        printf("preparing to play %s\n", audioFilePath);

        audioPlayFile(audioFilePath);
    } else {
        printf("no audio file detected using override or via sequence\n");
    }

    // start sequence timer loop
    sleepTimerLoop(playerHandleNextFrame, gPlaying.frameStepTimeMillis);

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (audioCheckPlaying())
        ;

    // cleanup resources
    audioStop();

    sequenceFree(&gPlaying);

    return false;
}
