#include "time.h"

#include "std/string.h"

#ifdef _WIN32
    #include <windows.h>
#endif

timeInstant timeGetNow(void) {
    timeInstant now = {0};

#ifdef _WIN32
    FILETIME ft = {0};
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER lg = {0};
    lg.LowPart = ft.dwLowDateTime;
    lg.HighPart = ft.dwHighDateTime;

    // convert Widnows' 1-1-1601 start date to 1-1-1970
    const __int64 abs = lg.QuadPart - 116444736000000000;

    now.tv_sec = abs / 10000000;
    now.tv_nsec = abs % 10000000 * 100;
#else
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
#endif

    return now;
}

int64_t timeElapsedNs(const timeInstant start, const timeInstant end) {
    return (end.tv_sec - start.tv_sec) * 1000000000 +
           (end.tv_nsec - start.tv_nsec);
}

char *timeElapsedString(const timeInstant start, const timeInstant end) {
    const int64_t diff = timeElapsedNs(start, end);
    const float ms = (float) diff / (float) 1e6;

    return dsprintf(ms >= 1 ? "%.1fms" : "%.3fms", ms);
}
