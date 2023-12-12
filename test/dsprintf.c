#undef NDEBUG
#include <assert.h>
#include <string.h>

#include "std/string.h"

int main(__attribute__((unused)) int argc,
         __attribute__((unused)) char **argv) {
    assert(strcmp(dsprintf("%s", "hello"), "hello") == 0);
    assert(strcmp(dsprintf("hello %s", "world"), "hello world") == 0);
    assert(strcmp(dsprintf("%s world", "hello"), "hello world") == 0);
    assert(strcmp(dsprintf("%s %s", "hello", "world"), "hello world") == 0);

    return 0;
}
