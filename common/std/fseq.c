#define TINYFSEQ_IMPLEMENTATION
#include "fseq.h"

#include <assert.h>

#include "stb_ds.h"

#define fwrite_type(v, f) fwrite(&v, sizeof(v), 1, f)

bool fseqWriteHeader(FILE *const dst, const TFHeader header) {
    if (fseek(dst, 0, SEEK_SET) != 0) return false;

    const uint8_t magic[4] = {'P', 'S', 'E', 'Q'};

    fwrite_type(magic, dst);
    fwrite_type(header.channelDataOffset, dst);
    fwrite_type(header.minorVersion, dst);
    fwrite_type(header.majorVersion, dst);
    fwrite_type(header.variableDataOffset, dst);
    fwrite_type(header.channelCount, dst);
    fwrite_type(header.frameCount, dst);
    fwrite_type(header.frameStepTimeMillis, dst);

    const uint8_t reserved = 0;

    fwrite_type(reserved, dst);

    const uint8_t compression = header.compressionType;

    fwrite_type(compression, dst);
    fwrite_type(header.compressionBlockCount, dst);
    fwrite_type(header.channelRangeCount, dst);
    fwrite_type(reserved, dst);
    fwrite_type(header.sequenceUid, dst);

    return true;
}

#define HEADER_SIZE 32

bool fseqWriteCompressionBlocks(FILE *const dst,
                                const TFCompressionBlock *const blocks) {
    if (fseek(dst, HEADER_SIZE, SEEK_SET) != 0) return false;

    for (size_t i = 0; i < arrlenu(blocks); i++) {
        const TFCompressionBlock block = blocks[i];

        fwrite_type(block.firstFrameId, dst);
        fwrite_type(block.size, dst);
    }

    return true;
}

#define VAR_HEADER_SIZE 4

static uint16_t fseqGetVarSize(const fseq_var_t var) {
    return strlen(var.string) + 1 + VAR_HEADER_SIZE;
}

bool fseqWriteVars(FILE *const dst,
                   const TFHeader header,
                   const fseq_var_t *const vars) {
    if (fseek(dst, header.variableDataOffset, SEEK_SET) != 0) return false;

    for (size_t i = 0; i < arrlenu(vars); i++) {
        const fseq_var_t var = vars[i];

        const uint16_t size = fseqGetVarSize(var);

        fwrite_type(size, dst);

        fwrite_type(var.idh, dst);
        fwrite_type(var.idl, dst);

        fwrite(var.string, strlen(var.string), 1, dst);

        fputc('\0', dst);
    }

    return true;
}

void fseqAlignOffsets(TFHeader *const header, const fseq_var_t *const vars) {
    uint16_t varDataSize = 0;

    for (size_t i = 0; i < arrlenu(vars); i++)
        varDataSize += fseqGetVarSize(vars[i]);

    // round to nearest product of 4 for 32-bit alignment
    if (varDataSize % 4 != 0) varDataSize += 4 - varDataSize % 4;

    // ensure the value can be safely downcasted to what the file format expects
    assert(varDataSize <= UINT16_MAX);

    const uint16_t firstVarOffset = 32 + header->compressionBlockCount * 8 +
                                    header->channelRangeCount * 6;

    header->variableDataOffset = firstVarOffset;
    header->channelDataOffset = firstVarOffset + varDataSize;
}
