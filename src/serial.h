#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct serial_opts_t {
    char *devName;
    int baudRate;
} SerialOpts;

bool serialInit(SerialOpts opts);

bool serialWriteFrame(const uint8_t *b, uint32_t size);

void serialExit(void);

#endif//FPLAYER_SERIAL_H
