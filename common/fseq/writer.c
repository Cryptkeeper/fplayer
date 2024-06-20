#include "writer.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "tinyfseq.h"

#include "fseq/enc.h"
#include "std2/errcode.h"
#include "std2/fc.h"

#define HEADER_SIZE 32

// FSEQ File Format Header
// https://github.com/Cryptkeeper/fseq-file-format
//
//                             ┌────────────────┬────────────────┬────────────────┬────────────────┐
//                             │'P'         0x50│'S'         0x53│'E'         0x45│'Q'         0x51│
//                             ├────────────────┴────────────────┼────────────────┼────────────────┤
//                             │Channel Data Offset        uint16│Minor Version   │Major Version  2│
// ┌────────────────────────┐  ├─────────────────────────────────┼────────────────┴────────────────┤
// │Extended Compression    │  │Variable Data Offset       uint16│Channel Count              uint32
// │Block Count (v2.1) uint4│  ├─────────────────────────────────┼─────────────────────────────────┤
// ├────────────────────────┤   Channel Count (contd)      uint32│Frame Count                uint32
// │Compression Type   uint4│  ├─────────────────────────────────┼────────────────┬────────────────┤
// └────────────────────────┘   Frame Count (contd)        uint32│Step Time Ms.   │Flags (unused)  │
//              │              ├────────┬───────┬────────────────┼────────────────┼────────────────┤
//              └─────────────▶│ECBC    │CT     │Compr. Block Cnt│Sparse Range Cnt│Reserved        │
//                             ├────────┴───────┴────────────────┴────────────────┴────────────────┤
//                             │Unique ID/Creation Time Microseconds                         uint64
//                             ├───────────────────────────────────────────────────────────────────┤
//                              Unique ID/Creation Time Microseconds (contd)                 uint64│
//                             └───────────────────────────────────────────────────────────────────┘

int fseqWriteHeader(struct FC* fc, const struct tf_header_t* header) {
    assert(fc != NULL);
    assert(header != NULL);

    uint8_t b[HEADER_SIZE] = {'P', 'S', 'E', 'Q'};

    enc_uint16_le(&b[4], header->channelDataOffset);
    enc_uint8_le(&b[6], header->minorVersion);
    enc_uint8_le(&b[7], header->majorVersion);
    enc_uint16_le(&b[8], header->variableDataOffset);
    enc_uint32_le(&b[10], header->channelCount);
    enc_uint32_le(&b[14], header->frameCount);
    enc_uint16_le(&b[18], header->frameStepTimeMillis);
    enc_uint8_le(&b[20], header->compressionType);
    enc_uint8_le(&b[21], header->compressionBlockCount);
    enc_uint32_le(&b[22], header->channelRangeCount);
    enc_uint64_le(&b[24], header->sequenceUid);

    if (FC_write(fc, 0, sizeof(b), b) != sizeof(b)) return -FP_ESYSCALL;

    return FP_EOK;
}

int fseqWriteCompressionBlocks(struct FC* fc,
                               const struct tf_header_t* header,
                               const struct tf_compression_block_t* blocks) {
    assert(fc != NULL);
    assert(header != NULL);
    assert(blocks != NULL);

    for (int i = 0; i < header->compressionBlockCount; i++) {
        uint8_t b[8] = {0};

        enc_uint32_le(b, blocks[i].firstFrameId);
        enc_uint32_le(&b[4], blocks[i].size);

        if (FC_write(fc, HEADER_SIZE + i * 8, sizeof(b), b) != sizeof(b))
            return -FP_ESYSCALL;
    }

    return FP_EOK;
}

#define VAR_HEADER_SIZE 4

/// @brief Calculates the size in bytes of the variable section according to the
/// given variables. The size is stored in the given pointer.
/// @param vars variables to calculate the size of
/// @param count number of variables in the array
/// @param align whether to align the size to the nearest multiple of 4
/// @param size pointer to store the calculated size
/// @return 0 on success, a negative error code on failure
static int fseqGetVarSectionSize(const struct fseq_var_s* vars,
                                 const int count,
                                 const bool align,
                                 uint16_t* size) {
    assert(vars != NULL);
    assert(count > 0);
    assert(size != NULL);

    size_t sect = 0;
    for (int i = 0; i < count; i++) sect += vars[i].size + VAR_HEADER_SIZE;

    // optionally round to nearest product of 4 for 32-bit alignment
    if (align && sect % 4 != 0) sect += 4 - sect % 4;

    if (sect > UINT16_MAX)
        return -FP_ERANGE;// too many variables for the format

    *size = sect;

    return FP_EOK;
}

int fseqWriteVars(struct FC* fc,
                  const struct tf_header_t* header,
                  const struct fseq_var_s* vars,
                  const int count) {
    assert(fc != NULL);
    assert(header != NULL);
    assert(vars != NULL);

    if (count == 0) return FP_EOK;// nothing to write

    uint16_t sect;
    int err;
    if ((err = fseqGetVarSectionSize(vars, count, false, &sect))) return err;

    // allocate a single buffer for encoding all variables
    uint8_t* b = malloc(sect);
    if (b == NULL) return -FP_ENOMEM;

    uint8_t* head = b;
    for (int i = 0; i < count; i++) {
        const struct fseq_var_s* var = &vars[i];

        enc_uint16_le(head, var->size + VAR_HEADER_SIZE);
        enc_uint8_le(&head[2], var->id[0]);
        enc_uint8_le(&head[3], var->id[1]);

        memcpy(&head[VAR_HEADER_SIZE], var->value, var->size);

        head += var->size + VAR_HEADER_SIZE;
    }

    const uint32_t w = FC_write(fc, header->variableDataOffset, sect, b);
    free(b);

    return w == sect ? FP_EOK : -FP_ESYSCALL;
}

int fseqRealignHeaderOffsets(struct tf_header_t* header,
                             const struct fseq_var_s* vars,
                             const int count) {
    assert(header != NULL);
    assert(vars != NULL);

    uint16_t sect;

    int err;
    if ((err = fseqGetVarSectionSize(vars, count, true, &sect))) return err;

    const uint16_t firstVarOffset = HEADER_SIZE +
                                    header->compressionBlockCount * 8 +
                                    header->channelRangeCount * 6;

    header->variableDataOffset = firstVarOffset;
    header->channelDataOffset = firstVarOffset + sect;

    return FP_EOK;
}
