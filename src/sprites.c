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
    int width_code = (p[1] >> 2) & 3;      /* width-1 (Sonic 1: 2 bits) */
    int height_code = p[1] & 3;            /* height-1 */
    uint8_t mflags = p[2];
    int m_pri   = (mflags >> 7) & 1;       /* priority flag */
    int m_pal   = (mflags >> 5) & 3;       /* palette line */
    int m_yflip = (mflags >> 4) & 1;       /* per-piece Y flip */
    int m_xflip = (mflags >> 3) & 1;       /* per-piece X flip */
    uint16_t tile = ((uint16_t)(mflags & 7) << 8) | p[3]; /* 11-bit tile */
    int x_off = (int8_t)p[4];
    *data = p + 5;

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

    uint16_t pattern = (uint16_t)gfx + tile;
    /* Per-piece mapping flags (obGfx already carries the palette bits) */
    if (m_pal)   pattern |= (uint16_t)(m_pal << 13); /* palette line in VDP word */
    if (m_xflip) pattern |= 1 << 11;   /* set X-flip in VDP word */
    if (m_yflip) pattern |= 1 << 12;   /* set Y-flip in VDP word */
    if (m_pri)   pattern |= 1 << 15;   /* priority: draws above planes/others */
    /* Object-level flips (obRender) toggle on top of the mapping flags */
    if (xflip) pattern ^= 1 << 11;
    if (yflip) pattern ^= 1 << 12;

    uint8_t *entry = &sprite_table[*sprite_index * 8];
    entry[0] = (uint8_t)(y & 0xFF);
    entry[1] = (uint8_t)((y >> 8) & 0xFF);
    entry[2] = (uint8_t)((width_code << 4) | height_code); /* dims WWHH */
    entry[3] = (uint8_t)((*sprite_index) + 1);             /* link */
    entry[4] = (uint8_t)(pattern & 0xFF);
    entry[5] = (uint8_t)((pattern >> 8) & 0xFF);
    entry[6] = (uint8_t)(x & 0xFF);
    entry[7] = (uint8_t)((x >> 8) & 0xFF);

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

    /* The MD fills the sprite table one priority layer at a time: all
       obPriority-0 objects first (lowest link / drawn on top, since the VDP
       follows the list front-to-back), then priority 1 ... 7. Within a layer
       DisplaySprite FIFO order is kept. This ordering is exactly what makes
       the title screen's "hide torso" trick work: the 30 filler sprites
       (priority 0) land BEFORE Sonic's pieces (priority 1), so on scanlines
       where the fillers + PRESS START + Sonic exceed the 20-sprite hardware
       limit, Sonic's trailing pieces are the ones dropped. */
    for (int layer = 0; layer < 8 && sprite_index < sprites_max; layer++) {
        for (int i = 0; i < sprite_queue_count && sprite_index < sprites_max; i++) {
            uint8_t *obj = sprite_queue[i];
            if (!obj || obID(obj) == 0) continue;
            if ((obPriority(obj) & 7) != layer) continue;

        /* --- Coordinate system (ASM BuildSprites:40-95) --- */
        uint8_t render = obRender(obj);
        uint16_t cam_field = render & (sprite_cam_field | sprite_cam_bg);
        int x;
        int y;

        if (cam_field == 0) {
            /* .screenCoords: on-screen positioning. No camera, no bounds check;
               obX/obScreenY already carry the VDP $-80 sprite-start offset */
            x = obX(obj);
            y = obScreenY(obj);
        } else {
            int cam_x;
            int cam_y;
            if (cam_field == sprite_cam_field) {
                cam_x = (int16_t)v_screenposx;
                cam_y = (int16_t)v_screenposy;
            } else if (cam_field == (sprite_cam_field | sprite_cam_bg)) {
                cam_x = (int16_t)v_bgscreenposx;
                cam_y = (int16_t)v_bgscreenposy;
            } else {
                cam_x = (int16_t)v_bg3screenposx;
                cam_y = (int16_t)v_bg3screenposy;
            }

            /* --- Screen bounds check for X-position (ASM:47-59) --- */
            int w = obActWid(obj);
            x = obX(obj) - cam_x;
            if (x + w < 0) continue;         /* left edge out of bounds */
            if (x - w >= 320) continue;      /* right edge out of bounds */

            /* --- Screen bounds check for Y-position (ASM:61-94) --- */
            y = obY(obj) - cam_y;
            if (render & sprite_customheight) {
                int h = obHeight(obj);
                if (y + h < 0) continue;            /* top edge out of bounds */
                if (y - h >= 224) continue;         /* bottom edge out of bounds */
            } else {
                if (y < -32) continue;              /* assumed height = 32 ($20) */
                if (y >= 192) continue;
            }

            x += 0x80;                     /* add VDP sprite start */
            y += 0x80;                     /* add VDP sprite start */
        }

        /* Get mapping pointer */
        const uint8_t *map = (const uint8_t *)(uintptr_t)obMap(obj);
        if (!map || !obj) continue;

        /* Get frame data (ASM BuildSprites: move.b obFrame(a0),d1)
           frame_offset = mappings table[obFrame] */
        int frame_idx = obFrame(obj);
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

        obRender(obj) |= sprite_rendered; /* ASM: bset #sprite_rendered_bit (bit 7) */
        }
    }

    v_spritecount = (uint8_t)sprite_index;
}

void Sprites_RenderToTexture(void *texture_pixels, int pitch) {
    /* TODO: Render the VDP sprite table to an SDL texture */
    (void)texture_pixels;
    (void)pitch;
}
