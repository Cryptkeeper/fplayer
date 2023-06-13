#include "audio.h"

#include <stdio.h>

#include <AL/alut.h>

static inline void alPrintError(const char *msg) {
    ALenum err;
    if ((err = alGetError()) == AL_NO_ERROR) return;

    fprintf(stderr, "OpenAL error: 0x%02x\n", err);
    fprintf(stderr, "%s\n", msg);
}

static inline void alutPrintError(const char *msg) {
    ALenum err;
    if ((err = alutGetError()) == ALUT_ERROR_NO_ERROR) return;

    fprintf(stderr, "ALUT error: %s (0x%02x)\n", alutGetErrorString(err), err);
    fprintf(stderr, "%s\n", msg);
}

static ALuint gSource;
static ALuint gCurrentBuffer = AL_NONE;

void audioInit(int *argc, char **argv) {
    alutInit(argc, argv);
    alutPrintError("error while initializing ALUT");

    alGenSources(1, &gSource);
    alPrintError("error generating default audio source");
}

void audioExit(void) {
    audioStop();

    alutExit();
    alutPrintError("error while exiting ALUT");
}

bool audioCheckPlaying(void) {
    ALint state;
    alGetSourcei(gSource, AL_SOURCE_STATE, &state);
    alPrintError("error checking audio source state");

    if (state != AL_PLAYING) audioStop();

    return state == AL_PLAYING;
}

void audioPlayFile(const char *filepath) {
    gCurrentBuffer = alutCreateBufferFromFile(filepath);
    alutPrintError("error decoding file into buffer");

    alSourcei(gSource, AL_BUFFER, (ALint) gCurrentBuffer);
    alPrintError("error assigning source buffer");

    alSourcePlay(gSource);
    alPrintError("error starting audio source playback");
}

void audioStop(void) {
    alSourceStop(gSource);
    alPrintError("error stopping audio source playback");

    if (gCurrentBuffer == AL_NONE) return;

    alSourceUnqueueBuffers(gSource, 1, &gCurrentBuffer);
    alPrintError("error dequeuing audio buffer from source");

    alDeleteBuffers(1, &gCurrentBuffer);
    alPrintError("error deleting audio buffer");

    gCurrentBuffer = AL_NONE;
}
