#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stddef.h>
#include <stdint.h>

#include "sds.h"

void serialInit(const char *devName, int baudRate);

void serialWrite(const uint8_t *b, size_t size);

void serialWriteHeartbeat(void);

void serialWriteAllOff(void);

void serialWriteFrame(const uint8_t *frameData,
                      const uint8_t *lastFrameData,
                      uint32_t size,
                      uint32_t frame);

void serialExit(void);

sds *serialEnumPorts(void);

#endif//FPLAYER_SERIAL_H
