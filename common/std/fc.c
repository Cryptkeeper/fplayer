#include "fc.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "err.h"

struct FC {
    char *fp;              /* duplicate of filepath provided to `FC_open` */
    FILE *file;            /* file pointer */
    pthread_mutex_t mutex; /* mutex for file access */
};

FCHandle FC_open(const char *const fp) {
    FILE *f = fopen(fp, "rb");
    if (f == NULL) fatalf(E_FIO, "error opening file: %s", fp);

    struct FC *const fc = mustMalloc(sizeof(struct FC));
    fc->fp = mustStrdup(fp);
    fc->file = f;

    int err;
    if ((err = pthread_mutex_init(&fc->mutex, NULL)) != 0)
        fatalf(E_SYS, "error initializing mutex: %d\n", err);

    return fc;
}

void FC_close(const FCHandle fc) {
    struct FC *const s = fc;

    if (s->file != NULL) fclose(s->file);

    int err;
    if ((err = pthread_mutex_destroy(&s->mutex)) != 0)
        fatalf(E_SYS, "error destroying mutex: %d\n", err);

    free(fc->fp);
    free(fc);
}

void FC_read(const FCHandle fc,
             const uint32_t offset,
             const uint32_t size,
             uint8_t *const b) {
    struct FC *const s = fc;

    pthread_mutex_lock(&s->mutex);

    if (fseek(s->file, offset, SEEK_SET) < 0) fatalf(E_FIO, NULL);
    if (fread(b, 1, size, s->file) != size) fatalf(E_FIO, NULL);

    pthread_mutex_unlock(&s->mutex);
}

uint32_t FC_readto(const FCHandle fc,
                   const uint32_t offset,
                   const uint32_t size,
                   const uint32_t maxCount,
                   uint8_t *const b) {
    struct FC *const s = fc;

    pthread_mutex_lock(&s->mutex);

    if (fseek(s->file, offset, SEEK_SET) < 0) fatalf(E_FIO, NULL);

    const size_t readCount = fread(b, size, maxCount, s->file);
    if (readCount == 0) fatalf(E_FIO, "unexpected EOF");

    pthread_mutex_unlock(&s->mutex);

    return readCount;
}

const char *FC_filepath(FCHandle fc) {
    return fc->fp;
}
