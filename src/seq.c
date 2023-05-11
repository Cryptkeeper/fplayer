#include "seq.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include "../libtinyfseq/tinyfseq.h"

#define tfPrintError(err, msg)                                                 \
    if (err != TF_OK) {                                                        \
        fprintf(stderr, "libtinyfseq error (version %s)\n", TINYFSEQ_VERSION); \
        fprintf(stderr, "%s (%d)\n", tf_err_str(err), err);                    \
        fprintf(stderr, "%s\n", msg);                                          \
        fprintf(stderr, "%s#L%d\n", __FILE_NAME__, __LINE__ - 1);              \
    }

void sequenceInit(Sequence *seq) {
    memset(seq, 0, sizeof(Sequence));

    // frame 0 is a valid frame id, use -1 as a sentinel value
    // this requires oversizing currentFrame (int64_t) against frameCount (uint32_t)
    seq->currentFrame = -1;
}

#define MAX_VAR_VALUE_SIZE 256

static void sequenceGetAudioFilePath(FILE *f, struct tf_file_header_t tfHeader,
                                     Sequence *seq) {
    const uint16_t varDataSize =
            tfHeader.channelDataOffset - tfHeader.variableDataOffset;

    uint8_t b[varDataSize];

    if (fread(b, sizeof(b), 1, f) == 0) {
        perror("error while reading sequence sequence variables table");

        return;
    }

    struct tf_var_header_t tfVarHeader;
    enum tf_err_t tfErr;

    uint8_t *readIdx = &b[0];

    uint8_t valueBuf[MAX_VAR_VALUE_SIZE];
    memset(valueBuf, 0, sizeof(valueBuf));

    // 4 is the packed sizeof(struct tf_var_header_t)
    for (uint16_t remaining = varDataSize; remaining > 4;) {
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
    if ((seq->openFile = f = fopen(filepath, "rb")) == NULL) {
        perror("error while opening sequence filepath");

        return true;
    }

    uint8_t b[32];
    if (fread(b, sizeof(b), 1, f) == 0) {
        perror("error while reading sequence file header");

        return true;
    }

    struct tf_file_header_t tfHeader;
    enum tf_err_t tfErr;
    if ((tfErr = tf_read_file_header(b, sizeof(b), &tfHeader, NULL)) != TF_OK) {
        tfPrintError(tfErr, "error when deserializing sequence file header");

        return true;
    }

    if (tfHeader.compressionType != TF_COMPRESSION_NONE) {
        fprintf(stderr, "compressed fseq files are currently unsupported\n");
        fprintf(stderr, "decompress your sequence using fsequtils first\n");

        return true;
    }

    seq->channelCount = tfHeader.channelCount;
    seq->frameCount = tfHeader.frameCount;
    seq->frameStepTimeMillis = tfHeader.frameStepTimeMillis;

    sequenceGetAudioFilePath(f, tfHeader, seq);

    return false;
}

void sequenceFree(Sequence *seq) {
    FILE *f;
    if ((f = seq->openFile) != NULL) {
        seq->openFile = NULL;

        fclose(f);
    }

    char *audioFilePath;
    if ((audioFilePath = seq->audioFilePath) != NULL) {
        seq->audioFilePath = NULL;

        free(audioFilePath);
    }

    seq->currentFrameData = NULL;
}

bool sequenceNextFrame(Sequence *seq) {
    if (seq->currentFrame >= seq->frameCount) return false;

    uint8_t *frameData = seq->currentFrameData;
    if (frameData == NULL)
        frameData = seq->currentFrameData = malloc(seq->channelCount);

    assert(frameData != NULL);

    FILE *f;
    assert((f = seq->openFile) != NULL);

    seq->currentFrame += 1;

    const uint32_t frameReadIdx = seq->currentFrame * seq->channelCount;

    if (fseek(f, frameReadIdx, SEEK_SET) != 0 ||
        fread(frameData, seq->channelCount, 1, f) != 1) {
        perror("error when seeking to next frame read position");

        return false;
    }

    return true;
}

void sequenceGetDuration(Sequence *seq, char *b, int c) {
    const int fps = 1000 / seq->frameStepTimeMillis;

    long framesRemaining = seq->frameCount;
    if (seq->currentFrame != -1) framesRemaining -= seq->currentFrame;

    const long seconds = framesRemaining / fps;

    snprintf(b, c, "%02ldm %02lds", seconds / 60, seconds % 60);
}
