#include "audio.h"

bool gAudioIgnoreErrors = false;

#ifdef ENABLE_OPENAL

    #include <assert.h>
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

static ALuint gSource = AL_NONE;
static ALuint gCurrentBuffer = AL_NONE;

void audioInit(int *const argc, char **const argv) {
    alutInit(argc, argv);
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
    audioStop();

    alutExit();
    alutCheckError("error while exiting ALUT");
}

bool audioCheckPlaying(void) {
    assert(gSource != AL_NONE);

    ALint state;
    alGetSourcei(gSource, AL_SOURCE_STATE, &state);
    alCheckError("error checking audio source state");

    if (state != AL_PLAYING) audioStop();

    return state == AL_PLAYING;
}

void audioPlayFile(const char *const filepath) {
    gCurrentBuffer = alutCreateBufferFromFile(filepath);
    alutCheckError("error decoding file into buffer");

    if (gCurrentBuffer == AL_NONE) return;

    assert(gSource == AL_NONE);

    alGenSources(1, &gSource);
    alCheckError("error generating default audio source");

    alSourcei(gSource, AL_BUFFER, (ALint) gCurrentBuffer);
    alCheckError("error assigning source buffer");

    alSourcePlay(gSource);
    alCheckError("error starting audio source playback");
}

#else

void audioInit(int *const argc, char **const argv) {
    (void) argc;
    (void) argv;
}

void audioExit(void) {
}

bool audioCheckPlaying(void) {
    return false;
}

void audioPlayFile(const char *const filepath) {
    (void) filepath;
}

#endif
