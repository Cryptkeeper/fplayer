#include "audio.h"

#include <assert.h>
#include <stdio.h>

#include <AL/alut.h>

static void alCheckError(const char *const msg) {
    ALenum err;
    if ((err = alGetError()) == AL_NO_ERROR) return;

    fprintf(stderr, "%s: OpenAL error 0x%02x\n", msg, err);
}

static void alutCheckError(const char *const msg) {
    ALenum err;
    if ((err = alutGetError()) == ALUT_ERROR_NO_ERROR) return;

    fprintf(stderr, "%s: ALUT error 0x%02x (%s)\n", msg, err,
            alutGetErrorString(err));
}

static ALuint gSource = AL_NONE;
static ALuint gCurrentBuffer = AL_NONE;

static bool gIsInit = false;

static void audioInit(void) {
    assert(!gIsInit);
    if (gIsInit) return;

    gIsInit = true;

    alutInit(0, NULL);
    alutCheckError("error initializing ALUT");
}

static void audioStop(void) {
    if (gSource != AL_NONE) {
        alSourceStop(gSource);
        alCheckError("error stopping audio source playback");

        alSourcei(gSource, AL_BUFFER, AL_NONE);
        alCheckError("error clearing source buffer assignment");

        alDeleteSources(1, &gSource);
        alCheckError("error deleting default audio source");

        gSource = AL_NONE;
    }

    if (gCurrentBuffer != AL_NONE) {
        alDeleteBuffers(1, &gCurrentBuffer);
        alCheckError("error deleting audio buffer");

        gCurrentBuffer = AL_NONE;
    }
}

void audioExit(void) {
    if (!gIsInit) return;
    gIsInit = false;

    audioStop();

    alutExit();
    alutCheckError("error while exiting ALUT");
}

bool audioCheckPlaying(void) {
    if (gSource == AL_NONE) return false;

    ALint state;
    alGetSourcei(gSource, AL_SOURCE_STATE, &state);
    alCheckError("error checking audio source state");

    if (state != AL_PLAYING) audioStop();

    return state == AL_PLAYING;
}

void audioPlayFile(const char *const filepath) {
    // lazy initialize until once an audio playback request is made
    if (!gIsInit) audioInit();

    gCurrentBuffer = alutCreateBufferFromFile(filepath);
    alutCheckError("error decoding file into buffer");

    if (gCurrentBuffer == AL_NONE) return;

    assert(gSource == AL_NONE);

    alGenSources(1, &gSource);
    alCheckError("error generating default audio source");

    alSourcei(gSource, AL_BUFFER, gCurrentBuffer);
    alCheckError("error assigning source buffer");

    alSourcePlay(gSource);
    alCheckError("error starting audio source playback");
}
