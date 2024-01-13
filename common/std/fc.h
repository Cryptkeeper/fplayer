#ifndef FPLAYER_FC_H
#define FPLAYER_FC_H

#include <stdint.h>

typedef struct FC *FCHandle;

FCHandle FC_open(const char *fp);

void FC_close(FCHandle fc);

void FC_read(FCHandle fc, uint32_t offset, uint32_t size, uint8_t *b);

uint32_t FC_readto(FCHandle fc,
                   uint32_t offset,
                   uint32_t size,
                   uint32_t maxCount,
                   uint8_t *b);

uint32_t FC_filesize(FCHandle fc);

const char *FC_filepath(FCHandle fc);

#endif//FPLAYER_FC_H
