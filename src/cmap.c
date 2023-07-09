#include "cmap.h"

#include <assert.h>
#include <stdio.h>

#include <sds.h>
#include <stb_ds.h>

#include "std/err.h"
#include "std/mem.h"
#include "std/parse.h"

static sds channelRangeValidate(const ChannelRange range) {
    const int64_t rid = range.eid - range.sid;

    if (rid < 0)
        return sdscatprintf(sdsempty(),
                            "start ID (%d, C0) is greater than end ID (%d, C1)",
                            range.sid, range.eid);

    const int rcircuit = range.ecircuit - range.scircuit;

    if (rcircuit < 0)
        return sdscatprintf(
                sdsempty(),
                "start circuit (%d, C3) is greater than end circuit (%d, C4)",
                range.scircuit, range.ecircuit);

    // LOR values should be 1-indexed
    if (range.unit == 0 || range.scircuit == 0)
        return sdscatprintf(
                sdsempty(),
                "unit (%d, C2) and start circuit (%d, C3) should be >=1",
                range.unit, range.scircuit);

    if (rid != rcircuit)
        return sdscatprintf(sdsempty(),
                            "ID range (%lld values) must be equal in "
                            "length to circuit range (%d values)",
                            rid, rcircuit);

    return NULL;
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

static ChannelRange *gRanges;

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
                    "invalid channel map entry L%d: `%s`, requires 5 "
                    "comma-seperated values\n",
                    i, row);

            *cmapParseErrs = true;

            goto continue_free;
        }

        for (int j = 0; j < nCols; j++) {
            if (sdslen(cols[j]) == 0) {
                fprintf(stderr, "empty channel map entry column L%d: C%d\n", i,
                        j);

                *cmapParseErrs = true;

                goto continue_free;
            }
        }

        const ChannelRange channelRange = channelRangeParseColumns(cols, nCols);

        sds error = channelRangeValidate(channelRange);

        if (error != NULL) {
            fprintf(stderr, "unmappable channel range L%d: %s\n", i, error);

            sdsfree(error);

            *cmapParseErrs = true;

            goto continue_free;
        }

        arrput(gRanges, channelRange);

    continue_free:
        sdsfreesplitres(cols, nCols);
    }

    sdsfreesplitres(rows, nRows);

    printf("configured %d valid channel map entries(s)\n",
           (int) arrlen(gRanges));
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

bool channelMapFind(const uint32_t id,
                    uint8_t *const unit,
                    uint16_t *const circuit) {
    for (int i = 0; i < arrlen(gRanges); i++) {
        const ChannelRange range = gRanges[i];

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

static inline bool channelMapContainsUid(const uint8_t *set, uint8_t value) {
    const int size = arrlen(set);

    if (size == 0) return false;

    for (int i = 0; i < size; i++) {
        if (set[i] == value) return true;
    }

    return false;
}

uint8_t *channelMapGetUids(void) {
    uint8_t *uids = NULL;

    for (int i = 0; i < arrlen(gRanges); i++) {
        const ChannelRange range = gRanges[i];

        if (!channelMapContainsUid(uids, range.unit)) arrput(uids, range.unit);
    }

    return uids;
}

void channelMapFree(void) {
    arrfree(gRanges);
}
