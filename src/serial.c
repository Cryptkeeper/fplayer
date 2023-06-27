#include "serial.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <libserialport.h>
#include <lightorama/lightorama.h>

#include "err.h"
#include "lor.h"
#include "mem.h"
#include "minifier.h"
#include "time.h"

static inline void spPrintError(enum sp_return err) {
    fprintf(stderr, "libserialport error: %d\n", err);

    // global error handling is only used with a single super error type
    if (err != SP_ERR_FAIL) return;

    char *msg = NULL;
    if ((msg = sp_last_error_message()) != NULL) {
        fprintf(stderr, "%s\n", msg);

        freeAndNullWith(&msg, sp_free_error_message);
    }
}

#define spTry(fn)                                                              \
    do {                                                                       \
        enum sp_return _err = fn;                                              \
        if (_err != SP_OK) spPrintError(_err);                                 \
    } while (0)

void serialOptsFree(SerialOpts *opts) {
    freeAndNull((void **) &opts->devName);
}

static struct sp_port *gPort;
static bool gStdio;

static void serialOpenPort(SerialOpts opts) {
    spTry(sp_get_port_by_name(opts.devName, &gPort));

    // NULL indicates a failure to open, which is the only seriously "fatal"
    // error that can occur during setup
    if (gPort == NULL)
        fatalf(E_FATAL, "error opening serial port: %s\n", opts.devName);

    // smaller errors from configuring the device connection are not fatal since
    // it may likely work anyway or otherwise disregard these values
    spTry(sp_open(gPort, SP_MODE_WRITE));
    spTry(sp_set_baudrate(gPort, opts.baudRate));
    spTry(sp_set_parity(gPort, SP_PARITY_NONE));
    spTry(sp_set_bits(gPort, 8));
    spTry(sp_set_stopbits(gPort, 1));
}

void serialInit(SerialOpts opts) {
    if (opts.devName == NULL) return;

    if (strcmp(opts.devName, "stdio") == 0) {
        gStdio = true;

        printf("warning: using stdio for LOR protocol output\n");
    } else
        serialOpenPort(opts);
}

static bool serialEnabled(void) {
    return gPort != NULL || gStdio;
}

static void serialWrite(const uint8_t *b, int size) {
    assert(serialEnabled());

    if (gStdio) {
        for (int i = 0; i < size; i++) {
            const uint8_t c = b[i];

            if (c == '\0') printf("\n");
            else
                printf("0x%02X ", c);
        }
    } else {
        spTry(sp_nonblocking_write(gPort, b, size));
    }
}

void serialWriteHeartbeat(void) {
    if (!serialEnabled()) return;

    bufadv(lor_write_heartbeat(bufhead()));

    bufflush(false, serialWrite);
}

static void serialWriteThrottledHeartbeat(void) {
    if (!serialEnabled()) return;

    static timeInstant gLastHeartbeat;

    // each frame will request a heartbeat be sent
    // this logic throttles that to every Nms according to a liblightorama magic value
    const timeInstant now = timeGetNow();

    if (timeElapsedNs(gLastHeartbeat, now) < LOR_HEARTBEAT_DELAY_NS) return;

    gLastHeartbeat = now;

    serialWriteHeartbeat();
}

void serialWriteFrame(const uint8_t *frameData,
                      const uint8_t *lastFrameData,
                      uint32_t size) {
    if (!serialEnabled()) return;

    serialWriteThrottledHeartbeat();

    minifyStream(frameData, lastFrameData, size, serialWrite);

    // ensure any written LOR packets are flushed
    bufflush(true, serialWrite);

    if (gPort != NULL) spTry(sp_drain(gPort));
}

static void serialPortFree(struct sp_port *port) {
    spTry(sp_close(port));

    sp_free_port(port);
}

void serialExit(void) {
    freeAndNullWith(&gPort, serialPortFree);
}
