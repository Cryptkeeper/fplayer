#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "sds.h"
#include "std/err.h"
#include "std/fseq.h"
#include "std/mem.h"

static void fseqCopyConfigBlocks(FILE *const dst,
                                 const struct tf_file_header_t header,
                                 FILE *const src) {
    const uint16_t size =
            header.compressionBlockCount * 8 + header.channelRangeCount * 6;

    uint8_t *const b = mustMalloc(size);

    if (fread(b, size, 1, src) != 1)
        fatalf(E_FIO, "error reading config blocks\n");

    fwrite(b, size, 1, dst);

    free(b);
}

#define CD_CHUNK_SIZE 4096

static uint32_t fseqCopyChannelData(FILE *const src, FILE *const dst) {
    uint8_t *const chunk = mustMalloc(CD_CHUNK_SIZE);

    uint32_t copied = 0;

    while (1) {
        const size_t read = fread(chunk, 1, CD_CHUNK_SIZE, src);

        // test for error or EOF
        if (read == 0) break;

        copied += read;

        fwrite(chunk, 1, read, dst);
    }

    free(chunk);

    return copied;
}

static void
fseqOpen(const sds fp, FILE **fd, struct tf_file_header_t *const header) {
    FILE *const f = *fd = fopen(fp, "rb");

    if (f == NULL) fatalf(E_FIO, "error opening file `%s`\n", fp);

    uint8_t b[32];

    if (fread(b, sizeof(b), 1, f) != 1) fatalf(E_FIO, "error reading header\n");

    enum tf_err_t err;

    if ((err = tf_read_file_header(b, sizeof(b), header, NULL)) != TF_OK)
        fatalf(E_APP, "error decoding fseq header: %s\n", tf_err_str(err));

    if (!(header->majorVersion == 2 && header->minorVersion == 0))
        fatalf(E_APP, "unsupported fseq file version: %d.%d\n",
               header->majorVersion, header->minorVersion);
}

static void
fseqCopySetVars(const sds sfp, const sds dfp, const fseq_var_t *const vars) {
    FILE *src;
    struct tf_file_header_t original;

    fseqOpen(sfp, &src, &original);

    FILE *const dst = fopen(dfp, "wb");

    if (dst == NULL) fatalf(E_FIO, "error opening file `%s`\n", dfp);

    struct tf_file_header_t header = original;// copy original header

    fseqAlignOffsets(&header, vars);

    fseqWriteHeader(dst, header);
    fseqCopyConfigBlocks(dst, header, src);

    fseqWriteVars(dst, header, vars);

    // ensure the channel data is aligned on both files before bulk copying
    fseek(src, original.channelDataOffset, SEEK_SET);
    fseek(dst, header.channelDataOffset, SEEK_SET);

    // bulk copy channel data
    const size_t copied = fseqCopyChannelData(src, dst);

    printf("copied %zu bytes of frame data\n", copied);

    fclose(src);
    fclose(dst);
}

#define VAR_HEADER_SIZE 4

static fseq_var_t *fseqReadVars(const sds fp) {
    FILE *src;
    struct tf_file_header_t header;

    fseqOpen(fp, &src, &header);

    fseek(src, header.variableDataOffset, SEEK_SET);

    const uint16_t varDataSize =
            header.channelDataOffset - header.variableDataOffset;

    assert(varDataSize > VAR_HEADER_SIZE);

    uint8_t *const varData = mustMalloc(varDataSize);

    if (fread(varData, varDataSize, 1, src) != 1)
        fatalf(E_FIO, sdscatprintf(sdsempty(),
                                   "error reading var data blob (%d bytes)",
                                   varDataSize));

    uint16_t pos = 0;
    fseq_var_t *vars = NULL;

    // ignore padding bytes at end (<=3), or ending 0-length value
    // 0-length values could be technically supported?
    while (pos < varDataSize - VAR_HEADER_SIZE) {
        // only valid until `pos` is modified at end of block
        uint8_t *head = &varData[pos];

        const uint16_t varSize = ((uint16_t *) head)[0];
        const int32_t varLen = varSize - VAR_HEADER_SIZE;

        assert(varLen > 0);

        // let sds do the dirty work of reading and terminating the string
        const fseq_var_t var = {
                .idh = head[2],
                .idl = head[3],
                .string = sdsnewlen(&head[4], varLen),
        };

        arrput(vars, var);

        // advance position forward for next loop's `*head`
        pos += varSize;
    }

    free(varData);

    fclose(src);

    return vars;
}

static void printUsage(void) {
    printf("Usage:\n\n"
           "mftool <fseq file>                  Enumerate sequence variables\n"
           "mftool <fseq file> <audio file>     Set sequence `mf` variable "
           "(copies file)\n");
}

static void renamePair(const sds sfp, const sds dfp) {
    // rename files to swap them
    const sds nsfp = sdscatprintf(sdsempty(), "%s.orig", sfp);

    if (rename(sfp, nsfp) != 0)
        fatalf(E_SYS, "error renaming `%s` -> `%s`\n", sfp,
               nsfp);// "$" -> "$.orig"

    if (rename(dfp, sfp) != 0)
        fatalf(E_SYS, "error renaming `%s` -> `%s`\n", dfp,
               sfp);// "$.tmp" -> "$"

    printf("renamed `%s` to `%s`\n", sfp, nsfp);

    sdsfree(nsfp);
}

int main(const int argc, char **const argv) {
    if (argc < 2 ||
        (strcasecmp(argv[1], "-h") == 0 || strcasecmp(argv[1], "-help") == 0)) {
        printUsage();

        return 0;
    }

    const sds sfp = sdsnew(argv[1]); /* source file path */
    sds dfp = NULL;                  /* destination file path (source + .tmp) */

    fseq_var_t *vars = fseqReadVars(sfp);

    // print and exit if not modifying the sequence
    if (argc < 3) {
        for (size_t i = 0; i < arrlenu(vars); i++) {
            const fseq_var_t var = vars[i];

            printf("%c%c\t%s\n", var.idh, var.idl, var.string);
        }

        goto exit;
    }

    // init optional args
    dfp = sdscatprintf(sdsempty(), "%s.tmp", sfp);

    // try to find a matching `mf` var to update
    for (ssize_t i = 0; i < arrlen(vars); i++) {
        fseq_var_t *const var = &vars[i];

        if (var->idh != 'm' || var->idl != 'f') continue;

        // overwrite previous decoded string value
        sdsfree(var->string);
        var->string = sdsnew(argv[2]);

        goto do_copy;
    }

    // no previous matching var, insert new
    const fseq_var_t new = {
            .idh = 'm',
            .idl = 'f',
            .string = sdsnew(argv[2]),
    };

    arrput(vars, new);

do_copy:
    fseqCopySetVars(sfp, dfp, vars);

    renamePair(sfp, dfp);

exit:
    sdsfree(sfp);
    sdsfree(dfp);

    fseqVarsFree(vars);

    return 0;
}
