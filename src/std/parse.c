#include "parse.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"

static inline long clampLong(long l, long min, long max) {
    if (l <= min) return min;
    else if (l >= max)
        return max;
    return l;
}

void parseLong(const char *s, void *i, int n, long min, long max) {
    if (s == NULL || strlen(s) == 0) goto fail;

    char *endptr = NULL;

    const long l = strtol(s, &endptr, 10);

    // "If there were no digits at
    //     all, however, strtol() stores the original value of str in *endptr."
    if (s == endptr) goto fail;

    // "If an overflow or underflow occurs, errno is set to ERANGE and the
    //     function return value is clamped according to the following table."
    if (l == 0 && errno == ERANGE) goto fail;

    const long clamped = clampLong(l, min, max);

    memcpy(i, (void *) &clamped, n);

    return;

fail:
    // auto exit due to parsing fail
    // this is an opinionated decision, built on the assumption that
    // input is only parsed once at program start and is otherwise not at risk
    // of hurting the program during active runtime
    fatalf(E_FATAL, "error parsing number: %s\n", s);
}
