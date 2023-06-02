#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stdint.h>

typedef struct serial_opts_t {
    char *devName;
    int baudRate;
} SerialOpts;

void serialOptsFree(SerialOpts *opts);

void serialInit(SerialOpts opts);

void serialWriteHeartbeat(void);

void serialWriteFrame(const uint8_t *currentData,
                      const uint8_t *lastData,
                      uint32_t size);

void serialExit(void);

#endif//FPLAYER_SERIAL_H
