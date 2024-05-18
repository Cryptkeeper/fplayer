#include "serial.h"

#include <assert.h>
#include <stdio.h>
#include <strings.h>

#include <libserialport.h>

#include <std2/errcode.h>

/// @brief Prints an error message to stderr for the given error code, including
/// the libserialport error message if available.
/// @param err the error code to print the message for
static void Serial_printError(const enum sp_return err) {
    fprintf(stderr, "libserialport error: %d\n", err);

    // global error handling is only used with a single super error type
    if (err != SP_ERR_FAIL) return;

    char* msg = NULL;
    if ((msg = sp_last_error_message()) != NULL) {
        fprintf(stderr, "%s\n", msg);
        sp_free_error_message(msg);
    }
}

typedef void (*SerialWriteFn)(const uint8_t* b, unsigned long size);

static SerialWriteFn gSerialWriteFn;
static struct sp_port* gPort;// !NULL if `gSerialWriteFn == Serial_write_port`

/// @brief Opens the serial port with the given device name and baud rate.
/// @param devName the device name to open
/// @param baudRate the baud rate to configure the device connection with
/// @return 0 on success, a negative error code on failure
static int Serial_openPort(const char* const devName, const int baudRate) {
    enum sp_return err;
    if ((err = sp_get_port_by_name(devName, &gPort))) {
        Serial_printError(err);
        return -FP_ENODEV;
    } else if ((err = sp_open(gPort, SP_MODE_WRITE))) {
        Serial_printError(err);
        return -FP_EDEVCONF;
    }

    // smaller errors from configuring the device connection are not fatal since
    // it may likely work anyway or otherwise disregard these values
    sp_set_baudrate(gPort, baudRate);
    sp_set_parity(gPort, SP_PARITY_NONE);
    sp_set_bits(gPort, 8);
    sp_set_stopbits(gPort, 1);

    return FP_EOK;
}

static void Serial_write_null(const uint8_t* const b,
                              const unsigned long size) {
    (void) b, (void) size;
}

static void Serial_write_stdio(const uint8_t* const b,
                               const unsigned long size) {
    for (unsigned long i = 0; i < size; i++) {
        if (b[i] == '\0') putchar('\n');
        else
            printf("0x%02X ", b[i]);
    }
}

static void Serial_write_port(const uint8_t* const b,
                              const unsigned long size) {
    enum sp_return err;
    if ((err = sp_nonblocking_write(gPort, b, size))) Serial_printError(err);
}

int Serial_init(const char* const devName, const int baudRate) {
    if (devName == NULL || strcasecmp(devName, "null") == 0) {
        gSerialWriteFn = Serial_write_null;
        return FP_EOK;
    }

    if (strcasecmp(devName, "stdout") == 0) {
        gSerialWriteFn = Serial_write_stdio;
        return FP_EOK;
    }

    gSerialWriteFn = Serial_write_port;
    return Serial_openPort(devName, baudRate);
}

void Serial_write(const uint8_t* const b, const unsigned long size) {
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

slist_t* Serial_getPorts(void) {
    struct sp_port** pl;
    if (sp_list_ports(&pl) != SP_OK) return NULL;

    slist_t* ports = NULL;
    for (int i = 0; pl[i] != NULL; i++) {
        char* portName = sp_get_port_name(pl[i]);
        if (portName == NULL) portName = "(null?)";
        if (sladd(&ports, portName) < 0) {
            slfree(ports), ports = NULL;
            break;
        }
    }

    sp_free_port_list(pl);
    return ports;
}
