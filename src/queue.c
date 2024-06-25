#include "queue.h"

#include <assert.h>
#include <stdlib.h>

#include "std2/errcode.h"

struct q_s {
    struct qentry_s* entries;
    int size;
    int cap;
};

int Q_init(struct q_s** q) {
    assert(q != NULL);
    *q = (struct q_s*) calloc(1, sizeof(struct q_s));
    if (*q == NULL) return -FP_ENOMEM;
    return FP_EOK;
}

int Q_append(struct q_s* q, struct qentry_s ent) {
    assert(q != NULL);
    if (q->size == q->cap) {
        struct qentry_s* ents = (struct qentry_s*) realloc(
                q->entries, (q->cap + 1) * sizeof(struct qentry_s));
        if (ents == NULL) return -FP_ENOMEM;
        q->entries = ents;
        q->cap++;
    }
    assert(q->size < q->cap);
    q->entries[q->size++] = ent;
    return FP_EOK;
}

int Q_next(struct q_s* q, struct qentry_s* ent) {
    assert(q != NULL);
    assert(ent != NULL);
    if (q->size == 0) return 1;
    *ent = q->entries[0];
    for (int i = 0; i < q->size - 1; i++) q->entries[i] = q->entries[i + 1];
    q->size--;
    assert(q->size >= 0);
    // TODO: capacity is never reduced
    return 0;
}

void Q_free(struct q_s* q) {
    if (q == NULL) return;
    free(q->entries);
    free(q);
}
