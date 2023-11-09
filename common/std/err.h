#ifndef FPLAYER_ERR_H
#define FPLAYER_ERR_H

enum err_t {
    E_OK,
    E_APP,// error in application layer
    E_SYS,// error in OS/stdlib layer
    E_FIO,// error in file I/O operation
};

void fatalf(enum err_t err, const char *format, ...);

#endif//FPLAYER_ERR_H
