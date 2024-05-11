#include <getopt.h>
#include <stdio.h>

#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#include "audio.h"
#include "crmap.h"
#include "player.h"
#include "serial.h"
#include "std2/fc.h"
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

static bool parseOpts(const int argc, char** const argv, int* const ec) {
    int c;
    while ((c = getopt(argc, argv, ":t:ilhf:c:a:w:d:b:")) != -1) {
        switch (c) {
            case 't': {
                struct cr_s* cmap = NULL;
                int err;
                if ((err = CR_parse(optarg, &cmap))) {
                    fprintf(stderr,
                            "failed to parse channel map file `%s`: %d\n",
                            optarg, err);
                    *ec = EXIT_FAILURE;
                }
                CR_free(cmap);
                return true;
            }
            case 'l':
                printSerialEnumPorts();
                return true;
            case 'h':
                printUsage();
                return true;
            case 'f':
                if ((gSequenceFilePath = strdup(optarg)) == NULL) {
                    *ec = EXIT_FAILURE;
                    return true;
                }
                break;
            case 'c':
                if ((gChannelMapFilePath = strdup(optarg)) == NULL) {
                    *ec = EXIT_FAILURE;
                    return true;
                }
                break;
            case 'a':
                if ((gAudioOverrideFilePath = strdup(optarg)) == NULL) {
                    *ec = EXIT_FAILURE;
                    return true;
                }
                break;
            case 'w':
                if (!strtolb(optarg, 0, UINT8_MAX, &gWaitSeconds,
                             sizeof(gWaitSeconds)))
                    break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                *ec = EXIT_FAILURE;
                return true;
            case 'd':
                if ((gSerialDevName = strdup(optarg)) == NULL) {
                    *ec = EXIT_FAILURE;
                    return true;
                }
                break;
            case 'b':
                if (!strtolb(optarg, 0, INT32_MAX, &gSerialBaudRate,
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

    int err;

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
