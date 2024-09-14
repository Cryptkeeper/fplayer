/// @file seq.h
/// @brief FSEQ sequence file loading API.
#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

struct FC;

struct tf_header_t;

/// @brief Reads a FSEQ header and initializes the header struct with the
/// sequence's metadata. The caller is responsible for freeing the header.
/// @param fc target file controller instance
/// @param seq the sequence header to populate with the FSEQ metadata
/// @return 0 on success, a negative error code on failure
int Seq_open(struct FC* fc, struct tf_header_t** seq);

/// @brief Retrieves the audio file path from the sequence for playback by
/// searching the FSEQ's variable table for the `mf` (media file) variable.
/// @param fc target file controller instance
/// @param seq sequence header for file layout information
/// @param value the value of the `mf` variable if found, otherwise set to NULL
/// @return 0 on success, a negative error code on failure
int Seq_getMediaFile(struct FC* fc, const struct tf_header_t *seq, char** value);

#endif//FPLAYER_SEQ_H
