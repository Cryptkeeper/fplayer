#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

#include <stdint.h>
#include <stdio.h>

#include <sds.h>
#include <tinyfseq.h>

#include "std/mutex.h"

extern FileMutex gFile;

void sequenceOpen(sds filepath, sds *audioFilePath);

void sequenceFree(void);

struct tf_file_header_t *sequenceData(void);

#define sequenceFPS() (1000 / sequenceData()->frameStepTimeMillis)

#endif//FPLAYER_SEQ_H
