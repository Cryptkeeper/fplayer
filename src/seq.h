#ifndef FPLAYER_SEQ_H
#define FPLAYER_SEQ_H

#include <stdint.h>

#include "std2/fc.h"
#include "tinyfseq.h"

extern TFHeader curSequence; /* current sequence opened for playback */

/**
 * \brief Reads a FSEQ header and initializes the `curSequence` global. This function must be called before any other sequence functions.
 * \param fc target file controller instance
 */
void Seq_initHeader(struct FC* fc);

/**
 * \brief Retrieves the audio file path from the sequence for playback by searching the FSEQ's variable table for the `mf` (media file) variable.
 * \param fc target file controller instance
 * \return Either NULL indicating no matching variable was found, or a duplicated `char *` containing the value of the `mf` (media file) variable. Caller is responsible for freeing the returned string.
 */
char *Seq_getMediaFile(struct FC* fc);

struct seq_read_args_t {
    uint32_t startFrame; /* the frame index to begin reading at */
    uint32_t frameSize;  /* the byte size of each individual frame */
    uint32_t frameCount; /* the number of frames to read */
};

/**
 * \brief Reads binary frame data from `fc` into `b` using the provided `args` configuration. Used to request frames at arbitrary positions. The out buffer `b` must be presized to at least `args.frameSize * arms.frameCount`.
 * \param fc target file controller instance
 * \param args configuration args which calculate read position and size
 * \param b frame data copy buffer, must be pre-sized correctly
 * \return the number of frames read, up to `args.frameCount`
 */
uint32_t Seq_readFrames(struct FC* fc, struct seq_read_args_t args, uint8_t *b);

#endif//FPLAYER_SEQ_H
