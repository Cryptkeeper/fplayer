#include "cmap.h"

#include <stdio.h>
#include <string.h>

#include "err.h"
#include "mem.h"
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

static void channelMapPut(ChannelRange channelRange, int line) {
    if (!channelRangeIsMappable(channelRange))
        fatalf(E_FATAL, "error registering unmappable channel range: L%d\n",
               line);

    // append to ChannelMap array
    const int index = gDefaultChannelMap.size;

    gDefaultChannelMap.size += 1;
    gDefaultChannelMap.ranges =
            reallocf(gDefaultChannelMap.ranges,
                     sizeof(ChannelMap) * gDefaultChannelMap.size);

    memcpy(&gDefaultChannelMap.ranges[index], &channelRange,
           sizeof(ChannelRange));
}

typedef enum channel_map_field_t {
    F_SID,
    F_EID,
    F_UNIT,
    F_SCIRCUIT,
    F_ECIRCUIT,
} CMapField;

#define F_COUNT 5

static void channelMapParseCSV(char *b) {
    char *lStart = NULL;
    char *lEnd = b;

    int lineAt = 0;

    while ((lStart = strsep(&lEnd, "\n")) != NULL) {
        // ignoring empty new lines
        if (strlen(lStart) == 0) continue;

        // ignore comment lines beginning with '#'
        if (lStart[0] == '#') continue;

        char *sStart = NULL;
        char *sEnd = lStart;

        ChannelRange newChannelRange;

        for (CMapField f = F_SID; f < F_COUNT; f++) {
            sStart = strsep(&sEnd, ",");

            // ensure all fields are set and read
            // removing this enables fields to be unexpectedly empty
            // since the parse loop won't otherwise break
            if (sStart == NULL || strlen(sStart) == 0)
                fatalf(E_FATAL, "error parsing channel map: L%d\n", lineAt);

            switch (f) {
                case F_SID:
                    parseLong(sStart, &newChannelRange.sid,
                              sizeof(newChannelRange.sid), 0, UINT32_MAX);
                    break;
                case F_EID:
                    parseLong(sStart, &newChannelRange.eid,
                              sizeof(newChannelRange.eid), 0, UINT32_MAX);
                    break;
                case F_UNIT:
                    parseLong(sStart, &newChannelRange.unit,
                              sizeof(newChannelRange.unit), 0, UINT8_MAX);
                    break;
                case F_SCIRCUIT:
                    parseLong(sStart, &newChannelRange.scircuit,
                              sizeof(newChannelRange.scircuit), 0, UINT16_MAX);
                    break;
                case F_ECIRCUIT:
                    parseLong(sStart, &newChannelRange.ecircuit,
                              sizeof(newChannelRange.ecircuit), 0, UINT16_MAX);
                    break;
            }
        }

        channelMapPut(newChannelRange, lineAt);

        lineAt += 1;
    }

    printf("loaded %d channel map(s)\n", lineAt);
}

void channelMapInit(const char *filepath) {
    FILE *f = fopen(filepath, "rb");

    if (f == NULL)
        fatalf(E_FILE_NOT_FOUND, "error opening channel map: %s\n", filepath);

    if (fseek(f, 0, SEEK_END) < 0) fatalf(E_FILE_IO, NULL);

    const long filesize = ftell(f);
    if (filesize <= 0) fatalf(E_FILE_IO, NULL);

    rewind(f);

    char *b = mustMalloc(filesize);

    if (fread(b, 1, filesize, f) != filesize) fatalf(E_FILE_IO, NULL);

    fclose(f);

    channelMapParseCSV(b);

    // cleanup local resources in same scope as allocation
    free(b);
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
    freeAndNull((void **) &gDefaultChannelMap.ranges);

    gDefaultChannelMap.size = 0;
}
