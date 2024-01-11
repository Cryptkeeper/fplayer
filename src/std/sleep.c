#include "sleep.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "std/string.h"
#include "time.h"

#define gSampleCount 20

static int64_t gSampleNs[gSampleCount];
static int gLastSampleIdx;

char *Sleep_status(void) {
    double avg = 0;
    int n = 1;

    for (int i = 0; i <= gLastSampleIdx; i++) {
        const int64_t drift = gSampleNs[i];

        avg += ((double) drift - avg) / n;
        n += 1;
    }

    const double ms = avg / 1e6;
    const double fps = ms > 0 ? 1000 / ms : 0;

    return dsprintf("%.4fms (%.2f fps)", ms, fps);
}

void Sleep_halt(struct sleep_loop_t *loop, char *msg) {
    loop->halt = true;
    loop->msg = msg;
}

static int64_t sleepEstimatedNs(int64_t ns);

static void sleepSpinLockNs(int64_t ns);

// This is a modified version of the C++ implementation shown in this article:
// https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
//
// This version ports it to C using nanosleep, and makes use of time conversion
// helper functions provided by fplayer's `time.h`. The two core sleep behaviors:
//
// 1. an approximate, repeating millisecond sleep loop
// and 2. a spin look for nanosecond precision at cost of CPU
//
// ...are split into individual functions for readability.
static void sleepPrecise(const int64_t ns) {
    const int64_t preciseTime = sleepEstimatedNs(ns);

    sleepSpinLockNs(preciseTime);
}

// The original `preciseSleep` function operates using double representation of
// seconds. My other functions operate off ns precision using int64_t, so you'll
// see conversions between the two types at code borders, but the core math
// remains the same.
static int64_t sleepEstimatedNs(const int64_t ns) {
    double estimate = 5e-3;

    double sampleMean = 5e-3;
    double sampleM2 = 0;
    unsigned int sampleCount = 1;

    double remainingTime = (double) ns / 1e9;

    while (remainingTime > estimate) {
        // measure the nanoseconds it takes for the system to perform
        // a one millisecond #nanosleep call
        const timeInstant start = timeGetNow();

#ifdef _WIN32
        Sleep(1);
#else
        const struct timespec *timeOneMs = &(const struct timespec){
                .tv_sec = 0,
                .tv_nsec = 1000000,
        };

        nanosleep(timeOneMs, NULL);
#endif

        const double sSample =
                (double) timeElapsedNs(start, timeGetNow()) / 1e9;

        remainingTime -= sSample;
        sampleCount += 1;

        const double delta = sSample - sampleMean;
        sampleMean += delta / sampleCount;
        sampleM2 += delta * (sSample - sampleMean);

        const double stddev = sqrt(sampleM2 / (sampleCount - 1));
        estimate = sampleMean + stddev;
    }

    return (int64_t) (remainingTime * 1e9);
}

static void sleepSpinLockNs(const int64_t ns) {
    const timeInstant start = timeGetNow();

spin:
    if (timeElapsedNs(start, timeGetNow()) < ns) goto spin;
}

static int gNextSampleIdx = 0;

static void sleepRecordSample(const int64_t ns) {
    gSampleNs[gNextSampleIdx++] = ns;
    gNextSampleIdx %= gSampleCount;

    if (gNextSampleIdx > gLastSampleIdx) gLastSampleIdx = gNextSampleIdx;
}

static void sleepTimerTick(const int64_t ns) {
    const timeInstant start = timeGetNow();

    sleepPrecise(ns);

    const timeInstant end = timeGetNow();

    sleepRecordSample(timeElapsedNs(start, end));
}

void Sleep_loop(struct sleep_loop_t *const loop) {
    assert(loop != NULL);
    assert(!loop->halt);
    assert(loop->intervalMs > 0);
    assert(loop->fn != NULL);

    static int64_t lostNs = 0;

    while (true) {
        const timeInstant start = timeGetNow();

        loop->fn(loop);

        if (loop->halt) break;

        const int64_t intervalNs = loop->intervalMs * 1000000;

        // if `sleep()` did not take the full time allowance, calculate
        // the remaining time budget and sleep for the full duration
        int64_t remainingNs = intervalNs - timeElapsedNs(start, timeGetNow());

        // if the previous loop ran too long, attempt to recover some of the lost time
        // by decreasing the maximum sleep period in this iteration
        if (lostNs > 0) remainingNs -= lostNs;

        if (remainingNs > 0) sleepTimerTick(remainingNs);

        // measure the full time spent ticking and sleeping, subtracting from the
        // allowed interval, and notify fplayer of any dropped frames
        const int64_t fullLoopNs = timeElapsedNs(start, timeGetNow());

        lostNs = fullLoopNs > intervalNs ? fullLoopNs - intervalNs : 0;
    }
}
