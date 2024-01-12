#ifndef PROTOWRITER_H
#define PROTOWRITER_H

#include <stddef.h>

#include "lorproto/coretypes.h"

struct protowriter_t {
    LorBuffer *(*checkout_msg)(void);        // request a buffer to write to
    size_t (*return_msg)(const LorBuffer *b);// returns buf & returns # of bytes
};

extern struct protowriter_t protowriter;

#endif//PROTOWRITER_H
