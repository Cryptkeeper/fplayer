#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stdint.h>

#include "std2/sl.h"

/// @brief Initializes a serial port with the provided device name and baud rate.
/// @param devName device name of the serial port
/// @param baudRate baud rate of the serial port
/// @return 0 on success, a negative error code on failure
int Serial_init(const char* devName, int baudRate);

/// @brief Writes the binary data to the open serial port. The data is copied to
/// an internal buffer and written to the serial port asynchronously.
/// @param b binary data to write
/// @param size size of the binary data
void Serial_write(const uint8_t* b, unsigned long size);

/// @brief Waits for all data to be written to the serial port.
void Serial_drain(void);

/// @brief Closes the open serial port and frees all resources.
void Serial_close(void);

/// @brief Retrieves a list of available serial ports. The caller is responsible
/// for freeing the list with `slfree`.
/// @return list of available serial ports, or NULL on failure or no ports found
slist_t* Serial_getPorts(void);

#endif//FPLAYER_SERIAL_H
