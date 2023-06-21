#ifndef FPLAYER_MINIFIER_H
#define FPLAYER_MINIFIER_H

#include <stdint.h>

typedef void (*minify_write_fn_t)(const uint8_t *b, int size);

void minifyStream(uint8_t unit,
                  uint8_t groupOffset,
                  uint8_t nCircuits,
                  const uint8_t frameData[16],
                  minify_write_fn_t write);

#endif//FPLAYER_MINIFIER_H
