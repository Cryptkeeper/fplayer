#ifndef FPLAYER_CMAP_H
#define FPLAYER_CMAP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct channel_range_t ChannelRange;

struct channel_range_t {
    uint32_t sid;
    uint32_t eid;
    uint8_t unit;
    uint16_t scircuit;
    uint16_t ecircuit;
};

void channelMapInit(const char *filepath);

bool channelMapFind(uint32_t id, uint8_t *unit, uint16_t *circuit);

uint8_t *channelMapGetUids(void);

void channelMapFree(void);

#endif//FPLAYER_CMAP_H
