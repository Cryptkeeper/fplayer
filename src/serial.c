#include "serial.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <libserialport.h>

#include "std2/errcode.h"

struct serialdev_s {
    _Bool virtual : 1;  /* if true, output is written to `vfile` */
    _Bool real : 1;     /* if true, output is written to `rport` */
    _Bool silenced : 1; /* if true, output is discarded */
    union {
        FILE* vfile;           /* virtual file handle */
        struct sp_port* rport; /* real serial port handle */
    } dev;
};

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

/// @brief Opens the serial port with the given device name and baud rate.
/// @param devName the device name to open
/// @param baudRate the baud rate to configure the device connection with
/// @return 0 on success, a negative error code on failure
static int Serial_openPort(struct serialdev_s** sdev,
                           const char* const devName,
                           const int baudRate) {
    assert(sdev != NULL);

    enum sp_return err;
    if ((err = sp_get_port_by_name(devName, &(*sdev)->dev.rport))) {
        Serial_printError(err);
        return -FP_ENODEV;
    } else if ((err = sp_open((*sdev)->dev.rport, SP_MODE_WRITE))) {
        Serial_printError(err);
        return -FP_EDEVCONF;
    }

    // smaller errors from configuring the device connection are not fatal since
    // it may likely work anyway or otherwise disregard these values
    struct sp_port* port = (*sdev)->dev.rport;

    sp_set_baudrate(port, baudRate);
    sp_set_parity(port, SP_PARITY_NONE);
    sp_set_bits(port, 8);
    sp_set_stopbits(port, 1);

    return FP_EOK;
}

int Serial_init(struct serialdev_s** const sdev,
                const char* const devName,
                const int baudRate) {
    assert(sdev != NULL);

    if ((*sdev = calloc(1, sizeof(struct serialdev_s))) == NULL)
        return -FP_ENOMEM;

    int err;
    if (devName == NULL || strcasecmp(devName, "null") == 0) {
        (*sdev)->silenced = 1;
    } else if (strcasecmp(devName, "stdout") == 0) {
        (*sdev)->virtual = 1;
        (*sdev)->dev.vfile = stdout;
    } else if (!(err = Serial_openPort(sdev, devName, baudRate))) {
        (*sdev)->real = 1;
    } else {
        free(*sdev), *sdev = NULL;
        return err;
    }

    return FP_EOK;
}

/// @brief Writes the binary data to the virtual file handle within the virtual
/// serial device structure.
/// @param sdev the virtual serial device structure
/// @param b binary data to write
/// @param size size of the binary data to write
/// @note This function is only called if the virtual device is `->virtual`.
static inline void Serial_writeVirtual(struct serialdev_s* const sdev,
                                       const uint8_t* const b,
                                       const unsigned long size) {
    assert(sdev != NULL);
    assert(b != NULL);
    assert(size > 0);
    assert(sdev->dev.vfile != NULL);

    for (unsigned long i = 0; i < size; i++) {
        if (b[i] == '\0') fputc('\n', sdev->dev.vfile);
        else
            fprintf(sdev->dev.vfile, "0x%02X ", b[i]);
    }
}

/// @brief Writes the binary data to the "real" opened serial port handle within
/// the virtual serial device structure.
/// @param sdev the virtual serial device structure
/// @param b binary data to write
/// @param size size of the binary data to write
/// @note This function is only called if the virtual device is `->real`.
static inline void Serial_writeReal(struct serialdev_s* const sdev,
                                    const uint8_t* const b,
                                    const unsigned long size) {
    assert(sdev != NULL);
    assert(b != NULL);
    assert(size > 0);
    assert(sdev->dev.rport != NULL);

    enum sp_return err;
    if ((err = sp_nonblocking_write(sdev->dev.rport, b, size)))
        Serial_printError(err);
}

void Serial_write(struct serialdev_s* const sdev,
                  const uint8_t* const b,
                  const unsigned long size) {
    assert(sdev != NULL);
    assert(b != NULL);
    assert(size > 0);

    if (sdev->silenced) return;

    if (sdev->virtual) {
        Serial_writeVirtual(sdev, b, size);
    } else if (sdev->real) {
        Serial_writeReal(sdev, b, size);
    }
}

void Serial_drain(struct serialdev_s* const sdev) {
    assert(sdev != NULL);

    if (!sdev->real) return;
    enum sp_return err;
    if ((err = sp_drain(sdev->dev.rport))) Serial_printError(err);
}

void Serial_close(struct serialdev_s* const sdev) {
    if (sdev == NULL) return;
    if (sdev->real) {
        sp_close(sdev->dev.rport);
        sp_free_port(sdev->dev.rport);
    }
    free(sdev);
}

slist_t* Serial_getPorts(void) {
    slist_t* ports = NULL; /* port name string list */
    struct sp_port** pl;   /* libserialport native port list */
    if (sp_list_ports(&pl) != SP_OK) return NULL;
    for (int i = 0; pl[i] != NULL; i++) {
        char* portName = sp_get_port_name(pl[i]);
        if (portName != NULL && sladd(&ports, portName) < 0) {
            slfree(ports), ports = NULL;
            break;
        }
    }
    sp_free_port_list(pl);
    return ports;
}
