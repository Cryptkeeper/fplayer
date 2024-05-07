#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "std/err.h"
#include "std/fseq.h"
#include "std2/fc.h"
#include "std2/string.h"

static void
fseqCopyConfigBlocks(FILE* const dst, const TFHeader header, struct FC* src) {
    const uint16_t size =
            header.compressionBlockCount * 8 + header.channelRangeCount * 6;

    uint8_t* const b = mustMalloc(size);

    FC_read(src, 32, size, b);

    fwrite(b, size, 1, dst);

    free(b);
}

#define CD_CHUNK_SIZE 4096

static uint32_t
fseqCopyChannelData(struct FC* src, const TFHeader header, FILE* const dst) {
    uint8_t* const chunk = mustMalloc(CD_CHUNK_SIZE);

    uint32_t copied = 0;

    while (1) {
        const uint32_t offset = header.channelDataOffset + copied;
        const uint32_t read = FC_readto(src, offset, 1, CD_CHUNK_SIZE, chunk);

        // test for error or EOF
        if (read == 0) break;

        copied += read;

        fwrite(chunk, 1, read, dst);
    }

    free(chunk);

    return copied;
}

static void fseqGetHeader(struct FC* fc, TFHeader* const header) {
    uint8_t b[32];
    FC_read(fc, 0, sizeof(b), b);

    TFError err;
    if ((err = TFHeader_read(b, sizeof(b), header, NULL)))
        fatalf(E_APP, "error decoding fseq header: %s\n", TFError_string(err));

    if (!(header->majorVersion == 2 && header->minorVersion == 0))
        fatalf(E_APP, "unsupported fseq file version: %d.%d\n",
               header->majorVersion, header->minorVersion);
}

static void fseqCopySetVars(const char* const sfp,
                            const char* const dfp,
                            const fseq_var_t* const vars) {
    struct FC* src = FC_open(sfp);
    if (src == NULL) fatalf(E_FIO, "error opening file `%s`\n", sfp);

    TFHeader original;
    fseqGetHeader(src, &original);

    FILE* const dst = fopen(dfp, "wb");

    if (dst == NULL) fatalf(E_FIO, "error opening file `%s`\n", dfp);

    TFHeader header = original;// copy original header

    fseqAlignOffsets(&header, vars);

    fseqWriteHeader(dst, header);
    fseqCopyConfigBlocks(dst, header, src);

    fseqWriteVars(dst, header, vars);

    // ensure the channel data is aligned on both files before bulk copying
    fseek(dst, header.channelDataOffset, SEEK_SET);

    // bulk copy channel data
    const size_t copied = fseqCopyChannelData(src, header, dst);

    printf("copied %zu bytes of frame data\n", copied);

    FC_close(src);
    fclose(dst);
}

#define VAR_HEADER_SIZE 4

static fseq_var_t* fseqReadVars(const char* const fp) {
    struct FC* fc = FC_open(fp);
    if (fc == NULL) fatalf(E_FIO, "error opening file `%s`\n", fp);

    TFHeader header;
    fseqGetHeader(fc, &header);

    const uint16_t varDataSize =
            header.channelDataOffset - header.variableDataOffset;

    assert(varDataSize > VAR_HEADER_SIZE);

    uint8_t* const varData = mustMalloc(varDataSize);

    FC_read(fc, header.variableDataOffset, varDataSize, varData);

    uint16_t pos = 0;
    fseq_var_t* vars = NULL;

    // ignore padding bytes at end (<=3), or ending 0-length value
    // 0-length values could be technically supported?
    while (pos < varDataSize - VAR_HEADER_SIZE) {
        // only valid until `pos` is modified at end of block
        const uint8_t* head = &varData[pos];

        const uint16_t varSize = ((uint16_t*) head)[0];
        const int32_t varLen = (int32_t) varSize - VAR_HEADER_SIZE;

        assert(varLen > 0);

        // size includes 4-byte structure, manually offset
        char* const varString = mustMalloc(varLen);

        // ensures string is NULL terminated and can truncate if necessary
        memcpy(varString, &head[4], varLen - 1);
        varString[varLen - 1] = '\0';

        const fseq_var_t var = {
                .idh = head[2],
                .idl = head[3],
                .string = varString,
        };

        arrput(vars, var);

        // advance position forward for next loop's `*head`
        pos += varSize;
    }

    free(varData);

    FC_close(fc);

    return vars;
}

static void printUsage(void) {
    printf("Usage:\n\n"
           "mftool <fseq file>                  Enumerate sequence variables\n"
           "mftool <fseq file> <audio file>     Set sequence `mf` variable "
           "(copies file)\n");
}

static void renamePair(const char* const sfp, const char* const dfp) {
    // rename files to swap them
    char* const nsfp = dsprintf("%s.orig", sfp);

    if (rename(sfp, nsfp) != 0)
        fatalf(E_SYS, "error renaming `%s` -> `%s`\n", sfp,
               nsfp);// "$" -> "$.orig"

    if (rename(dfp, sfp) != 0)
        fatalf(E_SYS, "error renaming `%s` -> `%s`\n", dfp,
               sfp);// "$.tmp" -> "$"

    printf("renamed `%s` to `%s`\n", sfp, nsfp);

    free(nsfp);
}

int main(const int argc, char** const argv) {
    if (argc < 2 ||
        (strcasecmp(argv[1], "-h") == 0 || strcasecmp(argv[1], "-help") == 0)) {
        printUsage();

        return 0;
    }

    const char* const sfp = argv[1]; /* source file path */
    char* dfp = NULL;                /* destination file path (source + .tmp) */

    fseq_var_t* vars = fseqReadVars(sfp);

    // print and exit if not modifying the sequence
    if (argc < 3) {
        for (size_t i = 0; i < arrlenu(vars); i++) {
            const fseq_var_t var = vars[i];

            printf("%c%c\t%s\n", var.idh, var.idl, var.string);
        }

        goto exit;
    }

    // init optional args
    dfp = dsprintf("%s.tmp", sfp);

    // try to find a matching `mf` var to update
    for (ssize_t i = 0; i < arrlen(vars); i++) {
        fseq_var_t* const var = &vars[i];

        if (var->idh != 'm' || var->idl != 'f') continue;

        // overwrite previous decoded string value
        free(var->string);
        var->string = mustStrdup(argv[2]);

        goto do_copy;
    }

    // no previous matching var, insert new
    const fseq_var_t new = {
            .idh = 'm',
            .idl = 'f',
            .string = mustStrdup(argv[2]),
    };

    arrput(vars, new);

do_copy:
    fseqCopySetVars(sfp, dfp, vars);
    renamePair(sfp, dfp);

exit:
    free(dfp);

    // free any allocated variable string values
    for (ssize_t i = 0; i < arrlen(vars); i++) free(vars[i].string);

    arrfree(vars);

    return 0;
}
