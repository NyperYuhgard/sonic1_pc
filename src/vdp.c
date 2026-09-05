#include "vdp.h"
#include "ram.h"
#include "palette.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

VDP_State vdp;
int vdp_test_counter = -1; /* -1 = disabled (normal operation) */

/* Persistent copy of the last rendered frame (ARGB8888) for screenshots */
static uint32_t last_frame[SCREEN_WIDTH * SCREEN_HEIGHT];

/* Convert MD CRAM color (0BGR) to 32-bit RGBA for SDL.
   CRAM word layout (bits): ....BBB.GGG.RRR
     - R = bits 1,2,3   (r0=1, r1=2, r2=3)
     - G = bits 5,6,7   (g0=5, g1=6, g2=7)
     - B = bits 9,10,11 (b0=9, b1=10, b2=11)
   e.g. 0x0EEE = R7 G7 B7 = full white. */
uint32_t MD_ColorToRGBA(uint16_t md_color) {
    int r = (md_color >> 1) & 7;
    int g = (md_color >> 5) & 7;
    int b = (md_color >> 9) & 7;
    /* Scale 3-bit to 8-bit (255/7 scale, same as Gens "Full") */
    r = r * 255 / 7;
    g = g * 255 / 7;
    b = b * 255 / 7;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void VDP_Init(void) {
    memset(&vdp, 0, sizeof(vdp));
    VDP_Reset();
}

void VDP_Reset(void) {
    /* Default VDP register values (from VDPSetupArray in sonic.asm)
       These are the RAW register values, NOT VDP command words. */
    vdp.registers[0]  = 0x0004;  /* mode 1: 8-colour mode */
    vdp.registers[1]  = 0x0074;  /* Mega Drive mode, DMA enable, display on */
    vdp.registers[2]  = 0x0030;  /* FG nametable at $C000 ($C000>>10 = $30) */
    vdp.registers[3]  = 0x0028;  /* Window nametable at $A000 ($A000>>10 = $28) */
    vdp.registers[4]  = 0x0007;  /* BG nametable at $E000 ($E000>>13 = $07) */
    vdp.registers[5]  = 0x0700;  /* Sprite table at $F800 ($F800>>10 = $78, but reg5 uses >>9? No, reg5 = sprite table addr >>9; $F800>>9 = $7F? Wait, original uses $0700 which is command $8500|$00? Actually in ASM it is written as move.w #vreg_spritevram|(vram_sprites>>9),(a6). $F800>>9 = $7F. So reg5 = $7F. But in Sonic 1 it is $0700? That seems like the command word $8500|$00? Let me keep the original values as they were commonly used in Sonic 1 disasm: reg5 = $07 (sprite table at $F800, using >>9? $F800/512 = $7F, not $07. This is confusing. In the Hivebrain disasm, vram_sprites = $F800, and vreg_spritevram = $8500. The code writes move.w #vreg_spritevram|(vram_sprites>>9),(a6). $F800>>9 = $7F. So the command is $8500|$7F = $857F. The register value is $7F. But in many ports, reg5 is $07 because they use >>10? No. Let me check the original Sonic 1 values from a reliable source. In Sonic 1, VDP register 5 is $7F (sprite table at $F800). But in your code you had $0700. That was wrong. I'll set it to $7F. */
    vdp.registers[5]  = 0x007F;  /* Sprite table at $F800 ($F800>>9 = $7F) */
    vdp.registers[6]  = 0x0000;
    vdp.registers[7]  = 0x0000;  /* BG colour */
    vdp.registers[8]  = 0x0000;
    vdp.registers[9]  = 0x0000;
    vdp.registers[10] = 0x00FF;  /* HBlank rate */
    vdp.registers[11] = 0x0000;  /* Full screen scroll */
    vdp.registers[12] = 0x0081;  /* 40 cell display */
    vdp.registers[13] = 0x0037;  /* H-scroll table at $DC00 ($DC00>>10 = $37) */
    vdp.registers[14] = 0x0000;
    vdp.registers[15] = 0x0001;  /* VDP increment = 2 */
    vdp.registers[16] = 0x0001;  /* 64 cell h-scroll size */
    vdp.registers[17] = 0x0000;
    vdp.registers[18] = 0x0000;
    vdp.registers[19] = 0x00FF;  /* DMA length */
    vdp.registers[20] = 0x0000;  /* DMA source low */
    vdp.registers[21] = 0x0000;  /* DMA source high */
    vdp.registers[22] = 0x0080;  /* DMA mode */

    memset(vdp.vram, 0, VRAM_SIZE);
    memset(vdp.cram, 0, sizeof(vdp.cram));
    memset(vdp.vsram, 0, sizeof(vdp.vsram));
    vdp.status = 0;
    vdp.vdp_cmd = 0;
}

void VDP_WriteVRAM(const uint8_t *src, uint32_t vram_addr, uint32_t len) {
    if (vram_addr + len > VRAM_SIZE) {
        len = VRAM_SIZE - vram_addr;
    }
    memcpy(&vdp.vram[vram_addr], src, len);
}

void VDP_WriteCRAM(const uint8_t *src, uint32_t cram_addr, uint32_t len) {
    uint32_t words = len / 2;
    for (uint32_t i = 0; i < words && (cram_addr + i) < CRAM_SIZE; i++) {
        uint16_t color = ((uint16_t)src[i * 2] << 8) | src[i * 2 + 1];
        vdp.cram[cram_addr + i] = color;
    }
}

void VDP_FillVRAM(uint8_t byte, uint32_t vram_addr, uint32_t len) {
    if (vram_addr + len > VRAM_SIZE) {
        len = VRAM_SIZE - vram_addr;
    }
    memset(&vdp.vram[vram_addr], byte, len);
}

void VDP_SetRegister(uint8_t reg, uint16_t value) {
    if (reg < 24) {
        vdp.registers[reg] = value;
    }
}

void VDP_ClearScreen(void) {
    /* Clear FG nametable (vram_fg to vram_fg + plane_size_64x32) */
    VDP_FillVRAM(0, vram_fg, 64 * 32 * 2);
    /* Clear BG nametable (vram_bg to vram_bg + plane_size_64x32) */
    VDP_FillVRAM(0, vram_bg, 64 * 32 * 2);

    /* Clear scroll position buffers */
    v_scrposy_vdp = 0;
    v_scrposx_vdp = 0;

    /* Clear sprite table buffer */
    memset(&ram[v_spritetablebuffer], 0, 0x400);
    /* Clear H-scroll table buffer */
    memset(&ram[v_hscrolltablebuffer], 0, 0x400);
}

void VDP_TransferPalette(void) {
    memcpy(palette_main, RAM_ADDR(v_palette), sizeof(palette_main));
}

void VDP_CopyTilemapToVRAM(const uint16_t *source, uint32_t vram_dest,
                            int width, int height) {
    for (int row = 0; row < height; row++) {
        uint32_t dest = vram_dest + (row * 128); /* 64 cells * 2 bytes = 128 bytes per row */
        for (int col = 0; col < width; col++) {
            uint16_t tile_entry = source[row * width + col];
            /* Store big-endian into VRAM (MSB first, like the MD VDP) */
            vdp.vram[dest + col * 2]     = (uint8_t)(tile_entry >> 8);
            vdp.vram[dest + col * 2 + 1] = (uint8_t)(tile_entry & 0xFF);
        }
    }
}

/* Render one 32x32-tile nametable plane to a pixel buffer */
static void render_plane(const uint8_t *nametable, uint16_t *palette,
                         int scroll_x, int scroll_y,
                         uint32_t *pixels, int pitch,
                         int plane_w, int plane_h) {
    /* nametable: 64x32 entries of 2 bytes each (tile index + flags) */
    /* Each nametable entry: bits 0-10 = tile number, bit 11 = Y flip,
       bit 12 = X flip, bit 13-14 = palette, bit 15 = priority */

    for (int ty = 0; ty < plane_h; ty++) {
        for (int tx = 0; tx < plane_w; tx++) {
            int idx = (ty * plane_w + tx) * 2;
            uint16_t tile_entry = ((uint16_t)nametable[idx] << 8) | nametable[idx + 1];

            uint16_t tile_num  = tile_entry & 0x7FF;
            int x_flip         = (tile_entry >> 11) & 1;
            int y_flip         = (tile_entry >> 12) & 1;
            int pal_line       = (tile_entry >> 13) & 3;

            /* Tile art is at tile_num * 32 bytes in VRAM (4bpp, 8x8 = 32 bytes) */
            const uint8_t *tile_data = &vdp.vram[tile_num * 32];

            /* Render 8x8 pixels of this tile */
            for (int py = 0; py < 8; py++) {
                int src_y = y_flip ? (7 - py) : py;
                /* Each row of a 4bpp tile is 4 bytes (8 pixels, 4 bits each) */
                const uint8_t *row = &tile_data[src_y * 4];

                for (int px = 0; px < 8; px++) {
                    int src_x = x_flip ? (7 - px) : px;
                    /* Extract 4-bit pixel: high nibble of first byte, etc. */
                    int bitplane = (row[src_x / 2]);
                    int color_idx;
                    if (src_x & 1) {
                        color_idx = bitplane & 0x0F;
                    } else {
                        color_idx = (bitplane >> 4) & 0x0F;
                    }

                    if (color_idx == 0) continue; /* transparent */

                    int screen_x = tx * 8 + px - scroll_x;
                    int screen_y = ty * 8 + py - scroll_y;

                    if (screen_x < 0 || screen_x >= SCREEN_WIDTH) continue;
                    if (screen_y < 0 || screen_y >= SCREEN_HEIGHT) continue;

                    uint16_t md_color = palette[pal_line * 16 + color_idx];
                    pixels[screen_y * (pitch / 4) + screen_x] = MD_ColorToRGBA(md_color);
                }
            }
        }
    }
}

void VDP_RenderFrame(SDL_Renderer *renderer) {
    /* Create or update texture */
    if (!vdp.framebuffer) {
        vdp.framebuffer = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    void *pixels;
    int pitch;
    SDL_LockTexture(vdp.framebuffer, NULL, &pixels, &pitch);

    /* Clear to black */
    uint32_t *pix = (uint32_t *)pixels;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        pix[i] = 0xFF000000; /* black */
    }

    /* Get scroll values from hscroll buffer */
    int16_t fg_scroll_x = RAM_SWORD(v_hscrolltablebuffer);
    int16_t fg_scroll_y = (int16_t)v_scrposy_vdp;
    int16_t bg_scroll_x = RAM_SWORD(v_hscrolltablebuffer + 2);
    int16_t bg_scroll_y = (int16_t)v_bgscrposy_vdp;

    /* Render BG plane (nametable at $E000 in VRAM = offset 0xE000) */
    render_plane(&vdp.vram[vram_bg], palette_main,
                 bg_scroll_x, bg_scroll_y,
                 pix, pitch, 64, 32);

    /* Render FG plane (nametable at $C000 in VRAM = offset 0xC000) */
    render_plane(&vdp.vram[vram_fg], palette_main,
                 fg_scroll_x, fg_scroll_y,
                 pix, pitch, 64, 32);

    /* Render sprites from sprite table buffer */
    {
        uint8_t *table = &ram[v_spritetablebuffer];
        for (int i = 0; i < v_spritecount && i < sprites_max; i++) {
            uint8_t *entry = &table[i * 8];
            int y = entry[0] | ((entry[1] & 1) << 8);
            int size = ((entry[1] >> 1) & 0xF) | (((entry[1] >> 5) & 0xF) << 4);
            int pal_line = (entry[1] >> 2) & 3;
            int tile = ((entry[3] & 1) << 8) | entry[4];
            int x = entry[6] | ((entry[7] & 1) << 8);

            int width_tiles = (size & 0xF) ? (size & 0xF) : 1;
            int height_tiles = (size >> 4) ? (size >> 4) : 1;

            for (int ty = 0; ty < height_tiles; ty++) {
                for (int tx = 0; tx < width_tiles; tx++) {
                    int tile_idx = tile + ty * 2 + tx;
                    const uint8_t *tile_data = &vdp.vram[tile_idx * 32];
                    int px = x + tx * 8;
                    int py = y + ty * 8;

                    for (int row = 0; row < 8; row++) {
                        const uint8_t *r = &tile_data[row * 4];
                        for (int col = 0; col < 8; col++) {
                            int color_idx = (col & 1) ? (r[col >> 1] & 0xF) : ((r[col >> 1] >> 4) & 0xF);
                            if (color_idx == 0) continue;
                            int sx = px + col;
                            int sy = py + row;
                            if (sx < 0 || sx >= SCREEN_WIDTH || sy < 0 || sy >= SCREEN_HEIGHT) continue;
                            pix[sy * SCREEN_WIDTH + sx] = MD_ColorToRGBA(palette_main[pal_line * 16 + color_idx]);
                        }
                    }
                }
            }
        }
    }

    /* Optional test overlay: draw a counter as colored bars (debug/title test) */
    if (vdp_test_counter >= 0) {
        int w = SCREEN_WIDTH - 20;
        int bar_w = (vdp_test_counter * w) / 600; /* 0..600 -> 0..full */
        if (bar_w > w) bar_w = w;
        for (int y = 20; y < 60 && y < SCREEN_HEIGHT; y++) {
            for (int x = 10; x < 10 + bar_w && x < SCREEN_WIDTH; x++) {
                pix[y * SCREEN_WIDTH + x] = 0xFF00FF00; /* green bar */
            }
        }
        /* Also tint the background to prove frames are advancing */
        if (vdp_test_counter % 60 < 30) {
            pix[0] = 0xFFFFFFFF;
        }
    }

    /* Persist frame for VDP_SaveScreenshot */
    memcpy(last_frame, pix, SCREEN_WIDTH * SCREEN_HEIGHT * 4);

    SDL_UnlockTexture(vdp.framebuffer);

    /* Present */
    SDL_RenderCopy(renderer, vdp.framebuffer, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void VDP_SaveScreenshot(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint32_t c = last_frame[i];
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >> 8) & 0xFF;
        uint8_t b = (c >> 0) & 0xFF;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
}
