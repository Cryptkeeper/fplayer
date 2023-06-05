#include <getopt.h>
#include <stdio.h>

#include <AL/alut.h>
#include <libserialport.h>

#ifdef ENABLE_ZSTD
#include <zstd.h>
#endif

#include "../libtinyfseq/tinyfseq.h"

#include "audio.h"
#include "cmap.h"
#include "mem.h"
#include "parse.h"
#include "player.h"
#include "serial.h"

static void printUsage(void) {
    printf("Usage: fplayer -f=FILE [options] ...\n");

    printf("\nOptions:\n");

    printf("\n[Playback]\n");
    printf("\t-f <file>\t\tFSEQ v2 sequence file path (required)\n");
    printf("\t-c <file>\t\tNetwork channel map file path\n");
    printf("\t-d <device name>\tDevice name for serial port connection\n");
    printf("\t-b <baud rate>\t\tSerial port baud rate (defaults to 19200)\n");

    printf("\n[Controls]\n");
    printf("\t-a <file>\t\tOverride audio with specified filepath\n");
    printf("\t-r <frame ms>\t\tOverride playback frame rate interval (in "
           "milliseconds)\n");
    printf("\t-w <seconds>\t\tPlayback start delay to allow connection "
           "setup\n");

    printf("\n[CLI]\n");
    printf("\t-h\t\t\tPrint this message and exit\n");
    printf("\t-v\t\t\tPrint library versions and exit\n");
}

static void printVersions(void) {
    printf("ALUT %d.%d\n", alutGetMajorVersion(), alutGetMinorVersion());
    printf("libtinyfseq %s\n", TINYFSEQ_VERSION);
    printf("libserialport %s\n", SP_PACKAGE_VERSION_STRING);
    printf("OpenAL %s\n", alGetString(AL_VERSION));

#ifdef ENABLE_ZSTD
    printf("zstd %s\n", ZSTD_versionString());
#else
    printf("zstd disabled\n");
#endif
}

static PlayerOpts gPlayerOpts;

static SerialOpts gSerialOpts = {
        .baudRate = 19200,
};

int main(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, ":hvf:c:a:r:w:d:b:")) != -1) {
        switch (c) {
            case 'h':
                printUsage();
                return 0;

            case 'v':
                printVersions();
                return 0;

            case ':':
                fprintf(stderr, "option is missing argument: %c\n", optopt);
                return 1;

            case '?':
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                return 1;

            case 'f':
                gPlayerOpts.sequenceFilePath = mustStrdup(optarg);
                break;

            case 'c':
                gPlayerOpts.channelMapFilePath = mustStrdup(optarg);
                break;

            case 'a':
                gPlayerOpts.audioOverrideFilePath = mustStrdup(optarg);
                break;

            case 'r':
                parseLong(optarg, &gPlayerOpts.frameStepTimeOverrideMs,
                          sizeof(gPlayerOpts.frameStepTimeOverrideMs), 1,
                          UINT8_MAX);
                break;

            case 'w':
                parseLong(optarg, &gPlayerOpts.connectionWaitS,
                          sizeof(gPlayerOpts.connectionWaitS), 0, UINT8_MAX);
                break;

            case 'd':
                gSerialOpts.devName = mustStrdup(optarg);
                break;

            case 'b':
                parseLong(optarg, &gSerialOpts.baudRate,
                          sizeof(gSerialOpts.baudRate), 0, UINT32_MAX);
                break;
        }
    }

    if (gPlayerOpts.sequenceFilePath == NULL) {
        printUsage();

        return 1;
    }

    argc -= optind;
    argv += optind;

    audioInit(&argc, argv);

    channelMapInit(gPlayerOpts.channelMapFilePath);

    serialInit(gSerialOpts);
    playerInit(gPlayerOpts);

    serialExit();

    channelMapFree();

    audioExit();

    serialOptsFree(&gSerialOpts);
    playerOptsFree(&gPlayerOpts);

    return 0;
}
