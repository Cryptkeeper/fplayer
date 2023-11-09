#include "err.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *errGetMessage(const enum err_t err) {
    switch (err) {
        case E_OK:
            return "E_OK";
        case E_APP:
            return "E_APP";
        case E_SYS:
            return "E_SYS";
        case E_FIO:
            return "E_FIO";
        default:
            return "unknown error";
    }
}

void fatalf(const enum err_t err, const char *const format, ...) {
    fprintf(stderr, "fatal error: %s (%d)\n", errGetMessage(err), (int) err);

    if (format != NULL) {
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }

    // `errno` is likely set
    if (err == E_SYS || err == E_FIO) {
        const char *const msg = strerror(errno);

        if (msg != NULL) fprintf(stderr, "%s\n", msg);
    }

    fflush(stderr);

    // Returning errors to propagate upward to the caller is cumbersome and
    // requires promoting error context/special error context upward as well,
    // resulting in a messy scope of inclusions.
    //
    // Given these fatal errors are unrecoverable, I've opted to inline the
    // failure using `exit`. I don't like the idea of functions randomly failing
    // internally, but it avoids the business logic being littered with unclear
    // error handling.
    exit(1);
}
