#ifndef FPLAYER_ERR_H
#define FPLAYER_ERR_H

#include <stdbool.h>
#include <stddef.h>

enum err_t {
    E_OK,
    E_APP,// error in application layer
    E_SYS,// error in OS/stdlib layer
    E_FIO,// error in file I/O operation
};

void fatalf(enum err_t err, const char* format, ...);

void* mustMalloc(size_t size);

bool strtolb(const char* str, long min, long max, void* p, size_t ps);

char* mustStrdup(const char* str);

#endif//FPLAYER_ERR_H
