#ifndef FPLAYER_COMBLOCK_H
#define FPLAYER_COMBLOCK_H

#include <stdint.h>

#include "std/fc.h"

void comBlocksInit(FCHandle fc);

uint8_t **comBlockGet(FCHandle fc, int index);

void comBlocksFree(void);

#endif//FPLAYER_COMBLOCK_H
