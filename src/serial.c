#include "serial.h"

#include <stdio.h>
#include <stdlib.h>

#include <libserialport.h>
#include <lightorama/easy.h>
#include <lightorama/lightorama.h>

#include "cmap.h"
#include "err.h"
#include "mem.h"
#include "time.h"

static inline void serialPrintLastError(enum sp_return err) {
    // global error handling is only used with a single super error type
    if (err != SP_ERR_FAIL) return;

    char *msg = NULL;
    if ((msg = sp_last_error_message()) != NULL) {
        fprintf(stderr, "%s\n", msg);

        sp_free_error_message(msg);
    }
}

#define spPrintError(err, msg)                                                 \
    do {                                                                       \
        if ((err) != SP_OK) {                                                  \
            fprintf(stderr, "libserialport error (version %s)\n",              \
                    SP_PACKAGE_VERSION_STRING);                                \
            fprintf(stderr, "0x%02x\n", err);                                  \
                                                                               \
            serialPrintLastError(err);                                         \
                                                                               \
            errPrintTrace(msg);                                                \
        }                                                                      \
    } while (0)

void serialOptsFree(SerialOpts *opts) { freeAndNull((void **) &opts->devName); }

static struct sp_port *gPort;

static bool serialOpenPort(SerialOpts opts) {
    enum sp_return err;

    err = sp_get_port_by_name(opts.devName, &gPort);
    spPrintError(err, "error when getting port by name");
    if (err != SP_OK) return true;

    err = sp_open(gPort, SP_MODE_WRITE);
    spPrintError(err, "error when opening port for writing");
    if (err != SP_OK) return true;

    err = sp_set_baudrate(gPort, opts.baudRate);
    spPrintError(err, "error when setting baud rate");
    if (err != SP_OK) return true;

    err = sp_set_parity(gPort, SP_PARITY_NONE);
    spPrintError(err, "error when setting parity mode");
    if (err != SP_OK) return true;

    err = sp_set_bits(gPort, 8);
    spPrintError(err, "error when setting databits");
    if (err != SP_OK) return true;

    err = sp_set_stopbits(gPort, 1);
    spPrintError(err, "error when setting stopbits");
    if (err != SP_OK) return true;

    return false;
}

bool serialInit(SerialOpts opts) {
    if (opts.devName != NULL) return serialOpenPort(opts);

    return false;
}

static uint8_t gEncodeBuffer[64];

static void serialWriteChannelData(uint32_t id, uint8_t newIntensity) {
    static struct lor_effect_setintensity_t gSetEffect;

    gSetEffect.intensity =
            lor_intensity_curve_vendor((float) newIntensity / 255);

    uint8_t unit;
    uint16_t circuit;
    if (!channelMapFind(id, &unit, &circuit)) return;

    const int written =
            lor_write_channel_effect(LOR_EFFECT_SET_INTENSITY, &gSetEffect,
                                     circuit - 1, unit, gEncodeBuffer);

    if (written > 0) {
        enum sp_return err;
        if ((err = sp_nonblocking_write(gPort, gEncodeBuffer, written)) !=
            SP_OK) {
            spPrintError(err, "error when writing LOR frame");
        }
    }
}

static timeInstant gLastHeartbeat;
static bool gHasSentHeartbeat = false;

static void serialWriteHeartbeat(void) {
    // each frame will request a heartbeat be sent
    // this logic throttles that to every Nms according to a liblightorama magic value
    if (gHasSentHeartbeat) {
        const timeInstant now = timeGetNow();

        if (timeElapsedNs(gLastHeartbeat, now) < LOR_HEARTBEAT_DELAY_NS) {
            return;
        }
    }

    gHasSentHeartbeat = true;
    gLastHeartbeat = timeGetNow();

    const int written = lor_write_heartbeat(gEncodeBuffer);

    enum sp_return err;
    if ((err = sp_nonblocking_write(gPort, gEncodeBuffer, written)) != SP_OK) {
        spPrintError(err, "error when writing LOR heartbeat");
    }
}

bool serialWriteFrame(const uint8_t *currentData, const uint8_t *lastData,
                      uint32_t size) {
    if (gPort == NULL) return false;

    serialWriteHeartbeat();

    for (uint32_t id = 0; id < size; id++) {
        // avoid duplicate writes of the same intensity value
        if (currentData[id] != lastData[id])
            serialWriteChannelData(id, currentData[id]);
    }

    enum sp_return err;
    if ((err = sp_drain(gPort)) != SP_OK) {
        spPrintError(err, "error when waiting for queue drain");

        return true;
    }

    return false;
}

void serialExit(void) {
    if (gPort == NULL) return;

    enum sp_return err;
    if ((err = sp_close(gPort)) != SP_OK) {
        spPrintError(err, "error when closing port");
    }

    sp_free_port(gPort);

    gPort = NULL;
    gHasSentHeartbeat = false;
}
