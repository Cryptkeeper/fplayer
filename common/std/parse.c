#include "parse.h"

#include <errno.h>
#include <stdlib.h>

#include "err.h"

long parseLong(const char *const str, const long min, const long max) {
    char *endptr = NULL;

    errno = 0;

    const long parsed = strtol(str, &endptr, 10);

    if (errno != 0 || endptr == NULL || *endptr != '\0')
        fatalf(E_FATAL, "error parsing number: %s\n", str);

    return parsed < min ? min : parsed > max ? max : parsed;
}
