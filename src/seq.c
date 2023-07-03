#include "seq.h"

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#include "stb_ds.h"

#include "std/err.h"
#include "std/mem.h"

FILE *gFile;
pthread_mutex_t gFileMutex = PTHREAD_MUTEX_INITIALIZER;

static struct tf_file_header_t gPlaying;

// 4 is the packed sizeof(struct tf_var_header_t)
#define VAR_HEADER_SIZE    4
#define MAX_VAR_VALUE_SIZE 512

static const char *sequenceLoadAudioFilePath(void) {
    const uint16_t varDataSize =
            gPlaying.channelDataOffset - gPlaying.variableDataOffset;

    pthread_mutex_lock(&gFileMutex);

    if (fseek(gFile, gPlaying.variableDataOffset, SEEK_SET) < 0)
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

    if ((err = tf_read_file_header(b, sizeof(b), &gPlaying, NULL)) != TF_OK)
        fatalf(E_FATAL, "error parsing sequence header: %s\n", tf_err_str(err));

    *audioFilePath = sequenceLoadAudioFilePath();
}

void sequenceFree(void) {
    pthread_mutex_lock(&gFileMutex);

    freeAndNullWith(&gFile, fclose);

    pthread_mutex_unlock(&gFileMutex);

    gPlaying = (struct tf_file_header_t){0};
}

struct tf_file_header_t *sequenceData(void) {
    return &gPlaying;
}
