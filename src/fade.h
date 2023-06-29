#ifndef FPLAYER_FADE_H
#define FPLAYER_FADE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct fade_t {
    uint32_t id;
    uint8_t from;
    uint8_t to;
    uint32_t startFrame;
    int frames;
    int rc;// reference counter for garbage collection
} Fade;

void fadePush(uint32_t startFrame, Fade fade);

void fadeFrameFree(uint32_t frame);

void fadeGetStatus(uint32_t frame,
                   uint32_t id,
                   Fade **started,
                   bool *finishing);

void fadeDump(void);

#endif//FPLAYER_FADE_H
