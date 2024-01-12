#undef NDEBUG
#include <assert.h>

#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include <zstd.h>

#include "lorproto/intensity.h"
#include "std/err.h"
#include "std/fseq.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

static fseq_var_t *fseqCreateProgramVars(void) {
    fseq_var_t *vars = NULL;

    const fseq_var_t sequenceProducer = {
            .idh = 's',
            .idl = 'p',
            .string = "fplayer/gentool",
    };

    arrpush(vars, sequenceProducer);

    const fseq_var_t generationMode = {
            .idh = 'g',
            .idl = 'm',
            .string = "intensity_oscillator_ramp_vendor",
    };

    arrpush(vars, generationMode);

    return vars;
}

static uint8_t intensityOscillatorRampVendorNext(void) {
    static int percentage = 0;
    static int mod = 1;

    percentage += mod;

    // flip direction when reaching min/max threshold
    if (mod > 0) {
        mod = percentage >= 100 ? -1 : mod;
    } else if (mod < 0) {
        mod = percentage <= 0 ? 1 : mod;
    }

    const float f = (float) percentage / 100.0f;

    return LorIntensityCurveVendor(f);
}

static void *compressZstd(const char *const src,
                          const size_t srcSize,
                          size_t *const dstSize) {
    const size_t dstCapacity = ZSTD_compressBound(srcSize);
    void *const dst = mustMalloc(dstCapacity);

    const int compressionLevel = 1;

    const size_t dstCompressed =
            ZSTD_compress(dst, dstCapacity, src, srcSize, compressionLevel);

    if (ZSTD_isError(dstCompressed))
        fatalf(E_APP, "error compressing zstd stream: %s (%zu)\n",
               ZSTD_getErrorName(dstCompressed), dstCompressed);

    *dstSize = dstCompressed;

    return dst;
}

static void generateChannelDataUncompressed(FILE *const dst,
                                            const uint8_t fps,
                                            const uint32_t frameCount,
                                            const uint32_t channelCount) {
    // generate each individual frame
    // all channels are set to the same value for each frame via a memory block
    uint8_t *const channelData = mustMalloc(channelCount);

    for (uint32_t frame = 0; frame <= frameCount; frame++) {
        memset(channelData, intensityOscillatorRampVendorNext(), channelCount);

        if (fwrite(channelData, channelCount, 1, dst) != 1)
            fatalf(E_FIO, "error writing frame %d\n", frame);

        if (frame > 0 && frame % fps == 0)
            printf("wrote frame bundle %d\n", frame / fps);
    }

    free(channelData);
}

static TFCompressionBlock *
generateChannelData(FILE *const dst,
                    const uint8_t fps,
                    const uint32_t frameCount,
                    const uint32_t channelCount,
                    const uint8_t compressionBlockCount) {
    TFCompressionBlock *blocks = NULL;

    if (compressionBlockCount > 0) {
        // divide frames evenly amongst the block count, adding the total reminder
        // to each frame as a cheap way to ensure all frames are accounted for
        const unsigned int framesPerBlock = frameCount / compressionBlockCount +
                                            frameCount % compressionBlockCount;

        // allocate a single block of channel data memory that will be compressed
        const size_t channelDataSize = framesPerBlock * channelCount;
        uint8_t *const channelData = mustMalloc(channelDataSize);

        // generate each frame, all channels are set to the same value per frame
        uint32_t remainingFrameCount = frameCount;

        for (int idx = 0; idx < compressionBlockCount; idx++) {
            const uint32_t firstFrameId = idx * framesPerBlock;

            for (uint32_t offset = 0;
                 offset < framesPerBlock && remainingFrameCount > 0;
                 offset++, remainingFrameCount--) {
                const uint8_t frameData = intensityOscillatorRampVendorNext();

                memset(&channelData[offset * channelCount], frameData,
                       channelCount);
            }

            // compress the entire block of channel data
            size_t compressedDataSize;
            void *const compressedData =
                    compressZstd((const char *) channelData, channelDataSize,
                                 &compressedDataSize);

            // write compressed data to the file
            fwrite(compressedData, compressedDataSize, 1, dst);

            free(compressedData);

            // append a new compression block entry to track the offsets
            const TFCompressionBlock newBlock = {
                    .firstFrameId = firstFrameId,
                    .size = compressedDataSize,
            };

            arrpush(blocks, newBlock);

            printf("wrote compressed frame bundle %d (%zu bytes)\n", idx,
                   compressedDataSize);
        }

        // free the original channel data memory
        free(channelData);
    } else {
        generateChannelDataUncompressed(dst, fps, frameCount, channelCount);
    }

    return blocks;
}

static void printUsage(void) {
    printf("Usage: gentool [options]\n\n"

           "Options:\n\n"

           "\t-o <file>\t\tOutput file path (default: generated.fseq)\n"
           "\t-f <fps>\t\tNumber of frames per second (default: 25)\n"
           "\t-c <channels>\t\tNumber of channels to use (default: 16)\n"
           "\t-d <frames>\t\tTotal number of frames (default: 250)\n"
           "\t-b <count>\t\tzstd compression block count (default: 2)\n");
}

int main(const int argc, char **const argv) {
    char *outputPath = NULL;
    uint16_t fps = 25;
    uint32_t channelCount = 16;
    uint32_t frameCount = 250;
    uint8_t compressionBlockCount = 2;

    int c;
    while ((c = getopt(argc, argv, ":o:f:c:d:hb:")) != -1) {
        switch (c) {
            case 'h':
                printUsage();
                return 0;

            case 'o':
                outputPath = mustStrdup(optarg);
                break;

            case 'f':
                // minimum 4 FPS = 250ms sleep time (stored in uint8_t, <= 255)
                // maximum 1000 FPS = 1ms sleep time
                if (strtolb(optarg, 4, 1000, &fps)) break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                return 1;

            case 'c':
                if (strtolb(optarg, 1, UINT32_MAX, &channelCount)) break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                return 1;

            case 'd':
                if (strtolb(optarg, 1, UINT32_MAX, &frameCount)) break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                return 1;

            case 'b':
                if (strtolb(optarg, 0, UINT8_MAX, &compressionBlockCount))
                    break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                return 1;

            case ':':
                fprintf(stderr, "option is missing argument: %c\n", optopt);
                return 1;

            case '?':
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                return 1;
        }
    }

    // avoid allocating a default value until potential early exit checks are done
    if (outputPath == NULL) outputPath = mustStrdup("generated.fseq");

    FILE *const f = fopen(outputPath, "wb");

    if (f == NULL)
        fatalf(E_FIO, "error opening output filepath: %s\n", outputPath);

    printf("generating test sequence (%d frames @ %d FPS)\n", frameCount, fps);
    printf("using %d bytes per frame (%.2fkb total)\n", channelCount,
           (float) (channelCount * frameCount) / 1024.0f);

    // generate a customized valid header for playback
    TFHeader header = {
            .minorVersion = 0,
            .majorVersion = 2,
            .channelCount = channelCount,
            .frameCount = frameCount,
            .frameStepTimeMillis = 1000 / fps,
            .compressionType = compressionBlockCount > 0 ? TF_COMPRESSION_ZSTD
                                                         : TF_COMPRESSION_NONE,
            .compressionBlockCount = compressionBlockCount,
    };

    fseq_var_t *vars = fseqCreateProgramVars();

    fseqAlignOffsets(&header, vars);

    fseqWriteHeader(f, header);

    // generate channel data
    // if uncompressed, this will return a NULL array pointer
    // if compressed, this will return an array of compression block metadata
    //  that need to be encoded directly past the initial header
    fseek(f, header.channelDataOffset, SEEK_SET);

    TFCompressionBlock *compressionBlocks = generateChannelData(
            f, fps, frameCount, channelCount, compressionBlockCount);

    assert(arrlenu(compressionBlocks) == compressionBlockCount);

    if (compressionBlockCount > 0) {
        fseqWriteCompressionBlocks(f, compressionBlocks);

        arrfree(compressionBlocks);
    }

    fseqWriteVars(f, header, vars);

    // done writing file
    arrfree(vars);

    fclose(f);

    free(outputPath);

    return 0;
}
