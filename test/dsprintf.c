#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "std/string.h"

#define assert_strcmp(s, v)                                                    \
    do {                                                                       \
        char *p = s;                                                           \
        assert(strcmp(p, v) == 0);                                             \
        free(p);                                                               \
    } while (0)

int main(__attribute__((unused)) int argc,
         __attribute__((unused)) char **argv) {
    assert_strcmp(dsprintf("%s", "hello"), "hello");
    assert_strcmp(dsprintf("hello %s", "world"), "hello world");
    assert_strcmp(dsprintf("%s world", "hello"), "hello world");
    assert_strcmp(dsprintf("%s %s", "hello", "world"), "hello world");

    return 0;
}
