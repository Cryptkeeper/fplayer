#include "cmap.h"

#include <inttypes.h>
#include <stdio.h>

#include "sds.h"
#include "stb_ds.h"

#include "std/err.h"
#include "std/mem.h"
#include "std/parse.h"

struct channel_range_t {
    uint32_t sid;
    uint32_t eid;
    uint8_t unit;
    uint16_t scircuit;
    uint16_t ecircuit;
};

static sds channelRangeValidate(const struct channel_range_t range) {
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
                            "ID range (%" PRIu64 " values) must be equal in "
                            "length to circuit range (%d values)",
                            rid, rcircuit);

    return NULL;
}

static struct channel_range_t *gRanges;

static void channelMapParseCSV(const char *const b) {
    const sds buf = sdsnew(b);

    int nRows = 0;
    sds *rows = sdssplitlen(buf, sdslen(buf), "\n", 1, &nRows);

    sdsfree(buf);

    int ignoredRows = 0;

    for (int i = 0; i < nRows; i++) {
        const sds row = rows[i];

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

            ignoredRows++;

            goto continue_free;
        }

        for (int j = 0; j < nCols; j++) {
            if (sdslen(cols[j]) == 0) {
                fprintf(stderr, "empty channel map entry column L%d: C%d\n", i,
                        j);

                ignoredRows++;

                goto continue_free;
            }
        }

        const struct channel_range_t cr = {
                .sid = (uint32_t) parseLong(cols[0], 0, UINT32_MAX),
                .eid = (uint32_t) parseLong(cols[1], 0, UINT32_MAX),
                .unit = (uint8_t) parseLong(cols[2], 0, UINT8_MAX),
                .scircuit = (uint16_t) parseLong(cols[3], 0, UINT16_MAX),
                .ecircuit = (uint16_t) parseLong(cols[4], 0, UINT16_MAX),
        };

        const sds error = channelRangeValidate(cr);

        if (error != NULL) {
            fprintf(stderr, "unmappable channel range L%d: %s\n", i, error);

            sdsfree(error);

            ignoredRows++;

            goto continue_free;
        }

        arrput(gRanges, cr);

    continue_free:
        sdsfreesplitres(cols, nCols);
    }

    sdsfreesplitres(rows, nRows);

    printf("configured %d valid channel map %s\n", (int) arrlen(gRanges),
           arrlen(gRanges) == 1 ? "entry" : "entries");

    if (ignoredRows > 0)
        fprintf(stderr, "warning: %d invalid channel map entries ignored\n",
                ignoredRows);
}

void channelMapInit(const char *const filepath) {
    FILE *f = fopen(filepath, "rb");

    if (f == NULL) fatalf(E_FIO, "error opening channel map: %s\n", filepath);

    if (fseek(f, 0, SEEK_END) < 0) fatalf(E_FIO, NULL);

    const long filesize = ftell(f);
    if (filesize <= 0) fatalf(E_FIO, NULL);

    rewind(f);

    char *b = mustMalloc(filesize + 1);

    if (fread(b, 1, filesize, f) != (unsigned long) filesize)
        fatalf(E_FIO, NULL);

    fclose(f);

    // manually NULL terminate string to avoid safety depending solely on sds
    b[filesize] = '\0';

    channelMapParseCSV(b);

    // cleanup local resources in same scope as allocation
    freeAndNull(b);
}

bool channelMapFind(const uint32_t id,
                    uint8_t *const unit,
                    uint16_t *const circuit) {
    for (int i = 0; i < arrlen(gRanges); i++) {
        const struct channel_range_t range = gRanges[i];

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

static bool channelMapContainsUid(const uint8_t *const set,
                                  const uint8_t value) {
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
        const struct channel_range_t range = gRanges[i];

        if (!channelMapContainsUid(uids, range.unit)) arrput(uids, range.unit);
    }

    return uids;
}

void channelMapFree(void) {
    arrfree(gRanges);
}
