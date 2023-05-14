#ifndef FPLAYER_CMAP_H
#define FPLAYER_CMAP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct channel_range_t ChannelRange;

struct channel_range_t {
    uint16_t sid;
    uint16_t eid;

    uint8_t unit;

    uint16_t scircuit;
    uint16_t ecircuit;
};

typedef struct channel_map_t {
    ChannelRange *ranges;
    int size;
} ChannelMap;

bool channelMapInit(const char *filepath);

bool channelMapFind(uint32_t id, uint8_t *unit, uint16_t *circuit);

void channelMapFree(void);

#endif//FPLAYER_CMAP_H
