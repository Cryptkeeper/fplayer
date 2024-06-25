#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "queue.h"

/// @brief Check if two qentry_s structs contain the same data field-by-field.
/// @param a first qentry_s struct to compare
/// @param b second qentry_s struct to compare
/// @return true if the structs contain the same data, false otherwise
static bool entry_eq(const struct qentry_s* a, const struct qentry_s* b) {
    return strcmp(a->seqfp, b->seqfp) == 0 &&
           strcmp(a->audiofp, b->audiofp) == 0 &&
           strcmp(a->cmapfp, b->cmapfp) == 0 && a->waitsec == b->waitsec;
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    struct q_s* q = NULL;
    assert(Q_init(&q) == 0);

    const struct qentry_s first = {
            .seqfp = "first.fseq",
            .audiofp = "first.wav",
            .cmapfp = "first.json",
            .waitsec = 1,
    };
    assert(Q_append(q, first) == 0);

    const struct qentry_s second = {
            .seqfp = "second.fseq",
            .audiofp = "second.wav",
            .cmapfp = "second.json",
            .waitsec = 2,
    };
    assert(Q_append(q, second) == 0);

    struct qentry_s ent;
    assert(Q_next(q, &ent) == 0);
    assert(entry_eq(&ent, &first));

    assert(Q_next(q, &ent) == 0);
    assert(entry_eq(&ent, &second));

    assert(Q_next(q, &ent) == 1);

    Q_free(q);

    return 0;
}
