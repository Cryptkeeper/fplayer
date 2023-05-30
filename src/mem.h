#ifndef FPLAYER_MEM_H
#define FPLAYER_MEM_H

#include <stdlib.h>

#define freeAndNull(p)                                                         \
    do {                                                                       \
        free(*p);                                                              \
        *p = NULL;                                                             \
    } while (0);

#endif//FPLAYER_MEM_H
