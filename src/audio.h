#ifndef FPLAYER_AUDIO_H
#define FPLAYER_AUDIO_H

#include <stdbool.h>

void audioInit(void);

void audioExit(void);

bool audioCheckPlaying(void);

void audioPlayFile(const char *filepath);

#endif//FPLAYER_AUDIO_H
