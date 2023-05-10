#include "audio.h"
#include "seq.h"

int main(int argc, char **argv) {
    /*audioInit(&argc, argv);

    audioPlayFile("../The Crypt Jam.wav");

    while (audioCheckPlaying())
        ;

    audioExit();*/

    Sequence seq;
    sequenceInit(&seq);

    if (sequenceOpen("../test.fseq", &seq)) return 1;

    printf("%s\n", seq.audioFilePath);

    while (sequenceNextFrame(&seq)) {
        printf("%d - %d\n", seq.currentFrame, seq.frameCount);
    }

    sequenceFree(&seq);

    return 0;
}
