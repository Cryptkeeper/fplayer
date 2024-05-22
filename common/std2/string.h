#ifndef FPLAYER_STRING_H
#define FPLAYER_STRING_H

/// @brief Attempts to parse the given string as a long using `strtol`. If the
/// parsed value is within the given range, it is stored in the given pointer
/// using memcpy, which results in a type-agnostic function. The size of the
/// destination type must be provided in bytes. The destination is only updated
/// if the parsed value is within the given range. The caller should take care
/// to ensure the destination type is large enough to store the parsed value.
/// @param str string to parse
/// @param min minimum value (inclusive) supported by the destination type
/// @param max maximum value (inclusive) supported by the destination type
/// @param p pointer to store the parsed value
/// @param ps size of the destination type in bytes
/// @return 0 on success, non-zero on failure
int strtolb(const char* str, long min, long max, void* p, int ps);

#endif//FPLAYER_STRING_H
