#ifndef FPLAYER_MINIFIER_H
#define FPLAYER_MINIFIER_H

#include <stdint.h>

typedef void (*minify_write_fn_t)(const uint8_t *b, int size);

void minifyStream(const uint8_t *frameData,
                  uint32_t size,
                  minify_write_fn_t write);

#endif//FPLAYER_MINIFIER_H
