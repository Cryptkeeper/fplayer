#undef NDEBUG
#include <assert.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#include <fseq/writer.h>
#include <std2/errcode.h>
#include <std2/fc.h>
#include <std2/string.h>

/// @brief Copies the source sequence's binary configuration data section for
/// compression blocks and channel ranges to the destination file. The data is
/// not validated or interpreted in any way.
/// @param dst destination file controller instance
/// @param src source file controller instance
/// @param header the sequence header
/// @return 0 on success, a negative error code on failure
static int fseqCopyConfigBlocks(struct FC* dst,
                                struct FC* src,
                                const struct tf_header_t* header) {
    const uint16_t size =
            header->compressionBlockCount * 8 + header->channelRangeCount * 6;

    uint8_t* b = malloc(size);
    if (b == NULL) return -FP_ENOMEM;

    int err = FP_EOK;
    if (FC_read(src, 32, size, b) != size || FC_write(dst, 32, size, b) != size)
        err = -FP_ESYSCALL;

    free(b);
    return err;
}

#define CD_CHUNK_SIZE 4096

/// @brief Copies channel data from the source file to the destination file.
/// @param dst destination file controller instance
/// @param src source file controller instance
/// @param header the sequence header
/// @return the number of bytes copied, or a negative error code on failure
static int64_t fseqCopyChannelData(struct FC* dst,
                                   struct FC* src,
                                   const struct tf_header_t* header) {
    uint8_t* const chunk = malloc(CD_CHUNK_SIZE);
    if (chunk == NULL) return -FP_ENOMEM;

    int64_t res = 0; /* bytes copied or error code */

    while (1) {
        const uint32_t offset = header->channelDataOffset + res;
        const uint32_t read = FC_readto(src, offset, 1, CD_CHUNK_SIZE, chunk);
        if (read == 0) break; /* test for error or EOF */

        res += read;

        if (FC_write(dst, offset, read, chunk) != read) {
            res = -FP_ESYSCALL;
            break;
        }
    }

    free(chunk);
    return res;
}

/// @brief Reads and roughly validates the sequence header from the given file
/// controller instance.
/// @param fc file controller instance to read from
/// @param header out pointer to the sequence header struct to populate
/// @return 0 on success, a negative error code on failure
static int fseqGetHeader(struct FC* fc, struct tf_header_t* header) {
    uint8_t b[32];
    if (FC_read(fc, 0, sizeof(b), b) != sizeof(b)) return -FP_ESYSCALL;

    if (TFHeader_read(b, sizeof(b), header, NULL)) return -FP_EDECODE;

    if (!(header->majorVersion == 2 && header->minorVersion == 0))
        return -FP_ENOSUP;

    return FP_EOK;
}

/// @brief Copies the sequence from the source file to the destination file,
/// rewriting the sequence variables with the provided values. All other
/// sequence data is copied as-is.
/// @param sfp source file path
/// @param dfp destination file path
/// @param vars array of sequence variables to write
/// @param count number of variables in the array
/// @return 0 on success, a negative error code on failure
static int fseqCopySetVars(const char* sfp,
                           const char* dfp,
                           const struct fseq_var_s* vars,
                           const int count) {
    int err;

    struct FC* src = FC_open(sfp, FC_MODE_READ);
    struct FC* dst = FC_open(dfp, FC_MODE_WRITE);

    if (src == NULL || dst == NULL) {
        err = -FP_ESYSCALL;
        goto ret;
    }

    struct tf_header_t origHeader;
    if ((err = fseqGetHeader(src, &origHeader))) goto ret;

    struct tf_header_t newHeader = origHeader;
    if ((err = fseqRealignHeaderOffsets(&newHeader, vars, count))) goto ret;

    // write the new header to the destination file
    if ((err = fseqWriteHeader(dst, &newHeader)) ||
        (err = fseqCopyConfigBlocks(dst, src, &newHeader)) ||
        (err = fseqWriteVars(dst, &newHeader, vars, count)))
        goto ret;

    int64_t copied;
    if ((copied = fseqCopyChannelData(dst, src, &newHeader)) <= 0) {
        err = (int) copied;
        goto ret;
    }

    printf("copied %" PRIi64 " bytes of frame data\n", copied);

ret:
    FC_close(src);
    FC_close(dst);

    return err;
}

/// @brief Appends the given variable to the array of sequence variables. The
/// array is reallocated to accommodate the new variable. The caller is
/// responsible for freeing the array and its contents.
/// @param vars out pointer to the array of sequence variables, or NULL
/// @param count out pointer to the number of variables in the array
/// @param var variable to append
/// @return 0 on success, a negative error code on failure
static int fseqVarsAppend(struct fseq_var_s** vars,
                          int* count,
                          const struct fseq_var_s* var) {
    struct fseq_var_s* newVars =
            realloc(*vars, (*count + 1) * sizeof(struct fseq_var_s));
    if (newVars == NULL) return -FP_ENOMEM;
    newVars[*count] = *var, *vars = newVars, (*count)++;
    return FP_EOK;
}

#define VAR_HEADER_SIZE 4

/// @brief Reads the variable size from the given variable data header. The
/// variable size is the total size of the variable data, minus the header (4).
/// @param head variable data header
/// @return the variable size, or -1 to indicate an invalid variable size
static int32_t fseqReadVarSize(const uint8_t* head) {
    uint16_t s = 0;
    memcpy(&s, head, sizeof(s));
    if (s <= VAR_HEADER_SIZE) return -1;
    return s - VAR_HEADER_SIZE;
}

/// @brief Reads the sequence variables from the given file path. The variables
/// are stored in a dynamically allocated array of `fseq_var_s` structs. The
/// caller is responsible for freeing the returned array and its contents.
/// @param fp file path to read from
/// @param vars out pointer to the array of sequence variables
/// @param count out pointer to the number of variables in the array
/// @return 0 on success, a negative error code on failure
static int fseqReadVars(const char* fp, struct fseq_var_s** vars, int* count) {
    assert(fp != NULL);
    assert(vars != NULL);
    assert(count != NULL);

    struct FC* fc = FC_open(fp, FC_MODE_READ);
    if (fc == NULL) return -FP_ESYSCALL;

    int err;

    uint8_t* varData = NULL;   /* variable data table buffer */
    struct tf_header_t header; /* sequence header */

    if ((err = fseqGetHeader(fc, &header))) goto ret;

    const uint16_t varDataSize =
            header.channelDataOffset - header.variableDataOffset;
    assert(varDataSize > VAR_HEADER_SIZE);

    if ((varData = malloc(varDataSize)) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    if (FC_read(fc, header.variableDataOffset, varDataSize, varData) !=
        varDataSize) {
        err = -FP_ESYSCALL;
        goto ret;
    }

    // initial zero values for out pointers
    *vars = NULL;
    *count = 0;

    uint16_t pos = 0;

    // ignore padding bytes at end (<=3), or ending 0-length value
    // 0-length values could be technically supported?
    while (pos < varDataSize - VAR_HEADER_SIZE) {
        // only valid until `pos` is modified at end of block
        const uint8_t* head = &varData[pos];

        const int32_t varLen = fseqReadVarSize(head);
        if (varLen <= 0) {
            err = -FP_EDECODE;
            goto ret;
        }

        // size includes 4-byte structure, manually offset
        char* const varString = malloc(varLen);
        if (varString == NULL) {
            err = -FP_ENOMEM;
            goto ret;
        }

        memcpy(varString, &head[4], varLen);

        // populate new variable and expand array
        const struct fseq_var_s newVar = {
                .id = {head[2], head[3]},
                .size = varLen,
                .value = varString,
        };
        if ((err = fseqVarsAppend(vars, count, &newVar))) goto ret;

        // advance position forward for next loop's `*head`
        pos += varLen + VAR_HEADER_SIZE;
    }

ret:
    if (err) {
        free(*vars);
        *vars = NULL, *count = 0;
    }

    free(varData);
    FC_close(fc);

    return err;
}

/// @brief Prints the sequence variables to the standard output.
/// @param vars array of sequence variables
/// @param count number of variables in the array
static void printVars(const struct fseq_var_s* vars, const int count) {
    for (int i = 0; i < count; i++) {
        const struct fseq_var_s* const var = &vars[i];
        char* value = var->value;
        if (value[var->size - 1] != '\0') value = "(binary data)";
        printf("%c%c\t%s\n", var->id[0], var->id[1], value);
    }
}

/// @brief Prints the usage information for the tool.
static void printUsage(void) {
    printf("Usage:\n\n"
           "mftool <fseq file>                  Enumerate sequence variables\n"
           "mftool <fseq file> <audio file>     Set sequence `mf` variable "
           "(copies file)\n");
}

/// @brief Appends the ".orig" suffix to the source file path by renaming the
/// file and renames the destination file to the initial value source file path.
/// @param sfp source file path to append ".orig" to
/// @param dfp destination file path to rename to the source file path
/// @return 0 on success, a negative error code on failure
static int renamePair(const char* const sfp, const char* const dfp) {
    // rename files to swap them
    char* const nsfp = dsprintf("%s.orig", sfp);
    if (nsfp == NULL) return -FP_ENOMEM;

    if (rename(sfp, nsfp) != 0) return -FP_ESYSCALL;
    if (rename(dfp, sfp) != 0) return -FP_ESYSCALL;

    printf("renamed `%s` to `%s`\n", sfp, nsfp);

    free(nsfp);

    return FP_EOK;
}

int main(const int argc, char** const argv) {
    if (argc < 2 ||
        (strcasecmp(argv[1], "-h") == 0 || strcasecmp(argv[1], "-help") == 0)) {
        printUsage();
        return 0;
    }

    int err;

    const char* const sfp = argv[1]; /* source file path */
    char* dfp = NULL;                /* formatted dest file path */
    struct fseq_var_s* vars = NULL;  /* sequence variables */
    int count = 0;                   /* length of variables array */

    if ((dfp = dsprintf("%s.tmp", sfp)) == NULL) {
        err = -FP_ENOMEM;
        goto exit;
    }

    if ((err = fseqReadVars(sfp, &vars, &count))) {
        fprintf(stderr, "failed to read sequence variables: %d\n", err);
        goto exit;
    }

    // print and exit if not modifying the sequence
    if (argc < 3) {
        printVars(vars, count);
        err = FP_EOK;
        goto exit;
    }

    char* newValue = strdup(argv[2]);
    if (newValue == NULL) {
        err = -FP_ENOMEM;
        goto exit;
    }

    const size_t newValueSize = strlen(newValue) + 1;
    if (newValueSize > UINT16_MAX) {
        err = -FP_ERANGE;
        goto exit;
    }

    // try to find a matching `mf` var to update
    for (int i = 0; i < count; i++) {
        struct fseq_var_s* const var = &vars[i];

        if (var->id[0] == 'm' && var->id[1] == 'f') {
            printf("changing `%s` to `%s`\n", var->value, newValue);
            free(var->value), var->value = newValue, var->size = newValueSize;
            goto do_copy;
        }
    }

    // no previous matching var, insert new
    const struct fseq_var_s newVar = {
            .id = {'m', 'f'},
            .size = newValueSize,
            .value = newValue,
    };
    if ((err = fseqVarsAppend(&vars, &count, &newVar))) goto exit;

do_copy:
    printf("new variable table:\n");
    printVars(vars, count);

    if ((err = fseqCopySetVars(sfp, dfp, vars, count))) goto exit;
    if ((err = renamePair(sfp, dfp))) goto exit;

exit:
    free(dfp);

    // free any allocated variable string values
    for (int i = 0; i < count; i++) free(vars[i].value);
    free(vars);

    return err ? 1 : 0;
}
