#include <getopt.h>
#include <stdio.h>

#ifdef ENABLE_OPENAL
    #include <AL/alut.h>
#endif

#ifdef ENABLE_ZSTD
    #include <zstd.h>
#endif

#include <libserialport.h>

#include "lightorama/version.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define TINYFSEQ_IMPLEMENTATION
#include "tinyfseq.h"

#include "audio.h"
#include "cmap.h"
#include "player.h"
#include "serial.h"
#include "std/err.h"

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

    printf("liblightorama %s\n", LIBLIGHTORAMA_VERSION_STRING);
    printf("libserialport %s\n", SP_PACKAGE_VERSION_STRING);
    printf("libtinyfseq %s\n", TINYFSEQ_VERSION);

#ifdef ENABLE_ZSTD
    printf("zstd %s\n", ZSTD_versionString());
#else
    printf("zstd disabled\n");
#endif
}

static sds gSequenceFilePath;
static sds gAudioOverrideFilePath;

static sds gChannelMapFilePath;

static PlayerOpts gPlayerOpts;

static sds gSerialDevName;
static int gSerialBaudRate = 19200;

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
    while ((c = getopt(argc, argv, ":t:ilhvf:c:a:r:w:pd:b:")) != -1) {
        switch (c) {
            case 't':
                channelMapInit(optarg);
                channelMapFree();
                return true;

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
                gSequenceFilePath = sdsnew(optarg);
                break;

            case 'c':
                gChannelMapFilePath = sdsnew(optarg);
                break;

            case 'a':
                gAudioOverrideFilePath = sdsnew(optarg);
                break;

            case 'r':
                gPlayerOpts.frameStepTimeOverrideMs =
                        (uint8_t) checked_strtol(optarg, 1, UINT8_MAX);
                break;

            case 'w':
                gPlayerOpts.connectionWaitS =
                        (uint8_t) checked_strtol(optarg, 0, UINT8_MAX);
                break;

            case 'p':
                gPlayerOpts.precomputeFades = true;
                break;

            case 'd':
                gSerialDevName = sdsnew(optarg);
                break;

            case 'b':
                gSerialBaudRate = (int) checked_strtol(optarg, 0, INT_MAX);
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

    if (gSequenceFilePath == NULL || gChannelMapFilePath == NULL) {
        printUsage();

        *ec = EXIT_FAILURE;
        return true;
    }

    return false;
}

static void freeArgs(void) {
    sdsfree(gSequenceFilePath);
    sdsfree(gAudioOverrideFilePath);
    sdsfree(gChannelMapFilePath);
    sdsfree(gSerialDevName);
}

int main(const int argc, char **const argv) {
    int ec = EXIT_SUCCESS;
    if (parseOpts(argc, argv, &ec)) return ec;

    // load required app context configs
    channelMapInit(gChannelMapFilePath);

    // initialize core subsystems
    audioInit();
    serialInit(gSerialDevName, gSerialBaudRate);

    // start the player as configured, this will start playback automatically
    playerRun(gSequenceFilePath, gAudioOverrideFilePath, gPlayerOpts);

    // teardown in reverse order
    serialExit();
    audioExit();

    channelMapFree();

    freeArgs();

    return 0;
}
