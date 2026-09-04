#include "sprites.h"
#include "ram.h"
#include "constants.h"
#include "objects.h"
#include <string.h>
#include <stdio.h>

/* Sprite priority queue (from ASM v_spritequeue)
    8 layers, each $80 bytes. First 2 bytes = count of objects in layer. */
#define SPRITE_QUEUE_LAYERS  8

/* Minimal sprite table writer helpers */
static void write_sprite(uint8_t *table, int index, int y, int size, int link, int tile, int x) {
    uint8_t *entry = &table[index * 8];
    entry[0] = (uint8_t)(y & 0xFF);
    entry[1] = (uint8_t)(((y & 0x100) ? 1 : 0) | ((size & 0xF) << 1) | (((size >> 4) & 0xF) << 5));
    entry[2] = (uint8_t)link;
    entry[3] = (uint8_t)((tile >> 8) & 0x1);
    entry[4] = (uint8_t)(tile & 0xFF);
    entry[5] = 0;
    entry[6] = (uint8_t)(x & 0xFF);
    entry[7] = (uint8_t)((x >> 8) & 0x1);
}

void BuildSprites(void) {
    extern uint8_t **sprite_queue;
    extern int sprite_queue_count;

    uint8_t *sprite_table = &ram[v_spritetablebuffer];
    memset(sprite_table, 0, sprites_max * 8);

    int sprite_index = 0;
    for (int i = 0; i < sprite_queue_count && sprite_index < sprites_max; i++) {
        uint8_t *obj = sprite_queue[i];
        if (!obj || obj[0] == 0) continue;

        int16_t y = (int16_t)((obj[0x0C] << 8) | obj[0x0D]);
        int16_t x = (int16_t)((obj[0x08] << 8) | obj[0x09]);

        int tile = ArtTile_Title_Sonic;
        int size = 0x22; /* 2x2 tiles */

        write_sprite(sprite_table, sprite_index, y, size, sprite_index + 1, tile, x);
        sprite_index++;
        if (sprite_index >= sprites_max) break;

        write_sprite(sprite_table, sprite_index, y + 16, size, 0, tile + 2, x);
        sprite_index++;
    }

    v_spritecount = (uint8_t)sprite_index;
}

void Sprites_RenderToTexture(void *texture_pixels, int pitch) {
    /* TODO: Render the VDP sprite table to an SDL texture */
    (void)texture_pixels;
    (void)pitch;
}
