#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

    /* The 68k Enigma/Nemesis decoders read a small look-ahead past the end
       of the compressed stream (in the original game this landed on ROM
       padding). Allocate slack so those reads stay in bounds. */
    uint8_t *buf = (uint8_t *)malloc((size_t)size + 16);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    for (int i = 0; i < 16; i++) {
        buf[rd + i] = 0;
    }

    if (out_size) *out_size = rd;
    return buf;
}

void Assets_Free(const uint8_t *buf) {
    free((void *)buf);
}
