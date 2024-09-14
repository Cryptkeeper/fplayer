/// @file crmap.c
/// @brief Channel range map implementation.
#include "crmap.h"

#include <assert.h>
#include <stdlib.h>

#include <cjson/cJSON.h>

#include "std2/errcode.h"
#include "std2/fc.h"

struct cr_s {
    uint32_t indexr[2];  ///< Start index (incl.), end index (incl.)
    uint16_t circuitr[2];///< Start circuit (incl.), end circuit (incl.)
    uint8_t unit;        ///< Unit ID
    struct cr_s* next;   ///< Next \p cr_s in the list, otherwise NULL
};

/// @brief Parses a single channel range map object into the given `cr` struct.
/// The object is expected to have the following structure:
/// ```json
/// {
///   "index": { "from": _, "to": _ },
///   "circuit": { "from": _, "to": _ },
///   "unit": _
/// }
/// ```
/// @param item cJSON object to parse
/// @param cr pointer to write the parsed channel range map to
/// @return 0 on success, or a negative error code on failure
int CR_parseOne(const cJSON* item, struct cr_s* cr) {
    assert(item != NULL);
    assert(cr != NULL);

    struct cr_s b = {0};

    {
        cJSON* index = cJSON_GetObjectItem(item, "index");
        if (!cJSON_IsObject(index)) return -FP_EINVLFMT;

        cJSON* from = cJSON_GetObjectItem(index, "from");
        cJSON* to = cJSON_GetObjectItem(index, "to");
        if (!cJSON_IsNumber(from) || !cJSON_IsNumber(to)) return -FP_EINVLFMT;

        b.indexr[0] = from->valueint;
        b.indexr[1] = to->valueint;
    }

    {
        cJSON* circuit = cJSON_GetObjectItem(item, "circuit");
        if (!cJSON_IsObject(circuit)) return -FP_EINVLFMT;

        cJSON* from = cJSON_GetObjectItem(circuit, "from");
        cJSON* to = cJSON_GetObjectItem(circuit, "to");
        if (!cJSON_IsNumber(from) || !cJSON_IsNumber(to)) return -FP_EINVLFMT;

        b.circuitr[0] = from->valueint;
        b.circuitr[1] = to->valueint;
    }

    cJSON* unit = cJSON_GetObjectItem(item, "unit");
    if (!cJSON_IsNumber(unit)) return -FP_EINVLFMT;

    b.unit = unit->valueint;

    *cr = b;
    return FP_EOK;
}

/// @brief Parses the given channel range map string into a linked list of
/// `struct cr_s` nodes. The string is expected to be JSON formatted with the
/// following structure:
/// ```json
/// [
///  {
///    "index": { "from": _, "to": _ },
///    "circuit": { "from": _, "to": _ },
///    "unit": _
///  }
/// ]
/// ```
/// @param s channel range map string to parse
/// @param cr pointer to write the channel range map to
/// @return 0 on success, or a negative error code on failure
static int CR_parse(const char* s, struct cr_s** cr) {
    assert(s != NULL);
    assert(cr != NULL);

    *cr = NULL;

    int err = FP_EOK;

    cJSON* obj = cJSON_Parse(s);
    if (obj == NULL || !cJSON_IsArray(obj)) {
        err = -FP_EINVLFMT;
        goto ret;
    }

    const int size = cJSON_GetArraySize(obj);
    if (size <= 0) {
        if (size < 0) err = -FP_EINVLFMT;// size == 0 is weird, but not an error
        goto ret;
    }

    // allocate a single block for backing the node linked list
    if ((*cr = calloc(size, sizeof(struct cr_s))) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    int index = 0;
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, obj) {
        if (!cJSON_IsObject(item)) {
            err = -FP_EINVLFMT;
            goto ret;
        }

        if ((err = CR_parseOne(item, &(*cr)[index++]))) goto ret;
    }

ret:
    cJSON_Delete(obj);
    if (err) CMap_free(*cr), *cr = NULL;

    return err;
}

int CMap_read(const char* fp, struct cr_s** cr) {
    assert(fp != NULL);
    assert(cr != NULL);

    int err = FP_EOK;

    struct FC* fc = NULL; /* opened file controller */
    uint8_t* b = NULL;    /* file contents buffer */

    if ((fc = FC_open(fp, FC_MODE_READ)) == NULL) {
        err = -FP_ESYSCALL;
        goto ret;
    }

    // read the full file into memory
    const uint32_t fsize = FC_filesize(fc);
    if ((b = malloc(fsize + 1)) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }
    if (FC_read(fc, 0, fsize, b) != fsize) {
        err = -FP_ESYSCALL;
        goto ret;
    }
    b[fsize] = '\0';// ensure null termination

    if ((err = CR_parse((char*) b, cr)) != FP_EOK) goto ret;

ret:
    FC_close(fc);
    free(b);

    return err;
}

void CMap_free(struct cr_s* cr) {
    while (cr != NULL) {
        struct cr_s* n = cr->next;
        free(cr), cr = n;
    }
}

int CMap_lookup(const struct cr_s* cr,
                const uint32_t id,
                uint8_t* unit,
                uint16_t* circuit) {
    assert(cr != NULL);
    assert(unit != NULL);
    assert(circuit != NULL);

    *unit = 0, *circuit = 0;

    for (; cr != NULL; cr = cr->next) {
        if (id >= cr->indexr[0] && id <= cr->indexr[1]) {
            *unit = cr->unit;
            *circuit = cr->circuitr[0] + (id - cr->indexr[0]);
            return 1;
        }
    }

    return 0;
}
