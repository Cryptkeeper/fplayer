#ifndef FPLAYER_COMBLOCK_H
#define FPLAYER_COMBLOCK_H

#include <stdint.h>

void comBlocksInit(void);

void comBlockGet(int index, uint8_t **frameData, uint32_t *size);

void comBlocksFree(void);

#endif//FPLAYER_COMBLOCK_H
