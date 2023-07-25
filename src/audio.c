#include "audio.h"

#include <assert.h>
#include <stdio.h>

#ifdef ENABLE_OPENAL
#include <AL/alut.h>

static inline void alPrintError(const char *const msg) {
    ALenum err;
    if ((err = alGetError()) == AL_NO_ERROR) return;

    fprintf(stderr, "OpenAL error: 0x%02x\n", err);
    fprintf(stderr, "%s\n", msg);
}

static inline void alutPrintError(const char *const msg) {
    ALenum err;
    if ((err = alutGetError()) == ALUT_ERROR_NO_ERROR) return;

    fprintf(stderr, "ALUT error: %s (0x%02x)\n", alutGetErrorString(err), err);
    fprintf(stderr, "%s\n", msg);
}

static ALuint gSource;
static ALuint gCurrentBuffer = AL_NONE;

static bool gAudioInit;
#endif

void audioInit(int *const argc, char **const argv) {
#ifdef ENABLE_OPENAL
    assert(!gAudioInit);

    alutInit(argc, argv);
    alutPrintError("error while initializing ALUT");

    alGenSources(1, &gSource);
    alPrintError("error generating default audio source");

    gAudioInit = true;
#endif
}

void audioExit(void) {
    audioStop();

#ifdef ENABLE_OPENAL
    if (!gAudioInit) return;

    alutExit();
    alutPrintError("error while exiting ALUT");

    gAudioInit = false;
#endif
}

bool audioCheckPlaying(void) {
#ifdef ENABLE_OPENAL
    if (!gAudioInit) return false;

    ALint state;
    alGetSourcei(gSource, AL_SOURCE_STATE, &state);
    alPrintError("error checking audio source state");

    if (state != AL_PLAYING) audioStop();

    return state == AL_PLAYING;
#else
    return false;
#endif
}

void audioPlayFile(const char *const filepath) {
#ifdef ENABLE_OPENAL
    gCurrentBuffer = alutCreateBufferFromFile(filepath);
    alutPrintError("error decoding file into buffer");

    alSourcei(gSource, AL_BUFFER, (ALint) gCurrentBuffer);
    alPrintError("error assigning source buffer");

    alSourcePlay(gSource);
    alPrintError("error starting audio source playback");

    // test for any playback failure
    // unload audio system since it can't be used
    if (!audioCheckPlaying()) audioExit();
#endif
}

void audioStop(void) {
#ifdef ENABLE_OPENAL
    if (!gAudioInit) return;

    alSourceStop(gSource);
    alPrintError("error stopping audio source playback");

    if (gCurrentBuffer == AL_NONE) return;

    alSourceUnqueueBuffers(gSource, 1, &gCurrentBuffer);
    alPrintError("error dequeuing audio buffer from source");

    alDeleteBuffers(1, &gCurrentBuffer);
    alPrintError("error deleting audio buffer");

    gCurrentBuffer = AL_NONE;
#endif
}
