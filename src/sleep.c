#include "sleep.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

static inline void markTime(struct timespec *spec) {
    clock_gettime(CLOCK_MONOTONIC_RAW, spec);
}

static inline long timeMillisElapsed(struct timespec start,
                                     struct timespec end) {
    return (long) ((end.tv_sec - start.tv_sec) * 1000 +
                   (end.tv_nsec - start.tv_nsec) / 1000000);
}

static inline void setTimeMillis(struct timespec *spec, long millis) {
    spec->tv_sec = millis / 1000;
    spec->tv_nsec = (millis % 1000) * 1000000;
}

static inline void sleepTimer(long millis, long *latency) {
    struct timespec start, end;

    markTime(&start);

    struct timespec t;
    setTimeMillis(&t, millis);

    if (nanosleep(&t, NULL) != 0) perror("error while invoking nanosleep");

    markTime(&end);

    // this offsets the timing of the next frame by the difference in duration of
    //  the actual sleep time vs the expected sleep time
    // TODO: there are better ways to handle this using stdev
    *latency = millis - timeMillisElapsed(start, end);
}

void sleepTimerLoop(sleep_fn_t sleep, long millis) {
    struct timespec start, end;

    long latency = 0;

    while (true) {
        markTime(&start);

        const bool doContinue = sleep();
        if (!doContinue) break;

        markTime(&end);

        const long remainingTime =
                millis - timeMillisElapsed(start, end) + latency;

        if (remainingTime > 0) sleepTimer(remainingTime, &latency);
    }
}
