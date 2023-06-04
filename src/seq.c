#include "seq.h"

#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include "../libtinyfseq/tinyfseq.h"

#ifdef TINYFSEQ_MEMCPY
#undef TINYFSEQ_MEMCPY
#endif
#define TINYFSEQ_MEMCPY memcpy

#include "err.h"
#include "mem.h"

void sequenceInit(Sequence *seq) {
    memset(seq, 0, sizeof(Sequence));

    // frame 0 is a valid frame id, use -1 as a sentinel value
    // this requires oversizing currentFrame (int64_t) against frameCount (uint32_t)
    seq->currentFrame = -1;
}

static void sequenceTrimCompressionBlockCount(Sequence *seq) {
    // a fseq file may include multiple empty compression blocks for padding purposes
    // these will appear with a 0 size value, trailing previously valid blocks
    // this function finds the first instance of a zero sized block and adjusts the
    // decoded compressionBlockCount to match
    for (int i = 0; i < seq->header.compressionBlockCount; i++) {
        const struct tf_compression_block_t compressionBlock =
                seq->compressionBlocks[i];

        if (compressionBlock.size == 0) {
            printf("corrected compression block count %d->%d\n",
                   seq->header.compressionBlockCount, i);

            seq->header.compressionBlockCount = i;

            break;
        }
    }
}

#define COMPRESSION_BLOCK_SIZE 8

static void sequenceGetCompressionBlocks(FILE *f, Sequence *seq) {
    const uint8_t comBlockCount = seq->header.compressionBlockCount;

    if (comBlockCount == 0) return;

    const uint16_t comDataSize = comBlockCount * COMPRESSION_BLOCK_SIZE;

    struct tf_compression_block_t *compressionBlocks = seq->compressionBlocks =
            mustMalloc(comDataSize);

    if (fseek(f, 32, SEEK_SET) < 0) fatalf(E_FILE_IO, NULL);

    if (fread(compressionBlocks, COMPRESSION_BLOCK_SIZE, comBlockCount, f) !=
        comBlockCount)
        fatalf(E_FILE_IO, "unexpected end of compression blocks\n");

    sequenceTrimCompressionBlockCount(seq);
}

#define VAR_HEADER_SIZE 4
#define MAX_VAR_VALUE_SIZE 256

static void sequenceGetAudioFilePath(FILE *f, Sequence *seq) {
    const uint16_t varDataSize =
            seq->header.channelDataOffset - seq->header.variableDataOffset;

    if (fseek(f, seq->header.variableDataOffset, SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    uint8_t *varTable = mustMalloc(varDataSize);

    if (fread(varTable, 1, varDataSize, f) != varDataSize) goto free_and_return;

    struct tf_var_header_t varHeader;
    enum tf_err_t err;

    uint8_t *readIdx = &varTable[0];

    uint8_t *varString = mustMalloc(MAX_VAR_VALUE_SIZE);

    // 4 is the packed sizeof(struct tf_var_header_t)
    for (uint16_t remaining = varDataSize; remaining > VAR_HEADER_SIZE;) {
        if ((err = tf_read_var_header(readIdx, remaining, &varHeader, varString,
                                      MAX_VAR_VALUE_SIZE, &readIdx)) != TF_OK) {
            fatalf(E_FATAL, "error parsing sequence variable: %s\n",
                   tf_err_str(err));

            goto free_and_return;
        }

        if (varHeader.id[0] == 'm' && varHeader.id[1] == 'f') {
            char *fp = seq->audioFilePath = mustMalloc((size_t) varHeader.size);

            strlcpy(fp, (const char *) &varString[0], varHeader.size);

            goto free_and_return;
        }

        remaining -= varHeader.size;
    }

free_and_return:
    freeAndNull((void **) &varTable);
    freeAndNull((void **) &varString);
}

#define FSEQ_HEADER_SIZE 32

void sequenceOpen(const char *filepath, Sequence *seq) {
    FILE *f = seq->openFile = fopen(filepath, "rb");

    if (f == NULL)
        fatalf(E_FILE_NOT_FOUND, "error opening sequence: %s\n", filepath);

    uint8_t b[FSEQ_HEADER_SIZE];

    if (fread(b, 1, FSEQ_HEADER_SIZE, f) != FSEQ_HEADER_SIZE)
        fatalf(E_FILE_IO, NULL);

    enum tf_err_t err;

    if ((err = tf_read_file_header(b, sizeof(b), &seq->header, NULL)) != TF_OK)
        fatalf(E_FATAL, "error parsing sequence header: %s\n", tf_err_str(err));

    sequenceGetCompressionBlocks(f, seq);

    sequenceGetAudioFilePath(f, seq);
}

void sequenceFree(Sequence *seq) {
    freeAndNullWith(&seq->openFile, fclose);

    freeAndNull((void **) &seq->compressionBlocks);
    freeAndNull((void **) &seq->audioFilePath);
}

bool sequenceNextFrame(Sequence *seq) {
    if (seq->currentFrame >= seq->header.frameCount - 1) return false;

    seq->currentFrame += 1;

    return true;
}

void sequenceGetDuration(Sequence *seq, char *b, int c) {
    long framesRemaining = seq->header.frameCount;
    if (seq->currentFrame != -1) framesRemaining -= seq->currentFrame;

    const long seconds = framesRemaining / sequenceGetFPS(seq);

    snprintf(b, c, "%02ldm %02lds", seconds / 60, seconds % 60);
}
