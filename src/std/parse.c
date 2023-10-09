#include "parse.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"

void parseLong(const char *const s,
               void *const i,
               const size_t n,
               const long min,
               const long max) {
    if (s == NULL || strlen(s) == 0) goto fail;

    char *endptr = NULL;

    const long l = strtol(s, &endptr, 10);

    // "If there were no digits at
    //     all, however, strtol() stores the original value of str in *endptr."
    if (s == endptr) goto fail;

    // "If an overflow or underflow occurs, errno is set to ERANGE and the
    //     function return value is clamped according to the following table."
    if (l == 0 && errno == ERANGE) goto fail;

    const long clamped = l < min ? min : (l > max ? max : l);

    memcpy(i, (void *) &clamped, n);

    return;

fail:
    // auto exit due to parsing fail
    // this is an opinionated decision, built on the assumption that
    // input is only parsed once at program start and is otherwise not at risk
    // of hurting the program during active runtime
    fatalf(E_FATAL, "error parsing number: %s\n", s);
}
