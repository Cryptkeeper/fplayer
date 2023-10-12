#ifndef FPLAYER_FSEQ_H
#define FPLAYER_FSEQ_H

#include <stddef.h>

#include "sds.h"
#include "tinyfseq.h"

typedef struct fseq_var_t {
    uint8_t idh;
    uint8_t idl;
    sds string;
} fseq_var_t;

void fseqVarsFree(fseq_var_t *vars);

#endif//FPLAYER_FSEQ_H
