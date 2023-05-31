#ifndef FPLAYER_ERR_H
#define FPLAYER_ERR_H

typedef enum err_t {
    E_OK,
    E_FATAL,
    E_FILE_NOT_FOUND,
    E_FILE_IO,
    E_ALLOC_FAIL,
} Err;

void fatalf(Err err, const char *format, ...);

#endif//FPLAYER_ERR_H
