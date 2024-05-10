#undef NDEBUG
#include <assert.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lorproto/intensity.h>
#include <tinyfseq.h>
#include <zstd.h>

#include "fseq/writer.h"
#include "std2/errcode.h"
#include "std2/fc.h"
#include "std2/string.h"

/// @brief Generates a variable wrapper struct for the given id and string
/// value. The size of the string is calculated using strlen, which requires a
/// null terminated string. The string is not copied, so the caller must ensure
/// the string remains valid for the lifetime of the variable.
/// @param id two character id for the variable
/// @param value variable data
/// @return variable wrapper struct
static struct fseq_var_s fseqWrapVarString(const char id[2],
                                           char* const value) {
    assert(id != NULL);
    assert(value != NULL);

    return (struct fseq_var_s){
            .id = {id[0], id[1]},
            .size = strlen(value) + 1,
            .value = value,
    };
}

/// @brief Creates a set of program-specific variables for the test sequence.
/// The variables are dynamically allocated and returned in the vars pointer and
/// their count in the count pointer. The caller is responsible for freeing the
/// returned memory.
/// @param vars pointer to store the variables
/// @param count pointer to store the number of variables
/// @return 0 on success, or a negative error code on failure
static int fseqCreateProgramVars(struct fseq_var_s** vars, int* count) {
    if ((*vars = calloc(2, sizeof(struct fseq_var_s))) == NULL)
        return -FP_ENOMEM;

    *vars[0] = fseqWrapVarString("sp", "fplayer/gentool");
    *vars[1] = fseqWrapVarString("gm", "intensity_oscillator_ramp_vendor");
    *count = 2;

    return FP_EOK;
}

/// @brief Returns a value between [0,100] which is converted to an intensity
/// value. The oscillation is then advanced for the next invocation of this
/// function.
/// @return intensity value
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

/// @brief Compresses the given source data using zstd. Decompressed data is
/// dynamically allocated and returned in the dst pointer and its size in the
/// dstSize pointer. The caller is responsible for freeing the returned memory.
/// @param src source data to compress
/// @param srcSize size of the source data
/// @param dst pointer to store the compressed data
/// @param dstSize pointer to store the size of the compressed data
/// @return 0 on success, or a negative error code on failure
static int compressZstd(const char* const src,
                        const size_t srcSize,
                        void** const dst,
                        size_t* const dstSize) {
    assert(src != NULL);
    assert(srcSize > 0);
    assert(dst != NULL);
    assert(dstSize != NULL);

    void* d = NULL;
    const size_t req = ZSTD_compressBound(srcSize);
    if ((d = malloc(req)) == NULL) return -FP_ENOMEM;

    int err = FP_EOK;
    size_t ds;
    if (ZSTD_isError((ds = ZSTD_compress(d, req, src, srcSize, 1))))
        err = -FP_EZSTD;
    if (!err) *dst = d, *dstSize = ds;
    return err;
}

/// @brief Generates uncompressed channel data using the given header specifics
/// and writes it to the file controller. The channel data is generated using a
/// simple intensity oscillator ramp vendor function.
/// @param fc file controller to write to
/// @param header header specifics for the file (frame count, channel count,
/// frame step time, channel data offset)
/// @return 0 on success, or a negative error code on failure
static int generateChannelDataUncompressed(struct FC* fc,
                                           const struct tf_header_t* header) {
    // generate each individual frame
    // all channels are set to the same value for each frame via a memory block
    uint8_t* const channelData = malloc(header->channelCount);
    if (channelData == NULL) return -FP_ENOMEM;

    int err = FP_EOK;

    const uint8_t fps = 1000 / header->frameStepTimeMillis;

    for (uint32_t frame = 0; frame <= header->frameCount; frame++) {
        memset(channelData, intensityOscillatorRampVendorNext(),
               header->channelCount);

        const uint32_t offset =
                header->channelDataOffset + frame * header->channelCount;

        if (FC_write(fc, offset, header->channelCount, channelData) !=
            header->channelCount) {
            err = -FP_ESYSCALL;
            goto ret;
        }

        if (frame > 0 && frame % fps == 0)
            printf("wrote frame bundle %d\n", frame / fps);
    }

ret:
    free(channelData);

    return err;
}

/// @brief Generates compressed channel data using the given header specifics
/// and writes it to the file controller. The channel data is generated using a
/// simple intensity oscillator ramp vendor function.
/// @param fc file controller to write to
/// @param header header specifics for the file
/// @param blocks pointer to store the generated compression block metadata, if
/// compression blocks are requested by the header
/// @return 0 on success, or a negative error code on failure
static int generateChannelData(struct FC* fc,
                               const struct tf_header_t* header,
                               struct tf_compression_block_t** blocks) {
    assert(fc != NULL);
    assert(header != NULL);
    assert(blocks != NULL);

    *blocks = NULL;

    if (header->compressionBlockCount == 0)
        return generateChannelDataUncompressed(fc, header);

    // divide frames evenly amongst the block count, adding the total reminder
    // to each frame as a cheap way to ensure all frames are accounted for
    const unsigned int framesPerBlock =
            header->frameCount / header->compressionBlockCount +
            header->frameCount % header->compressionBlockCount;

    int err = FP_EOK;

    // allocate a single block of channel data memory that will be compressed
    const size_t channelDataSize = framesPerBlock * header->channelCount;
    uint8_t* const channelData = malloc(channelDataSize);
    if (channelData == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    // allocate the array of compression block structs to return
    if ((*blocks = calloc(header->compressionBlockCount,
                          sizeof(struct tf_compression_block_t))) == NULL) {
        err = -FP_ENOMEM;
        goto ret;
    }

    // generate each frame, all channels are set to the same value per frame
    uint32_t remainingFrameCount = header->frameCount;
    uint32_t writePos = header->channelDataOffset;

    for (int idx = 0; idx < header->compressionBlockCount; idx++) {
        const uint32_t firstFrameId = idx * framesPerBlock;

        for (uint32_t offset = 0;
             offset < framesPerBlock && remainingFrameCount > 0;
             offset++, remainingFrameCount--) {
            const uint8_t frameData = intensityOscillatorRampVendorNext();

            memset(&channelData[offset * header->channelCount], frameData,
                   header->channelCount);
        }

        // compress the entire block of channel data
        size_t cmpSize;
        void* cmpData;
        if ((err = compressZstd((const char*) channelData, channelDataSize,
                                &cmpData, &cmpSize)))
            goto ret;

        // write compressed data to the file
        const uint32_t w = FC_write(fc, writePos, cmpSize, cmpData);
        free(cmpData);
        if (w != cmpSize) {
            err = -FP_ESYSCALL;
            goto ret;
        }
        writePos += cmpSize;

        // update the matching compression block struct
        *blocks[idx] = (struct tf_compression_block_t){
                .firstFrameId = firstFrameId,
                .size = cmpSize,
        };

        printf("wrote compressed frame bundle %d (%zu bytes)\n", idx, cmpSize);
    }

ret:
    free(channelData);
    if (err) free(*blocks), *blocks = NULL;

    return err;
}

/// @brief Prints the usage information for the tool.
static void printUsage(void) {
    printf("Usage: gentool [options]\n\n"

           "Options:\n\n"

           "\t-o <file>\t\tOutput file path (default: generated.fseq)\n"
           "\t-f <fps>\t\tNumber of frames per second (default: 25)\n"
           "\t-c <channels>\t\tNumber of channels to use (default: 16)\n"
           "\t-d <frames>\t\tTotal number of frames (default: 250)\n"
           "\t-b <count>\t\tzstd compression block count (default: 2)\n");
}

int main(const int argc, char** const argv) {
    char* outputPath = NULL;
    uint16_t fps = 25;

    struct tf_header_t header = {
            .channelCount = 16,
            .frameCount = 250,
            .compressionBlockCount = 2,
    };

    int c;
    while ((c = getopt(argc, argv, ":o:f:c:d:hb:")) != -1) {
        switch (c) {
            case 'h':
                printUsage();
                return 0;

            case 'o':
                if ((outputPath = strdup(optarg)) == NULL) return 1;
                break;

            case 'f':
                // minimum 4 FPS = 250ms sleep time (stored in uint8_t, <= 255)
                // maximum 1000 FPS = 1ms sleep time
                if (!strtolb(optarg, 4, 1000, &fps, sizeof(fps))) break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                return 1;

            case 'c':
                if (!strtolb(optarg, 1, UINT32_MAX, &header.channelCount,
                            sizeof(header.channelCount)))
                    break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                return 1;

            case 'd':
                if (!strtolb(optarg, 1, UINT32_MAX, &header.frameCount,
                            sizeof(header.frameCount)))
                    break;
                fprintf(stderr, "error parsing `%s` as an integer\n", optarg);
                return 1;

            case 'b':
                if (!strtolb(optarg, 0, UINT8_MAX, &header.compressionBlockCount,
                            sizeof(header.compressionBlockCount)))
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

    int err = FP_EOK;

    struct FC* fc = NULL;                         /* output file controller */
    struct fseq_var_s* vars = NULL;               /* variable array */
    int varCount = 0;                             /* variable count */
    struct tf_compression_block_t* blocks = NULL; /* generated metadata */

    // avoid allocating a default value until potential early exit checks are done
    if (outputPath == NULL)
        if ((outputPath = strdup("generated.fseq")) == NULL) {
            err = -FP_ENOMEM;
            goto exit;
        }

    // open the output file for writing
    if ((fc = FC_open(outputPath, FC_MODE_WRITE)) == NULL) {
        fprintf(stderr, "error opening file `%s`\n", outputPath);
        err = -FP_ESYSCALL;
        goto exit;
    }

    printf("generating test sequence (%d frames @ %d FPS), using %d bytes per "
           "frame (%.2fkb total)\n",
           header.frameCount, fps, header.channelCount,
           (float) (header.channelCount * header.frameCount) / 1024.0f);

    // configure several specific header fields for exporting
    header.minorVersion = 0;
    header.majorVersion = 2;
    header.frameStepTimeMillis = 1000 / fps;
    header.compressionType = header.compressionBlockCount > 0
                                     ? TF_COMPRESSION_ZSTD
                                     : TF_COMPRESSION_NONE;

    // generate program-specific variables
    if ((err = fseqCreateProgramVars(&vars, &varCount))) goto exit;

    // re-align header data offsets based on the variable data and encode/write
    // the header to the file controller
    if ((err = fseqRealignHeaderOffsets(&header, vars, varCount)) ||
        (err = fseqWriteHeader(fc, &header)))
        goto exit;

    // generate channel data
    // if uncompressed, this will return a NULL array pointer
    // if compressed, this will return an array of compression block metadata
    //  that need to be encoded directly past the initial header
    if ((err = generateChannelData(fc, &header, &blocks))) goto exit;

    if (header.compressionBlockCount > 0)
        if ((err = fseqWriteCompressionBlocks(fc, &header, blocks))) goto exit;

    // write variable table
    if ((err = fseqWriteVars(fc, &header, vars, varCount))) goto exit;

exit:
    FC_close(fc);

    for (int i = 0; i < varCount && vars != NULL; i++) free(vars[i].value);
    free(vars);
    free(blocks);
    free(outputPath);

    return err ? 1 : 0;
}
