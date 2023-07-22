#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

#include <stdint.h>

#include <sds.h>
#include <tinyfseq.h>

void sequenceOpen(sds filepath, sds *audioFilePath);

struct seq_read_args_t {
    uint32_t startFrame; /* the frame index to begin reading at */
    uint32_t frameSize;  /* the byte size of each individual frame */
    uint32_t frameCount; /* the number of frames to read */
};

uint32_t sequenceReadFrames(struct seq_read_args_t args, uint8_t *frameData);

void sequenceRead(uint32_t start, uint32_t n, void *data);

void sequenceFree(void);

struct tf_file_header_t *sequenceData(void);

#define sequenceFPS() (1000 / sequenceData()->frameStepTimeMillis)

#endif//FPLAYER_SEQ_H
