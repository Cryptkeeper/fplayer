#include "sleep.h"

#include <assert.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "std2/time.h"

/// @brief Appends the given sleep time to the sleep collector, updating the
/// collector's internal state. This should be called after each sleep operation
/// to provide historical data for better sleep time estimation. Once full, the
/// collector will overwrite the oldest sleep time.
/// @param coll target sleep collector
/// @param ns sleep time in nanoseconds
static void Sleep_record(struct sleep_coll_s* coll, const int64_t ns) {
    coll->ns[coll->idx] = ns;
    coll->idx = (coll->idx + 1) % SLEEP_COLL_SAMPLE_COUNT;

    if (coll->cnt < SLEEP_COLL_SAMPLE_COUNT) coll->cnt++;
}

int64_t Sleep_average(const struct sleep_coll_s* coll) {
    assert(coll != NULL);

    if (coll->cnt == 0) return 0;
    int64_t sum = 0;
    for (int i = 0; i < coll->cnt; i++) sum += coll->ns[i];
    return sum / coll->cnt;
}

/// @brief Sleeps for one millisecond using the system's equivalent sleep
/// function. This is used to provide a baseline for sleep time estimation.
static inline void Sleep_oneMilli(void) {
#ifdef _WIN32
    Sleep(1);
#else
    static const struct timespec timeOneMs = {
            .tv_sec = 0,
            .tv_nsec = 1000000,
    };
    nanosleep(&timeOneMs, NULL);
#endif
}

/// @brief Sleeps for the given number of nanoseconds using incremental sleep
/// calls that estimate the time remaining to sleep using historical sleep data.
/// @param ns number of nanoseconds to sleep
static void Sleep_step(const int64_t ns) {
    int64_t est = 1000000; /* estimated ns required to sleep one millisecond */
    int64_t mean = 0;      /* sum of sleep times used to calculate mean */
    int64_t m2 = 0;        /* sum of squared differences from the mean */
    unsigned itr = 0;      /* number of sleep iterations */
    int64_t rem = ns;      /* remaining ns to sleep */

    while (rem > est) {
        const timeInstant start = timeGetNow();
        Sleep_oneMilli();

        const int64_t elapsed = timeElapsedNs(start, timeGetNow());
        rem -= elapsed;// decrement remaining time according to the real cost
        ++itr;

        // update mean and variance for the next iteration
        const int64_t d = elapsed - mean;
        mean += d / itr;
        m2 += d * (elapsed - mean);

        // update the estimated time to sleep one millisecond
        est = mean + (int64_t) sqrt((double) m2 / (itr - 1));
    }
}

/// @brief Sleeps for the given number of nanoseconds using a spin lock. This is
/// a blocking operation that will consume CPU cycles until the given time has
/// elapsed.
/// @param ns number of nanoseconds to sleep
static void Sleep_spinLock(const int64_t ns) {
    const timeInstant start = timeGetNow();
spin:
    if (timeElapsedNs(start, timeGetNow()) < ns) goto spin;
}

void Sleep_do(struct sleep_coll_s* coll, const uint32_t ms) {
    assert(coll != NULL);
    assert(ms > 0);

    const int64_t ns = ms * 1000000;
    const timeInstant start = timeGetNow();
    Sleep_step(ns);

    // consume any remaining time with a spin lock
    const int64_t elapsed = timeElapsedNs(start, timeGetNow());
    if (ns > elapsed) Sleep_spinLock(ns - elapsed);

    // record a sample of the full time slept
    const int64_t end = timeElapsedNs(start, timeGetNow());
    Sleep_record(coll, end);
}
