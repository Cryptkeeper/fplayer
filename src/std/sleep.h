#ifndef FPLAYER_SLEEP_H
#define FPLAYER_SLEEP_H

#include <stdbool.h>

char *Sleep_status(void);

struct sleep_loop_t {
    void *args;               /* optional argument to pass to fn */
    bool halt;                /* halted status */
    char *msg;                /* halt message, != NULL if halt = true */
    unsigned long intervalMs; /* target sleep interval time in milliseconds */
    void (*fn)(struct sleep_loop_t *loop);
};

void Sleep_halt(struct sleep_loop_t *loop, char *msg);

void Sleep_loop(struct sleep_loop_t *loop);

#endif//FPLAYER_SLEEP_H
