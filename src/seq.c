#include "seq.h"

#include <assert.h>
#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

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

#define COMPRESSION_BLOCK_SIZE 8

static void sequenceTrimCompressionBlockCount(Sequence *seq) {
    // a fseq file may include multiple empty compression blocks for padding purposes
    // these will appear with a 0 size value, trailing previously valid blocks
    // this function finds the first instance of a zero sized block and adjusts the
    // decoded compressionBlockCount to match
    for (int i = 0; i < seq->header.compressionBlockCount; i++) {
        if (seq->compressionBlocks[i].size == 0) {
            printf("shrinking compression block count %d->%d\n",
                   seq->header.compressionBlockCount, i);

            // re-allocate the backing array to trim the empty compression block structs
            // this only saves a few bytes of memory, but more importantly ensures the
            // `compressionBlockCount` field accurately represents the `compressionBlocks`
            // array allocation length
            seq->compressionBlocks = mustRealloc(seq->compressionBlocks,
                                                 i * COMPRESSION_BLOCK_SIZE);

            seq->header.compressionBlockCount = i;

            return;
        }
    }
}

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

// 4 is the packed sizeof(struct tf_var_header_t)
#define VAR_HEADER_SIZE    4
#define MAX_VAR_VALUE_SIZE 512

static void sequenceGetAudioFilePath(FILE *f, Sequence *seq) {
    const uint16_t varDataSize =
            seq->header.channelDataOffset - seq->header.variableDataOffset;

    if (fseek(f, seq->header.variableDataOffset, SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    uint8_t *varTable = mustMalloc(varDataSize);

    if (fread(varTable, 1, varDataSize, f) != varDataSize)
        fatalf(E_FILE_IO, NULL);

    struct tf_var_header_t varHeader;
    enum tf_err_t err;

    uint8_t *readIdx = &varTable[0];

    char *varString = mustMalloc(MAX_VAR_VALUE_SIZE);

    for (int remaining = varDataSize; remaining > VAR_HEADER_SIZE;) {
        if ((err = tf_read_var_header(readIdx, remaining, &varHeader,
                                      (uint8_t *) varString, MAX_VAR_VALUE_SIZE,
                                      &readIdx)) != TF_OK)
            fatalf(E_FATAL, "error parsing sequence variable: %s\n",
                   tf_err_str(err));

        // ensure the variable string value is null terminated
        // size includes 4-byte structure, manually offset
        varString[varHeader.size - 1 - VAR_HEADER_SIZE] = '\0';

        printf("var '%c%c': %s\n", varHeader.id[0], varHeader.id[1], varString);

        // mf = Media File variable, contains audio filepath
        if (varHeader.id[0] == 'm' && varHeader.id[1] == 'f')
            seq->audioFilePath = mustStrdup(varString);

        remaining -= varHeader.size;
    }

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

inline uint32_t sequenceGetFrameSize(const Sequence *seq) {
    return seq->header.channelCount;
}

inline uint32_t sequenceGetFrame(const Sequence *const seq) {
    // `currentFrame` is signed+oversized to enable -1 as a sentinel value
    // it should never be -1 in this state, and can be downcasted to its
    // true uint32 value
    assert(seq->currentFrame >= 0 && seq->currentFrame <= UINT32_MAX);

    return (uint32_t) seq->currentFrame;
}

inline int sequenceGetFPS(const Sequence *seq) {
    return 1000 / seq->header.frameStepTimeMillis;
}

sds sequenceGetRemaining(const Sequence *seq) {
    long framesRemaining = seq->header.frameCount;
    if (seq->currentFrame != -1) framesRemaining -= seq->currentFrame;

    const long seconds = framesRemaining / sequenceGetFPS(seq);

    return sdscatprintf(sdsempty(), "%02ldm %02lds", seconds / 60,
                        seconds % 60);
}
