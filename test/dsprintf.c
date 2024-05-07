#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <std2/string.h>

static inline void assert_strcmp(char* s, const char* expected) {
    assert(strcmp(s, expected) == 0);
    free(s);
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;
    assert_strcmp(dsprintf("%s", "hello"), "hello");
    assert_strcmp(dsprintf("hello %s", "world"), "hello world");
    assert_strcmp(dsprintf("%s world", "hello"), "hello world");
    assert_strcmp(dsprintf("%s %s", "hello", "world"), "hello world");

    return 0;
}
