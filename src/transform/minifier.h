#ifndef FPLAYER_MINIFIER_H
#define FPLAYER_MINIFIER_H

#include <stdint.h>

typedef void (*minify_write_fn_t)(const uint8_t *b, int size);

void minifyStream(const uint8_t *frameData,
                  const uint8_t *lastFrameData,
                  uint32_t size,
                  uint32_t frame,
                  minify_write_fn_t write);

#endif//FPLAYER_MINIFIER_H
