#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TINYFSEQ_IMPLEMENTATION
#include <tinyfseq.h>

#include <sds.h>

#define VAR_HEADER_SIZE 4

int main(const int argc, char **const argv) {
    assert(argc >= 2);

    char *const fp = argv[1];

    FILE *const f = fopen(fp, "rb");
    assert(f != NULL);

    uint8_t b[32];
    fread(b, sizeof(b), 1, f);

    struct tf_file_header_t header = {0};
    tf_read_file_header(b, sizeof(b), &header, NULL);

    const uint16_t vsize = header.channelDataOffset - header.variableDataOffset;
    assert(vsize > VAR_HEADER_SIZE);

    uint8_t *const vdata = malloc(vsize);
    assert(vdata != NULL);

    fseek(f, header.variableDataOffset, SEEK_SET);
    fread(vdata, vsize, 1, f);

    uint16_t pos = 0;

    // ignore padding bytes at end (<=3), or ending 0-length value
    // 0-length values could be technically supported?
    while (pos < vsize - VAR_HEADER_SIZE) {
        // only valid until `pos` is modified at end of block
        uint8_t *head = &vdata[pos];

        const uint16_t hsize = ((uint16_t *) head)[0];
        const int32_t ssize = hsize - VAR_HEADER_SIZE;

        assert(ssize > 0);

        const uint8_t idh = head[2];
        const uint8_t idl = head[3];

        // let sds handle the dirty work of reading and null terminating the string
        sds str = sdsnewlen(&head[4], ssize);

        printf("var `%c%c` has %d bytes: `%s`\n", idh, idl, ssize, str);

        sdsfree(str);

        // advance position forward for next loop's `*head`
        pos += hsize;
    }

    free(vdata);

    fclose(f);

    return 0;
}