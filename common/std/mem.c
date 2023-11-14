#include "mem.h"

#include <stdlib.h>

#include "err.h"

void *mustMalloc(const size_t size) {
    void *const p = malloc(size);
    if (p == NULL) fatalf(E_SYS, "error allocating %ull bytes\n", size);
    return p;
}
