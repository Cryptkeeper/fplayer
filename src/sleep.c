#include "sleep.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

void timeGetNow(struct timespec *spec) {
    clock_gettime(CLOCK_MONOTONIC_RAW, spec);
}

long timeMillisElapsed(const struct timespec *start,
                       const struct timespec *end) {
    return (long) ((end->tv_sec - start->tv_sec) * 1000 +
                   (end->tv_nsec - start->tv_nsec) / 1000000);
}

void timeSetMillis(struct timespec *spec, long millis) {
    spec->tv_sec = millis / 1000;
    spec->tv_nsec = (millis % 1000) * 1000000;
}

#define gLatencyRecordsCount 20

static long gLatencyRecords[gLatencyRecordsCount];
static int gNextRecordIndex = 0;
static int gEndRecordIndex = 0;

static void sleepRecordLatency(long millis) {
    gLatencyRecords[gNextRecordIndex++] = millis;
    gNextRecordIndex %= gLatencyRecordsCount;

    if (gNextRecordIndex > gEndRecordIndex) gEndRecordIndex = gNextRecordIndex;
}

void sleepGetLatency(char *b, int c) {
    double avg = 0;
    int n = 1;

    for (int i = 0; i <= gEndRecordIndex; i++) {
        long latency = gLatencyRecords[i];

        avg += ((double) latency - avg) / n;
        n++;
    }

    snprintf(b, c, "%.2fms", avg);
}

static inline void sleepTimer(long millis, long *latency) {
    struct timespec start, end;

    timeGetNow(&start);

    struct timespec t;
    timeSetMillis(&t, millis);

    if (nanosleep(&t, NULL) != 0) perror("error while invoking nanosleep");

    timeGetNow(&end);

    // this offsets the timing of the next frame by the difference in duration of
    //  the actual sleep time vs the expected sleep time
    // TODO: there are better ways to handle this using stdev
    long l = *latency = millis - timeMillisElapsed(&start, &end);

    sleepRecordLatency(l);
}

void sleepTimerLoop(sleep_fn_t sleep, long millis) {
    struct timespec start, end;

    long latency = 0;

    while (true) {
        timeGetNow(&start);

        const bool doContinue = sleep();
        if (!doContinue) break;

        timeGetNow(&end);

        const long remainingTime =
                millis - timeMillisElapsed(&start, &end) + latency;

        if (remainingTime > 0) sleepTimer(remainingTime, &latency);
    }
}
