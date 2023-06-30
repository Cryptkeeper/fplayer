#include "time.h"

timeInstant timeGetNow(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);

    return (timeInstant) now;
}

int64_t timeElapsedNs(const timeInstant start, const timeInstant end) {
    return ((end.tv_sec - start.tv_sec) * 1000000000) +
           (end.tv_nsec - start.tv_nsec);
}
