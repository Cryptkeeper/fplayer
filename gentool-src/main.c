#undef NDEBUG
#include <assert.h>

#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include "lightorama/intensity.h"
#include "sds.h"
#include "std/err.h"
#include "std/fseq.h"
#include "std/mem.h"
#include "std/parse.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#ifdef ENABLE_ZSTD
    #include <zstd.h>
#endif

// TODO: copy from mftool-src/main.c, merge into libtinyfseq?
#define VAR_HEADER_SIZE 4

// TODO: copy from mftool-src/main.c, merge into libtinyfseq?
static struct tf_file_header_t fseqResize(const struct tf_file_header_t header,
                                          const fseq_var_t *const vars) {
    unsigned int varDataSize = 0;

    for (size_t i = 0; i < arrlenu(vars); i++)
        varDataSize += sdslen(vars[i].string) + VAR_HEADER_SIZE + 1;

    // round to nearest product of 4 for 32-bit alignment
    const size_t rem = varDataSize % 4;
    if (rem != 0) varDataSize += 4 - rem;

    // ensure the value can be safely downcasted to what the file format expects
    assert(varDataSize <= UINT16_MAX);

    const uint16_t firstVarOffset = 32 + header.compressionBlockCount * 8 +
                                    header.channelRangeCount * 6;

    struct tf_file_header_t resized = header;

    resized.variableDataOffset = firstVarOffset;
    resized.channelDataOffset = firstVarOffset + varDataSize;

    return resized;
}

// TODO: copy from mftool-src/main.c, merge into libtinyfseq?
#define fwrite_auto(v, f) fwrite(&v, sizeof(v), 1, f)

// TODO: copy from mftool-src/main.c, merge into libtinyfseq?
static void fseqWriteHeader(FILE *const dst,
                            const struct tf_file_header_t header) {
    const uint8_t magic[4] = {'P', 'S', 'E', 'Q'};

    fwrite_auto(magic, dst);

    fwrite_auto(header.channelDataOffset, dst);

    fputc(0, dst);// minor version
    fputc(2, dst);// major version

    fwrite_auto(header.variableDataOffset, dst);

    fwrite_auto(header.channelCount, dst);
    fwrite_auto(header.frameCount, dst);

    fwrite_auto(header.frameStepTimeMillis, dst);

    fputc(0, dst);// reserved flags

    const uint8_t compression = (uint8_t) header.compressionType;

    fwrite_auto(compression, dst);

    fwrite_auto(header.compressionBlockCount, dst);
    fwrite_auto(header.channelRangeCount, dst);

    fputc(0, dst);// reserved empty

    fwrite_auto(header.sequenceUid, dst);
}

static void
fseqWriteCompressionBlocks(FILE *const dst,
                           const struct tf_compression_block_t *const blocks) {
    fseek(dst, 32, SEEK_SET);

    for (size_t i = 0; i < arrlenu(blocks); i++) {
        const struct tf_compression_block_t block = blocks[i];

        fwrite_auto(block.firstFrameId, dst);
        fwrite_auto(block.size, dst);
    }
}

// TODO: copy from mftool-src/main.c, merge into libtinyfseq?
static void fseqWriteVars(FILE *const dst, const fseq_var_t *const vars) {
    for (size_t i = 0; i < arrlenu(vars); i++) {
        const fseq_var_t var = vars[i];

        const uint16_t size = sdslen(var.string) + VAR_HEADER_SIZE + 1;

        fwrite_auto(size, dst);

        fwrite_auto(var.idh, dst);
        fwrite_auto(var.idl, dst);

        fwrite(var.string, sdslen(var.string), 1, dst);

        fputc('\0', dst);
    }
}

static fseq_var_t *fseqCreateProgramVars(void) {
    fseq_var_t *vars = NULL;

    const fseq_var_t sequenceProducer = {
            .idh = 's',
            .idl = 'p',
            .string = sdsnew("fplayer/gentool"),
    };

    arrpush(vars, sequenceProducer);

    const fseq_var_t generationMode = {
            .idh = 'g',
            .idl = 'm',
            .string = sdsnew("intensity_oscillator_ramp_vendor"),
    };

    arrpush(vars, generationMode);

    return vars;
}

static inline uint8_t intensityOscillatorRampVendorNext(void) {
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

    return (uint8_t) LorIntensityCurveVendor(f);
}

#ifdef ENABLE_ZSTD
static void *compressZstd(const char *const src,
                          const size_t srcSize,
                          size_t *const dstSize) {
    const size_t dstCapacity = ZSTD_compressBound(srcSize);
    void *const dst = mustMalloc(dstCapacity);

    const int compressionLevel = 1;

    const size_t dstCompressed =
            ZSTD_compress(dst, dstCapacity, src, srcSize, compressionLevel);

    if (ZSTD_isError(dstCompressed))
        fatalf(E_FATAL, "error compressing zstd stream: %s (%zu)\n",
               ZSTD_getErrorName(dstCompressed), dstCompressed);

    *dstSize = dstCompressed;

    return dst;
}
#endif

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
            fatalf(E_FILE_IO, "error writing frame %d\n", frame);

        if (frame > 0 && frame % fps == 0)
            printf("wrote frame bundle %d\n", frame / fps);
    }

    free(channelData);
}

static struct tf_compression_block_t *
generateChannelData(FILE *const dst,
                    const uint8_t fps,
                    const uint32_t frameCount,
                    const uint32_t channelCount,
                    const uint8_t compressionBlockCount) {
    struct tf_compression_block_t *blocks = NULL;

    if (compressionBlockCount > 0) {
#ifdef ENABLE_ZSTD
        // divide frames evenly amongst the block count, adding the total reminder
        // to each frame as a cheap way to ensure all frames are accounted for
        const unsigned int framesPerBlock =
                (frameCount / compressionBlockCount) +
                (frameCount % compressionBlockCount);

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
                    compressZstd((const void *) channelData, channelDataSize,
                                 &compressedDataSize);

            // write compressed data to the file
            fwrite(compressedData, compressedDataSize, 1, dst);

            free(compressedData);

            // append a new compression block entry to track the offsets
            const struct tf_compression_block_t newBlock = {
                    .firstFrameId = firstFrameId,
                    .size = compressedDataSize,
            };

            arrpush(blocks, newBlock);

            printf("wrote compressed frame bundle %d (%zu bytes)\n", idx,
                   compressedDataSize);
        }

        // free the original channel data memory
        free(channelData);
#else
        fatalf("zstd compression is not enabled\n");
#endif
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
    sds outputPath = NULL;
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
                outputPath = sdsnew(optarg);
                break;

            case 'f':
                // minimum 4 FPS = 250ms sleep time (stored in uint8_t, <= 255)
                // maximum 1000 FPS = 1ms sleep time
                fps = (uint16_t) parseLong(optarg, 4, 1000);
                break;

            case 'c':
                channelCount = (uint32_t) parseLong(optarg, 1, UINT32_MAX);
                break;

            case 'd':
                frameCount = (uint32_t) parseLong(optarg, 1, UINT32_MAX);
                break;

            case 'b':
                compressionBlockCount =
                        (uint8_t) parseLong(optarg, 1, UINT8_MAX);
                break;

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
    if (outputPath == NULL) outputPath = sdsnew("generated.fseq");

    FILE *const f = fopen(outputPath, "wb");

    if (f == NULL)
        fatalf(E_FILE_IO, "error opening output filepath: %s\n", outputPath);

    printf("generating test sequence (%d frames @ %d FPS)\n", frameCount, fps);
    printf("using %d bytes per frame (%.2fkb total)\n", channelCount,
           (float) (channelCount * frameCount) / 1024.0f);

    // generate a customized valid header for playback
    struct tf_file_header_t initialHeader = {
            .channelCount = channelCount,
            .frameCount = frameCount,
            .frameStepTimeMillis = 1000 / fps,
            .compressionType = compressionBlockCount > 0 ? TF_COMPRESSION_ZSTD
                                                         : TF_COMPRESSION_NONE,
            .compressionBlockCount = compressionBlockCount,
    };

    fseq_var_t *const vars = fseqCreateProgramVars();

    const struct tf_file_header_t header = fseqResize(initialHeader, vars);

    fseqWriteHeader(f, header);

    // generate channel data
    // if uncompressed, this will return a NULL array pointer
    // if compressed, this will return an array of compression block metadata
    //  that need to be encoded directly past the initial header
    fseek(f, header.channelDataOffset, SEEK_SET);

    struct tf_compression_block_t *compressionBlocks = generateChannelData(
            f, fps, frameCount, channelCount, compressionBlockCount);

    assert(arrlenu(compressionBlocks) == compressionBlockCount);

    if (compressionBlockCount > 0) {
        fseqWriteCompressionBlocks(f, compressionBlocks);

        arrfree(compressionBlocks);
    }

    // encode variable data last
    // space is pre-allocated using `fseqResize`
    fseek(f, header.variableDataOffset, SEEK_SET);

    fseqWriteVars(f, vars);

    fseqVarsFree(vars);

    // done writing file
    fclose(f);

    sdsfree(outputPath);

    return 0;
}
