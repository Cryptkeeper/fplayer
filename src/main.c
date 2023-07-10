#include <getopt.h>
#include <stdio.h>

#ifdef ENABLE_OPENAL
#include <AL/alut.h>
#endif

#include <libserialport.h>

#ifdef ENABLE_ZSTD
#include <zstd.h>
#endif

#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#include "audio.h"
#include "cmap.h"
#include "player.h"
#include "serial.h"
#include "std/parse.h"

static void printUsage(void) {
    printf("Usage: fplayer -f=FILE -c=FILE [options] ...\n\n"

           "Options:\n\n"

           "[Playback]\n"
           "\t-f <file>\t\tFSEQ v2 sequence file path (required)\n"
           "\t-c <file>\t\tNetwork channel map file path (required)\n"
           "\t-d <device name|stdout>\tDevice name for serial port "
           "connection\n"
           "\t-b <baud rate>\t\tSerial port baud rate (defaults to 19200)\n"
           "\t-p\t\t\tPrecompute fades for smoother playback and reduced "
           "bandwidth (experimental)\n\n"

           "[Controls]\n"
           "\t-a <file>\t\tOverride audio with specified filepath\n"
           "\t-r <frame ms>\t\tOverride playback frame rate interval (in "
           "milliseconds)\n"
           "\t-w <seconds>\t\tPlayback start delay to allow connection "
           "setup\n\n"

           "[CLI]\n"
           "\t-t <file>\t\tTest load channel map and exit\n"
           "\t-l\t\t\tPrint available serial port list and exit\n"
           "\t-h\t\t\tPrint this message and exit\n"
           "\t-v\t\t\tPrint library versions and exit\n");
}

static void printVersions(void) {
#ifdef ENABLE_OPENAL
    printf("ALUT %d.%d\n", alutGetMajorVersion(), alutGetMinorVersion());
    printf("OpenAL %s\n", alGetString(AL_VERSION));
#else
    printf("ALUT disabled\n");
    printf("OpenAL disabled\n");
#endif

    printf("libtinyfseq %s\n", TINYFSEQ_VERSION);
    printf("libserialport %s\n", SP_PACKAGE_VERSION_STRING);

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

static void testConfigurations(const char *const filepath) {
    channelMapInit(filepath);

    channelMapFree();
}

static void printSerialEnumPorts(void) {
    sds *ports = serialEnumPorts();

    for (int i = 0; i < arrlen(ports); i++) {
        printf("%s\n", ports[i]);

        sdsfree(ports[i]);
    }

    arrfree(ports);
}

static int parseOpts(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, ":t:lhvf:c:a:r:w:pd:b:")) != -1) {
        switch (c) {
            case 't':
                testConfigurations(optarg);
                return cReturnOK;

            case 'l':
                printSerialEnumPorts();
                return cReturnOK;

            case 'h':
                printUsage();
                return cReturnOK;

            case 'v':
                printVersions();
                return cReturnOK;

            case 'f':
                gPlayerOpts.sequenceFilePath = sdsnew(optarg);
                break;

            case 'c':
                gPlayerOpts.channelMapFilePath = sdsnew(optarg);
                break;

            case 'a':
                gPlayerOpts.audioOverrideFilePath = sdsnew(optarg);
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
                gSerialOpts.devName = sdsnew(optarg);
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
    channelMapInit(gPlayerOpts.channelMapFilePath);

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
