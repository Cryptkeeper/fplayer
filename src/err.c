#include "err.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const char *errGetMessage(Err err) {
    switch (err) {
        case E_OK:
            return "E_OK";
        case E_FATAL:
            return "E_FATAL";
        case E_FILE_NOT_FOUND:
            return "E_FILE_NOT_FOUND";
        case E_FILE_IO:
            return "E_FILE_IO";
        case E_ALLOC_FAIL:
            return "E_ALLOC_FAIL";
        default:
            return "unknown error";
    }
}

void fatalf(Err err, const char *format, ...) {
    fprintf(stderr, "fatal error: %s (%d)\n", errGetMessage(err), (int) err);

    if (format != NULL) {
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }

    // `errno` is likely set
    if (err == E_FILE_NOT_FOUND || err == E_ALLOC_FAIL) perror(NULL);

    fflush(stderr);

    exit(1);
}
