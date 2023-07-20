#include "seq.h"

#include <sds.h>
#include <tinyfseq.h>

#include "std/err.h"
#include "std/mem.h"

FileMutex gFile;

static struct tf_file_header_t gPlaying;

// 4 is the packed sizeof(struct tf_var_header_t)
#define VAR_HEADER_SIZE    4
#define MAX_VAR_VALUE_SIZE 512

static sds sequenceLoadAudioFilePath(void) {
    const uint16_t varDataSize =
            gPlaying.channelDataOffset - gPlaying.variableDataOffset;

    FILE *f;
    fileMutexLock(&gFile, &f);

    if (fseek(f, gPlaying.variableDataOffset, SEEK_SET) < 0)
        fatalf(E_FILE_IO, NULL);

    uint8_t *varTable = mustMalloc(varDataSize);

    if (fread(varTable, 1, varDataSize, f) != varDataSize)
        fatalf(E_FILE_IO, NULL);

    fileMutexUnlock(&gFile, &f);

    struct tf_var_header_t varHeader;
    enum tf_err_t err;

    uint8_t *readIdx = &varTable[0];

    void *varData = mustMalloc(MAX_VAR_VALUE_SIZE);

    sds audioFilePath = NULL;

    for (int remaining = varDataSize; remaining > VAR_HEADER_SIZE;) {
        if ((err = tf_read_var_header(readIdx, remaining, &varHeader,
                                      (uint8_t *) varData, MAX_VAR_VALUE_SIZE,
                                      &readIdx)) != TF_OK)
            fatalf(E_FATAL, "error parsing sequence variable: %s\n",
                   tf_err_str(err));

        // ensure the variable string value is null terminated
        // size includes 4-byte structure, manually offset
        sds varString = sdsnewlen(varData, varHeader.size - VAR_HEADER_SIZE);

        printf("var '%c%c': %s\n", varHeader.id[0], varHeader.id[1], varString);

        // mf = Media File variable, contains audio filepath
        // caller is responsible for freeing `audioFilePath` copy return
        if (varHeader.id[0] == 'm' && varHeader.id[1] == 'f')
            if (audioFilePath == NULL) audioFilePath = sdsdup(varString);

        sdsfree(varString);

        remaining -= varHeader.size;
    }

    freeAndNull((void **) &varData);
    freeAndNull((void **) &varTable);

    return audioFilePath;
}

#define FSEQ_HEADER_SIZE 32

static void sequenceInitMutex(sds filepath) {
    FILE *const f = fopen(filepath, "rb");

    if (f == NULL)
        fatalf(E_FILE_NOT_FOUND, "error opening sequence: %s\n", filepath);

    // init FileMutex, this is used to manage file locking across threads
    // this assumes no `gFile` is already initialized and is now a leaking reference
    fileMutexInit(&gFile, f);
}

void sequenceOpen(sds filepath, sds *const audioFilePath) {
    sequenceInitMutex(filepath);

    FILE *f;
    fileMutexLock(&gFile, &f);

    uint8_t b[FSEQ_HEADER_SIZE];

    if (fread(b, 1, FSEQ_HEADER_SIZE, f) != FSEQ_HEADER_SIZE)
        fatalf(E_FILE_IO, NULL);

    fileMutexUnlock(&gFile, &f);

    enum tf_err_t err;

    if ((err = tf_read_file_header(b, sizeof(b), &gPlaying, NULL)) != TF_OK)
        fatalf(E_FATAL, "error parsing sequence header: %s\n", tf_err_str(err));

    *audioFilePath = sequenceLoadAudioFilePath();
}

void sequenceFree(void) {
    fileMutexClose(&gFile);

    gFile = (FileMutex){0};
    gPlaying = (struct tf_file_header_t){0};
}

struct tf_file_header_t *sequenceData(void) {
    return &gPlaying;
}
