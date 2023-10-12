#define TINYFSEQ_IMPLEMENTATION
#include "fseq.h"

#include "stb_ds.h"

void fseqVarsFree(fseq_var_t *vars) {
    for (size_t i = 0; i < arrlenu(vars); i++) sdsfree(vars[i].string);

    arrfree(vars);
}