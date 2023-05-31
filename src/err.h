#ifndef FPLAYER_ERR_H
#define FPLAYER_ERR_H

typedef enum err_t {
    E_OK,
    E_FILE_NOT_FOUND,
    E_FILE_IO,
    E_ALLOC_FAIL,
    E_INVALID_RANGE,
    E_INVALID_CONF,
} Err;

const char *errorGetMessage(Err err);

void fatalError(Err err);

#define tryOrDie(fn)                                                           \
    do {                                                                       \
        Err caught;                                                            \
        if ((caught = fn) != E_OK) {                                           \
            fprintf(stderr, "%s#%d\n", __FILE__, __LINE__);                    \
            fatalError(caught);                                                \
        }                                                                      \
    } while (0)

#endif//FPLAYER_ERR_H
