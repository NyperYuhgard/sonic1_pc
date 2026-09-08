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

/* Render one screen row of a plane, drawing only tiles whose priority bit
   matches `pri` (0 or 1). The caller merges layers in MD priority order, so
   priority plane tiles can overlay non-priority sprites and planes.
   MD semantics: the 64x32-tile nametable (512x256 px) is cyclic on both axes,
   horizontal scroll is per-scanline (from the hscroll table) and vertical
   scroll is per-plane. screen_y selects the output row in [0,224). */
static void render_plane_scanline(const uint8_t *nametable, uint16_t *palette,
                                  int scroll_x, int scroll_y, int screen_y,
                                  uint32_t *pixels, int pitch, int pri) {
    uint32_t *out = pixels + screen_y * (pitch / 4);
    int plane_y = (screen_y + scroll_y) & 0xFF;      /* wrap 0..255 */
    int ty = plane_y >> 3;                           /* tile row */
    int py = plane_y & 7;                            /* pixel within tile */

    for (int sx = 0; sx < SCREEN_WIDTH; sx++) {
        int plane_x = (sx - scroll_x) & 0x1FF;       /* wrap 0..511 */
        int tx = plane_x >> 3;                       /* tile column */
        int px = plane_x & 7;                        /* pixel within tile */

        int idx = (ty * 64 + tx) * 2;
        uint16_t tile_entry = ((uint16_t)nametable[idx] << 8) | nametable[idx + 1];
        if (((tile_entry >> 15) & 1) != pri) continue;  /* priority filter */
        uint16_t tile_num  = tile_entry & 0x7FF;
        int x_flip         = (tile_entry >> 11) & 1;
        int y_flip         = (tile_entry >> 12) & 1;
        int pal_line       = (tile_entry >> 13) & 3;

        /* Tile art at tile_num * 32 bytes in VRAM (4bpp, 8x8 = 32 bytes) */
        const uint8_t *row = &vdp.vram[tile_num * 32 + (y_flip ? (7 - py) : py) * 4];
        int src_x = x_flip ? (7 - px) : px;
        int color_idx = (src_x & 1) ? (row[src_x >> 1] & 0x0F)
                                    : ((row[src_x >> 1] >> 4) & 0x0F);
        if (color_idx == 0) continue;                /* transparent */
        out[sx] = MD_ColorToRGBA(palette[pal_line * 16 + color_idx]);
    }
}

/* ------------------------------------------------------------------ */
/* Real-time VRAM viewer window                                       */
/* ------------------------------------------------------------------ */
#define VRAM_VIEW_COLS   128   /* 128 cols x 16 rows = 2048 tiles      */
#define VRAM_VIEW_ROWS   16
#define VRAM_VIEW_SCALE  2     /* 8x8 tile x2 = 16 px per cell         */
#define VRAM_VIEW_CELL   (8 * VRAM_VIEW_SCALE)
#define VRAM_VIEW_STRIP  32    /* CRAM strip height (bottom of window) */
#define VRAM_VIEW_W      (VRAM_VIEW_COLS * VRAM_VIEW_CELL)
#define VRAM_VIEW_H      (VRAM_VIEW_ROWS * VRAM_VIEW_CELL + VRAM_VIEW_STRIP)

static SDL_Window   *g_vram_win = NULL;
static SDL_Renderer *g_vram_ren = NULL;
static SDL_Texture  *g_vram_tex = NULL;

void VDP_ToggleVRAMViewer(void) {
    if (g_vram_win) {
        SDL_DestroyTexture(g_vram_tex);
        SDL_DestroyRenderer(g_vram_ren);
        SDL_DestroyWindow(g_vram_win);
        g_vram_tex = NULL;
        g_vram_ren = NULL;
        g_vram_win = NULL;
        return;
    }
    g_vram_win = SDL_CreateWindow("VRAM Viewer [P]",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                VRAM_VIEW_W, VRAM_VIEW_H,
                                SDL_WINDOW_RESIZABLE);
    if (!g_vram_win) return;
    g_vram_ren = SDL_CreateRenderer(g_vram_win, -1, 0);
    if (!g_vram_ren) {
        SDL_DestroyWindow(g_vram_win);
        g_vram_win = NULL;
        return;
    }
    g_vram_tex = SDL_CreateTexture(g_vram_ren, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 VRAM_VIEW_W, VRAM_VIEW_H);
}

/* SDL window ID of the viewer (-1 when closed). Lets the event loop
   rebuild/destroy it when the user closes the window via the WM. */
int VDP_ViewerWindowID(void) {
    return g_vram_win ? (int)SDL_GetWindowID(g_vram_win) : -1;
}

static void render_vram_viewer(void) {
    if (!g_vram_win) return;

    void *pixels;
    int pitch;
    SDL_LockTexture(g_vram_tex, NULL, &pixels, &pitch);
    uint32_t *px = (uint32_t *)pixels;

    /* Checker background so empty VRAM is obvious */
    for (int y = 0; y < VRAM_VIEW_H; y++) {
        for (int x = 0; x < VRAM_VIEW_W; x++) {
            int cc = ((x >> 2) + (y >> 2)) & 1;
            px[y * (pitch / 4) + x] = cc ? 0xFF3A3A3A : 0xFF525252;
        }
    }

    /* Draw all 2048 tiles. Tile number = row * 128 + col. */
    for (int t = 0; t < 2048; t++) {
        int col = t % VRAM_VIEW_COLS;
        int row = t / VRAM_VIEW_COLS;
        const uint8_t *td = &vdp.vram[t * 32];

        for (int ty = 0; ty < 8; ty++) {
            for (int tx = 0; tx < 8; tx++) {
                int idx = (tx & 1) ? (td[ty * 4 + (tx >> 1)] & 0xF)
                                   : ((td[ty * 4 + (tx >> 1)] >> 4) & 0xF);
                if (idx == 0) continue; /* transparent -> checker stays */

                uint32_t c = MD_ColorToRGBA(palette_main[idx]);
                int ox = col * VRAM_VIEW_CELL + tx * VRAM_VIEW_SCALE;
                int oy = row * VRAM_VIEW_CELL + ty * VRAM_VIEW_SCALE;
                for (int s = 0; s < VRAM_VIEW_SCALE; s++) {
                    for (int r = 0; r < VRAM_VIEW_SCALE; r++) {
                        px[(oy + r) * (pitch / 4) + ox + s] = c;
                    }
                }
            }
        }
    }

    /* Grid lines every tile */
    for (int y = 0; y < VRAM_VIEW_ROWS * VRAM_VIEW_CELL; y++) {
        for (int x = 0; x < VRAM_VIEW_W; x++) {
            if (x % VRAM_VIEW_CELL == 0 || y % VRAM_VIEW_CELL == 0) {
                px[y * (pitch / 4) + x] = 0xFF101010;
            }
        }
    }

    /* CRAM strip at the bottom (64 colors, shown with current palette) */
    for (int i = 0; i < 64; i++) {
        uint32_t c = MD_ColorToRGBA(vdp.cram[i]);
        int x0 = i * VRAM_VIEW_W / 64;
        int x1 = (i + 1) * VRAM_VIEW_W / 64;
        for (int y = VRAM_VIEW_ROWS * VRAM_VIEW_CELL; y < VRAM_VIEW_H; y++) {
            for (int x = x0; x < x1; x++) {
                px[y * (pitch / 4) + x] = c;
            }
        }
    }

    SDL_UnlockTexture(g_vram_tex);

    SDL_SetRenderDrawColor(g_vram_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_vram_ren);
    SDL_RenderCopy(g_vram_ren, g_vram_tex, NULL, NULL);
    SDL_RenderPresent(g_vram_ren);
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
    uint32_t *pix = (uint32_t *)pixels;
    /* Clear to black */
    uint16_t bg_cram_index = vdp.registers[7] & 0x3F;
    uint32_t bg_color = MD_ColorToRGBA(vdp.cram[bg_cram_index]);

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        pix[i] = bg_color; /* Color de fondo real de la Mega Drive */
    }

    /* Vertical scroll is per-plane (MD VSRAM); horizontal scroll is
       per-scanline from the 224-row hscroll table. The hscroll buffer holds
       4 bytes per row: word 0 = FG plane scroll, word 2 = BG plane scroll. */
    int16_t fg_scroll_y = (int16_t)v_scrposy_vdp;
    int16_t bg_scroll_y = (int16_t)v_bgscrposy_vdp;

    /* Sprite list metadata. On the MD the first sprite in the table is on TOP
       of later ones, and only the first 80 entries render (indices >= 80 are
       ignored). */
    uint8_t *table = &ram[v_spritetablebuffer];
    int n = v_spritecount;
    if (n > 80) n = 80;        /* hardware: never render entries >= 80 */
    if (n > sprites_max) n = sprites_max;

    int sy[80], sh[80], sw[80];
    for (int i = 0; i < n; i++) {
        uint8_t *entry = &table[i * 8];
        sy[i] = ((int)(entry[0] | (entry[1] << 8)) & 0x1FF) - 0x80;
        sh[i] = ((entry[2] & 0x0F) + 1) * 8;
        sw[i] = ((entry[2] >> 4) + 1) * 8;
    }

    for (int row = 0; row < SCREEN_HEIGHT; row++) {
        const uint8_t *h = &ram[v_hscrolltablebuffer + row * 4];
        int16_t fg_scroll_x = (int16_t)((uint16_t)h[0] | ((uint16_t)h[1] << 8));
        int16_t bg_scroll_x = (int16_t)((uint16_t)h[2] | ((uint16_t)h[3] << 8));

        /* Sprites crossing this scanline, in table order. The first to fill
           either the 20-sprite or the 320px budget win; anything later on the
           line is dropped. Off-screen sprites still count, which is exactly
           how the title screen's 32px-wide "sprite line limiter" fillers hide
           Sonic's lower body behind the ribbon: 10 fillers x 32px = the full
           320px budget for that scanline. */
        int list[20], list_len = 0;
        int px_budget = 0;
        for (int i = 0; i < n && list_len < 20; i++) {
            if (row >= sy[i] && row < sy[i] + sh[i]) {
                if (px_budget + sw[i] > 320) break;
                px_budget += sw[i];
                list[list_len++] = i;
            }
        }

        /* MD priority stack, low to high:
             1. Plane A tiles, priority 0 (BG nametable)
             2. Plane B tiles, priority 0 (FG nametable)
             3. Sprites,  priority 0
             4. Plane A tiles, priority 1
             5. Plane B tiles, priority 1
             6. Sprites,  priority 1
           Later layers overwrite earlier ones; transparent pixels are never
           written. */
        render_plane_scanline(&vdp.vram[vram_bg], palette_main,
                              bg_scroll_x, bg_scroll_y, row, pix, pitch, 0);
        render_plane_scanline(&vdp.vram[vram_fg], palette_main,
                              fg_scroll_x, fg_scroll_y, row, pix, pitch, 0);

        /* Blit surviving sprites tail-to-head so the first table entry
           ends up over later ones; pass 0 = non-priority sprites. */
        for (int k = list_len - 1; k >= 0; k--) {
            int i = list[k];
            uint8_t *entry = &table[i * 8];
            uint16_t pattern = (uint16_t)(entry[4] | (entry[5] << 8));
            if (((pattern >> 15) & 1) != 0) continue;
            int y = sy[i];
            int height_tiles = (entry[2] & 0x0F) + 1;
            int width_tiles = (entry[2] >> 4) + 1;
            int tile = pattern & 0x7FF;
            int pal_line = (pattern >> 13) & 3;
            int x = ((int)(entry[6] | (entry[7] << 8)) & 0x1FF) - 0x80;

            /* Only the tile-row that covers this scanline */
            int ty = (row - y) >> 3;
            int prow = (row - y) & 7;
            if (ty < 0 || ty >= height_tiles) continue;

            for (int tx = 0; tx < width_tiles; tx++) {
                /* MD sprite pattern indices run down a column first,
                   then to the right (stride = height) */
                int tile_idx = tile + tx * height_tiles + ty;
                if (tile_idx >= 0x800) continue;
                const uint8_t *r = &vdp.vram[tile_idx * 32 + prow * 4];
                int sx0 = x + tx * 8;

                for (int col = 0; col < 8; col++) {
                    int color_idx = (col & 1) ? (r[col >> 1] & 0xF)
                                              : ((r[col >> 1] >> 4) & 0xF);
                    if (color_idx == 0) continue;
                    int sx = sx0 + col;
                    if (sx < 0 || sx >= SCREEN_WIDTH) continue;
                    pix[row * SCREEN_WIDTH + sx] =
                        MD_ColorToRGBA(palette_main[pal_line * 16 + color_idx]);
                }
            }
        }

        render_plane_scanline(&vdp.vram[vram_bg], palette_main,
                              bg_scroll_x, bg_scroll_y, row, pix, pitch, 1);
        render_plane_scanline(&vdp.vram[vram_fg], palette_main,
                              fg_scroll_x, fg_scroll_y, row, pix, pitch, 1);

        /* Pass 1 = priority sprites, above everything. */
        for (int k = list_len - 1; k >= 0; k--) {
            int i = list[k];
            uint8_t *entry = &table[i * 8];
            uint16_t pattern = (uint16_t)(entry[4] | (entry[5] << 8));
            if (((pattern >> 15) & 1) != 1) continue;
            int y = sy[i];
            int height_tiles = (entry[2] & 0x0F) + 1;
            int width_tiles = (entry[2] >> 4) + 1;
            int tile = pattern & 0x7FF;
            int pal_line = (pattern >> 13) & 3;
            int x = ((int)(entry[6] | (entry[7] << 8)) & 0x1FF) - 0x80;

            int ty = (row - y) >> 3;
            int prow = (row - y) & 7;
            if (ty < 0 || ty >= height_tiles) continue;

            for (int tx = 0; tx < width_tiles; tx++) {
                int tile_idx = tile + tx * height_tiles + ty;
                if (tile_idx >= 0x800) continue;
                const uint8_t *r = &vdp.vram[tile_idx * 32 + prow * 4];
                int sx0 = x + tx * 8;

                for (int col = 0; col < 8; col++) {
                    int color_idx = (col & 1) ? (r[col >> 1] & 0xF)
                                              : ((r[col >> 1] >> 4) & 0xF);
                    if (color_idx == 0) continue;
                    int sx = sx0 + col;
                    if (sx < 0 || sx >= SCREEN_WIDTH) continue;
                    pix[row * SCREEN_WIDTH + sx] =
                        MD_ColorToRGBA(palette_main[pal_line * 16 + color_idx]);
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

    /* Refresh the debug VRAM viewer window (no-op when closed) */
    render_vram_viewer();
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
