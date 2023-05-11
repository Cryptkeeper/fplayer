#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stdbool.h>

typedef struct serial_opts_t {
    char *devName;
    int baudRate;
} SerialOpts;

bool serialInit(SerialOpts opts);

#endif//FPLAYER_SERIAL_H
