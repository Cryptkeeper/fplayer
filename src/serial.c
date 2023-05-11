#include "serial.h"

#include <stdio.h>

#include <libserialport.h>

#define slPrintError(err, msg)                                                 \
    {                                                                          \
        if (err != SP_OK) {                                                    \
            fprintf(stderr, "libserialport error (version %s)\n",              \
                    SP_PACKAGE_VERSION_STRING);                                \
            fprintf(stderr, "0x%02x\n", err);                                  \
                                                                               \
            if (err == SP_ERR_FAIL) {                                          \
                char *lastErrMsg = sp_last_error_message();                    \
                                                                               \
                fprintf(stderr, "last error message: %s\n", lastErrMsg);       \
                sp_free_error_message(lastErrMsg);                             \
            }                                                                  \
                                                                               \
            fprintf(stderr, "%s\n", msg);                                      \
            fprintf(stderr, "%s#L%d\n", __FILE_NAME__, __LINE__ - 1);          \
        }                                                                      \
    }

static struct sp_port *gPort;

static bool serialOpenPort(SerialOpts opts) {
    enum sp_return err;

    if ((err = sp_get_port_by_name(opts.devName, &gPort)) != SP_OK) {
        slPrintError(err, "error when getting port by name");
        return true;
    }

    if ((err = sp_open(gPort, SP_MODE_WRITE)) != SP_OK) {
        slPrintError(err, "error when opening port for writing");
        return true;
    }

    if ((err = sp_set_baudrate(gPort, opts.baudRate)) != SP_OK) {
        slPrintError(err, "error when setting baud rate");
        return true;
    }

    sp_set_parity(gPort, SP_PARITY_NONE);
    sp_set_bits(gPort, 8);
    sp_set_stopbits(gPort, 1);

    return false;
}

bool serialInit(SerialOpts opts) {
    if (opts.devName != NULL) return serialOpenPort(opts);

    return false;
}
