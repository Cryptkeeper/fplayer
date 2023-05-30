#ifndef FPLAYER_COMPRESS_H
#define FPLAYER_COMPRESS_H

#include <stdbool.h>
#include <stdint.h>

#include "seq.h"

bool decompressBlock(Sequence *seq,
                     int comBlockIndex,
                     uint8_t **frameData,
                     uint32_t *size);

#endif//FPLAYER_COMPRESS_H
