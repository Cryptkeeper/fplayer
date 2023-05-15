#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../libtinyfseq/tinyfseq.h"

typedef struct sequence_t {
    FILE *openFile;

    struct tf_file_header_t header;

    char *audioFilePath;

    int64_t currentFrame;
    uint8_t *currentFrameData;

    uint8_t *lastFrameData;
} Sequence;

void sequenceInit(Sequence *seq);

bool sequenceOpen(const char *filepath, Sequence *seq);

void sequenceFree(Sequence *seq);

bool sequenceNextFrame(Sequence *seq);

size_t sequenceGetFrameSize(const Sequence *seq);

void sequenceGetDuration(Sequence *seq, char *b, int c);

#endif//FPLAYER_SEQ_H
