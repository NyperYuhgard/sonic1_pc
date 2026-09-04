#include "sprites.h"
#include "ram.h"
#include "constants.h"
#include <string.h>

/* Sprite priority queue (from ASM v_spritequeue)
   8 layers, each $80 bytes. First 2 bytes = count of objects in layer. */
#define SPRITE_QUEUE_LAYERS  8

void BuildSprites(void) {
    uint8_t *queue = &ram[v_spritequeue];
    uint8_t *sprite_table = &ram[v_spritetablebuffer];
    int total_sprites = 0;

    /* Clear sprite table */
    memset(sprite_table, 0, sprites_max * 8);

    /* Process each priority layer */
    for (int layer = 0; layer < SPRITE_QUEUE_LAYERS; layer++) {
        uint8_t *layer_ptr = queue + layer * spritelayer_size;
        uint16_t count = *(uint16_t *)layer_ptr;

        if (count == 0) continue;

        int offset = 2; /* skip count word */
        for (uint16_t j = 0; j < count; j++) {
            if (total_sprites >= sprites_max) goto done;

            /* Read object pointer from queue (2 bytes, little-endian) */
            uint16_t obj_addr = *(uint16_t *)(layer_ptr + offset);
            offset += 2;

            if (obj_addr == 0) continue;

            uint8_t *obj = &ram[obj_addr];

            /* Check if object is still valid */
            if (obj[0] == 0) continue; /* obID == 0 */

            /* TODO: Full BuildSprites implementation
               For now, just increment the counter */
            total_sprites++;
        }
    }

done:
    v_spritecount = (uint8_t)total_sprites;

    /* Write end-of-sprites marker */
    if (total_sprites < sprites_max) {
        uint32_t *end = (uint32_t *)(&sprite_table[total_sprites * 8]);
        *end = 0;
    }
}

void Sprites_RenderToTexture(void *texture_pixels, int pitch) {
    /* TODO: Render the VDP sprite table to an SDL texture */
    (void)texture_pixels;
    (void)pitch;
}
