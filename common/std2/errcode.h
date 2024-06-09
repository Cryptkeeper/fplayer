#ifndef FPLAYER_ERRCODE_H
#define FPLAYER_ERRCODE_H

#define FP_EOK      0  /* no error                      */
#define FP_ESYSCALL 1  /* system call failed            */
#define FP_ERANGE   2  /* value out of range            */
#define FP_ENOMEM   3  /* out of memory                 */
#define FP_EDECODE  4  /* file data decoding error      */
#define FP_ENOSUP   5  /* operation not supported       */
#define FP_EZSTD    6  /* zstd library usage error      */
#define FP_EALCTL   7  /* OpenAL/alut control error     */
#define FP_EPLAYAUD 8  /* audio playback error          */
#define FP_ESEQEND  9  /* end of sequence               */
#define FP_ENODEV   10 /* no device available           */
#define FP_EDEVCONF 11 /* device configuration error    */
#define FP_EBADJSON 12 /* JSON parsing error            */
#define FP_EINVAL   13 /* invalid argument              */
#define FP_EPTHREAD 14 /* pthread error                 */
#define FP_ECOUNT   15 /* error code count              */

const char* FP_strerror(int err);

#endif//FPLAYER_ERRCODE_H
