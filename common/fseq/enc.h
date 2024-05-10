#ifndef FPLAYER_ENC_H
#define FPLAYER_ENC_H

#include <stdint.h>

#define encode_uint8(b, v) ((b)[0] = (v))

/// @brief Encodes a 16-bit unsigned integer into a byte array.
/// @param b byte array to encode into
/// @param v value to encode
void encode_uint16(uint8_t* b, uint16_t v);

/// @brief Encodes a 32-bit unsigned integer into a byte array.
/// @param b byte array to encode into
/// @param v value to encode
void encode_uint32(uint8_t* b, uint32_t v);

/// @brief Encodes a 64-bit unsigned integer into a byte array.
/// @param b byte array to encode into
/// @param v value to encode
void encode_uint64(uint8_t* b, uint64_t v);

#endif//FPLAYER_ENC_H
