#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stddef.h>
#include <stdint.h>

void serialInit(const char *devName, int baudRate);

void serialWrite(const uint8_t *b, size_t size);

void serialWaitForDrain(void);

void serialExit(void);

char **serialEnumPorts(void);

#endif//FPLAYER_SERIAL_H
