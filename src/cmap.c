#include "cmap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void channelMapPut(ChannelMap *map, const ChannelRange channelRange) {
    const int index = map->size;

    map->size += 1;
    map->ranges = reallocf(map->ranges, sizeof(ChannelRange) * map->size);

    memcpy(&map[index], &channelRange, sizeof(ChannelRange));
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

    int lines = 0;

    while ((lStart = strsep(&lEnd, "\n")) != NULL) {
        // ignoring empty new lines
        if (strlen(lStart) == 0) continue;

        // ignore comment lines beginning with '#'
        if (lStart[0] == '#') continue;

        char *sStart = NULL;
        char *sEnd = lStart;

        ChannelRange newChannelRange;

        for (int i = 0; i < 5; i++) {
            sStart = strsep(&sEnd, ",");

            // ensure all fields are set and read
            // removing this enables fields to be unexpectedly empty
            // since the parse loop won't otherwise break
            assert(sStart != NULL && strlen(sStart) > 0);

            switch (i) {
                case 0:
                    newChannelRange.sid =
                            channelMapParseAttr(sStart, UINT16_MAX);
                    break;
                case 1:
                    newChannelRange.eid =
                            channelMapParseAttr(sStart, UINT16_MAX);
                    break;
                case 2:
                    newChannelRange.unit =
                            channelMapParseAttr(sStart, UINT8_MAX);
                    break;
                case 3:
                    newChannelRange.scircuit =
                            channelMapParseAttr(sStart, UINT16_MAX);
                    break;
                case 4:
                    newChannelRange.ecircuit =
                            channelMapParseAttr(sStart, UINT16_MAX);
                    break;
                default:
                    assert(false && "unreachable statement");
            }
        }

        channelMapPut(map, newChannelRange);

        lines += 1;
    }

    printf("loaded %d channel map(s)\n", lines);

    return false;
}

static ChannelMap *gDefaultChannelMap;

bool channelMapInit(ChannelMap *map, const char *filepath) {
    assert(gDefaultChannelMap == NULL);
    gDefaultChannelMap = map;

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

static void channelRangeMap(const ChannelRange *range, uint32_t id,
                            uint8_t *unit, uint16_t *circuit) {
    // ensure we can reliably map using the configured range
    assert((range->eid - range->sid) == (range->ecircuit - range->scircuit));

    *unit = range->unit;

    // relativize `id` against the first channel id in `range`, then offset
    // against output range for final value
    *circuit = range->scircuit + (id - range->sid);
}

bool channelMapFind(const ChannelMap *map, uint32_t id, uint8_t *unit,
                    uint16_t *circuit) {
    for (int i = 0; i < map->size; i++) {
        const ChannelRange *range = &map->ranges[i];

        if (id >= range->sid && id <= range->eid) {
            channelRangeMap(range, id, unit, circuit);

            return true;
        }
    }

    return false;
}

void channelMapFree(ChannelMap *map) {
    ChannelRange *ranges;
    if ((ranges = map->ranges) != NULL) {
        map->ranges = NULL;

        free(ranges);
    }

    map->size = 0;

    if (gDefaultChannelMap == map) gDefaultChannelMap = NULL;
}

// we'll likely need multiple ChannelMaps in the future
// this is a temporary helper method for getting the current, global ChannelMap
ChannelMap *channelMapInstance(void) {
    assert(gDefaultChannelMap != NULL);

    return gDefaultChannelMap;
}
