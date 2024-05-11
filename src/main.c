#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#include "audio.h"
#include "crmap.h"
#include "player.h"
#include "serial.h"
#include "std2/errcode.h"
#include "std2/fc.h"
#include "std2/sl.h"
#include "std2/string.h"

static void printUsage(void) {
    printf("Usage: fplayer -f=FILE -c=FILE [options] ...\n\n"

           "Options:\n\n"

           "[Playback]\n"
           "\t-f <file>\t\tFSEQ v2 sequence file path (required)\n"
           "\t-c <file>\t\tNetwork channel map file path (required)\n"
           "\t-d <device name|stdout>\tDevice name for serial port "
           "connection\n"
           "\t-b <baud rate>\t\tSerial port baud rate (defaults to 19200)\n"

           "[Controls]\n"
           "\t-a <file>\t\tOverride audio with specified filepath\n"
           "\t-r <frame ms>\t\tOverride playback frame rate interval (in "
           "milliseconds)\n"
           "\t-w <seconds>\t\tPlayback start delay to allow connection "
           "setup\n\n"

           "[CLI]\n"
           "\t-t <file>\t\tTest load channel map and exit\n"
           "\t-l\t\t\tPrint available serial port list and exit\n"
           "\t-h\t\t\tPrint this message and exit\n");
}

static char* gSequenceFilePath;
static char* gAudioOverrideFilePath;

static char* gChannelMapFilePath;

static uint8_t gWaitSeconds;

static char* gSerialDevName;
static int gSerialBaudRate = 19200;

static void printSerialEnumPorts(void) {
    slist_t* ports = Serial_getPorts();

    for (int i = 0; ports != NULL && ports[i] != NULL; i++)
        printf("%s\n", ports[i]);

    slfree(ports);
}

/// @brief Parse command line options and sets global variables for program
/// execution.
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
                const int err = CR_read(optarg, &cmap);
                CR_free(cmap);
                if (err) {
                    fprintf(stderr,
                            "failed to parse channel map file `%s`: %d\n",
                            optarg, err);
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
                if ((gSequenceFilePath = strdup(optarg)) == NULL)
                    return -FP_ENOMEM;
                break;
            case 'c':
                if ((gChannelMapFilePath = strdup(optarg)) == NULL)
                    return -FP_ENOMEM;
                break;
            case 'a':
                if ((gAudioOverrideFilePath = strdup(optarg)) == NULL)
                    return -FP_ENOMEM;
                break;
            case 'w':
                if (strtolb(optarg, 0, UINT8_MAX, &gWaitSeconds,
                            sizeof(gWaitSeconds))) {
                    fprintf(stderr, "error parsing `%s` as an integer\n",
                            optarg);
                    return -FP_EINVAL;
                }
                break;
            case 'd':
                if ((gSerialDevName = strdup(optarg)) == NULL)
                    return -FP_ENOMEM;
                break;
            case 'b':
                if (strtolb(optarg, 0, INT32_MAX, &gSerialBaudRate,
                            sizeof(gSerialBaudRate))) {
                    fprintf(stderr, "error parsing `%s` as an integer\n",
                            optarg);
                    return -FP_EINVAL;
                }
                break;
            case ':':
                fprintf(stderr, "option is missing argument: %c\n", optopt);
                return -FP_EINVAL;
            case '?':
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                return -FP_EINVAL;
        }
    }

    if (gSequenceFilePath == NULL || gChannelMapFilePath == NULL) {
        printUsage();
        return -FP_EINVAL;
    }

    return FP_EOK;
}

static void freeArgs(void) {
    free(gSequenceFilePath);
    free(gAudioOverrideFilePath);
    free(gChannelMapFilePath);
    free(gSerialDevName);
}

int main(const int argc, char** const argv) {
    int err;
    if ((err = parseOpts(argc, argv))) {
        if (err < 0) fprintf(stderr, "failed to parse options: %d\n", err);

        // demote positive values (which indicate an early return request) to 0
        return err < 0 ? 1 : 0;
    }

    // load required app context configs
    struct cr_s* cmap = NULL;
    if ((err = CR_read(gChannelMapFilePath, &cmap))) {
        fprintf(stderr, "failed to read/parse channel map file `%s`: %d\n",
                gChannelMapFilePath, err);
        return 1;
    }

    // open sequence file and init controller handler
    struct FC* fc = FC_open(gSequenceFilePath, FC_MODE_READ);
    if (fc == NULL) {
        fprintf(stderr, "failed to open sequence file `%s`\n",
                gSequenceFilePath);
        return 1;
    }

    // initialize core subsystems
    Serial_init(gSerialDevName, gSerialBaudRate);

    struct player_s player = {
            .fc = fc,
            .audiofp = gAudioOverrideFilePath,
            .wait_s = gWaitSeconds,
            .cmap = cmap,
    };

    if ((err = PL_play(&player)))
        fprintf(stderr, "failed to play sequence: %d\n", err);

    FC_close(fc);

    // teardown in reverse order
    Serial_close();
    audioExit();

    CR_free(cmap);

    freeArgs();

    return 0;
}
