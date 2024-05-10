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

    va_list args2;

    // determine required buffer size
    va_copy(args2, args);
    const int len = vsnprintf(NULL, 0, fmt, args2);
    if (len <= 0) return NULL;

    // allocate buffer and format string
    char* str = malloc(len + 1);
    if (str == NULL) return NULL;
    va_copy(args2, args);
    vsnprintf(str, len + 1, fmt, args2);
    return str;
}

char* dsprintf(const char* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* str = vdsprintf(fmt, args);
    va_end(args);
    return str;
}

int strtolb(const char* const str,
            const long min,
            const long max,
            void* const p,
            const int ps) {
    assert(str != NULL);
    assert(max >= min);
    assert(p != NULL);
    assert(ps > 0);

    // try to avoid values larger than what can be stored in the dest. type
    // (e.g. 64-bit values in a 32-bit int)
    assert(max <= ps * 8);

    errno = 0;
    char* endptr = NULL;
    long parsed = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0') return -1;
    parsed = parsed < min ? min : (parsed > max ? max : parsed);
    memcpy(p, &parsed, ps);
    return 0;
}
