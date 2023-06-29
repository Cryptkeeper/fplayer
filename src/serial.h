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

void serialWriteAllOff(void);

void serialWriteFrame(const uint8_t *frameData,
                      const uint8_t *lastFrameData,
                      uint32_t size,
                      uint32_t frame);

void serialExit(void);

#endif//FPLAYER_SERIAL_H
