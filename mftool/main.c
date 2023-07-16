#undef NDEBUG
#include <assert.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#include <sds.h>

#define VAR_HEADER_SIZE 4

struct var_t {
    uint8_t idh;
    uint8_t idl;
    sds string;
};

static struct tf_file_header_t fseqResize(const struct tf_file_header_t header,
                                          const struct var_t *const vars) {
    uint16_t varDataSize = 0;

    for (size_t i = 0; i < arrlen(vars); i++)
        varDataSize += sdslen(vars[i].string) + VAR_HEADER_SIZE + 1;

    // round to nearest product of 4 for 32-bit alignment
    const size_t rem = varDataSize % 4;
    if (rem != 0) varDataSize += 4 - rem;

    // ensure the value can be safely downcasted to what the file format expects
    assert(varDataSize <= UINT16_MAX);

    const uint16_t firstVarOffset = 32 + header.compressionBlockCount * 8 +
                                    header.channelRangeCount * 6;

    struct tf_file_header_t resized = header;

    resized.variableDataOffset = firstVarOffset;
    resized.channelDataOffset = firstVarOffset + varDataSize;

    return resized;
}

static void fseqWriteHeader(FILE *const dst,
                            const struct tf_file_header_t header) {
    const uint8_t magic[4] = {'P', 'S', 'E', 'Q'};

    fwrite(magic, sizeof(magic), 1, dst);

    fwrite(&header.channelDataOffset, sizeof(header.channelDataOffset), 1, dst);

    const uint8_t version[2] = {0, 2};// 2.0

    fwrite(version, sizeof(version), 1, dst);

    fwrite(&header.variableDataOffset, sizeof(header.variableDataOffset), 1,
           dst);

    fwrite(&header.channelCount, sizeof(header.channelCount), 1, dst);
    fwrite(&header.frameCount, sizeof(header.frameCount), 1, dst);

    // several single byte fields wrapped up together for brevity
    const uint8_t config[6] = {
            header.frameStepTimeMillis,
            0,// reserved flags
            (uint8_t) header.compressionType,
            header.compressionBlockCount,
            header.channelRangeCount,
            0,//reserved empty
    };

    fwrite(config, sizeof(config), 1, dst);

    fwrite(&header.sequenceUid, sizeof(header.sequenceUid), 1, dst);
}

static void fseqCopyConfigBlocks(FILE *const dst,
                                 const struct tf_file_header_t header,
                                 FILE *const src) {
    const uint16_t size =
            header.compressionBlockCount * 8 + header.channelRangeCount * 6;

    uint *const b = malloc(size);

    assert(b != NULL);

    fread(b, size, 1, src);
    fwrite(b, size, 1, dst);

    free(b);
}

static void fseqWriteVars(FILE *const dst, const struct var_t *const vars) {
    for (size_t i = 0; i < arrlen(vars); i++) {
        const struct var_t var = vars[i];

        const uint16_t size = sdslen(var.string) + VAR_HEADER_SIZE + 1;

        fwrite(&size, sizeof(size), 1, dst);

        fwrite(&var.idh, 1, 1, dst);
        fwrite(&var.idl, 1, 1, dst);

        fwrite(var.string, sdslen(var.string), 1, dst);

        const uint8_t nb = '\0';
        fwrite(&nb, sizeof(nb), 1, dst);
    }
}

#define CD_CHUNK_SIZE 4096

static uint32_t fseqCopyChannelData(FILE *const src, FILE *const dst) {
    uint *const chunk = malloc(CD_CHUNK_SIZE);

    uint32_t copied = 0;

    while (1) {
        const unsigned long read = fread(chunk, 1, CD_CHUNK_SIZE, src);

        // test for error or EOF
        if (read == 0) break;

        copied += read;

        fwrite(chunk, 1, read, dst);
    }

    free(chunk);

    return copied;
}

static void fseqOpen(const char *const fp,
                     FILE **fd,
                     struct tf_file_header_t *const header) {
    FILE *const f = *fd = fopen(fp, "rb");
    assert(f != NULL);

    uint8_t b[32];

    fread(b, sizeof(b), 1, f);

    const enum tf_err_t err = tf_read_file_header(b, sizeof(b), header, NULL);
    assert(err == TF_OK && "error decoding fseq header");
}

static void fseqCopySetVars(const char *const sfp,
                            const char *const dfp,
                            const struct var_t *const vars) {
    FILE *src;
    struct tf_file_header_t header;

    fseqOpen(sfp, &src, &header);

    FILE *const dst = fopen(dfp, "wb");
    assert(dst != NULL);

    const struct tf_file_header_t resized = fseqResize(header, vars);

    fseqWriteHeader(dst, resized);
    fseqCopyConfigBlocks(dst, resized, src);
    fseqWriteVars(dst, vars);

    // ensure the channel data is aligned on both files before bulk copying
    fseek(src, header.channelDataOffset, SEEK_SET);
    fseek(dst, resized.channelDataOffset, SEEK_SET);

    // bulk copy channel data
    const size_t copied = fseqCopyChannelData(src, dst);

    printf("copied %zu bytes of frame data\n", copied);

    fclose(src);
    fclose(dst);
}

static struct var_t *fseqReadVars(const char *const fp) {
    FILE *src;
    struct tf_file_header_t header;

    fseqOpen(fp, &src, &header);

    fseek(src, header.variableDataOffset, SEEK_SET);

    const uint16_t varDataSize =
            header.channelDataOffset - header.variableDataOffset;

    assert(varDataSize > VAR_HEADER_SIZE);

    uint8_t *const varData = malloc(varDataSize);
    assert(varData != NULL);

    fread(varData, varDataSize, 1, src);

    uint16_t pos = 0;
    struct var_t *vars = NULL;

    // ignore padding bytes at end (<=3), or ending 0-length value
    // 0-length values could be technically supported?
    while (pos < varDataSize - VAR_HEADER_SIZE) {
        // only valid until `pos` is modified at end of block
        uint8_t *head = &varData[pos];

        const uint16_t varSize = ((uint16_t *) head)[0];
        const int32_t varLen = varSize - VAR_HEADER_SIZE;

        assert(varLen > 0);

        // let sds do the dirty work of reading and terminating the string
        const struct var_t var = {
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

static void freeVars(struct var_t *vars) {
    for (size_t i = 0; i < arrlen(vars); i++) sdsfree(vars[i].string);

    arrfree(vars);
}

static void printUsage(void) {
    printf("Usage:\n\n"
           "Enumerate sequence variables: mftool <fseq file>\n"
           "Copy sequence with new `mf` variable: mftool <fseq file> <new fseq "
           "file> <new audio file>\n");
}

int main(const int argc, char **const argv) {
    if (argc < 2) {
        printUsage();

        return 0;
    }

    const char *const sfp = argv[1];

    struct var_t *vars = fseqReadVars(sfp);

    const bool isModifying = argc >= 4;

    if (!isModifying) {
        for (size_t i = 0; i < arrlen(vars); i++) {
            const struct var_t var = vars[i];

            printf("%c%c\t%s\n", var.idh, var.idl, var.string);
        }

        goto exit;
    }

    const char *const dfp = argv[2];
    const char *const mf = argv[3];

    // find matching `mf` var or insert new
    ssize_t index = -1;

    for (ssize_t i = 0; i < arrlen(vars); i++) {
        const struct var_t var = vars[i];

        if (var.idh == 'm' && var.idl == 'f') {
            index = i;
            break;
        }
    }

    if (index >= 0) {
        struct var_t *var = &vars[index];

        sdsfree(var->string);

        var->string = sdsnew(mf);
    } else {
        const struct var_t new = {
                .idh = 'm',
                .idl = 'f',
                .string = sdsnew(mf),
        };

        arrput(vars, new);
    }

    fseqCopySetVars(sfp, dfp, vars);

exit:
    freeVars(vars);

    return 0;
}