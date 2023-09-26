#ifndef FPLAYER_BUFFER_H
#define FPLAYER_BUFFER_H

#include "lightorama/coretypes.h"

#include <stddef.h>

extern LorBuffer gWriteBuffer;

size_t writeBufferFlush(void);

#endif//FPLAYER_BUFFER_H
