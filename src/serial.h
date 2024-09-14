/// @file serial.h
/// @brief Serial port communication interface.
#ifndef FPLAYER_SERIAL_H
#define FPLAYER_SERIAL_H

#include <stdint.h>

#include "sl.h"

/// @struct serialdev_s
/// @brief Serial port communication device.
struct serialdev_s;

/// @brief Initializes a write-mode serial port with the provided device name
/// and baud rate pre-configured for use as a LOR network connection.
/// @param sdev serial device to initialize, must not be NULL
/// @param devName device name of the serial port
/// @param baudRate baud rate of the serial port
/// @return 0 on success, a negative error code on failure
int Serial_init(struct serialdev_s** sdev, const char* devName, int baudRate);

/// @brief Writes the binary data to the open serial port. The data is copied to
/// an internal buffer and written to the serial port asynchronously.
/// @param sdev serial device to write to, must not be NULL
/// @param b binary data to write
/// @param size size of the binary data
void Serial_write(struct serialdev_s* sdev,
                  const uint8_t* b,
                  unsigned long size);

/// @brief Waits for all data to be written to the serial device.
/// @param sdev serial device to drain, must not be NULL
void Serial_drain(struct serialdev_s* sdev);

/// @brief Closes the open serial device and frees all resources.
/// @param sdev serial device to close, may be NULL
void Serial_close(struct serialdev_s* sdev);

/// @brief Retrieves a list of available serial ports. The caller is responsible
/// for freeing the list with `slfree`.
/// @return list of available serial ports, or NULL on failure or no ports found
slist_t* Serial_getPorts(void);

#endif//FPLAYER_SERIAL_H
