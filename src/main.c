#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TINYFSEQ_IMPLEMENTATION
#include "tinyfseq.h"

#include "audio.h"
#include "crmap.h"
#include "player.h"
#include "queue.h"
#include "serial.h"
#include "std2/errcode.h"
#include "std2/sl.h"
#include "std2/string.h"

/// @brief Prints the program usage message to stdout.
static void printUsage(void) {
    printf("Usage: fplayer -f=FILE -c=FILE [options] ...\n\n"

           "Options:\n\n"

           "[Playback]\n"
           "\t-f <file>\t\tFSEQ v2 sequence file path (required)\n"
           "\t-c <file>\t\tNetwork channel map file path (required)\n"
           "\t-d <device name|stdout>\tDevice name for serial port "
           "connection\n"
           "\t-b <baud rate>\t\tSerial port baud rate (defaults to 19200)\n\n"

           "[Controls]\n"
           "\t-a <file>\t\tOverride audio with specified filepath\n"
           "\t-w <seconds>\t\tPlayback start delay to allow connection "
           "setup\n\n"

           "[CLI]\n"
           "\t-t <file>\t\tTest load channel map and exit\n"
           "\t-l\t\t\tPrint available serial port list and exit\n"
           "\t-h\t\t\tPrint this message and exit\n");
}

/// @brief Enumerates available serial ports and prints their reported device
/// names to stdout.
static void printSerialEnumPorts(void) {
    slist_t* ports = Serial_getPorts();
    for (int i = 0; ports != NULL && ports[i] != NULL; i++)
        printf("%s\n", ports[i]);
    slfree(ports);
}

static struct {
    char* seqfp;          /* sequence file path        */
    char* audiofp;        /* audio override file path  */
    char* cmapfp;         /* channel map file path     */
    unsigned int waitsec; /* playback start delay      */
    char* spname;         /* serial port device name   */
    int spbaud;           /* serial port baud rate     */
} gOpts;

/// @brief Parse command line options and sets global variables for program
/// execution via `gOpts`.
/// @param argc argument count
/// @param argv argument vector
/// @return negative on error to indicate the program should exit (with an error
/// code of 1), positive to indicate the program should exit without an error
/// code, and zero to indicate the program should continue execution
static int parseOpts(const int argc, char** const argv) {
    int c;
    while ((c = getopt(argc, argv, ":t:ilhf:c:a:w:d:b:")) != -1) {
        switch (c) {
            case 't': {
                struct cr_s* cmap = NULL;
                const int err = CMap_read(optarg, &cmap);
                CMap_free(cmap);
                if (err) {
                    fprintf(stderr,
                            "failed to parse channel map file `%s`: %s %d\n",
                            optarg, FP_strerror(err), err);
                    return err;
                }
                return 1;
            }
            case 'l':
                printSerialEnumPorts();
                return 1;
            case 'h':
                printUsage();
                return 1;
            case 'f':
                if ((gOpts.seqfp = strdup(optarg)) == NULL) return -FP_ENOMEM;
                break;
            case 'c':
                if ((gOpts.cmapfp = strdup(optarg)) == NULL) return -FP_ENOMEM;
                break;
            case 'a':
                if ((gOpts.audiofp = strdup(optarg)) == NULL) return -FP_ENOMEM;
                break;
            case 'w':
                if (strtolb(optarg, 0, UINT_MAX, &gOpts.waitsec,
                            sizeof(gOpts.waitsec))) {
                    fprintf(stderr, "error parsing `%s` as an integer\n",
                            optarg);
                    return -FP_EINVLARG;
                }
                break;
            case 'd':
                if ((gOpts.spname = strdup(optarg)) == NULL) return -FP_ENOMEM;
                break;
            case 'b':
                if (strtolb(optarg, 0, UINT_MAX, &gOpts.spbaud,
                            sizeof(gOpts.spbaud))) {
                    fprintf(stderr, "error parsing `%s` as an integer\n",
                            optarg);
                    return -FP_EINVLARG;
                }
                break;
            case ':':
                fprintf(stderr, "option is missing argument: %c\n", optopt);
                return -FP_EINVLARG;
            case '?':
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                return -FP_EINVLARG;
        }
    }

    if (gOpts.seqfp == NULL || gOpts.cmapfp == NULL) {
        printUsage();
        return 1;
    }

    return FP_EOK;
}

static void freeOpts(void) {
    free(gOpts.seqfp);
    free(gOpts.audiofp);
    free(gOpts.cmapfp);
    free(gOpts.spname);
}

int main(const int argc, char** const argv) {
    struct q_s* pq = NULL;           /* playback queue */
    struct serialdev_s* sdev = NULL; /* serial device */

    int err;
    if ((err = parseOpts(argc, argv))) {
        // demote positive values (which indicate an early return request) to 0
        // to avoid activating the error handling routine at exit
        if (err > 0) err = FP_EOK;
        goto ret;
    }

    // initialize serial port device
    const int br = gOpts.spbaud ? gOpts.spbaud : 19200;
    if ((err = Serial_init(&sdev, gOpts.spname, br))) {
        fprintf(stderr,
                "failed to initialize serial port `%s` at %d baud: %s %d\n",
                gOpts.spname, br, FP_strerror(err), err);
        goto ret;
    }

    // initialize a queue with the single requested entry
    // TODO: expose ability to queue multiple/schedule playlist
    if ((err = Q_init(&pq)) ||
        (err = Q_append(pq, (struct qentry_s){
                                    .seqfp = gOpts.seqfp,
                                    .audiofp = gOpts.audiofp,
                                    .cmapfp = gOpts.cmapfp,
                                    .waitsec = gOpts.waitsec,
                            }))) {
        fprintf(stderr, "failed to initialize playback queue: %s %d\n",
                FP_strerror(err), err);
        goto ret;
    }
    
    // loop through the queue and execute each entry
    struct qentry_s req;
    while (Q_next(pq, &req)) {
        printf("playing: %s (channel map: %s)\n", req.seqfp, req.cmapfp);

        if (req.audiofp != NULL) printf("audio override: %s\n", req.audiofp);

        if ((err = Player_exec(&req, sdev))) {
            fprintf(stderr, "failed to execute player: %s %d\n",
                    FP_strerror(err), err);
            goto ret;
        }
    }

ret:
    // attempt shutdown of controlled systems
    Q_free(pq);
    Serial_close(sdev);
    Audio_exit();

    // free immediately owned resources
    freeOpts();

    if (err)
        fprintf(stderr, "exiting with internal error code: %s %d\n",
                FP_strerror(err), err);

    return err ? 1 : 0;
}
