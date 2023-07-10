#ifndef FPLAYER_PCF_H
#define FPLAYER_PCF_H

#include <stdbool.h>
#include <stdint.h>

struct pcf_directory_t {
    unsigned char magic[4];
    uint32_t nFades; // pcf_fade_t[]
    uint32_t nFrames;// pcf_frame_t[]
} __attribute__((packed));

typedef struct pcf_directory_t pcf_directory_t;

struct pcf_fade_t {
    uint8_t from;
    uint8_t to;
    uint16_t frames;
} __attribute__((packed));

typedef struct pcf_fade_t pcf_fade_t;

struct pcf_frame_t {
    uint32_t frame;
    uint32_t nEvents;// pcf_event_t[]
} __attribute__((packed));

typedef struct pcf_frame_t pcf_frame_t;

struct pcf_event_t {
    uint32_t circuit;
    uint32_t fade;
} __attribute__((packed));

typedef struct pcf_event_t pcf_event_t;

typedef struct pcf_file_t {
    pcf_fade_t *fades;
    pcf_frame_t *frames;
    pcf_event_t **events;// index matches `frames` index
} pcf_file_t;

bool pcfOpen(const char *fp, pcf_file_t *file);

bool pcfSave(const char *fp, const pcf_file_t *file);

void pcfFree(pcf_file_t *file);

#endif//FPLAYER_PCF_H
