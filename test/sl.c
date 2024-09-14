#undef NDEBUG
#include <assert.h>
#include <string.h>

#define SL_IMPL
#include "sl.h"

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    slist_t* sl = NULL;
    assert(sladd(&sl, "hello") == 0);
    assert(sladd(&sl, "world") == 0);

    assert(strcmp(sl[0], "hello") == 0);
    assert(strcmp(sl[1], "world") == 0);
    assert(sl[2] == NULL);

    slfree(sl);

    return 0;
}
