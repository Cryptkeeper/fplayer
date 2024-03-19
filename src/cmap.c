#include "cmap.h"

#include <inttypes.h>
#include <stdio.h>

#include "stb_ds.h"

#include "std/err.h"
#include "std/fc.h"
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

#ifdef _WIN32
// Public Domain strsep implementation by Dan Cross
// via https://unixpapa.com/incnote/string.html
static char *strsep(char **sp, char *sep) {
    char *p, *s;
    if (sp == NULL || *sp == NULL || **sp == '\0') return (NULL);
    s = *sp;
    p = s + strcspn(s, sep);
    if (*p != '\0') *p++ = '\0';
    *sp = p;
    return (s);
}
#endif

static bool channelMapParseCSVRow(const int line,
                                  const char *const row,
                                  struct channel_range_t *const range) {
    char *dup;
    char *ptr = dup = mustStrdup(row);

    bool ok = true;

    int col = 0;

    char *token;
    while ((token = strsep(&dup, ",")) != NULL) {
        if (token == NULL || strlen(token) == 0) {
            fprintf(stderr,
                    "invalid channel map entry L%d: `%s`, missing/empty value "
                    "at C%d\n",
                    line, row, col);
            ok = false;
            goto cleanup;
        }

        switch (col) {
            case 0:
                if (!strtolb(token, 0, UINT32_MAX, &range->sid,
                             sizeof(range->sid))) {
                    ok = false;
                    goto cleanup;
                }
                break;
            case 1:
                if (!strtolb(token, 0, UINT32_MAX, &range->eid,
                             sizeof(range->eid))) {
                    ok = false;
                    goto cleanup;
                }
                break;
            case 2:
                if (!strtolb(token, 0, UINT8_MAX, &range->unit,
                             sizeof(range->unit))) {
                    ok = false;
                    goto cleanup;
                }
                break;
            case 3:
                if (!strtolb(token, 0, UINT16_MAX, &range->scircuit,
                             sizeof(range->scircuit))) {
                    ok = false;
                    goto cleanup;
                }
                break;
            case 4:
                if (!strtolb(token, 0, UINT16_MAX, &range->ecircuit,
                             sizeof(range->ecircuit))) {
                    ok = false;
                    goto cleanup;
                }
                break;
            default:
                fprintf(stderr,
                        "invalid channel map entry L%d: `%s`, too many "
                        "values at C%d\n",
                        line, row, col);
                ok = false;
                goto cleanup;
        }

        col++;
    }

cleanup:
    free(ptr);

    return ok;
}

static struct channel_range_t *gRanges;

enum cmap_parse_res_t channelMapParseCSVLine(const int line,
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

cmap_parse_info_t channelMapParseCSV(const char *const b) {
    char *const str = mustStrdup(b);
    char *last;

    cmap_parse_info_t info = {0};

    int line = 0;

    for (const char *row = strtok_r(str, "\n", &last); row != NULL;
         row = strtok_r(NULL, "\n", &last)) {

        const enum cmap_parse_res_t result =
                channelMapParseCSVLine(line++, row);

        switch (result) {
            case CMAP_PARSE_OK:
                info.valid_rows++;
                break;
            case CMAP_PARSE_EMPTY:
                continue;
            case CMAP_PARSE_ERROR:
                info.invalid_rows++;
                break;
        }
    }

    free(str);

    return info;
}

void channelMapInit(const char *const filepath) {
    FCHandle fc = FC_open(filepath);

    const uint32_t filesize = FC_filesize(fc);
    uint8_t *b = mustMalloc(filesize + 1);

    FC_read(fc, 0, filesize, b);
    FC_close(fc);

    // ensure file contents are treated as null terminated string
    b[filesize] = '\0';

    const cmap_parse_info_t info = channelMapParseCSV((char *) b);

    // print parsed statistics
    printf("configured %d valid channel map %s\n", info.valid_rows,
           info.valid_rows == 1 ? "entry" : "entries");

    if (info.invalid_rows > 0)
        fprintf(stderr, "warning: %d invalid channel map entries ignored\n",
                info.invalid_rows);

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
