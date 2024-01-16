#include <stddef.h>
#include <stdint.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "cmap.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size) {
    (void) size;

    const enum cmap_parse_res_t result =
            channelMapParseCSVLine(0, (const char *) data);

    return result == CMAP_PARSE_OK ? 0 : -1;
}