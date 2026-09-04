#ifndef SONIC1_ASSETS_H
#define SONIC1_ASSETS_H

#include <stdint.h>
#include <stddef.h>

/* Load an entire binary file into a newly allocated buffer.
   Returns pointer on success, NULL on failure. Buffer must be freed with
   Assets_Free(). */
uint8_t *Assets_Load(const char *path, size_t *out_size);

/* Free a buffer returned by Assets_Load */
void Assets_Free(uint8_t *buf);

#endif /* SONIC1_ASSETS_H */
