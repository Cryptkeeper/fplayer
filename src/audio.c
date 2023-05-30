#include "audio.h"

#include <stdio.h>

#include <AL/alut.h>

static inline void alPrintError(const char *msg) {
    ALenum err;
    if ((err = alGetError()) == AL_NO_ERROR) return;

    fprintf(stderr, "OpenAL error (version 0x%04x)\n", AL_VERSION);
    fprintf(stderr, "0x%02x\n", err);
    fprintf(stderr, "%s\n", msg);
}

static inline void alutPrintError(const char *msg) {
    ALenum err;
    if ((err = alutGetError()) == ALUT_ERROR_NO_ERROR) return;

    fprintf(stderr, "ALUT error (version %d.%d)\n", alutGetMajorVersion(),
            alutGetMinorVersion());
    fprintf(stderr, "%s (0x%02x)\n", alutGetErrorString(err), err);
    fprintf(stderr, "%s\n", msg);
}

static ALuint alSource;
static ALuint alBuffer = AL_NONE;

void audioInit(int *argc, char **argv) {
    alutInit(argc, argv);
    alutPrintError("error when initializing ALUT");

    alGenSources(1, &alSource);
    alPrintError("error generating default audio source");
}

void audioExit(void) {
    audioStop();

    alutExit();
    alutPrintError("error when exiting ALUT");
}

bool audioCheckPlaying(void) {
    ALint state;
    alGetSourcei(alSource, AL_SOURCE_STATE, &state);
    alPrintError("error checking audio source state");

    if (state != AL_PLAYING) {
        audioStop();

        return false;
    } else {
        return true;
    }
}

void audioPlayFile(const char *filepath) {
    alBuffer = alutCreateBufferFromFile(filepath);
    alutPrintError("error decoding file into buffer");

    alSourcei(alSource, AL_BUFFER, (ALint) alBuffer);
    alPrintError("error assigning source buffer");

    alSourcePlay(alSource);
    alPrintError("error starting audio source playback");
}

void audioStop(void) {
    alSourceStop(alSource);
    alPrintError("error stopping audio source playback");

    if (alBuffer != AL_NONE) {
        alSourceUnqueueBuffers(alSource, 1, &alBuffer);
        alPrintError("error dequeuing audio buffer from source");

        alDeleteBuffers(1, &alBuffer);
        alPrintError("error deleting audio buffer");

        alBuffer = AL_NONE;
    }
}
