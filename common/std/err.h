#ifndef FPLAYER_ERR_H
#define FPLAYER_ERR_H

#include <stdbool.h>
#include <stddef.h>

enum err_t {
    E_APP = 1,// error in application layer
    E_SYS = 2,// error in OS/stdlib layer
    E_FIO = 3,// error in file I/O operation
};

void fatalf(enum err_t err, const char* format, ...);

bool strtolb(const char* str, long min, long max, void* p, size_t ps);

#endif//FPLAYER_ERRCODE_H
