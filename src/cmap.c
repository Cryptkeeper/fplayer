#include "cmap.h"

#include <inttypes.h>
#include <stdio.h>

#include "stb_ds.h"

#include "std/err.h"
#include "std/string.h"

struct channel_range_t {
    uint32_t sid;
    uint32_t eid;
    uint8_t unit;
    uint16_t scircuit;
    uint16_t ecircuit;
};

static char *channelRangeValidate(const struct channel_range_t range) {
    const int64_t rid = range.eid - range.sid;

    if (rid < 0)
        return dsprintf("start ID (%d, C0) is greater than end ID (%d, C1)",
                        range.sid, range.eid);

    const int rcircuit = range.ecircuit - range.scircuit;

    if (rcircuit < 0)
        return dsprintf(
                "start circuit (%d, C3) is greater than end circuit (%d, C4)",
                range.scircuit, range.ecircuit);

    // LOR values should be 1-indexed
    if (range.unit == 0 || range.scircuit == 0)
        return dsprintf(
                "unit (%d, C2) and start circuit (%d, C3) should be >=1",
                range.unit, range.scircuit);

    if (rid != rcircuit)
        return dsprintf("ID range (%" PRIu64 " values) must be equal in "
                        "length to circuit range (%d values)",
                        rid, rcircuit);

    return NULL;
}

static bool channelMapParseCSVRow(const int line,
                                  const char *const row,
                                  struct channel_range_t *const range) {
    char *const str = mustStrdup(row);
    char *last;

    for (int col = 0; col < 5; col++) {
        const char *v = col == 0 ? strtok_r(str, ",", &last)
                                 : strtok_r(NULL, ",", &last);

        if (v == NULL || strlen(v) == 0) {
            fprintf(stderr,
                    "invalid channel map entry L%d: `%s`, missing/empty value "
                    "at C%d\n",
                    line, row, col);

            free(str);

            return false;
        }

        switch (col) {
            case 0:
                range->sid = (uint32_t) mustStrtol(v, 0, UINT32_MAX);
                break;
            case 1:
                range->eid = (uint32_t) mustStrtol(v, 0, UINT32_MAX);
                break;
            case 2:
                range->unit = (uint8_t) mustStrtol(v, 0, UINT8_MAX);
                break;
            case 3:
                range->scircuit = (uint16_t) mustStrtol(v, 0, UINT16_MAX);
                break;
            case 4:
                range->ecircuit = (uint16_t) mustStrtol(v, 0, UINT16_MAX);
            default:
                break;
        }
    }

    free(str);

    return true;
}

static struct channel_range_t *gRanges;

enum cmap_parse_result_t { CMAP_PARSE_OK, CMAP_PARSE_EMPTY, CMAP_PARSE_ERROR };

static enum cmap_parse_result_t channelMapParseCSVLine(const int line,
                                                       const char *const row) {
    // ignoring empty new lines
    if (strlen(row) == 0) return CMAP_PARSE_EMPTY;

    // ignore comment lines beginning with '#'
    if (row[0] == '#') return CMAP_PARSE_EMPTY;

    struct channel_range_t cr;
    if (!channelMapParseCSVRow(line, row, &cr)) return CMAP_PARSE_ERROR;

    char *const error = channelRangeValidate(cr);

    if (error != NULL) {
        fprintf(stderr, "unmappable channel range L%d: %s\n", line, error);
        free(error);

        return CMAP_PARSE_ERROR;
    }

    arrput(gRanges, cr);

    return CMAP_PARSE_OK;
}

static void channelMapParseCSV(const char *const b) {
    char *const str = mustStrdup(b);
    char *last;

    int ignoredRows = 0;
    int line = 0;

    for (const char *row = strtok_r(str, "\n", &last); row != NULL;
         row = strtok_r(NULL, "\n", &last)) {

        const enum cmap_parse_result_t result =
                channelMapParseCSVLine(line, row);

        if (result == CMAP_PARSE_ERROR) ignoredRows++;

        line++;
    }

    free(str);

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

    char *const b = mustMalloc(filesize + 1);

    if (fread(b, 1, filesize, f) != (size_t) filesize) fatalf(E_FIO, NULL);

    fclose(f);

    b[filesize] = '\0';

    channelMapParseCSV(b);
    free(b);
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
