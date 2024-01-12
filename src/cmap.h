#ifndef FPLAYER_CMAP_H
#define FPLAYER_CMAP_H

#include <stdbool.h>
#include <stdint.h>

enum cmap_parse_result_t { CMAP_PARSE_OK, CMAP_PARSE_EMPTY, CMAP_PARSE_ERROR };

enum cmap_parse_result_t channelMapParseCSVLine(int line, const char *row);

void channelMapInit(const char *filepath);

bool channelMapFind(uint32_t id, uint8_t *unit, uint16_t *circuit);

uint8_t *channelMapGetUids(void);

void channelMapFree(void);

#endif//FPLAYER_CMAP_H
