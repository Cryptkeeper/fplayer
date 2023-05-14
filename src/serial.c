#include "serial.h"

#include <stdio.h>

#include <libserialport.h>
#include <lightorama/easy.h>
#include <lightorama/lightorama.h>

#include "cmap.h"
#include "time.h"

#define spPrintError(err, msg)                                                 \
    do {                                                                       \
        if (err != SP_OK) {                                                    \
            fprintf(stderr, "libserialport error (version %s)\n",              \
                    SP_PACKAGE_VERSION_STRING);                                \
            fprintf(stderr, "0x%02x\n", err);                                  \
                                                                               \
            if (err == SP_ERR_FAIL) {                                          \
                char *lastErrMsg = NULL;                                       \
                                                                               \
                if ((lastErrMsg = sp_last_error_message()) != NULL) {          \
                    fprintf(stderr, "%s\n", lastErrMsg);                       \
                                                                               \
                    sp_free_error_message(lastErrMsg);                         \
                }                                                              \
            }                                                                  \
                                                                               \
            fprintf(stderr, "%s\n", msg);                                      \
            fprintf(stderr, "%s#L%d\n", __FILE_NAME__, __LINE__ - 1);          \
        }                                                                      \
    } while (0)

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
    ChannelData *data;
    if (!channelMapFind(id, &unit, &circuit, &data)) return;

    // avoid duplicate writes of the same intensity value
    if (data->intensity == newIntensity) return;
    data->intensity = newIntensity;

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

bool serialWriteFrame(const uint8_t *b, uint32_t size) {
    if (gPort == NULL) return false;

    serialWriteHeartbeat();

    for (uint32_t id = 0; id < size; id++) {
        serialWriteChannelData(id, b[id]);
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
