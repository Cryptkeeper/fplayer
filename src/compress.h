#ifndef FPLAYER_COMPRESS_H
#define FPLAYER_COMPRESS_H

#include <stdint.h>

void decompressBlock(uint32_t comBlockIndex,
                     uint8_t **frameData,
                     uint32_t *size);

#endif//FPLAYER_COMPRESS_H
