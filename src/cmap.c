#include "cmap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

static ChannelMap gDefaultChannelMap;

static bool channelRangeIsMappable(const ChannelRange range) {
    const int rid = range.eid - range.sid;
    if (rid < 0) return false;

    const int rcircuit = range.ecircuit - range.scircuit;
    if (rcircuit < 0) return false;

    if (rid != rcircuit) return false;

    return true;
}

static void channelMapPut(ChannelRange channelRange) {
    assert(channelRangeIsMappable(channelRange));

    // append to ChannelMap array
    const int index = gDefaultChannelMap.size;

    gDefaultChannelMap.size += 1;
    gDefaultChannelMap.ranges =
            reallocf(gDefaultChannelMap.ranges,
                     sizeof(ChannelMap) * gDefaultChannelMap.size);

    memcpy(&gDefaultChannelMap.ranges[index], &channelRange,
           sizeof(ChannelRange));
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
                    parseLong(sStart, &newChannelRange.sid,
                              sizeof(newChannelRange.sid), 0, UINT16_MAX);
                    break;
                case 1:
                    parseLong(sStart, &newChannelRange.eid,
                              sizeof(newChannelRange.eid), 0, UINT16_MAX);
                    break;
                case 2:
                    parseLong(sStart, &newChannelRange.unit,
                              sizeof(newChannelRange.unit), 0, UINT8_MAX);
                    break;
                case 3:
                    parseLong(sStart, &newChannelRange.scircuit,
                              sizeof(newChannelRange.scircuit), 0, UINT16_MAX);
                    break;
                case 4:
                    parseLong(sStart, &newChannelRange.ecircuit,
                              sizeof(newChannelRange.ecircuit), 0, UINT16_MAX);
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

bool channelMapFind(uint32_t id, uint8_t *unit, uint16_t *circuit) {
    for (int i = 0; i < gDefaultChannelMap.size; i++) {
        const ChannelRange range = gDefaultChannelMap.ranges[i];

        if (id >= range.sid && id <= range.eid) {
            *unit = range.unit;

            // relativize `id` against the first channel id in `range`, then offset
            // against output range for final value
            *circuit = range.scircuit + (id - range.sid);

            return true;
        }
    }

    return false;
}

void channelMapFree(void) {
    free(gDefaultChannelMap.ranges);
    gDefaultChannelMap.ranges = NULL;

    gDefaultChannelMap.size = 0;
}
