#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <sds.h>

#include "../libtinyfseq/tinyfseq.h"

typedef struct sequence_t {
    FILE *openFile;

    struct tf_file_header_t header;
    struct tf_compression_block_t *compressionBlocks;

    char *audioFilePath;

    int64_t currentFrame;
} Sequence;

void sequenceInit(Sequence *seq);

void sequenceOpen(const char *filepath, Sequence *seq);

void sequenceFree(Sequence *seq);

bool sequenceNextFrame(Sequence *seq);

uint32_t sequenceGetFrameSize(const Sequence *seq);

int sequenceGetFPS(const Sequence *seq);

sds sequenceGetRemaining(const Sequence *seq);

#endif//FPLAYER_SEQ_H
