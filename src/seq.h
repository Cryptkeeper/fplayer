#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

struct FC;

extern struct tf_header_t* curSequence; /* opened sequence for playback */

/// @brief Reads a FSEQ header and initializes the `curSequence` global. This
/// function must be called before any other sequence functions.
/// @param fc target file controller instance
/// @return 0 on success, a negative error code on failure
int Seq_open(struct FC* fc);

/// @brief Frees the global `curSequence` variable and any associated resources.
/// This function should be called when the sequence is no longer needed.
void Seq_close(void);

/// @brief Retrieves the audio file path from the sequence for playback by
/// searching the FSEQ's variable table for the `mf` (media file) variable.
/// @param fc target file controller instance
/// @param value the value of the `mf` variable if found, otherwise set to NULL
/// @return 0 on success, a negative error code on failure
int Seq_getMediaFile(struct FC* fc, char** value);

#endif//FPLAYER_SEQ_H
