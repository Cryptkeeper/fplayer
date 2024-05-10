#include "audio.h"

#include <stdio.h>

#include <AL/alut.h>

#include "std2/errcode.h"

static void perror_al(const char* const msg) {
    ALenum err;
    if ((err = alGetError()) != AL_NO_ERROR)
        fprintf(stderr, "%s: OpenAL error 0x%02x\n", msg, err);
}

static void perror_alut(const char* const msg) {
    ALenum err;
    if ((err = alutGetError()) != ALUT_ERROR_NO_ERROR)
        fprintf(stderr, "%s: ALUT error 0x%02x (%s)\n", msg, err,
                alutGetErrorString(err));
}

static struct {
    bool init;
    ALuint sid; /* allocated source id */
    ALuint bid; /* allocated buffer id */
} gAudio;

static int audioInit(void) {
    if (gAudio.init) return FP_EOK;
    gAudio.init = true;

    alutInit(0, NULL);
    if (alutGetError()) return -FP_EALCTL;

    return FP_EOK;
}

static void audioStopPlayback(void) {
    if (!gAudio.init) return;

    ALuint sid;
    if ((sid = gAudio.sid), gAudio.sid = AL_NONE, sid != AL_NONE) {
        alSourceStop(sid);
        alSourcei(sid, AL_BUFFER, AL_NONE);
        alDeleteSources(1, &sid);

        if (alGetError() != AL_NO_ERROR)
            perror_al("error deleting audio source");
    }

    ALuint bid;
    if ((bid = gAudio.bid), gAudio.bid = AL_NONE, bid != AL_NONE) {
        alDeleteBuffers(1, &bid);
        perror_al("error deleting audio buffer");
    }
}

void audioExit(void) {
    bool init;
    if ((init = gAudio.init), gAudio.init = false, init) {
        audioStopPlayback();

        alutExit();
        perror_alut("error while exiting ALUT");
    }
}

bool audioCheckPlaying(void) {
    if (!gAudio.init || gAudio.sid == AL_NONE) return false;

    ALint state;
    alGetSourcei(gAudio.sid, AL_SOURCE_STATE, &state);
    perror_al("error checking audio source state");

    // free resources if playback has stopped
    if (state != AL_PLAYING) audioStopPlayback();

    return state == AL_PLAYING;
}

int audioPlayFile(const char* const fp) {
    if (audioCheckPlaying()) audioStopPlayback();

    // lazy initialize until once an audio playback request is made
    int err;
    if ((err = audioInit())) return err;

    if ((gAudio.bid = alutCreateBufferFromFile(fp)) == AL_NONE) {
        perror_alut("error decoding file into buffer");
        return -FP_EPLAYAUD;
    }

    alGenSources(1, &gAudio.sid);
    alSourcei(gAudio.sid, AL_BUFFER, gAudio.bid);
    alSourcePlay(gAudio.sid);

    if (alGetError() != AL_NO_ERROR) {
        perror_al("error starting audio playback");
        return -FP_EPLAYAUD;
    }

    return FP_EOK;
}
