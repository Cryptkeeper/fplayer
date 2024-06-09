#include "errcode.h"

static const char* FP_errstr[FP_ECOUNT] = {
        "FP_EOK",    "FP_ESYSCALL", "FP_ERANGE",   "FP_ENOMEM",   "FP_EDECODE",
        "FP_ENOSUP", "FP_EZSTD",    "FP_EALCTL",   "FP_EPLAYAUD", "FP_ESEQEND",
        "FP_ENODEV", "FP_EDEVCONF", "FP_EBADJSON", "FP_EINVAL",   "FP_EPTHREAD",
};

const char* FP_strerror(int err) {
    if (err < 0) err = -err;
    if (err >= FP_ECOUNT) return "FP_EUNKNOWN";
    return FP_errstr[err];
}
