#ifndef FPLAYER_COMBLOCK_H
#define FPLAYER_COMBLOCK_H

#include <stdint.h>

void comBlocksInit(void);

uint8_t **comBlockGet(int index);

void comBlocksFree(void);

#endif//FPLAYER_COMBLOCK_H
