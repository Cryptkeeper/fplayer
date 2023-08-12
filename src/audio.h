#ifndef FPLAYER_AUDIO_H
#define FPLAYER_AUDIO_H

#include <stdbool.h>

extern bool gAudioIgnoreErrors;

void audioInit(int *argc, char **argv);

void audioExit(void);

bool audioCheckPlaying(void);

void audioPlayFile(const char *filepath);

#endif//FPLAYER_AUDIO_H
