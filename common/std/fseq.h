#ifndef FPLAYER_FSEQ_H
#define FPLAYER_FSEQ_H

#include <stdbool.h>
#include <stdio.h>

#include "tinyfseq.h"

bool fseqWriteHeader(FILE *dst, TFHeader header);

bool fseqWriteCompressionBlocks(FILE *dst, const TFCompressionBlock *blocks);

typedef struct fseq_var_t {
    uint8_t idh;
    uint8_t idl;
    char *string;
} fseq_var_t;

bool fseqWriteVars(FILE *dst, TFHeader header, const fseq_var_t *vars);

void fseqAlignOffsets(TFHeader *header, const fseq_var_t *vars);

#endif//FPLAYER_FSEQ_H
