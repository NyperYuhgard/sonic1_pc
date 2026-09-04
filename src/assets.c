#include "assets.h"
#include <stdio.h>
#include <stdlib.h>

uint8_t *Assets_Load(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[rd] = 0;

    if (out_size) *out_size = rd;
    return buf;
}

void Assets_Free(uint8_t *buf) {
    free(buf);
}
