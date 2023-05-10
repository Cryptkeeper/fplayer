#include "audio.h"

int main(int argc, char **argv) {
    audioInit(&argc, argv);

    audioPlayFile("../The Crypt Jam.wav");

    while (audioCheckPlaying())
        ;

    audioExit();

    return 0;
}
