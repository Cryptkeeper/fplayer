#include "seq.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <tinyfseq.h>

#include <fseq/writer.h>
#include <std2/errcode.h>
#include <std2/fc.h>

struct tf_header_t* curSequence;

int Seq_open(struct FC* fc) {
    assert(fc != NULL);

    uint8_t b[32] = {0};
    if (!FC_read(fc, 0, sizeof(b), b)) return -FP_ESYSCALL;

    if ((curSequence = calloc(1, sizeof(*curSequence))) == NULL)
        return -FP_ENOMEM;

    if (TFHeader_read(b, sizeof(b), curSequence, NULL)) {
        free(curSequence), curSequence = NULL;
        return -FP_EDECODE;
    }

    return FP_EOK;
}

void Seq_close(void) {
    free(curSequence), curSequence = NULL;
}

#define VARHEADER_SIZE 4

/// @brief Reads a variable from the variable table and populates the provided
/// `var` struct with the variable id, size, and value. The variable value is
/// allocated and must be freed by the caller.
/// @param head pointer to the current variable table position, updated to the
/// next variable position on return
/// @param remaining the number of bytes remaining in the variable table
/// @param var the variable struct to populate with the read variable
/// @return 0 on success, a negative error code on failure
static int
Seq_readVar(uint8_t** head, const int remaining, struct fseq_var_s* var) {
    assert(head != NULL);
    assert(var != NULL);

    // fetch the variable header for sizing information
    struct tf_var_header_t h = {0};
    if (TFVarHeader_read(*head, remaining, &h, NULL, 0, NULL))
        return -FP_EDECODE;

    // allocate a buffer for the decoded variable data
    // remove extra bytes that store id+size
    if ((var->value = malloc(h.size - VARHEADER_SIZE)) == NULL)
        return -FP_ENOMEM;

    // read the variable data into the buffer
    if (TFVarHeader_read(*head, remaining, &h, (uint8_t*) var->value,
                         h.size - VARHEADER_SIZE, head)) {
        free(var->value);
        var->value = NULL;
        return -FP_EDECODE;
    }

    var->size = h.size - VARHEADER_SIZE;

    return FP_EOK;
}

int Seq_getMediaFile(struct FC* fc, char** value) {
    assert(fc != NULL);
    assert(value != NULL);

    const uint16_t varTableSize =
            curSequence->channelDataOffset - curSequence->variableDataOffset;
    if (varTableSize == 0) return FP_EOK;

    uint8_t* varTable = NULL; /* table of all variable data */
    if ((varTable = malloc(varTableSize)) == NULL) return -FP_ENOMEM;

    if (FC_read(fc, curSequence->variableDataOffset, varTableSize, varTable) <
        varTableSize) {
        free(varTable);
        return -FP_ESYSCALL;
    }

    struct fseq_var_s var = {0};

    uint8_t* head = &varTable[0];
    for (int remaining = varTableSize; remaining > VARHEADER_SIZE;) {
        // read the next available variable
        int err;
        if ((err = Seq_readVar(&head, remaining, &var))) {
            free(varTable);
            return err;
        }

        if (var.id[0] == 'm' && var.id[1] == 'f') {
            *value = var.value;
            goto ret;
        }

        free(var.value);// throw away the read value, it's not the media file
        remaining -= var.size + VARHEADER_SIZE;// update remaining bytes
    }

    *value = NULL;// no match

ret:
    free(varTable);
    return FP_EOK;
}
