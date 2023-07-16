#include "mem.h"

#include <stdlib.h>

#include "err.h"

void freeAndNull(void **const p) {
    free(*p);
    *p = NULL;
}

void *mustMalloc(const size_t size) {
    void *const p = malloc(size);
    if (p == NULL) fatalf(E_ALLOC_FAIL, "error allocating %ull bytes\n", size);
    return p;
}
