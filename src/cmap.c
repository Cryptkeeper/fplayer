#include "cmap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ChannelMap gDefaultChannelMap;

static bool channelRangeIsMappable(const ChannelRange range) {
    const int rid = range.eid - range.sid;
    if (rid < 0) return false;

    const int rcircuit = range.ecircuit - range.scircuit;
    if (rcircuit < 0) return false;

    if (rid != rcircuit) return false;

    return true;
}

static int channelRangeCount(const ChannelRange range) {
    return range.eid - range.sid;
}

static void channelMapPut(ChannelRange channelRange) {
    assert(channelRangeIsMappable(channelRange));

    channelRange.data =
            calloc(channelRangeCount(channelRange), sizeof(ChannelData));

    assert(channelRange.data != NULL);

    // append to ChannelMap array
    const int index = gDefaultChannelMap.size;

    gDefaultChannelMap.size += 1;
    gDefaultChannelMap.ranges =
            reallocf(gDefaultChannelMap.ranges,
                     sizeof(ChannelMap) * gDefaultChannelMap.size);

    memcpy(&gDefaultChannelMap.ranges[index], &channelRange,
           sizeof(ChannelRange));
}

static long channelMapParseAttr(const char *b, long max) {
    const long l = strtol(b, NULL, 10);

    if (l <= 0) return 0;
    else if (l >= max)
        return max;
    else
        return l;
}

static bool channelMapParseCSV(char *b) {
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

        channelMapPut(newChannelRange);

        lines += 1;
    }

    printf("loaded %d channel map(s)\n", lines);

    return false;
}

bool channelMapInit(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) return true;

    fseek(f, 0, SEEK_END);

    const long filesize = ftell(f);

    rewind(f);

    assert(filesize > 0);

    char *b = malloc(filesize);
    assert(b != NULL);

    if (fread(b, 1, filesize, f) != filesize) {
        fclose(f);
        free(b);

        return true;
    }

    fclose(f);

    const bool err = channelMapParseCSV(b);

    // cleanup local resources in same scope as allocation
    free(b);

    return err;
}

bool channelMapFind(uint32_t id, uint8_t *unit, uint16_t *circuit,
                    ChannelData **data) {
    for (int i = 0; i < gDefaultChannelMap.size; i++) {
        const ChannelRange range = gDefaultChannelMap.ranges[i];

        if (id >= range.sid && id <= range.eid) {
            *unit = range.unit;

            // relativize `id` against the first channel id in `range`, then offset
            // against output range for final value
            *circuit = range.scircuit + (id - range.sid);

            *data = &range.data[id - range.sid];

            return true;
        }
    }

    return false;
}

static inline void channelRangeFree(ChannelRange *range) {
    free(range->data);
    range->data = NULL;
}

void channelMapFree(void) {
    if (gDefaultChannelMap.size > 0) {
        assert(gDefaultChannelMap.ranges != NULL);

        for (int i = 0; i < gDefaultChannelMap.size; i++) {
            channelRangeFree(&gDefaultChannelMap.ranges[i]);
        }

        gDefaultChannelMap.size = 0;
    }

    free(gDefaultChannelMap.ranges);
    gDefaultChannelMap.ranges = NULL;
}
