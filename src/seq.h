#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

#include <stdint.h>
#include <stdio.h>

#include <pthread.h>

#include <tinyfseq.h>

extern FILE *gFile;
extern pthread_mutex_t gFileMutex;

void sequenceOpen(const char *filepath, const char **audioFilePath);

void sequenceFree(void);

struct tf_file_header_t *sequenceData(void);

#define sequenceFPS() (1000 / sequenceData()->frameStepTimeMillis)

#endif//FPLAYER_SEQ_H
