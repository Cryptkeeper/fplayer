#include "mem.h"

#include <stdlib.h>
#include <string.h>

#include "err.h"

void freeAndNull(void **p) {
    free(*p);
    *p = NULL;
}

void *mustMalloc(size_t size) {
    void *p = malloc(size);
    if (p == NULL) fatalf(E_ALLOC_FAIL, "error allocating %ull bytes\n", size);
    return p;
}

void *mustRealloc(void *p, size_t size) {
    void *r = reallocf(p, size);
    if (r == NULL)
        fatalf(E_ALLOC_FAIL, "error re-allocating %ull bytes\n", size);
    return r;
}

char *mustStrdup(const char *src) {
    char *p = strdup(src);
    if (p == NULL) fatalf(E_ALLOC_FAIL, "error duplicating string: %s\n", src);
    return p;
}
