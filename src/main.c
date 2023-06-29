#include <getopt.h>
#include <stdio.h>

#include <AL/alut.h>
#include <libserialport.h>

#ifdef ENABLE_ZSTD
#include <zstd.h>
#endif

#include <tinyfseq.h>

#include "audio.h"
#include "cmap.h"
#include "mem.h"
#include "parse.h"
#include "player.h"
#include "serial.h"

static void printUsage(void) {
    printf("Usage: fplayer -f=FILE -c=FILE [options] ...\n");

    printf("\nOptions:\n");

    printf("\n[Playback]\n");
    printf("\t-f <file>\t\tFSEQ v2 sequence file path (required)\n");
    printf("\t-c <file>\t\tNetwork channel map file path (required)\n");
    printf("\t-d <device name|stdout>\tDevice name for serial port "
           "connection\n");
    printf("\t-b <baud rate>\t\tSerial port baud rate (defaults to 19200)\n");
    printf("\t-p\t\t\tPrecompute fades for smoother playback and reduced "
           "bandwidth (experimental)\n");

    printf("\n[Controls]\n");
    printf("\t-a <file>\t\tOverride audio with specified filepath\n");
    printf("\t-r <frame ms>\t\tOverride playback frame rate interval (in "
           "milliseconds)\n");
    printf("\t-w <seconds>\t\tPlayback start delay to allow connection "
           "setup\n");

    printf("\n[CLI]\n");
    printf("\t-t <file>\t\tTest load channel map and exit\n");
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

#define cReturnOK  0 /* early return, 0 */
#define cReturnErr 1 /* early return, 1 */
#define cContinue  2 /* no return */

static int testConfigurations(const char *filepath) {
    bool parseErrs = false;

    channelMapInit(filepath, &parseErrs);

    channelMapFree();

    return parseErrs ? cReturnErr : cReturnOK;
}

static int parseOpts(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, ":t:hvf:c:a:r:w:pd:b:")) != -1) {
        switch (c) {
            case 't':
                return testConfigurations(optarg);

            case 'h':
                printUsage();
                return cReturnOK;

            case 'v':
                printVersions();
                return cReturnOK;

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

            case 'p':
                gPlayerOpts.precomputeFades = true;
                break;

            case 'd':
                gSerialOpts.devName = mustStrdup(optarg);
                break;

            case 'b':
                parseLong(optarg, &gSerialOpts.baudRate,
                          sizeof(gSerialOpts.baudRate), 0, UINT32_MAX);
                break;

            case ':':
                fprintf(stderr, "option is missing argument: %c\n", optopt);
                return cReturnErr;

            case '?':
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                return cReturnErr;
        }
    }

    if (gPlayerOpts.sequenceFilePath == NULL ||
        gPlayerOpts.channelMapFilePath == NULL) {
        printUsage();

        return cReturnErr;
    }

    return cContinue;
}

int main(int argc, char **argv) {
    switch (parseOpts(argc, argv)) {
        case cReturnOK:
            return 0;
        case cReturnErr:
            return 1;
    }

    argc -= optind;
    argv += optind;

    // load required app context configs
    bool cmapParseErrs = false;

    channelMapInit(gPlayerOpts.channelMapFilePath, &cmapParseErrs);

    if (cmapParseErrs)
        fprintf(stderr, "warning: channel map parsing errors detected!\n");

    // initialize core subsystems
    audioInit(&argc, argv);
    serialInit(gSerialOpts);

    // start the player as configured, this will start playback automatically
    playerRun(gPlayerOpts);

    // teardown in reverse order
    serialExit();
    audioExit();

    channelMapFree();

    serialOptsFree(&gSerialOpts);
    playerOptsFree(&gPlayerOpts);

    return 0;
}
