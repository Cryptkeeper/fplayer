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

sds timeElapsedString(timeInstant start, timeInstant end) {
    const int64_t diff = timeElapsedNs(start, end);
    const float ms = (float) diff / (float) 1e6;

    return sdscatprintf(sdsempty(), ms >= 1 ? "%.1fms" : "%.3fms", ms);
}