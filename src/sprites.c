#include "sprites.h"
#include "ram.h"
#include "constants.h"
#include "objects.h"
#include <string.h>
#include <stdio.h>

/* ===========================================================================
   Helpers
   =========================================================================== */

static void build_sprite_piece(uint8_t *sprite_table, int *sprite_index,
                               int base_y, int base_x, const uint8_t **data,
                               uint16_t gfx, int xflip, int yflip) {
    if (*sprite_index >= sprites_max) return;

    const uint8_t *p = *data;
    int y_off = (int8_t)p[0];
    int width_code = p[1] >> 4;
    int height_code = p[1] & 0x0F;
    uint16_t tile = ((uint16_t)p[2] << 8) | p[3];
    int x_off = (int8_t)p[4];
    *data = p + 5;

    tile += gfx;

    if (yflip) {
        y_off = -y_off;
        y_off -= (height_code + 1) * 8;
    }
    int y = base_y + y_off;

    if (xflip) {
        x_off = -x_off;
        x_off -= (width_code + 1) * 8;
    }
    int x = base_x + x_off;
    x &= 0x1FF;
    if (x == 0) x = 1;

    int size = ((width_code & 0xF) << 4) | (height_code & 0xF);
    int pal_line = (gfx >> 13) & 3;
    int link = (*sprite_index) + 1;

    uint8_t *entry = &sprite_table[*sprite_index * 8];
    entry[0] = (uint8_t)(y & 0xFF);
    entry[1] = (uint8_t)(((y & 0x100) ? 1 : 0) | ((size & 0xF) << 1) | ((pal_line & 3) << 2) | (((size >> 4) & 0xF) << 5));
    entry[2] = (uint8_t)link;
    entry[3] = (uint8_t)((tile >> 8) & 0x1);
    entry[4] = (uint8_t)(tile & 0xFF);
    entry[5] = 0;
    entry[6] = (uint8_t)(x & 0xFF);
    entry[7] = (uint8_t)((x >> 8) & 0x1);

    (*sprite_index)++;
}

/* ===========================================================================
   BuildSprites - Port of _inc/BuildSprites.asm
   Converts object mappings into Mega Drive sprite table entries.
   =========================================================================== */

void BuildSprites(void) {
    extern uint8_t **sprite_queue;
    extern int sprite_queue_count;

    uint8_t *sprite_table = &ram[v_spritetablebuffer];
    memset(sprite_table, 0, sprites_max * 8);

    int sprite_index = 0;

    for (int i = 0; i < sprite_queue_count && sprite_index < sprites_max; i++) {
        uint8_t *obj = sprite_queue[i];
        if (!obj || obID(obj) == 0) continue;

        /* Screen bounds check */
        int width = obActWid(obj);
        int x = obX(obj);
        if (x < -width || x > 320 + width) continue;

        int y = obScreenY(obj);
        if (y < 0 || y > 224) continue;

        /* Coordinate adjustment */
        x += 0x80;
        y += 0x80;

        /* Get mapping pointer */
        const uint8_t *map = (const uint8_t *)(uintptr_t)obMap(obj);
        if (!map || !obj) continue;

        /* Get frame data */
        int frame_idx = obAniFrame(obj);
        if (frame_idx < 0 || frame_idx > 255) continue;
        uint16_t frame_offset = ((const uint16_t *)map)[frame_idx];
        if (frame_offset == 0 || frame_offset > 64000) continue;
        const uint8_t *frame_data = map + frame_offset;

        int num_pieces = frame_data[0];
        if (num_pieces <= 0 || num_pieces > 32) continue;

        const uint8_t *piece_data = frame_data + 1;
        uint16_t gfx = obGfx(obj);

        int xflip = (obRender(obj) & sprite_xflip) ? 1 : 0;
        int yflip = (obRender(obj) & sprite_yflip) ? 1 : 0;

        for (int p = 0; p < num_pieces && sprite_index < sprites_max; p++) {
            build_sprite_piece(sprite_table, &sprite_index, y, x,
                               &piece_data, gfx, xflip, yflip);
        }

        obRender(obj) |= sprite_rendered_bit;
    }

    v_spritecount = (uint8_t)sprite_index;
}

void Sprites_RenderToTexture(void *texture_pixels, int pitch) {
    /* TODO: Render the VDP sprite table to an SDL texture */
    (void)texture_pixels;
    (void)pitch;
}
