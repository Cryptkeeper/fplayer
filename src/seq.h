#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

#include <stdint.h>
#include <stdio.h>

#include <pthread.h>

#include <tinyfseq.h>

typedef struct sequence_t Sequence;

extern FILE *gFile;
extern pthread_mutex_t gFileMutex;

void sequenceOpen(const char *filepath, const char **audioFilePath);

void sequenceFree(void);

struct tf_file_header_t *sequenceData(void) __attribute__((deprecated()));

enum seq_info_t {
    SI_FRAME_SIZE,
    SI_FRAME_COUNT,
    SI_FPS,
};

uint32_t sequenceGet(enum seq_info_t info);

uint32_t sequenceCompressionBlockSize(int i);

#endif//FPLAYER_SEQ_H
