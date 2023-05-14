#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "cmap.h"
#include "player.h"
#include "serial.h"

static void printUsage(void) {
    printf("Usage: fplayer -f=FILE [options] ...\n");
    printf("Options:\n");
    printf("\t-f <file>\t\tFSEQ v2 sequence file path (required)\n");
    printf("\t-c <file>\t\tNetwork channel map file path\n");
    printf("\t-a <file>\t\tOverride audio with specified filepath\n");
    printf("\t-r <frame ms>\t\tOverride playback frame rate interval (in "
           "milliseconds)\n");
    printf("\t-d <device name>\tDevice name for serial port connection\n");
    printf("\t-b <baud rate>\t\tSerial port baud rate (defaults to 19200)\n");
    printf("\t-h\t\t\tPrint this message and exit\n");
}

static PlayerOpts gPlayerOpts;

static SerialOpts gSerialOpts = {
        .baudRate = 19200,
};

int main(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, ":hf:c:a:r:d:b:")) != -1) {
        switch (c) {
            case 'h':
                printUsage();
                return 0;

            case ':':
                fprintf(stderr, "option is missing argument: %c\n", optopt);
                return 1;

            case '?':
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                return 1;

            case 'f':
                gPlayerOpts.sequenceFilePath = strdup(optarg);
                assert(gPlayerOpts.sequenceFilePath != NULL);
                break;

            case 'c':
                gPlayerOpts.channelMapFilePath = strdup(optarg);
                assert(gPlayerOpts.channelMapFilePath != NULL);
                break;

            case 'a':
                gPlayerOpts.audioOverrideFilePath = strdup(optarg);
                assert(gPlayerOpts.audioOverrideFilePath != NULL);
                break;

            case 'r': {
                const long l = strtol(optarg, NULL, 10);

                if (l <= 0 || l > UINT8_MAX) {
                    fprintf(stderr,
                            "invalid frame step time (milliseconds): %s\n",
                            optarg);
                    return 1;
                }

                gPlayerOpts.frameStepTimeOverrideMillis = (uint8_t) l;
                break;
            }

            case 'd':
                gSerialOpts.devName = strdup(optarg);
                assert(gSerialOpts.devName != NULL);
                break;

            case 'b': {
                const long l = strtol(optarg, NULL, 10);

                if (l < UINT_MAX || l > UINT_MAX) {
                    fprintf(stderr, "invalid baud rate: %s\n", optarg);
                    return 1;
                }

                gSerialOpts.baudRate = (int) l;
                break;
            }
        }
    }

    if (gPlayerOpts.sequenceFilePath == NULL) {
        printUsage();

        return 1;
    }

    argc -= optind;
    argv += optind;

    audioInit(&argc, argv);

    if (channelMapInit(gPlayerOpts.channelMapFilePath)) return 1;

    if (serialInit(gSerialOpts)) return 1;
    if (playerInit(gPlayerOpts)) return 1;

    serialExit();

    channelMapFree();

    audioExit();

    serialOptsFree(&gSerialOpts);
    playerOptsFree(&gPlayerOpts);

    return 0;
}
