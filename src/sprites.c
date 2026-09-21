#include "sprites.h"
#include "ram.h"
#include "constants.h"
#include "objects.h"
#include "data.h"
#include "vdp.h"
#include <string.h>
#include <stdio.h>

/* ===========================================================================
   Helpers
   =========================================================================== */

void Sprites_EmitPiece(uint8_t *sprite_table, int *sprite_index,
                       int base_y, int base_x, const uint8_t **data,
                       uint16_t gfx, int xflip, int yflip) {
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
    /* SIN masking de 9 bits y SIN el workaround x==0.
       Guardamos los 16 bits completos con signo; el renderer los
       decodifica como int16_t. Esto evita que piezas con posición
       < -128 o > +383 hagan wrap-around al otro lado de la pantalla.
       Con g_render_left == 0 el comportamiento es idéntico al original
       (todas las posiciones caen dentro de [-128, 383] por el culling). */

    uint16_t pattern = (uint16_t)gfx + tile;
    if (m_pal)   pattern |= (uint16_t)(m_pal << 13);
    if (m_xflip) pattern |= 1 << 11;
    if (m_yflip) pattern |= 1 << 12;
    if (m_pri)   pattern |= 1 << 15;
    if (xflip) pattern ^= 1 << 11;
    if (yflip) pattern ^= 1 << 12;

    uint8_t *entry = &sprite_table[*sprite_index * 8];
    entry[0] = (uint8_t)(y & 0xFF);
    entry[1] = (uint8_t)((y >> 8) & 0xFF);
    entry[2] = (uint8_t)((width_code << 4) | height_code);
    entry[3] = (uint8_t)((*sprite_index) + 1);
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

    for (int layer = 0; layer < 8 && sprite_index < sprites_max; layer++) {
        for (int i = 0; i < sprite_queue_count && sprite_index < sprites_max; i++) {
            uint8_t *obj = sprite_queue[i];
            if (!obj || obID(obj) == 0) continue;
            if ((obPriority(obj) & 7) != layer) continue;
            obRender(obj) &= ~sprite_rendered;
            uint8_t render = obRender(obj);
            uint16_t cam_field = render & (sprite_cam_field | sprite_cam_bg);
            int x, y;

            if (cam_field == 0) {
                x = obX(obj);
                y = obScreenY(obj);
            } else {
                int cam_x, cam_y;
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

                int w = obActWid(obj);
                int margin = g_render_left;          /* 0 en modo 1:1 → comportamiento original */
                x = obX(obj) - cam_x;
                if (x + w < -margin) continue;
                if (x - w >= 320 + margin) continue;

                y = obY(obj) - cam_y;
                if (render & sprite_customheight) {
                    int h = obHeight(obj);
                    if (y + h < 0) continue;
                    if (y - h >= 224) continue;
                } else {
                    if (y < -32) continue;
                    if (y >= 256) continue;
                }

                x += 0x80;
                y += 0x80;
            }

            const uint8_t *map = (const uint8_t *)(uintptr_t)obMap(obj);
            if (!map) continue;

            uint16_t gfx = obGfx(obj);
            int xflip = (render & sprite_xflip) ? 1 : 0;
            int yflip = (render & sprite_yflip) ? 1 : 0;

            /* ---- NUEVA RAMA: raw mappings ---- */
            if (render & sprite_rawmappings) {
                /* obMap apunta directamente a UNA pieza (5 bytes).
                 *                  No se consulta Map_LookupLength ni la tabla de frames. */
                const uint8_t *piece_data = map;
                if (sprite_index < sprites_max) {
                    Sprites_EmitPiece(sprite_table, &sprite_index, y, x,
                                       &piece_data, gfx, xflip, yflip);
                }
                obRender(obj) |= sprite_rendered;
                continue;
            }
            /* ----------------------------------- */

            size_t map_len = Map_LookupLength(map);
            if (map_len == 0) continue;

            int frame_idx = obFrame(obj);
            if ((size_t)(frame_idx * 2 + 1) >= map_len) continue;
            uint16_t frame_offset = ((const uint16_t *)map)[frame_idx];

            if (frame_offset >= map_len) continue;
            const uint8_t *frame_data = map + frame_offset;

            int num_pieces = frame_data[0];
            if (num_pieces <= 0 || num_pieces > 32) continue;

            if ((size_t)frame_offset + 1 + (size_t)num_pieces * 5 > map_len) continue;

            const uint8_t *piece_data = frame_data + 1;

            for (int p = 0; p < num_pieces && sprite_index < sprites_max; p++) {
                Sprites_EmitPiece(sprite_table, &sprite_index, y, x,
                                   &piece_data, gfx, xflip, yflip);
            }

            obRender(obj) |= sprite_rendered;
        }
    }

    v_spritecount = (uint8_t)sprite_index;
}

void Sprites_RenderToTexture(void *texture_pixels, int pitch) {
    /* TODO: Render the VDP sprite table to an SDL texture */
    (void)texture_pixels;
    (void)pitch;
}
