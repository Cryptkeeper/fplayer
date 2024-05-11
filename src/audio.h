#ifndef FPLAYER_AUDIO_H
#define FPLAYER_AUDIO_H

#include <stdbool.h>

/// @brief Exits the audio system and frees all resources. This function should
/// be called before the program exits, or when the audio system is no longer
/// needed. This function is safe to call even if the audio system has not been
/// initialized.
void Audio_exit(void);

/// @brief Checks the playback status of the audio system.
/// @return true if the audio system is playing audio, false otherwise (e.g. the
/// audio system has completed playback or failed to start playback)
bool Audio_isPlaying(void);

/// @brief Attempts to play the audio file at the given file path. This function
/// initializes the audio system if it has not been initialized yet. If the audio
/// system is already playing audio, the current audio playback is stopped before
/// the new audio file is played.
/// @param fp the file path of the audio file to play
/// @return 0 on success, a negative error code on failure
int Audio_play(const char* fp);

#endif//FPLAYER_AUDIO_H
