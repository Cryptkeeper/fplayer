#include "seq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyfseq.h"

#include "std/err.h"

TFHeader curSequence;

void Seq_initHeader(FCHandle fc) {
    uint8_t b[32];
    FC_read(fc, 0, sizeof(b), b);

    TFError err;
    if ((err = TFHeader_read(b, sizeof(b), &curSequence, NULL)))
        fatalf(E_APP, "error parsing fseq: %s\n", TFError_string(err));
}

static char *Seq_readVar(uint8_t **readIdx,
                         const int remaining,
                         TFVarHeader *varHeader,
                         void *varData,
                         const uint16_t varDataSize) {
    TFError err;
    if ((err = TFVarHeader_read(*readIdx, remaining, varHeader, varData,
                                varDataSize, readIdx)))
        fatalf(E_APP, "error parsing fseq var: %s\n", TFError_string(err));

    const int varHeaderSize = 4; /* size of binary var header */

    const size_t varLen = varHeader->size - varHeaderSize;
    char *const varString = mustMalloc(varLen);

    // never copy final character since it will be NULL terminated, this may
    // accidentally truncate a non-NULL terminated string
    memcpy(varString, varData, varLen - 1);

    // ensures string is NULL terminated
    varString[varLen - 1] = '\0';

    return varString;
}

char *Seq_getMediaFile(FCHandle fc) {
    const uint16_t varTableSize =
            curSequence.channelDataOffset - curSequence.variableDataOffset;

    if (varTableSize == 0) return NULL;

    uint8_t *const varTable = mustMalloc(varTableSize);

    FC_read(fc, curSequence.variableDataOffset, varTableSize, varTable);

    // re-use varTableSize since the size of any individual variable cannot
    // exceed the total size of the variable data table
    char *varData = mustMalloc(varTableSize);

    char *mf = NULL; /* matched media file path variable value */

    TFVarHeader hdr;
    uint8_t *head = &varTable[0];

    for (int remaining = varTableSize; remaining > 4 /* var header size */;
         remaining -= hdr.size) {
        char *str = Seq_readVar(&head, remaining, &hdr, varData, varTableSize);

        printf("var '%c%c': %s\n", hdr.id[0], hdr.id[1], str);

        // mf = Media File variable, contains audio filepath
        // caller is responsible for freeing `audioFilePath` copy return
        if (hdr.id[0] == 'm' && hdr.id[1] == 'f' && mf == NULL) {
            mf = str;
            continue;
        }

        free(str);
    }

    free(varData);
    free(varTable);

    return mf;
}

uint32_t Seq_readFrames(FCHandle fc,
                        const struct seq_read_args_t args,
                        uint8_t *const b) {
    uint32_t frameCount = args.frameCount;

    // ensure the requested frame count does not exceed the total frame count
    if (args.startFrame + args.frameCount > curSequence.frameCount)
        frameCount = curSequence.frameCount - args.startFrame;

    const uint32_t pos =
            curSequence.channelDataOffset + args.startFrame * args.frameSize;

    const uint32_t framesRead =
            FC_readto(fc, pos, args.frameSize, frameCount, b);

    return framesRead;
}
