#include "seq.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyfseq.h"

#include "std/err.h"

static FILE *gFile;
static pthread_mutex_t gFileMutex = PTHREAD_MUTEX_INITIALIZER;

static struct tf_file_header_t gPlaying;

// 4 is the packed sizeof(struct tf_var_header_t)
#define VAR_HEADER_SIZE    4
#define MAX_VAR_VALUE_SIZE 512

static char *sequenceLoadAudioFilePath(void) {
    const uint16_t varDataSize =
            gPlaying.channelDataOffset - gPlaying.variableDataOffset;

    pthread_mutex_lock(&gFileMutex);

    if (fseek(gFile, gPlaying.variableDataOffset, SEEK_SET) < 0)
        fatalf(E_FIO, NULL);

    uint8_t *varTable = mustMalloc(varDataSize);

    if (fread(varTable, 1, varDataSize, gFile) != varDataSize)
        fatalf(E_FIO, NULL);

    pthread_mutex_unlock(&gFileMutex);

    struct tf_var_header_t varHeader;
    enum tf_err_t err;

    uint8_t *readIdx = &varTable[0];

    void *varData = mustMalloc(MAX_VAR_VALUE_SIZE);

    char *audioFilePath = NULL;

    for (int remaining = varDataSize; remaining > VAR_HEADER_SIZE;) {
        if ((err = tf_read_var_header(readIdx, remaining, &varHeader, varData,
                                      MAX_VAR_VALUE_SIZE, &readIdx)) != TF_OK)
            fatalf(E_APP, "error parsing sequence variable: %s\n",
                   tf_err_str(err));

        // size includes 4-byte structure, manually offset
        const size_t varLen = varHeader.size - VAR_HEADER_SIZE;
        char *const varString = mustMalloc(varLen);

        // ensures string is NULL terminated and can truncate if necessary
        memcpy(varString, varData, varLen - 1);
        varString[varLen - 1] = '\0';

        printf("var '%c%c': %s\n", varHeader.id[0], varHeader.id[1], varString);

        // mf = Media File variable, contains audio filepath
        // caller is responsible for freeing `audioFilePath` copy return
        if (varHeader.id[0] == 'm' && varHeader.id[1] == 'f')
            if (audioFilePath == NULL) audioFilePath = mustStrdup(varString);

        free(varString);

        remaining -= varHeader.size;
    }

    free(varData);
    free(varTable);

    return audioFilePath;
}

#define FSEQ_HEADER_SIZE 32

void sequenceOpen(const char *const filepath, char **const audioFilePath) {
    FILE *const f = gFile = fopen(filepath, "rb");

    if (f == NULL) fatalf(E_FIO, "error opening sequence: %s\n", filepath);

    pthread_mutex_lock(&gFileMutex);

    uint8_t b[FSEQ_HEADER_SIZE];

    if (fread(b, 1, FSEQ_HEADER_SIZE, f) != FSEQ_HEADER_SIZE)
        fatalf(E_FIO, NULL);

    pthread_mutex_unlock(&gFileMutex);

    enum tf_err_t err;

    if ((err = tf_read_file_header(b, sizeof(b), &gPlaying, NULL)) != TF_OK)
        fatalf(E_APP, "error parsing sequence header: %s\n", tf_err_str(err));

    *audioFilePath = sequenceLoadAudioFilePath();
}

uint32_t sequenceReadFrames(const struct seq_read_args_t args,
                            uint8_t *const frameData) {
    pthread_mutex_lock(&gFileMutex);

    uint32_t frameCount = args.frameCount;

    // ensure the requested frame count does not exceed the total frame count
    if (args.startFrame + args.frameCount > gPlaying.frameCount)
        frameCount = gPlaying.frameCount - args.startFrame;

    const uint32_t pos = sequenceData()->channelDataOffset +
                         args.startFrame * args.frameSize;

    if (fseek(gFile, pos, SEEK_SET) < 0) fatalf(E_FIO, NULL);

    const size_t framesRead =
            fread(frameData, args.frameSize, frameCount, gFile);

    pthread_mutex_unlock(&gFileMutex);

    return framesRead;
}

void sequenceRead(const uint32_t start, const uint32_t n, void *const data) {
    pthread_mutex_lock(&gFileMutex);

    if (fseek(gFile, start, SEEK_SET) < 0) fatalf(E_FIO, NULL);

    const size_t read = fread(data, 1, n, gFile);
    if (read != n)
        fatalf(E_FIO, "read " PRIu64 " bytes, wanted %d", (uint64_t) read, n);

    pthread_mutex_unlock(&gFileMutex);
}

void sequenceFree(void) {
    pthread_mutex_lock(&gFileMutex);

    if (gFile != NULL) {
        fclose(gFile);

        gFile = NULL;
    }

    pthread_mutex_unlock(&gFileMutex);

    gPlaying = (struct tf_file_header_t){0};
}

struct tf_file_header_t *sequenceData(void) {
    return &gPlaying;
}
