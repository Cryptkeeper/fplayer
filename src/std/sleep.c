#include "sleep.h"

#include <assert.h>
#include <math.h>

#include "time.h"

#define gSampleCount 20

static int64_t gSampleNs[gSampleCount];
static int gLastSampleIdx;

static double gAccumulatedLoss;

sds sleepGetStatus(void) {
    double avg = 0;
    int n = 1;

    for (int i = 0; i <= gLastSampleIdx; i++) {
        const int64_t drift = gSampleNs[i];

        avg += ((double) drift - avg) / n;
        n += 1;
    }

    const double ms = avg / 1e6;
    const double fps = ms > 0 ? 1000 / ms : 0;

    return sdscatprintf(sdsempty(), "%.4fms (%.2f fps) [%.1fms loss]", ms, fps,
                        gAccumulatedLoss);
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
static inline void sleepPrecise(const int64_t ns) {
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

    timeInstant start, end;

    while (remainingTime > estimate) {
        // measure the nanoseconds it takes for the system to perform
        // a one millisecond #nanosleep call
        start = timeGetNow();

        const struct timespec *timeOneMs = &(const struct timespec){
                .tv_sec = 0,
                .tv_nsec = 1000000,
        };

        nanosleep(timeOneMs, NULL);

        end = timeGetNow();

        const double sSample = (double) timeElapsedNs(start, end) / 1e9;
        remainingTime -= sSample;
        sampleCount++;

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

    timeInstant now;

spin:
    now = timeGetNow();

    if (timeElapsedNs(start, now) < ns) goto spin;
}

static int gNextSampleIdx = 0;

static inline void sleepRecordSample(const int64_t ns) {
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

void sleepTimerLoop(const struct sleep_loop_config_t config) {
    assert(config.intervalMillis > 0);
    assert(config.sleep != NULL);
    assert(config.skip != NULL);

    timeInstant start;

    while (true) {
        start = timeGetNow();

        if (!config.sleep()) break;

        const int64_t interval = config.intervalMillis * 1000000;

        // if `config.sleep` did not take the full time allowance, calculate
        // the remaining time budget and sleep for the full duration
        const int64_t remainingTime =
                interval - timeElapsedNs(start, timeGetNow());

        if (remainingTime > 0) sleepTimerTick(remainingTime);

        // measure the full time spent ticking and sleeping, subtracting from the
        // allowed interval, and notify fplayer of any dropped frames
        const int64_t fullLoopTime = timeElapsedNs(start, timeGetNow());

        if (fullLoopTime > interval) {
            const double loss =
                    ((double) fullLoopTime / 1e6) - ((double) interval / 1e6);

            gAccumulatedLoss += loss;

            // check if the loss has accumulated to the value of at least one frame
            // in duration, and if so, skip the frame(s) and subtract the time gained
            const int lostFrames = (int) floor(gAccumulatedLoss /
                                               (double) config.intervalMillis);

            if (lostFrames > 0) {
                config.skip((uint32_t) lostFrames);

                gAccumulatedLoss -=
                        (double) (lostFrames * config.intervalMillis);
            }
        }
    }
}
