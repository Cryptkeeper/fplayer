#ifndef FPLAYER_ERR_H
#define FPLAYER_ERR_H

#include <stddef.h>

enum err_t {
    E_SYS = 2,// error in OS/stdlib layer
};

void fatalf(enum err_t err, const char* format, ...);

#endif//FPLAYER_ERR_H
