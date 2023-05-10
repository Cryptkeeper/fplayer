#include "player.h"

#include <stdio.h>

#include "audio.h"
#include "seq.h"
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

    // TODO: handle frame data

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
    audioExit();

    sequenceFree(&gPlaying);

    return false;
}
