#include "audio.h"

bool gAudioIgnoreErrors = false;

#ifdef ENABLE_OPENAL

#include <stdio.h>

#include <AL/alut.h>

#include "std/err.h"

static inline void alCheckError(const char *const msg) {
    ALenum err;
    if ((err = alGetError()) == AL_NO_ERROR) return;

    if (gAudioIgnoreErrors) {
        fprintf(stderr, "%s: OpenAL error 0x%02x\n", msg, err);
    } else {
        fatalf(E_FATAL, "%s: OpenAL error 0x%02x\n", msg, err);
    }
}

static inline void alutCheckError(const char *const msg) {
    ALenum err;
    if ((err = alutGetError()) == ALUT_ERROR_NO_ERROR) return;
    
    if (gAudioIgnoreErrors) {
        fprintf(stderr, "%s: ALUT error 0x%02x (%s)\n", msg, err,
                alutGetErrorString(err));
    } else {
        fatalf(E_FATAL, "%s: ALUT error 0x%02x (%s)\n", msg, err,
               alutGetErrorString(err));
    }
}

static ALuint gSource;
static ALuint gCurrentBuffer = AL_NONE;

void audioInit(int *const argc, char **const argv) {
    alutInit(argc, argv);
    alutCheckError("error initializing ALUT");

    alGenSources(1, &gSource);
    alCheckError("error generating default audio source");
}

static void audioStop(void) {
    alSourceStop(gSource);
    alCheckError("error stopping audio source playback");

    alSourcei(gSource, AL_BUFFER, AL_NONE);
    alCheckError("error clearing source buffer assignment");

    if (gCurrentBuffer == AL_NONE) return;

    alDeleteBuffers(1, &gCurrentBuffer);
    alCheckError("error deleting audio buffer");

    gCurrentBuffer = AL_NONE;
}

void audioExit(void) {
    alutExit();
    alutCheckError("error while exiting ALUT");
}

bool audioCheckPlaying(void) {
    ALint state;
    alGetSourcei(gSource, AL_SOURCE_STATE, &state);
    alCheckError("error checking audio source state");

    if (state != AL_PLAYING) audioStop();

    return state == AL_PLAYING;
}

void audioPlayFile(const char *const filepath) {
    gCurrentBuffer = alutCreateBufferFromFile(filepath);
    alutCheckError("error decoding file into buffer");

    alSourcei(gSource, AL_BUFFER, (ALint) gCurrentBuffer);
    alCheckError("error assigning source buffer");

    alSourcePlay(gSource);
    alCheckError("error starting audio source playback");

    // test for any playback failure
    // unload audio system since it can't be used
    if (!audioCheckPlaying()) audioExit();
}

#else

void audioInit(int *const argc, char **const argv) {
}

void audioExit(void) {
}

bool audioCheckPlaying(void) {
    return false;
}

void audioPlayFile(const char *const filepath) {
}

#endif
