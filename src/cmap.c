#include "cmap.h"

#include <assert.h>
#include <stdio.h>

#include <sds.h>

#include "err.h"
#include "mem.h"
#include "parse.h"

static ChannelMap gDefaultChannelMap;

static bool channelRangeIsMappable(const ChannelRange range) {
    const int64_t rid = range.eid - range.sid;
    if (rid < 0) return false;

    const int rcircuit = range.ecircuit - range.scircuit;
    if (rcircuit < 0) return false;

    // LOR values should be 1-indexed
    if (range.unit == 0 || range.scircuit == 0) return false;

    return rid == rcircuit;
}

static void channelMapAppend(ChannelRange channelRange) {
    // append to ChannelMap array
    const int index = gDefaultChannelMap.size;

    gDefaultChannelMap.size += 1;
    gDefaultChannelMap.ranges =
            mustRealloc(gDefaultChannelMap.ranges,
                        sizeof(ChannelMap) * gDefaultChannelMap.size);

    gDefaultChannelMap.ranges[index] = channelRange;
}

static ChannelRange channelRangeParseColumns(sds *cols, int nCols) {
    ChannelRange cr;

    assert(nCols == 5);

    parseLong(cols[0], &cr.sid, sizeof(cr.sid), 0, UINT32_MAX);
    parseLong(cols[1], &cr.eid, sizeof(cr.eid), 0, UINT32_MAX);
    parseLong(cols[2], &cr.unit, sizeof(cr.unit), 0, UINT8_MAX);
    parseLong(cols[3], &cr.scircuit, sizeof(cr.scircuit), 0, UINT16_MAX);
    parseLong(cols[4], &cr.ecircuit, sizeof(cr.ecircuit), 0, UINT16_MAX);

    return cr;
}

static void channelMapParseCSV(const char *b, bool *cmapParseErrs) {
    sds buf = sdsnew(b);

    int nRows = 0;
    sds *rows = sdssplitlen(buf, sdslen(buf), "\n", 1, &nRows);

    sdsfree(buf);

    for (int i = 0; i < nRows; i++) {
        sds row = rows[i];

        // ignoring empty new lines
        if (sdslen(row) == 0) continue;

        // ignore comment lines beginning with '#'
        if (row[0] == '#') continue;

        int nCols = 0;
        sds *cols = sdssplitlen(row, sdslen(row), ",", 1, &nCols);

        if (nCols != 5) {
            fprintf(stderr,
                    "invalid channel map entry: `%s`, requires 5 columns\n",
                    row);

            *cmapParseErrs = true;

            goto continue_free;
        }

        for (int j = 0; j < nCols; j++) {
            if (sdslen(cols[j]) == 0) {
                fprintf(stderr, "empty channel map entry column: %d\n", j);

                *cmapParseErrs = true;

                goto continue_free;
            }
        }

        const ChannelRange channelRange = channelRangeParseColumns(cols, nCols);

        if (!channelRangeIsMappable(channelRange)) {
            fatalf(E_FATAL, "error registering unmappable channel range: L%d\n",
                   i);

            *cmapParseErrs = true;

            goto continue_free;
        }

        channelMapAppend(channelRange);

    continue_free:
        sdsfreesplitres(cols, nCols);
    }

    sdsfreesplitres(rows, nRows);

    printf("configured %d channel map entries(s)\n", gDefaultChannelMap.size);
}

void channelMapInit(const char *filepath, bool *cmapParseErrs) {
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

    channelMapParseCSV(b, cmapParseErrs);

    // cleanup local resources in same scope as allocation
    freeAndNull((void **) &b);
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
