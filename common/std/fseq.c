//
#define TINYFSEQ_IMPLEMENTATION
#include "fseq.h"

#include "stb_ds.h"

#define fwrite_type(v, f) fwrite(&v, sizeof(v), 1, f)

unsigned long fseqWriteHeader(FILE *const dst,
                              const struct tf_file_header_t header) {
    const uint8_t magic[4] = {'P', 'S', 'E', 'Q'};

    unsigned long n = 0;

    n += fwrite_type(magic, dst);
    n += fwrite_type(header.channelDataOffset, dst);
    n += fwrite_type(header.minorVersion, dst);
    n += fwrite_type(header.majorVersion, dst);
    n += fwrite_type(header.variableDataOffset, dst);
    n += fwrite_type(header.channelCount, dst);
    n += fwrite_type(header.frameCount, dst);
    n += fwrite_type(header.frameStepTimeMillis, dst);

    const uint8_t reserved = 0;

    n += fwrite_type(reserved, dst);

    const uint8_t compression = (uint8_t) header.compressionType;

    n += fwrite_type(compression, dst);
    n += fwrite_type(header.compressionBlockCount, dst);
    n += fwrite_type(header.channelRangeCount, dst);
    n += fwrite_type(reserved, dst);
    n += fwrite_type(header.sequenceUid, dst);

    return n;
}

void fseqVarsFree(fseq_var_t *vars) {
    for (size_t i = 0; i < arrlenu(vars); i++) sdsfree(vars[i].string);

    arrfree(vars);
}