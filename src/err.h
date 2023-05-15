#ifndef FPLAYER_ERR_H
#define FPLAYER_ERR_H

#define errPrintTrace(msg)                                                     \
    do {                                                                       \
        fprintf(stderr, "%s\n", msg);                                          \
        fprintf(stderr, "%s#L%d\n", __FILE_NAME__, __LINE__ - 1);              \
    } while (0)

#endif//FPLAYER_ERR_H
