#include "mem.h"

#include <string.h>

#include "err.h"

void *mustMalloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) fatalf(E_ALLOC_FAIL, "error allocating %ull bytes\n", size);
    return p;
}

char *mustStrdup(const char *src) {
    char *p = strdup(src);
    if (p == NULL) fatalf(E_ALLOC_FAIL, "error duplicating string: %s\n", src);
    return p;
}
