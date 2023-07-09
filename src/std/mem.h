#ifndef FPLAYER_MEM_H
#define FPLAYER_MEM_H

#include <stddef.h>

void freeAndNull(void **p);

#define freeAndNullWith(p, fn)                                                 \
    do {                                                                       \
        if (*(p) != NULL) fn(*(p));                                            \
        *(p) = NULL;                                                           \
    } while (0)

void *mustMalloc(size_t size);

char *mustStrdup(const char *src);

#endif//FPLAYER_MEM_H
