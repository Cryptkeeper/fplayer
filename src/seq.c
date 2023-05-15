#include "seq.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include "../libtinyfseq/tinyfseq.h"

#include "compress.h"
#include "err.h"

#define tfPrintError(err, msg)                                                 \
    do {                                                                       \
        if (err != TF_OK) {                                                    \
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
    if ((seq->openFile = f = fopen(filepath, "rb")) == NULL) {
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
    if (seq->openFile != NULL) fclose(seq->openFile);
    seq->openFile = NULL;

    free(seq->compressionBlocks);
    seq->compressionBlocks = NULL;

    free(seq->audioFilePath);
    seq->audioFilePath = NULL;

    free(seq->currentFrameData);
    seq->currentFrameData = NULL;

    free(seq->lastFrameData);
    seq->lastFrameData = NULL;
}

bool sequenceNextFrame(Sequence *seq) {
    if (seq->currentFrame >= seq->header.frameCount) return false;

    const uint32_t frameSize = sequenceGetFrameSize(seq);

    uint8_t *lastFrameData = seq->lastFrameData;
    if (lastFrameData == NULL)
        lastFrameData = seq->lastFrameData = calloc(frameSize, 1);

    assert(lastFrameData != NULL);

    uint8_t *frameData = seq->currentFrameData;
    if (frameData == NULL)
        frameData = seq->currentFrameData = calloc(frameSize, 1);

    assert(frameData != NULL);

    FILE *f;
    assert((f = seq->openFile) != NULL);

    // copy previous frame data prior to overwrite with new data
    // this allows the program to more easily diff between the two frames
    if (seq->currentFrame > 0) memcpy(lastFrameData, frameData, frameSize);

    seq->currentFrame += 1;

    const uint32_t frameReadIdx = seq->currentFrame * frameSize;

    if (fseek(f, frameReadIdx, SEEK_SET) != 0 ||
        fread(frameData, frameSize, 1, f) != 1) {
        fprintf(stderr,
                "error when seeking to next frame read position: %d %d\n",
                ferror(f), feof(f));

        return false;
    }

    return true;
}

size_t sequenceGetFrameSize(const Sequence *seq) {
    return seq->header.channelCount * sizeof(uint8_t);
}

void sequenceGetDuration(Sequence *seq, char *b, int c) {
    const int fps = 1000 / seq->header.frameStepTimeMillis;

    long framesRemaining = seq->header.frameCount;
    if (seq->currentFrame != -1) framesRemaining -= seq->currentFrame;

    const long seconds = framesRemaining / fps;

    snprintf(b, c, "%02ldm %02lds", seconds / 60, seconds % 60);
}
