#include <stddef.h>
#include <stdint.h>

#define STB_DS_IMPLEMENTATION
// ReSharper disable once CppUnusedIncludeDirective
#include "stb_ds.h"

#include "cmap.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size) {
    (void) size;

    const enum cmap_parse_res_t result =
            channelMapParseCSVLine(0, (const char *) data);
    (void) result;

    return 0;
}