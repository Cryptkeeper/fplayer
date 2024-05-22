#include "putil.h"

#include <stdio.h>

#include <lorproto/easy.h>
#include <lorproto/effect.h>
#include <lorproto/heartbeat.h>
#include <lorproto/uid.h>
#include <tinyfseq.h>

#include "seq.h"
#include "serial.h"
#include <lor/protowriter.h>
#include <std2/errcode.h>
#include <std2/string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

int PU_wait(const unsigned int seconds) {
    // LOR hardware may require several heartbeat messages are sent
    // before it considers itself connected to the player
    // This artificially waits prior to starting playback to ensure the device is
    // considered connected and ready for frame data
    if (seconds == 0) return FP_EOK;

    printf("waiting %u seconds for connection...\n", seconds);

    // generate a single heartbeat message to repeatedly send
    LorBuffer* msg;
    if ((msg = LB_alloc()) == NULL) return -FP_ENOMEM;

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

    return FP_EOK;
}

int PU_lightsOff(void) {
    LorBuffer* msg;
    if ((msg = LB_alloc()) == NULL) return -FP_ENOMEM;

    for (uint8_t u = LOR_UNIT_MIN; u <= LOR_UNIT_MAX; u++) {
        lorAppendUnitEffect(msg, LOR_EFFECT_SET_OFF, NULL, u);
        Serial_write(msg->buffer, msg->offset);
        LB_rewind(msg);
    }

    LB_free(msg);

    return FP_EOK;
}

char* PU_timeRemaining(const uint32_t frame) {
    if (curSequence->frameCount < frame) return NULL;

    const uint32_t framesRemaining = curSequence->frameCount - frame;
    const long seconds =
            framesRemaining / (1000 / curSequence->frameStepTimeMillis);

    return dsprintf("%02ldm %02lds", seconds / 60, seconds % 60);
}

int PU_doHeartbeat(timeInstant* last) {
    const timeInstant now = timeGetNow();
    if (timeElapsedNs(*last, now) < LOR_HEARTBEAT_DELAY_NS) return FP_EOK;
    *last = now;

    LorBuffer* msg = LB_alloc();
    if (msg == NULL) return -FP_ENOMEM;

    lorAppendHeartbeat(msg);
    Serial_write(msg->buffer, msg->offset);
    LB_free(msg);

    return FP_EOK;
}
