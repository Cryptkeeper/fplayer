#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool Serial_init(const char *devName, int baudRate);

void Serial_write(const uint8_t *b, size_t size);

void Serial_drain(void);

void Serial_close(void);

char **Serial_getPorts(void);

#endif//FPLAYER_SERIAL_H
