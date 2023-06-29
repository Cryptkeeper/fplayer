#ifndef FPLAYER_FADE_H
#define FPLAYER_FADE_H

#include <stdbool.h>
#include <stdint.h>

bool fadeApplySmoothing(uint32_t id,
                        uint8_t oldIntensity,
                        uint8_t newIntensity);

#endif//FPLAYER_FADE_H
