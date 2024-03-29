#include "string.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "err.h"

char *dsprintf(const char *const fmt, ...) {
    assert(fmt != NULL);
    assert(strlen(fmt) > 0);

    va_list args;

    // determine required buffer size
    va_start(args, fmt);
    const int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // avoid allocating a zero-length buffer/formating an empty string
    if (len <= 0) return NULL;

    // allocate buffer and format string
    va_start(args, fmt);
    char *const str = mustMalloc(len + 1);
    vsnprintf(str, len + 1, fmt, args);
    va_end(args);

    return str;
}
