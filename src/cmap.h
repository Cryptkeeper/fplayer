#ifndef FPLAYER_CMAP_H
#define FPLAYER_CMAP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct channel_node_t {
    uint16_t id;
    uint8_t unit;
    uint16_t circuit;
} ChannelNode;

typedef struct channel_map_t {
    ChannelNode *nodes;
    uint16_t size;
} ChannelMap;

bool channelMapInit(ChannelMap *map, const char *filepath);

bool channelMapGet(const ChannelMap *map, uint32_t id, ChannelNode **node);

void channelMapFree(ChannelMap *map);

ChannelMap *channelMapInstance(void);

#endif//FPLAYER_CMAP_H
