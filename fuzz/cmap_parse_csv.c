#include <stddef.h>
#include <stdint.h>

#define STB_DS_IMPLEMENTATION
// ReSharper disable once CppUnusedIncludeDirective
#include "stb_ds.h"

#include "cmap.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size) {
    (void) size;

    const cmap_parse_info_t info = channelMapParseCSV((char *) data);
    (void) info;

    return 0;
}