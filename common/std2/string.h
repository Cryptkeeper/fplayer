#ifndef FPLAYER_STRING_H
#define FPLAYER_STRING_H

/// @brief Returns a dynamically allocated string formatted according to the
/// given format string and arguments. The caller is responsible for freeing
/// the returned string. This function is a wrapper around `vasprintf` and
/// behaves similarly to most `printf` equivalents.
/// @param fmt format string
/// @param ... arguments to format
/// @return a dynamically allocated string, or NULL if an error occurred
char* dsprintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

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
/// @return zero on success, non-zero on failure
int strtolb(const char* str, long min, long max, void* p, int ps);

#endif//FPLAYER_STRING_H
