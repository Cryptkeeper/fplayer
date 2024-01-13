#include <stddef.h>
#include <stdint.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "cmap.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size) {
    (void) size;

    const cmap_parse_info_t info = channelMapParseCSV((char *) data);

    if (info.valid_rows > 0 || info.invalid_rows > 0) return 0;

    return -1;
}