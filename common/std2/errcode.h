#ifndef FPLAYER_ERRCODE_H
#define FPLAYER_ERRCODE_H

#define FP_EOK       0  /* BASIC:   no error                    */
#define FP_ERANGE    1  /*          value out of range          */
#define FP_EINVLARG  2  /*          invalid argument            */
#define FP_ESYSCALL  3  /* OS:      system call failed          */
#define FP_ENOMEM    4  /*          out of memory               */
#define FP_EPTHREAD  5  /*          pthread error               */
#define FP_EZSTD     6  /* LIB:     zstd library usage error    */
#define FP_EAUDINIT  7  /*          alut init error             */
#define FP_EAUDPLAY  8  /*          audio playback error        */
#define FP_EINVLBIN  9  /* DATA:    invalid binary file layout  */
#define FP_EINVLFMT  10 /*          invalid JSON file format    */
#define FP_ENOSDEV   11 /* SERIAL:  device not found            */
#define FP_ESDEVINIT 12 /*          device init error           */
#define FP_ECOUNT    13 /* CODE:    error code count            */

/// @brief Returns the error message string for the given error code.
/// @param err the error code to get the message for, must be in the range of
/// [0, FP_ECOUNT), negative values are treated as positive
/// @return the error message string, otherwise "FP_EUNKNOWN" if the error code
/// is out of range
const char* FP_strerror(int err);

#endif//FPLAYER_ERRCODE_H
