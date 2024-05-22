#undef NDEBUG
#include <assert.h>
#include <limits.h>

#include <std2/string.h>

static void assert_eq(const char* str,
                      const long min,
                      const long max,
                      const long expected) {
    long actual;
    assert(strtolb(str, min, max, &actual, sizeof(actual)) == 0);
    assert(actual == expected);
}

static void assert_fail(const char* str) {
    long actual;
    assert(strtolb(str, LONG_MIN, LONG_MAX, &actual, sizeof(actual)) != 0);
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    assert_eq("0", 0, 10, 0);
    assert_eq("1", 0, 10, 1);
    assert_eq("10", 0, 10, 10);
    assert_eq("11", 0, 10, 10);
    assert_eq("0", -10, -5, -5);
    assert_eq("-5", -10, -5, -5);
    assert_eq("-5", 0, 1, 0);
    assert_eq("10", 0, 5, 5);
    assert_eq("-10", 0, 5, 0);

    assert_fail("5.0");
    assert_fail("5,0");
    assert_fail(".5");

    assert_fail("b10");
    assert_fail("10b");
    assert_fail("0x10");
    assert_fail("10x0");
    assert_fail("_10");
    assert_fail("10_");

    return 0;
}
