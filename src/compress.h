#ifndef FPLAYER_COMPRESS_H
#define FPLAYER_COMPRESS_H

#include <stdint.h>

#include "seq.h"

void decompressBlock(Sequence *seq,
                     int comBlockIndex,
                     uint8_t **frameData,
                     uint32_t *size);

#endif//FPLAYER_COMPRESS_H
