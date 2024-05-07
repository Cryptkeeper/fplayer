#ifndef FPLAYER_MINIFIER_H
#define FPLAYER_MINIFIER_H

#include <stdint.h>

void minifyStream(const uint8_t* frameData,
                  const uint8_t* lastFrameData,
                  uint32_t size);

#endif//FPLAYER_MINIFIER_H
