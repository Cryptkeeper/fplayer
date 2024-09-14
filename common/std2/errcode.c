/// @file errcode.c
/// @brief Error code definitions and message strings.
#include "errcode.h"

/// @brief Error message strings for each error code.
static const char* FP_errstr[FP_ECOUNT] = {
        "FP_EOK",       "FP_ERANGE",   "FP_EINVLARG", "FP_ESYSCALL",
        "FP_ENOMEM",    "FP_EPTHREAD", "FP_EZSTD",    "FP_EAUDINIT",
        "FP_EAUDPLAY",  "FP_EINVLBIN", "FP_EINVLFMT", "FP_ENOSDEV",
        "FP_ESDEVINIT",
};

const char* FP_strerror(int err) {
    if (err < 0) err = -err;
    if (err >= FP_ECOUNT) return "FP_EUNKNOWN";
    return FP_errstr[err];
}
