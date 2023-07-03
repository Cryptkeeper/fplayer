#include "seq.h"

#include <assert.h>
#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#ifdef TINYFSEQ_MEMCPY
#undef TINYFSEQ_MEMCPY
#endif
#define TINYFSEQ_MEMCPY memcpy

#include "std/err.h"
#include "std/mem.h"

struct sequence_t {
    struct tf_file_header_t header;
    struct tf_compression_block_t *compressionBlocks;
};

FILE *gFile;
pthread_mutex_t gFileMutex = PTHREAD_MUTEX_INITIALIZER;

static Sequence gPlaying;

#define COMPRESSION_BLOCK_SIZE 8

static void sequenceTrimCompressionBlockCount(void) {
    // a fseq file may include multiple empty compression blocks for padding purposes
    // these will appear with a 0 size value, trailing previously valid blocks
    // this function finds the first instance of a zero sized block and adjusts the
    // decoded compressionBlockCount to match
    for (int i = 0; i < gPlaying.header.compressionBlockCount; i++) {
        if (gPlaying.compressionBlocks[i].size == 0) {
            printf("shrinking compression block count %d->%d\n",
                   gPlaying.header.compressionBlockCount, i);

            // re-allocate the backing array to trim the empty compression block structs
            // this only saves a few bytes of memory, but more importantly ensures the
            // `compressionBlockCount` field accurately represents the `compressionBlocks`
            // array allocation length
            gPlaying.compressionBlocks = mustRealloc(
                    gPlaying.compressionBlocks, i * COMPRESSION_BLOCK_SIZE);

            gPlaying.header.compressionBlockCount = i;

            return;
        }
    }
}

static void sequenceLoadCompressionBlocks(void) {
    const uint8_t comBlockCount = gPlaying.header.compressionBlockCount;

    if (comBlockCount == 0) return;

    const uint16_t comDataSize = comBlockCount * COMPRESSION_BLOCK_SIZE;

    struct tf_compression_block_t *compressionBlocks =
            gPlaying.compressionBlocks = mustMalloc(comDataSize);

    pthread_mutex_lock(&gFileMutex);

    if (fseek(gFile, 32, SEEK_SET) < 0) fatalf(E_FILE_IO, NULL);

    if (fread(compressionBlocks, COMPRESSION_BLOCK_SIZE, comBlockCount,
              gFile) != comBlockCount)
        fatalf(E_FILE_IO, "unexpected end of compression blocks\n");

    pthread_mutex_unlock(&gFileMutex);

    sequenceTrimCompressionBlockCount();
}

// 4 is the packed sizeof(struct tf_var_header_t)
#define VAR_HEADER_SIZE    4
#define MAX_VAR_VALUE_SIZE 512

static const char *sequenceLoadAudioFilePath(void) {
    const uint16_t varDataSize = gPlaying.header.channelDataOffset -
                                 gPlaying.header.variableDataOffset;

    pthread_mutex_lock(&gFileMutex);

    if (fseek(gFile, gPlaying.header.variableDataOffset, SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    uint8_t *varTable = mustMalloc(varDataSize);

    if (fread(varTable, 1, varDataSize, gFile) != varDataSize)
        fatalf(E_FILE_IO, NULL);

    pthread_mutex_unlock(&gFileMutex);

    struct tf_var_header_t varHeader;
    enum tf_err_t err;

    uint8_t *readIdx = &varTable[0];

    char *varString = mustMalloc(MAX_VAR_VALUE_SIZE);

    char *audioFilePath = NULL;

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
            audioFilePath = mustStrdup(varString);

        remaining -= varHeader.size;
    }

    freeAndNull((void **) &varTable);
    freeAndNull((void **) &varString);

    return audioFilePath;
}

#define FSEQ_HEADER_SIZE 32

void sequenceOpen(const char *const filepath,
                  const char **const audioFilePath) {
    pthread_mutex_lock(&gFileMutex);

    FILE *const f = gFile = fopen(filepath, "rb");

    if (f == NULL)
        fatalf(E_FILE_NOT_FOUND, "error opening sequence: %s\n", filepath);

    uint8_t b[FSEQ_HEADER_SIZE];

    if (fread(b, 1, FSEQ_HEADER_SIZE, f) != FSEQ_HEADER_SIZE)
        fatalf(E_FILE_IO, NULL);

    pthread_mutex_unlock(&gFileMutex);

    enum tf_err_t err;

    if ((err = tf_read_file_header(b, sizeof(b), &gPlaying.header, NULL)) !=
        TF_OK)
        fatalf(E_FATAL, "error parsing sequence header: %s\n", tf_err_str(err));

    sequenceLoadCompressionBlocks();

    *audioFilePath = sequenceLoadAudioFilePath();
}

void sequenceFree(void) {
    pthread_mutex_lock(&gFileMutex);

    freeAndNullWith(&gFile, fclose);

    pthread_mutex_unlock(&gFileMutex);

    freeAndNull((void **) &gPlaying.compressionBlocks);

    gPlaying = (Sequence){0};
}

struct tf_file_header_t *sequenceData(void) {
    return &gPlaying.header;
}

uint32_t sequenceGet(const enum seq_info_t info) {
    switch (info) {
        case SI_FRAME_SIZE:
            return gPlaying.header.channelCount;
        case SI_FRAME_COUNT:
            return gPlaying.header.frameCount;
        case SI_FPS:
            return 1000 / gPlaying.header.frameStepTimeMillis;
    }
}

uint32_t sequenceCompressionBlockSize(int i) {
    assert(i >= 0 && i < gPlaying.header.compressionBlockCount);

    return gPlaying.compressionBlocks[i].size;
}
