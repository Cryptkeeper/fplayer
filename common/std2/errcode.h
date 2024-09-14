/// @file errcode.h
/// @brief Error code definitions and message strings.
#ifndef FPLAYER_ERRCODE_H
#define FPLAYER_ERRCODE_H

/// @def FP_EOK
/// @brief No error
#define FP_EOK 0

/// @def FP_ERANGE
/// @brief Value out of range
#define FP_ERANGE 1

/// @def FP_EINVLARG
/// @brief Invalid argument
#define FP_EINVLARG 2

/// @def FP_ESYSCALL
/// @brief System call error
#define FP_ESYSCALL 3

/// @def FP_ENOMEM
/// @brief Out of memory
#define FP_ENOMEM 4

/// @def FP_EPTHREAD
/// @brief pthread error
#define FP_EPTHREAD 5

/// @def FP_EZSTD
/// @brief zstd error
#define FP_EZSTD 6

/// @def FP_EAUDINIT
/// @brief ALUT init error
#define FP_EAUDINIT 7

/// @def FP_EAUDPLAY
/// @brief Audio playback error
#define FP_EAUDPLAY 8

/// @def FP_EINVLBIN
/// @brief Invalid binary data
#define FP_EINVLBIN 9

/// @def FP_EINVLFMT
/// @brief Invalid JSON data
#define FP_EINVLFMT 10

/// @def FP_ENOSDEV
/// @brief Serial device not found
#define FP_ENOSDEV 11

/// @def FP_ESDEVINIT
/// @brief Serial device init error
#define FP_ESDEVINIT 12

/// @def FP_ECOUNT
/// @brief Number of error codes
#define FP_ECOUNT 13

/// @brief Returns the error message string for the given error code.
/// @param err the error code to get the message for, must be in the range of
/// [0, FP_ECOUNT), negative values are treated as positive
/// @return the error message string, otherwise "FP_EUNKNOWN" if the error code
/// is out of range
const char* FP_strerror(int err);

#endif//FPLAYER_ERRCODE_H
