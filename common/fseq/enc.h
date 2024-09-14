/// @file enc.h
/// @brief Binary encoding utility functions.
#ifndef FPLAYER_ENC_H
#define FPLAYER_ENC_H

#include <stdint.h>

/// @brief Encodes a "little-endian" 8-bit unsigned integer into a byte array.
/// This macro is provided for consistency when used with other encoding
/// functions provided by the header.
/// @note This macro is equivalent to `b[0] = v`.
/// @param b byte array to encode into
/// @param v value to encode
#define enc_uint8_le(b, v) ((b)[0] = (v))

/// @brief Encodes a little-endian 16-bit unsigned integer into a byte array.
/// @param b byte array to encode into
/// @param v value to encode
void enc_uint16_le(uint8_t* b, uint16_t v);

/// @brief Encodes a little-endian 32-bit unsigned integer into a byte array.
/// @param b byte array to encode into
/// @param v value to encode
void enc_uint32_le(uint8_t* b, uint32_t v);

/// @brief Encodes a little-endian 64-bit unsigned integer into a byte array.
/// @param b byte array to encode into
/// @param v value to encode
void enc_uint64_le(uint8_t* b, uint64_t v);

#endif//FPLAYER_ENC_H
