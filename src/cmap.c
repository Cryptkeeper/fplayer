#include "cmap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void channelMapPut(ChannelMap *map, const ChannelNode *node) {
    if (node->id >= map->size) {
        const uint16_t oldSize = map->size;
        const uint16_t newSize = node->id + 1;

        map->size = newSize;
        map->nodes = realloc(map->nodes, sizeof(ChannelNode) * newSize);

        assert(map->nodes != NULL);

        memset(&map->nodes[oldSize], 0,
               sizeof(ChannelNode) * (newSize - oldSize));
    }

    memcpy(&map->nodes[node->id], node, sizeof(ChannelNode));
}

static long channelMapParseAttr(const char *b, long max) {
    const long l = strtol(b, NULL, 10);

    if (l <= 0) return 0;
    else if (l >= max)
        return max;
    else
        return l;
}

static bool channelMapParseCSV(ChannelMap *map, char *b) {
    char *lStart = NULL;
    char *lEnd = b;

    while ((lStart = strsep(&lEnd, "\n")) != NULL) {
        // ignoring empty new lines
        if (strlen(lStart) == 0) continue;

        // ignore comment lines beginning with '#'
        if (lStart[0] == '#') continue;

        char *sStart = NULL;
        char *sEnd = lStart;

        ChannelNode channelNode;

        for (int i = 0; i < 3; i++) {
            sStart = strsep(&sEnd, ",");
            assert(sStart != NULL);

            switch (i) {
                case 0:
                    channelNode.id = channelMapParseAttr(sStart, UINT16_MAX);
                    break;
                case 1:
                    channelNode.unit = channelMapParseAttr(sStart, UINT8_MAX);
                    break;
                case 2:
                    channelNode.circuit =
                            channelMapParseAttr(sStart, UINT16_MAX);
                    break;
                default:
                    continue;
            }

            channelMapPut(map, &channelNode);
        }
    }

    return false;
}

bool channelMapInit(ChannelMap *map, const char *filepath) {
    memset(map, 0, sizeof(ChannelMap));

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) return true;

    fseek(f, 0, SEEK_END);

    const long filesize = ftell(f);

    rewind(f);

    char *b = malloc(filesize);
    assert(b != NULL);

    if (fread(b, 1, filesize, f) != filesize) {
        fclose(f);
        free(b);

        return true;
    }

    fclose(f);

    const bool err = channelMapParseCSV(map, b);

    // cleanup local resources in same scope as allocation
    free(b);

    return err;
}

void channelMapFree(ChannelMap *map) {
    ChannelNode *nodes;
    if ((nodes = map->nodes) != NULL) {
        map->nodes = NULL;

        free(nodes);
    }

    map->size = 0;
}
