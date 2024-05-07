#include "fc.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FC {
    char* fp;              /* duplicate of filepath provided to `FC_open` */
    FILE* file;            /* file pointer */
    pthread_mutex_t mutex; /* mutex for file access */
};

struct FC* FC_open(const char* const fp) {
    struct FC* fc = calloc(1, sizeof(struct FC));
    if (fc == NULL) return NULL;
    if ((fc->file = fopen(fp, "rb")) == NULL || (fc->fp = strdup(fp)) == NULL ||
        pthread_mutex_init(&fc->mutex, NULL) != 0) {
        FC_close(fc);
        fc = NULL;
    }
    return fc;
}

void FC_close(struct FC* fc) {
    if (fc == NULL) return;
    if (fc->file != NULL) fclose(fc->file);
    free(fc->fp);
    pthread_mutex_destroy(&fc->mutex);
    free(fc);
}

uint32_t FC_read(struct FC* fc,
                 const uint32_t offset,
                 const uint32_t size,
                 uint8_t* const b) {
    uint32_t r = 0;
    pthread_mutex_lock(&fc->mutex);
    if (fseek(fc->file, offset, SEEK_SET) == 0) r = fread(b, 1, size, fc->file);
    pthread_mutex_unlock(&fc->mutex);
    return r;
}

uint32_t FC_readto(struct FC* fc,
                   const uint32_t offset,
                   const uint32_t size,
                   const uint32_t maxCount,
                   uint8_t* const b) {
    uint32_t r = 0;
    pthread_mutex_lock(&fc->mutex);
    if (fseek(fc->file, offset, SEEK_SET) == 0)
        r = fread(b, size, maxCount, fc->file);
    pthread_mutex_unlock(&fc->mutex);
    return r;
}

uint32_t FC_filesize(struct FC* fc) {
    uint32_t s = 0;
    pthread_mutex_lock(&fc->mutex);
    if (fseek(fc->file, 0, SEEK_END) == 0) s = ftell(fc->file);
    rewind(fc->file);
    pthread_mutex_unlock(&fc->mutex);
    return s;
}

const char* FC_filepath(const struct FC* fc) {
    return fc->fp;
}
