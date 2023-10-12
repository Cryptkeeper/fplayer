#ifndef FPLAYER_FSEQ_H
#define FPLAYER_FSEQ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "sds.h"
#include "tinyfseq.h"

bool fseqWriteHeader(FILE *dst, struct tf_file_header_t header);

bool fseqWriteCompressionBlocks(FILE *dst,
                                struct tf_file_header_t header,
                                const struct tf_compression_block_t *blocks);

typedef struct fseq_var_t {
    uint8_t idh;
    uint8_t idl;
    sds string;
} fseq_var_t;

bool fseqWriteVars(FILE *dst,
                   struct tf_file_header_t header,
                   const fseq_var_t *vars);

void fseqVarsFree(fseq_var_t *vars);

void fseqAlignOffsets(struct tf_file_header_t *header, const fseq_var_t *vars);

#endif//FPLAYER_FSEQ_H
