#include "mem.h"

#include "err.h"

void *mustMalloc(const size_t size) {
    void *const p = malloc(size);
    if (p == NULL) fatalf(E_ALLOC_FAIL, "error allocating %ull bytes\n", size);
    return p;
}
