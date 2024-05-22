#include "string.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int strtolb(const char* const str,
            const long min,
            const long max,
            void* const p,
            const int ps) {
    assert(str != NULL);
    assert(max >= min);
    assert(p != NULL);
    assert(ps > 0);

    errno = 0;
    char* endptr = NULL;
    long parsed = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0') return -1;
    parsed = parsed < min ? min : (parsed > max ? max : parsed);
    memcpy(p, &parsed, ps);
    return 0;
}
