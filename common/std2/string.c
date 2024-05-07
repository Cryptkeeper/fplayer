#include "string.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* vdsprintf(const char* fmt, va_list args) {
    assert(fmt != NULL);

    // determine required buffer size
    const int len = vsnprintf(NULL, 0, fmt, args);
    if (len <= 0) return NULL;

    // allocate buffer and format string
    char* str = malloc(len + 1);
    if (str != NULL) vsnprintf(str, len + 1, fmt, args);
    return str;
}

char* dsprintf(const char* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* str = vdsprintf(fmt, args);
    va_end(args);
    return str;
}

void dfatalf(const char* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* str = vdsprintf(fmt, args);
    if (str != NULL) fprintf(stderr, "fatal error: %s\n", str);
    free(str);
    va_end(args);

    if (errno != 0) {
        const char* const msg = strerror(errno);
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
