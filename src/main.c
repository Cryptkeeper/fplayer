#include <getopt.h>
#include <stdio.h>

#include <AL/alut.h>
#include <zstd.h>

#include <libserialport.h>

#include "lorproto/version.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define TINYFSEQ_IMPLEMENTATION
#include "tinyfseq.h"

#include "audio.h"
#include "cmap.h"
#include "player.h"
#include "serial.h"
#include "std/err.h"
#include "std2/fc.h"

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
    printf("ALUT %d.%d\n", alutGetMajorVersion(), alutGetMinorVersion());
    printf("OpenAL %s\n", alGetString(AL_VERSION));
    printf("liblorproto %s\n", LIBLORPROTO_VERSION_STRING);
    printf("libserialport %s\n", SP_PACKAGE_VERSION_STRING);
    printf("libtinyfseq %s\n", TINYFSEQ_VERSION);
    printf("zstd %s\n", ZSTD_versionString());
}

static char* gSequenceFilePath;
static char* gAudioOverrideFilePath;

static char* gChannelMapFilePath;

static PlayerOpts gPlayerOpts;

static char* gSerialDevName;
static int gSerialBaudRate = 19200;

static void printSerialEnumPorts(void) {
    char** ports = Serial_getPorts();

    for (int i = 0; i < arrlen(ports); i++) {
        printf("%s\n", ports[i]);

        free(ports[i]);
    }

    arrfree(ports);
}

static bool parseOpts(const int argc, char** const argv, int* const ec) {
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
                gSequenceFilePath = mustStrdup(optarg);
                break;
            case 'c':
                gChannelMapFilePath = mustStrdup(optarg);
                break;
            case 'a':
                gAudioOverrideFilePath = mustStrdup(optarg);
                break;
            case 'r':
                if (strtolb(optarg, 1, UINT8_MAX,
                            &gPlayerOpts.frameStepTimeOverrideMs,
                            sizeof(gPlayerOpts.frameStepTimeOverrideMs)))
                    break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                *ec = EXIT_FAILURE;
                return true;
            case 'w':
                if (strtolb(optarg, 0, UINT8_MAX, &gPlayerOpts.connectionWaitS,
                            sizeof(gPlayerOpts.connectionWaitS)))
                    break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                *ec = EXIT_FAILURE;
                return true;
            case 'p':
                gPlayerOpts.precomputeFades = true;
                break;
            case 'd':
                gSerialDevName = mustStrdup(optarg);
                break;
            case 'b':
                if (strtolb(optarg, 0, INT_MAX, &gSerialBaudRate,
                            sizeof(gSerialBaudRate)))
                    break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                *ec = EXIT_FAILURE;
                return true;
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
    free(gSequenceFilePath);
    free(gAudioOverrideFilePath);
    free(gChannelMapFilePath);
    free(gSerialDevName);
}

int main(const int argc, char** const argv) {
    int ec = EXIT_SUCCESS;
    if (parseOpts(argc, argv, &ec)) return ec;

    // load required app context configs
    channelMapInit(gChannelMapFilePath);

    // open sequence file and init controller handler
    struct FC* fc = FC_open(gSequenceFilePath);
    if (fc == NULL) {
        fatalf(E_FIO, "failed to open sequence file `%s`\n", gSequenceFilePath);
    }

    // initialize core subsystems
    Serial_init(gSerialDevName, gSerialBaudRate);

    // start the player as configured, this will start playback automatically
    playerRun(fc, gAudioOverrideFilePath, gPlayerOpts);

    FC_close(fc);

    // teardown in reverse order
    Serial_close();
    audioExit();

    channelMapFree();

    freeArgs();

    return 0;
}
