#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "player.h"

static void print_usage(void) {
    printf("Usage: fplayer -f=FILE [options] ...\n");
    printf("Options:\n");
    printf("\t-f=FILE\t\tFSEQ v2 sequence file path (required)\n");
    printf("\t-a=FILE\t\tAudio filepath for playback (defaults to sequence "
           "preset)\n");
    printf("\t-h\t\tPrint this message and exit\n");
}

static PlayerOpts gPlayerOpts;

int main(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, ":hf:a:")) != -1) {
        switch (c) {
            case 'h':
                print_usage();
                return 0;
            case ':':
                fprintf(stderr, "argument is missing option: %c\n", optopt);
                return 1;
            case '?':
            default:
                fprintf(stderr, "unknown argument: %c\n", optopt);
                return 1;
            case 'f':
                gPlayerOpts.sequenceFilePath = strdup(optarg);
                assert(gPlayerOpts.sequenceFilePath != NULL);
                break;
            case 'a':
                gPlayerOpts.audioOverrideFilePath = strdup(optarg);
                assert(gPlayerOpts.audioOverrideFilePath != NULL);
                break;
        }
    }

    if (gPlayerOpts.sequenceFilePath == NULL) {
        print_usage();

        return 1;
    }

    argc -= optind;
    argv += optind;

    audioInit(&argc, argv);

    const bool err = playerInit(gPlayerOpts);

    return err ? 1 : 0;
}
