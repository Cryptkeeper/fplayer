#include "seq.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include "../libtinyfseq/tinyfseq.h"

#include "err.h"
#include "mem.h"

#define tfPrintError(err, msg)                                                 \
    do {                                                                       \
        if ((err) != TF_OK) {                                                  \
            fprintf(stderr, "libtinyfseq error (version %s)\n",                \
                    TINYFSEQ_VERSION);                                         \
            fprintf(stderr, "%s (%d)\n", tf_err_str(err), err);                \
                                                                               \
            errPrintTrace(msg);                                                \
        }                                                                      \
    } while (0)

void sequenceInit(Sequence *seq) {
    memset(seq, 0, sizeof(Sequence));

    // frame 0 is a valid frame id, use -1 as a sentinel value
    // this requires oversizing currentFrame (int64_t) against frameCount (uint32_t)
    seq->currentFrame = -1;
}

#define COMPRESSION_BLOCK_SIZE 8

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

static bool sequenceGetCompressionBlocks(FILE *f, Sequence *seq) {
    const uint8_t comBlockCount = seq->header.compressionBlockCount;

    if (comBlockCount == 0) return false;

    const uint16_t comDataSize = comBlockCount * COMPRESSION_BLOCK_SIZE;

    struct tf_compression_block_t *compressionBlocks = seq->compressionBlocks =
            malloc(comDataSize);

    assert(compressionBlocks != NULL);

    fseek(f, 32, SEEK_SET);

    if (fread(compressionBlocks, COMPRESSION_BLOCK_SIZE, comBlockCount, f) !=
        comBlockCount)
        return true;

    sequenceTrimCompressionBlockCount(seq);

    return false;
}

#define VAR_HEADER_SIZE 4
#define MAX_VAR_VALUE_SIZE 256

static void sequenceGetAudioFilePath(FILE *f, Sequence *seq) {
    const uint16_t varDataSize =
            seq->header.channelDataOffset - seq->header.variableDataOffset;

    fseek(f, seq->header.variableDataOffset, SEEK_SET);

    uint8_t b[varDataSize];

    if (fread(b, sizeof(b), 1, f) == 0) {
        perror("error while reading sequence variables table");

        return;
    }

    struct tf_var_header_t tfVarHeader;
    enum tf_err_t tfErr;

    uint8_t *readIdx = &b[0];

    uint8_t valueBuf[MAX_VAR_VALUE_SIZE];
    memset(valueBuf, 0, sizeof(valueBuf));

    // 4 is the packed sizeof(struct tf_var_header_t)
    for (uint16_t remaining = varDataSize; remaining > VAR_HEADER_SIZE;) {
        if ((tfErr = tf_read_var_header(readIdx, remaining, &tfVarHeader,
                                        valueBuf, sizeof(valueBuf),
                                        &readIdx)) != TF_OK) {
            tfPrintError(tfErr, "error when reading sequence variable header");

            return;
        }

        if (tfVarHeader.id[0] == 'm' && tfVarHeader.id[1] == 'f') {
            char *fp = seq->audioFilePath = malloc((size_t) tfVarHeader.size);

            assert(fp != NULL);
            strcpy(fp, (const char *) &valueBuf[0]);

            return;
        }

        remaining -= tfVarHeader.size;
    }
}

bool sequenceOpen(const char *filepath, Sequence *seq) {
    FILE *f;
    if ((f = seq->openFile = fopen(filepath, "rb")) == NULL) {
        perror("error while opening sequence filepath");

        return true;
    }

    uint8_t b[32];
    if (fread(b, sizeof(b), 1, f) == 0) {
        perror("error while reading sequence file header");

        return true;
    }

    enum tf_err_t tfErr;
    if ((tfErr = tf_read_file_header(b, sizeof(b), &seq->header, NULL)) !=
        TF_OK) {
        tfPrintError(tfErr, "error when deserializing sequence file header");

        return true;
    }

    if (sequenceGetCompressionBlocks(f, seq)) return true;

    sequenceGetAudioFilePath(f, seq);

    return false;
}

void sequenceFree(Sequence *seq) {
    freeAndNullWith(&seq->openFile, fclose);

    freeAndNull((void **) seq->compressionBlocks);
    freeAndNull((void **) seq->audioFilePath);
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
