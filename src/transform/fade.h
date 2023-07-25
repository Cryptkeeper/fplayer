#ifndef FPLAYER_FADE_H
#define FPLAYER_FADE_H

#include <stdbool.h>
#include <stdint.h>

enum fade_type_t {
    FADE_SLOPE,
    FADE_FLASH,
};

typedef struct fade_t {
    uint8_t from;
    uint8_t to;
    uint32_t startFrame;
    uint16_t frames;// max dur. of 25s (`LOR_TIME_SECONDS_MAX`) @ 1k FPS = 25000
    enum fade_type_t type;
} Fade;

void fadePush(uint32_t id, Fade fade);

int fadeTableSize(void);

bool fadeTableCache(const char *fp);

bool fadeTableLoadCache(const char *fp);

void fadeFrameFree(uint32_t frame);

void fadeFree(void);

bool fadeGet(int handle, Fade *fade);

void fadeGetChange(uint32_t frame, uint32_t id, int *started, bool *finishing);

bool fadeRelocateFrameForward(uint32_t frame);

#endif//FPLAYER_FADE_H
