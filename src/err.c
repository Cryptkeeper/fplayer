#include "err.h"

#include <stdio.h>
#include <stdlib.h>

const char *errorGetMessage(Err err) {
    switch (err) {
        case E_OK:
            return "E_OK";
        case E_FILE_NOT_FOUND:
            return "E_FILE_NOT_FOUND";
        case E_FILE_IO:
            return "E_FILE_IO";
        case E_ALLOC_FAIL:
            return "E_ALLOC_FAIL";
        case E_INVALID_RANGE:
            return "E_INVALID_RANGE";
        case E_INVALID_CONF:
            return "E_INVALID_CONF";
        default:
            return "unknown error";
    }
}

void fatalError(Err err) {
    fprintf(stderr, "fatal error: %d\n", (int) err);
    fprintf(stderr, "%s\n", errorGetMessage(err));

    fflush(stderr);

    exit(1);
}
