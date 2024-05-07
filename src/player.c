#include "player.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lorproto/easy.h"
#include "lorproto/heartbeat.h"
#include "stb_ds.h"

#include "audio.h"
#include "cmap.h"
#include "fseq/comblock.h"
#include "lor/protowriter.h"
#include "pump.h"
#include "seq.h"
#include "serial.h"
#include "std/err.h"
#include "std2/sleep.h"
#include "std2/string.h"
#include "std2/time.h"
#include "transform/minifier.h"
#include "transform/netstats.h"
#include "transform/precompute.h"

#ifdef _WIN32
    #include <windows.h>
#endif

static FramePump gFramePump;

static uint32_t gNextFrame;

static uint8_t* gLastFrameData;

// LOR hardware may require several heartbeat messages are sent
// before it considers itself connected to the player
// This artificially waits prior to starting playback to ensure the device is
// considered connected and ready for frame data
static void playerWaitForConnection(const unsigned int seconds) {
    if (seconds == 0) return;

    printf("waiting %u seconds for connection...\n", seconds);

    // generate a single heartbeat message to repeatedly send
    LorBuffer* msg = LB_alloc();
    if (msg == NULL) fatalf(E_SYS, NULL);

    lorAppendHeartbeat(msg);

    // assumes 2 heartbeat messages per second (500ms delay)
    for (unsigned int toSend = seconds * 2; toSend > 0; toSend--) {
        Serial_write(msg->buffer, msg->offset);

#ifdef _WIN32
        Sleep(LOR_HEARTBEAT_DELAY_MS);
#else
        const struct timespec itrSleep = {
                .tv_sec = 0,
                .tv_nsec = LOR_HEARTBEAT_DELAY_NS,
        };

        nanosleep(&itrSleep, NULL);
#endif
    }

    LB_free(msg);
}

static void playerPlayFirstAudioFile(const char* const override,
                                     const char* const sequence) {
    // select the override, if set, otherwise fallback to the sequence's hint
    const char* const priority = override != NULL ? override : sequence;

    if (priority != NULL) {
        printf("preparing to play %s\n", priority);

        audioPlayFile(priority);
    } else {
        printf("no audio file detected using override or via sequence\n");
    }
}

static char* playerGetRemaining(void) {
    const uint32_t framesRemaining = curSequence.frameCount - gNextFrame;
    const long seconds =
            framesRemaining / (1000 / curSequence.frameStepTimeMillis);

    return dsprintf("%02ldm %02lds", seconds / 60, seconds % 60);
}

static void playerLogStatus(const struct sleep_coll_s* coll) {
    static timeInstant gLastLog;

    const timeInstant now = timeGetNow();
    if (timeElapsedNs(gLastLog, now) < 1000000000) return;

    gLastLog = now;

    const double ms = (double) Sleep_average(coll) / 1e6;
    const double fps = ms > 0 ? 1000 / ms : 0;
    char* const sleep = dsprintf("%.4fms (%.2f fps)", ms, fps);

    char* const remaining = playerGetRemaining();
    char* const netstats = nsGetStatus();

    printf("remaining: %s\tdt: %s\tpump: %4d\t%s\n", remaining, sleep,
           framePumpGetRemaining(&gFramePump), netstats);

    free(remaining);
    free(sleep);
    free(netstats);
}

static void playerHandleNextFrame(struct FC* fc) {
    assert(gNextFrame < curSequence.frameCount);

    const uint32_t frameSize = curSequence.channelCount;

    if (gLastFrameData == NULL) {
        gLastFrameData = mustMalloc(frameSize);

        // zero out the array to represent all existing intensity values as off
        memset(gLastFrameData, 0, frameSize);
    }

    const uint32_t frame = gNextFrame++;

    // send a heartbeat if it has been >500ms
    static timeInstant lastHeartbeat;

    if (timeElapsedNs(lastHeartbeat, timeGetNow()) > LOR_HEARTBEAT_DELAY_NS) {
        lastHeartbeat = timeGetNow();

        LorBuffer* msg = LB_alloc();
        if (msg == NULL) fatalf(E_SYS, NULL);

        lorAppendHeartbeat(msg);
        Serial_write(msg->buffer, msg->offset);

        LB_free(msg);
    }

    // fetch the current frame data
    const uint8_t* const frameData = framePumpGet(fc, &gFramePump, frame, true);

    minifyStream(frameData, gLastFrameData, frameSize, frame);

    // wait for serial to drain outbound
    // this creates back pressure that results in fps loss if the serial can't keep up
    Serial_drain();

    // copy previous frame to the secondary frame buffer
    // this enables the serial system to diff between the two frames and only
    // write outgoing state changes
    memcpy(gLastFrameData, frameData, frameSize);
}

static void playerTurnOffAllLights(void) {
    uint8_t* uids = channelMapGetUids();

    LorBuffer* msg = LB_alloc();
    if (msg == NULL) fatalf(E_SYS, NULL);

    for (size_t i = 0; i < arrlenu(uids); i++) {
        lorAppendUnitEffect(msg, LOR_EFFECT_SET_OFF, NULL, uids[i]);
        Serial_write(msg->buffer, msg->offset);
        LB_rewind(msg);
    }

    LB_free(msg);

    arrfree(uids);
}

static void playerStartPlayback(struct FC* fc) {
    // iterate over all frames, sleeping the appropriate time between each output
    struct sleep_coll_s coll = {0};
    while (gNextFrame < curSequence.frameCount) {
        Sleep_do(&coll, curSequence.frameStepTimeMillis);

        playerHandleNextFrame(fc);
        playerLogStatus(&coll);
    }

    printf("turning off lights, waiting for end of audio...\n");

    playerTurnOffAllLights();

    // continue blocking until audio is finished
    // playback will continue until sequence and audio are both complete
    while (audioCheckPlaying())
        ;

    printf("end of sequence!\n");

    // print closing remarks
    char* const netstats = nsGetSummary();

    printf("%s\n", netstats);
    free(netstats);
}

static void playerFree(void) {
    free(gLastFrameData);

    gLastFrameData = NULL;
    gNextFrame = 0;
}

void playerRun(struct FC* fc,
               const char* const audioOverrideFilePath,
               const PlayerOpts opts) {
    Seq_initHeader(fc);

    if (opts.precomputeFades) {
        char* const cacheFilePath = dsprintf("%s.pcf", FC_filepath(fc));

        // load existing data or precompute and save new data
        precomputeRun(cacheFilePath, fc);

        free(cacheFilePath);
    }

    playerWaitForConnection(opts.connectionWaitS);

    char* audioFilePath = Seq_getMediaFile(fc);
    playerPlayFirstAudioFile(audioOverrideFilePath, audioFilePath);
    free(audioFilePath);// only needed to init playback

    // optionally override the sequence's playback rate with the CLI's value
    if (opts.frameStepTimeOverrideMs > 0)
        curSequence.frameStepTimeMillis = opts.frameStepTimeOverrideMs;

    playerStartPlayback(fc);

    // playback finished, free resources and exit cleanly
    precomputeFree();

    framePumpFree(&gFramePump);

    playerFree();
}
