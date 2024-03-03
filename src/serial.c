#include "serial.h"

#include <assert.h>
#include <stdio.h>

#include <libserialport.h>

#include "stb_ds.h"

#include "protowriter.h"
#include "std/err.h"

static void Serial_printError(const enum sp_return err) {
    fprintf(stderr, "libserialport error: %d\n", err);

    // global error handling is only used with a single super error type
    if (err != SP_ERR_FAIL) return;

    char *msg = NULL;
    if ((msg = sp_last_error_message()) != NULL) {
        fprintf(stderr, "%s\n", msg);

        sp_free_error_message(msg);
    }
}

typedef void (*SerialWriteFn)(const uint8_t *b, size_t size);

static SerialWriteFn gSerialWriteFn;
static struct sp_port *gPort;// !NULL if `gSerialWriteFn == Serial_write_port`

static bool Serial_openPort(const char *const devName, const int baudRate) {
    enum sp_return err;
    if ((err = sp_get_port_by_name(devName, &gPort))) {
        Serial_printError(err);
        return false;
    } else if ((err = sp_open(gPort, SP_MODE_WRITE))) {
        Serial_printError(err);
        return false;
    }

    // smaller errors from configuring the device connection are not fatal since
    // it may likely work anyway or otherwise disregard these values
    sp_set_baudrate(gPort, baudRate);
    sp_set_parity(gPort, SP_PARITY_NONE);
    sp_set_bits(gPort, 8);
    sp_set_stopbits(gPort, 1);

    return true;
}

static void Serial_write_null(__attribute__((unused)) const uint8_t *const b,
                              __attribute__((unused)) const size_t size) {
}

static void Serial_write_stdio(const uint8_t *const b, const size_t size) {
    for (size_t i = 0; i < size; i++) {
        const uint8_t c = b[i];

        if (c == '\0') printf("\n");
        else
            printf("0x%02X ", c);
    }
}

static void Serial_write_port(const uint8_t *const b, const size_t size) {
    enum sp_return err;
    if ((err = sp_nonblocking_write(gPort, b, size))) Serial_printError(err);
}

bool Serial_init(const char *const devName, const int baudRate) {
    if (devName == NULL || strcasecmp(devName, "null") == 0) {
        gSerialWriteFn = Serial_write_null;
        return true;
    }

    if (strcasecmp(devName, "stdout") == 0) {
        gSerialWriteFn = Serial_write_stdio;
        return true;
    }

    gSerialWriteFn = Serial_write_port;
    return Serial_openPort(devName, baudRate);
}

void Serial_write(const uint8_t *const b, const size_t size) {
    assert(gSerialWriteFn != NULL);
    if (gSerialWriteFn) gSerialWriteFn(b, size);
}

void Serial_drain(void) {
    if (gPort == NULL) return;

    const enum sp_return err = sp_drain(gPort);
    if (err) Serial_printError(err);
}

void Serial_close(void) {
    if (gPort == NULL) return;

    sp_close(gPort);
    sp_free_port(gPort);

    gPort = NULL;
}

char **Serial_getPorts(void) {
    struct sp_port **pl;

    // FIXME: exit if sp query fails
    const enum sp_return err = sp_list_ports(&pl);
    if (err) {
        Serial_printError(err);
        return NULL;
    }

    char **ports = NULL;

    // https://github.com/martinling/libserialport/blob/master/examples/list_ports.c#L28
    for (int i = 0; pl[i] != NULL; i++) {
        char *portName = mustStrdup(sp_get_port_name(pl[i]));
        arrpush(ports, portName);
    }

    sp_free_port_list(pl);

    return ports;
}
