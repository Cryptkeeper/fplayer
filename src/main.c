#include <getopt.h>
#include <stdio.h>

#ifdef ENABLE_OPENAL
#include <AL/alut.h>
#endif

#include <libserialport.h>
#include <lightorama/version.h>

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
           "\t-i\t\t\tPrint audio playback errors instead of exiting\n"
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

    printf("liblightorama %s\n", LIBLIGHTORAMA_VERSION_STRING);
    printf("libserialport %s\n", SP_PACKAGE_VERSION_STRING);
    printf("libtinyfseq %s\n", TINYFSEQ_VERSION);

#ifdef ENABLE_ZSTD
    printf("zstd %s\n", ZSTD_versionString());
#else
    printf("zstd disabled\n");
#endif

#ifdef ENABLE_PTHREAD
    printf("pthread\n");
#else
    printf("pthread disabled\n");
#endif
}

static PlayerOpts gPlayerOpts;

static SerialOpts gSerialOpts = {
        .baudRate = 19200,
};

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

static bool parseOpts(const int argc, char **const argv, int *const ec) {
    int c;
    while ((c = getopt(argc, argv, ":t:ilhvf:c:a:r:w:pid:b:")) != -1) {
        switch (c) {
            case 't':
                testConfigurations(optarg);
                return true;

            case 'i':
                gAudioIgnoreErrors = true;
                break;

            case 'l':
                printSerialEnumPorts();
                return true;

            case 'h':
                printUsage();
                return true;

            case 'v':
                printVersions();
                return true;

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
                *ec = EXIT_FAILURE;
                return true;

            case '?':
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                *ec = EXIT_FAILURE;
                return true;
        }
    }

    if (gPlayerOpts.sequenceFilePath == NULL ||
        gPlayerOpts.channelMapFilePath == NULL) {
        printUsage();

        *ec = EXIT_FAILURE;
        return true;
    }

    return false;
}

int main(int argc, char **argv) {
    int ec = EXIT_SUCCESS;
    if (parseOpts(argc, argv, &ec)) return ec;

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
