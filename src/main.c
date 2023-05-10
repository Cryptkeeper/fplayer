#include "audio.h"
#include "seq.h"
#include "sleep.h"

static Sequence seq;

static bool next_frame(void) {
    if (!sequenceNextFrame(&seq)) return false;

    printf("%d - %d\n", seq.currentFrame, seq.frameCount);

    return true;
}

int main(int argc, char **argv) {
    /*audioInit(&argc, argv);

    audioPlayFile("../The Crypt Jam.wav");

    while (audioCheckPlaying())
        ;

    audioExit();*/

    sequenceInit(&seq);

    if (sequenceOpen("../test.fseq", &seq)) return 1;

    printf("%s\n", seq.audioFilePath);

    sleepTimerLoop(next_frame, seq.frameStepTimeMillis);

    sequenceFree(&seq);

    return 0;
}
