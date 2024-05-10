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

#endif//FPLAYER_STRING_H
