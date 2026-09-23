#include "vdp.h"
#include "ram.h"
#include "palette.h"
#include "collision.h"
#include "data.h"
#include "planeview.h"
#include "font8x8.h"
#include "postprocess.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

VDP_State vdp;
int vdp_test_counter = -1; /* -1 = disabled (normal operation) */
int g_render_w    = 320;
int g_render_left = 0;

void VDP_ApplyWidescreen(int new_w) {
    if (new_w < 320) new_w = 320;
    new_w &= ~1;
    g_render_w    = new_w;
    g_render_left = (new_w - 320) / 2;

    if (vdp.framebuffer) {
        SDL_DestroyTexture(vdp.framebuffer);
        vdp.framebuffer = NULL;
    }
    v_fg_scroll_flags |= 0x0F;
    v_bg1_scroll_flags |= 0x0F;
    v_bg2_scroll_flags |= 0x0F;
    v_bg3_scroll_flags |= 0x0F;
    /* El viewport lógico lo ajusta main.c (tiene el renderer a mano). */
}

/* Persistent copy of the last rendered frame (ARGB8888) for screenshots */
#define MAX_RENDER_W 640
static uint32_t last_frame[MAX_RENDER_W * SCREEN_HEIGHT];

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
    v_scrposy_vdp    = 0;
    v_bgscrposy_vdp  = 0;
    v_scrposx_vdp    = 0;
    v_bgscrposx_vdp  = 0;
    v_bg3scrposy_vdp = 0;
    v_bg3scrposx_vdp = 0;

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
            uint32_t addr = dest + (uint32_t)col * 2;
            if (addr + 1 >= VRAM_SIZE) continue;
            uint16_t tile_entry = source[row * width + col];
            /* Store big-endian into VRAM (MSB first, like the MD VDP) */
            vdp.vram[addr]     = (uint8_t)(tile_entry >> 8);
            vdp.vram[addr + 1] = (uint8_t)(tile_entry & 0xFF);
        }
    }
}
/* El registro $10 codifica el alto de cada plano: bits 4-5 para plane A y
   bits 6-7 para plane B. 00 = 32 filas, 01 = 64 filas, 11 = 128 filas
   (10 se trata como 32). El Special Stage usa $11 (A = 64, B = 32 filas);
   si se aplicara el alto de A a los dos planos, B leería VRAM por encima de
   su área (basura). */
int VDP_PlaneRows(int plane_b) {
    int height_bits = (vdp.registers[16] >> (plane_b ? 6 : 4)) & 0x3;
    return (height_bits == 1) ? 64
         : (height_bits == 3) ? 128
         : 32;
}
static int vdp_plane_height_mask(int plane_b) {
    return VDP_PlaneRows(plane_b) * 8 - 1;   /* 0xFF, 0x1FF o 0x3FF */
}
/* Render one screen row of a plane, drawing only tiles whose priority bit
   matches `pri` (0 or 1). The caller merges layers in MD priority order, so
   priority plane tiles can overlay non-priority sprites and planes.
   MD semantics: the nametable is cyclic on both axes; Sonic 1 normally uses
   64 columns, while register $10 selects 32/64/128 rows for vertical wrap.
   horizontal scroll is per-scanline (from the hscroll table) and vertical
   scroll is per-plane. screen_y selects the output row in [0,224). */
static void render_plane_scanline(const uint8_t *nametable, uint16_t *palette,
                                  int scroll_x, int scroll_y, int screen_y,
                                  uint32_t *pixels, int pitch, int pri, int plane_b)
{
    int stride = pitch / 4;
    uint32_t *out = pixels + screen_y * stride;
    int y_mask = vdp_plane_height_mask(plane_b);
    int plane_y = (screen_y + scroll_y) & y_mask;
    int ty = plane_y >> 3;
    int py = plane_y & 7;

    for (int sx = 0; sx < g_render_w; sx++) {
        /* sx=0 → leftmost extra column; sx=g_render_left → centro (equivale al x=0 original) */
        int plane_x = (sx - g_render_left - scroll_x) & 0x1FF;
        int tx = plane_x >> 3;
        int px = plane_x & 7;
        int idx = (ty * 64 + tx) * 2;
        uint16_t tile_entry = ((uint16_t)nametable[idx] << 8) | nametable[idx + 1];
        if (((tile_entry >> 15) & 1) != pri) continue;
        uint16_t tile_num  = tile_entry & 0x7FF;
        int x_flip         = (tile_entry >> 11) & 1;
        int y_flip         = (tile_entry >> 12) & 1;
        int pal_line       = (tile_entry >> 13) & 3;

        const uint8_t *row = &vdp.vram[tile_num * 32 + (y_flip ? (7 - py) : py) * 4];
        int src_x = x_flip ? (7 - px) : px;
        int color_idx = (src_x & 1) ? (row[src_x >> 1] & 0x0F)
                                    : ((row[src_x >> 1] >> 4) & 0x0F);
        if (color_idx == 0) continue;
        out[sx] = MD_ColorToRGBA(palette[pal_line * 16 + color_idx]);
    }
}

/* ------------------------------------------------------------------ */
/* Real-time VRAM viewer window                                       */
/* ------------------------------------------------------------------ */
/* The sheet is not fixed-size anymore: every frame the number of
   columns/rows and the tile cell size (integer scale, 8..64 px) are
   recomputed from the CURRENT window size so the 2048 tiles always fit
   without being stretched or cropped. The backing texture is recreated at
   the window size, so SDL renders everything 1:1. */
#define VRAM_VIEW_TILES  2048 /* tiles $000..$7FF                          */
#define VRAM_VIEW_LABEL  56   /* left margin for VRAM address labels       */
#define VRAM_VIEW_HDR    16   /* top OSD band                              */
#define VRAM_VIEW_STRIP  67   /* palette: 4 square-swatch palette lines    */

static SDL_Window   *g_vram_win = NULL;
static SDL_Renderer *g_vram_ren = NULL;
static SDL_Texture  *g_vram_tex = NULL;
static int g_vram_tex_w = 0, g_vram_tex_h = 0;

void VDP_ToggleVRAMViewer(void) {
    if (g_vram_win) {
        SDL_DestroyTexture(g_vram_tex);
        SDL_DestroyRenderer(g_vram_ren);
        SDL_DestroyWindow(g_vram_win);
        g_vram_tex = NULL;
        g_vram_ren = NULL;
        g_vram_win = NULL;
        g_vram_tex_w = 0;
        g_vram_tex_h = 0;
        return;
    }
    g_vram_win = SDL_CreateWindow("VRAM Viewer [P]",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                1200, 760, SDL_WINDOW_RESIZABLE);
    if (!g_vram_win) return;
    g_vram_ren = SDL_CreateRenderer(g_vram_win, -1, 0);
    if (!g_vram_ren) {
        SDL_DestroyWindow(g_vram_win);
        g_vram_win = NULL;
        return;
    }
    g_vram_tex = SDL_CreateTexture(g_vram_ren, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, 1200, 760);
    g_vram_tex_w = 1200;
    g_vram_tex_h = 760;
}

/* SDL window ID of the viewer (-1 when closed). Lets the event loop
   rebuild/destroy it when the user closes the window via the WM. */
int VDP_ViewerWindowID(void) {
    return g_vram_win ? (int)SDL_GetWindowID(g_vram_win) : -1;
}

/* Draw one palette section: 4 lines x 16 colors as SQUARE swatches that
   never stretch and adapt like the tile grid (integer swatch size), lines
   stacked with 1 px separators, per-line labels just left of the section
   (e.g. "PAL P0", "P1".."P3"). */
static void render_pal_section(uint32_t *px, int stride, int pitch, int ww,
                               int wh, int sx, int sy, int cell,
                               const uint32_t rgba[64], const char *cap,
                               char lch) {
    int pitch_h = cell + 1;                     /* line pitch (1 px sep) */
    for (int li = 0; li < 4; li++) {
        int ry = sy + li * pitch_h;
        for (int i = 0; i < 16; i++) {
            int rx = sx + i * (cell + 1);
            uint32_t c = rgba[li * 16 + i];
            for (int y = ry; y < ry + cell; y++) {
                for (int x = rx; x < rx + cell; x++) {
                    if (x >= 0 && x < ww && y >= 0 && y < wh) {
                        px[y * stride + x] = c;
                    }
                }
            }
        }
        if (li < 3) {                           /* separator under the line */
            int y = ry + cell;
            for (int x = sx; x < sx + 16 * (cell + 1); x++) {
                if (x >= 0 && x < ww && y >= 0 && y < wh) {
                    px[y * stride + x] = 0xFF101010;
                }
            }
        }
        char lab[16];
        if (li == 0) snprintf(lab, sizeof lab, "%s %c0", cap, lch);
        else         snprintf(lab, sizeof lab, "%c%d", lch, li);
        font8x8_blit_shadow(px, pitch, ww, wh, sx - 44,
                            ry + (cell - 8) / 2, lab, 0xFFC0C0C0);
    }
}

static void render_vram_viewer(void) {
    if (!g_vram_win) return;

    int ww, wh;
    SDL_GetWindowSize(g_vram_win, &ww, &wh);
    if (ww < 120 || wh < 120) return;

    int avail_w = ww - VRAM_VIEW_LABEL;
    int avail_h = wh - VRAM_VIEW_HDR - VRAM_VIEW_STRIP;
    if (avail_w < 16 || avail_h < 16) return;

    /* Largest integer cell (8..64 px) whose column/row split still fits the
       whole sheet in the grid area. Small windows get small cells and more
       rows; maximized windows get big cells and fewer rows. */
    int cell = 8, cols = 1, rows = VRAM_VIEW_TILES;
    for (int c = 8; c <= 64; c += 8) {
        int cc = avail_w / c;
        if (cc < 2) break;
        int rr = (VRAM_VIEW_TILES + cc - 1) / cc;
        if (rr * c > avail_h) break;
        cell = c;
        cols = cc;
        rows = rr;
    }
    int grid_w = cols * cell;
    int grid_h = rows * cell;
    int gx = VRAM_VIEW_LABEL + (avail_w - grid_w) / 2;
    int gy = VRAM_VIEW_HDR + (avail_h - grid_h) / 2;
    /* When the window is too small for even the 8 px cell the sheet would
       overflow the area: clamp the grid origin so no write ever leaves the
       texture (the extra rows are simply clipped at the bottom). */
    if (gx < VRAM_VIEW_LABEL) gx = VRAM_VIEW_LABEL;
    if (gy < VRAM_VIEW_HDR) gy = VRAM_VIEW_HDR;

    /* Keep the backing texture at the window size so RenderCopy is 1:1. */
    if (!g_vram_tex || g_vram_tex_w != ww || g_vram_tex_h != wh) {
        if (g_vram_tex) SDL_DestroyTexture(g_vram_tex);
        g_vram_tex = SDL_CreateTexture(g_vram_ren, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING, ww, wh);
        g_vram_tex_w = ww;
        g_vram_tex_h = wh;
        if (!g_vram_tex) return;
    }

    void *pixels;
    int pitch;
    SDL_LockTexture(g_vram_tex, NULL, &pixels, &pitch);
    uint32_t *px = (uint32_t *)pixels;
    int stride = pitch / 4;

    for (int i = 0; i < ww * wh; i++) px[i] = 0xFF202020;

    /* OSD band: current plane bases + scroll values the renderer uses */
    {
        uint32_t plane_a = ((uint32_t)(vdp.registers[2] & 0x38)) << 10;
        uint32_t plane_b = ((uint32_t)(vdp.registers[4] & 0x07)) << 13;
        plane_a &= VRAM_SIZE - 1;
        plane_b &= VRAM_SIZE - 1;
        const uint8_t *h = &ram[v_hscrolltablebuffer];
        int16_t ax = (int16_t)((uint16_t)h[0] | ((uint16_t)h[1] << 8));
        int16_t bx = (int16_t)((uint16_t)h[2] | ((uint16_t)h[3] << 8));
        char buf[128];
        snprintf(buf, sizeof buf,
                 "A=$%04X B=$%04X R10=$%02X  AX=%d AY=%d BX=%d BY=%d",
                 plane_a, plane_b, vdp.registers[16] & 0xFF,
                 ax, (int16_t)v_scrposy_vdp, bx, (int16_t)v_bgscrposy_vdp);
        font8x8_blit_shadow(px, pitch, ww, wh, 2, 2, buf, 0xFFC0C0C0);
    }

    /* Checker background inside the grid area */
    for (int y = gy; y < gy + grid_h && y < wh; y++) {
        for (int x = gx; x < gx + grid_w && x < ww; x++) {
            int cc = ((x >> 2) + (y >> 2)) & 1;
            px[y * stride + x] = cc ? 0xFF3A3A3A : 0xFF525252;
        }
    }

    /* All 2048 tiles; the wall of columns reorganizes to the window size */
    int scale = cell / 8;
    for (int t = 0; t < VRAM_VIEW_TILES; t++) {
        int col = t % cols;
        int row = t / cols;
        int ox0 = gx + col * cell;
        int oy0 = gy + row * cell;
        const uint8_t *td = &vdp.vram[t * 32];

        for (int ty = 0; ty < 8; ty++) {
            for (int tx = 0; tx < 8; tx++) {
                int idx = (tx & 1) ? (td[ty * 4 + (tx >> 1)] & 0xF)
                                   : ((td[ty * 4 + (tx >> 1)] >> 4) & 0xF);
                if (idx == 0) continue; /* transparent -> checker stays */

                uint32_t c = MD_ColorToRGBA(palette_main[idx]);
                for (int s = 0; s < scale; s++) {
                    for (int r = 0; r < scale; r++) {
                        int xx = ox0 + tx * scale + s;
                        int yy = oy0 + ty * scale + r;
                        if (xx >= 0 && yy >= 0 && xx < ww && yy < wh) {
                            px[yy * stride + xx] = c;
                        }
                    }
                }
            }
        }
    }

    /* Grid lines at cell boundaries */
    for (int y = gy; y < gy + grid_h && y < wh; y++) {
        for (int x = gx; x < gx + grid_w && x < ww; x++) {
            if ((x - gx) % cell == 0 || (y - gy) % cell == 0) {
                px[y * stride + x] = 0xFF101010;
            }
        }
    }

    /* Left margin: VRAM address of the first tile of each row */
    char buf[16];
    for (int r = 0; r < rows; r++) {
        int tile0 = r * cols;
        if (tile0 >= VRAM_VIEW_TILES) break;
        snprintf(buf, sizeof buf, "$%04X", tile0 * 32);
        font8x8_blit_shadow(px, pitch, ww, wh, 6,
                            gy + r * cell + (cell - 8) / 2, buf, 0xFFA0A0A0);
    }
    for (int y = VRAM_VIEW_HDR; y < wh; y++) {
        px[y * stride + (VRAM_VIEW_LABEL - 2)] = 0xFF4A4A4A;
    }

    /* Palette (bottom): PAL = palette_main (what the renderer uses), CRAM =
       raw CRAM. Each is a 4x16 block of SQUARE swatches that never stretch
       and adapt like the tile grid (integer swatch size 8..16 px); PAL and
       CRAM sit side by side with a bounded gap. */
    {
        uint32_t pal_rgba[64], cram_rgba[64];
        for (int i = 0; i < 64; i++) {
            pal_rgba[i] = MD_ColorToRGBA(palette_main[i]);
            cram_rgba[i] = MD_ColorToRGBA(vdp.cram[i]);
        }
        int cell = 16;                          /* swatch edge, 8..16 px */
        while (cell > 8 && 2 * 16 * cell + 52 > avail_w) cell -= 4;
        int sec_w = 16 * cell;
        int gap = avail_w - 2 * sec_w;
        if (gap > 80) gap = 80;
        if (gap < 52) gap = 52;
        int total = 2 * sec_w + gap;
        int left = VRAM_VIEW_LABEL + (avail_w - total) / 2;
        int sec_h = 4 * (cell + 1) - 1;
        int strip_top = wh - VRAM_VIEW_STRIP;
        int sy = strip_top + (VRAM_VIEW_STRIP - sec_h) / 2;
        render_pal_section(px, stride, pitch, ww, wh, left, sy, cell,
                           pal_rgba, "PAL", 'P');
        render_pal_section(px, stride, pitch, ww, wh,
                           left + sec_w + gap, sy, cell,
                           cram_rgba, "CRAM", 'C');
    }

    SDL_UnlockTexture(g_vram_tex);

    SDL_SetRenderDrawColor(g_vram_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_vram_ren);
    SDL_RenderCopy(g_vram_ren, g_vram_tex, NULL, NULL);
    SDL_RenderPresent(g_vram_ren);
}

/* ============================================================================
   Debug collision overlay. Activar con SONIC_DEBUG_COLLISION=1.
   ============================================================================ */
static int _dbg_col_init = -1;
static int _dbg_col_on = 0;
static int _dbg_col_fill = 0;   /* SONIC_DEBUG_COLLISION=fill → relleno, no borde */

static void debug_collision_overlay(uint32_t *pix) {
    if (_dbg_col_init < 0) {
        _dbg_col_init = 1;
        const char *v = getenv("SONIC_DEBUG_COLLISION");
        if (v) {
            _dbg_col_on = 1;
            if (v[0] == 'f' || v[0] == 'F') _dbg_col_fill = 1;
        }
    }
    if (!_dbg_col_on) return;
    if (!col_index_ptr) return;
    if (!Col_AngleMap || !Col_CollArray1 || !Col_CollArray2) return;

    int cam_x = (int16_t)v_screenposx;
    int cam_y = (int16_t)v_screenposy;
    uint8_t *player = RAM_ADDR(v_player);

    int cell_x0 = (cam_x - 16) & ~0xF;
    int cell_y0 = (cam_y - 16) & ~0xF;
    int cells_w = (320 + 64) / 16 + 2;
    int cells_h = (224 + 64) / 16 + 2;

    for (int cy = 0; cy < cells_h; cy++) {
        for (int cx = 0; cx < cells_w; cx++) {
            int world_x = cell_x0 + cx * 16;
            int world_y = cell_y0 + cy * 16;
            int screen_x0 = world_x - cam_x;
            int screen_y0 = world_y - cam_y;

            if (screen_x0 + 16 < 0 || screen_x0 >= SCREEN_WIDTH) continue;
            if (screen_y0 + 16 < 0 || screen_y0 >= SCREEN_HEIGHT) continue;

            uint8_t *a1;
            uint16_t word;
            FindNearestTile((int16_t)(world_y + 8), (int16_t)(world_x + 8),
                            player, &a1, &word);

            uint16_t block_id = word & 0x7FF;
            if (block_id == 0) continue;

            int s_top = (word >> 13) & 1;
            int s_lr  = (word >> 14) & 1;
            int s_bot = (word >> 15) & 1;
            int xflip = (word >> 11) & 1;
            int yflip = (word >> 12) & 1;

            /* --- Borde de la celda 16x16 (referencia de la rejilla) --- */
            uint32_t border_color;
            if (s_top && s_lr && s_bot) border_color = 0xFF505050;
            else if (s_top)             border_color = 0xFF008000;
            else if (s_lr)              border_color = 0xFF000080;
            else if (s_bot)             border_color = 0xFF808000;
            else                        border_color = 0xFF400000;

            if (_dbg_col_fill) {
                for (int j = 0; j < 16; j++) {
                    int y = screen_y0 + j;
                    if (y < 0 || y >= SCREEN_HEIGHT) continue;
                    for (int i = 0; i < 16; i++) {
                        int x = screen_x0 + i;
                        if (x < 0 || x >= SCREEN_WIDTH) continue;
                        if (i == 0 || i == 15 || j == 0 || j == 15)
                            pix[y * SCREEN_WIDTH + x] = border_color;
                    }
                }
            } else {
                for (int i = 0; i < 16; i++) {
                    int x1 = screen_x0 + i, y1 = screen_y0;
                    int x2 = screen_x0, y2 = screen_y0 + i;
                    if (x1 >= 0 && x1 < SCREEN_WIDTH && y1 >= 0 && y1 < SCREEN_HEIGHT)
                        pix[y1 * SCREEN_WIDTH + x1] = border_color;
                    if (x2 >= 0 && x2 < SCREEN_WIDTH && y2 >= 0 && y2 < SCREEN_HEIGHT)
                        pix[y2 * SCREEN_WIDTH + x2] = border_color;
                }
            }

            /* --- Perfil REAL de la colisión --- */
            uint8_t hmap_id = col_index_ptr[block_id];
            if (hmap_id == 0) continue;

            /* Ángulo (afectado por xflip/yflip) */
            uint8_t angle = Col_AngleMap[hmap_id];
            if (xflip) angle = (uint8_t)(0 - angle);
            if (yflip) {
                angle = (uint8_t)(angle + 0x40);
                angle = (uint8_t)(0 - angle);
                angle = (uint8_t)(angle - 0x40);
            }

            /* Color según ángulo:
               amarillo = plano, rojo = boca abajo, cian = inclinado */
            uint32_t line_color;
            if (angle == 0)          line_color = 0xFFFFFF00;  /* plano */
            else if (angle == 0x80)  line_color = 0xFFFF4040;  /* boca abajo */
            else                     line_color = 0xFF00FFFF;  /* inclinado */

            /* --- Perfil de SUELO (CollArray1) --- */
            if (s_top) {
                const uint8_t *hmap = &Col_CollArray1[hmap_id * 16];
                for (int col = 0; col < 16; col++) {
                    int c = xflip ? (15 - col) : col;
                    int8_t h = (int8_t)hmap[c];
                    int py;
                    if (h == 0x10) {
                        py = screen_y0;               /* max floor = tope de la celda */
                    } else if (h > 0) {
                        py = screen_y0 + (0xF - h);   /* superficie exacta */
                    } else {
                        continue;                      /* 0 = vacío, <0 = techo */
                    }
                    int px = screen_x0 + col;
                    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT)
                        pix[py * SCREEN_WIDTH + px] = line_color;
                }
            }

            /* --- Perfil de PARED (CollArray2) --- */
            if (s_lr) {
                const uint8_t *hmap = &Col_CollArray2[hmap_id * 16];
                for (int row = 0; row < 16; row++) {
                    int r = yflip ? (15 - row) : row;
                    int8_t h = (int8_t)hmap[r];
                    int px;
                    if (h == 0x10) {
                        px = screen_x0;               /* max = borde izquierdo */
                    } else if (h > 0) {
                        px = screen_x0 + (0xF - h);   /* superficie exacta */
                    } else {
                        continue;
                    }
                    int py = screen_y0 + row;
                    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT)
                        pix[py * SCREEN_WIDTH + px] = line_color;
                }
            }
        }
    }

    /* --- Hitbox de Sonic --- */
    int sw = (int8_t)obWidth(player);
    int sh = (int8_t)obHeight(player);
    int son_x = (int16_t)obX(player) - cam_x;
    int son_y = (int16_t)obY(player) - cam_y;

    int hb_left   = son_x - sw;
    int hb_right  = son_x + sw - 1;
    int hb_top    = son_y - sh;
    int hb_bottom = son_y + sh - 1;

    for (int i = hb_left; i <= hb_right; i++) {
        if (i < 0 || i >= SCREEN_WIDTH) continue;
        if (hb_top >= 0 && hb_top < SCREEN_HEIGHT)
            pix[hb_top * SCREEN_WIDTH + i] = 0xFFFF2020;
        if (hb_bottom >= 0 && hb_bottom < SCREEN_HEIGHT)
            pix[hb_bottom * SCREEN_WIDTH + i] = 0xFFFF2020;
    }
    for (int j = hb_top; j <= hb_bottom; j++) {
        if (j < 0 || j >= SCREEN_HEIGHT) continue;
        if (hb_left >= 0 && hb_left < SCREEN_WIDTH)
            pix[j * SCREEN_WIDTH + hb_left] = 0xFFFF2020;
        if (hb_right >= 0 && hb_right < SCREEN_WIDTH)
            pix[j * SCREEN_WIDTH + hb_right] = 0xFFFF2020;
    }

    /* --- Origen de FindFloor: (obX, obY + obHeight) --- */
    int feet_x = son_x;
    int feet_y = son_y + sh;
    for (int i = -4; i <= 4; i++) {
        int px1 = feet_x + i, py1 = feet_y;
        int px2 = feet_x, py2 = feet_y + i;
        if (px1 >= 0 && px1 < SCREEN_WIDTH && py1 >= 0 && py1 < SCREEN_HEIGHT)
            pix[py1 * SCREEN_WIDTH + px1] = 0xFFFFFFFF;
        if (px2 >= 0 && px2 < SCREEN_WIDTH && py2 >= 0 && py2 < SCREEN_HEIGHT)
            pix[py2 * SCREEN_WIDTH + px2] = 0xFFFFFFFF;
    }

    /* --- Origen de Sonic (obX, obY) como referencia --- */
    if (son_x >= 0 && son_x < SCREEN_WIDTH && son_y >= 0 && son_y < SCREEN_HEIGHT)
        pix[son_y * SCREEN_WIDTH + son_x] = 0xFF00FF00;
}

void VDP_RenderFrame(SDL_Renderer *renderer) {
    /* Texture recreada cuando cambia el ancho lógico (toggle widescreen). */
    static int tex_w = 0;
    if (!vdp.framebuffer || tex_w != g_render_w) {
        if (vdp.framebuffer) SDL_DestroyTexture(vdp.framebuffer);
        vdp.framebuffer = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            g_render_w, SCREEN_HEIGHT);
        tex_w = g_render_w;
    }

    /* Buffer interno persistente donde renderizamos todo (mismo esquema
       que antes, pero sin SDL_LockTexture en el camino caliente).
       pitch es "ficticio": los planos se dibujan con stride = pitch/4,
       por eso vale g_render_w * 4 para que stride == g_render_w. */
    static uint32_t frame_buf[PP_MAX_W * PP_MAX_H];
    uint32_t *pix = frame_buf;
    int pitch = g_render_w * 4;   /* dummy, para render_plane_scanline */
    int stride = g_render_w;

    /* Clear to black (color de fondo real de la Mega Drive) */
    uint16_t bg_cram_index = vdp.registers[7] & 0x3F;
    uint32_t bg_color = MD_ColorToRGBA(vdp.cram[bg_cram_index]);

    int total_px = g_render_w * SCREEN_HEIGHT;
    for (int i = 0; i < total_px; i++) {
        pix[i] = bg_color;
    }

    /* ---------------------------------------------------------------------
       Dynamic plane addresses (registros $02 y $04).
       --------------------------------------------------------------------- */
    uint32_t plane_a_addr = ((uint32_t)(vdp.registers[2] & 0x38)) << 10;
    uint32_t plane_b_addr = ((uint32_t)(vdp.registers[4] & 0x07)) << 13;
    if (plane_a_addr >= VRAM_SIZE) plane_a_addr &= (VRAM_SIZE - 1);
    if (plane_b_addr >= VRAM_SIZE) plane_b_addr &= (VRAM_SIZE - 1);

    int16_t fg_scroll_y = (int16_t)v_scrposy_vdp;
    int16_t bg_scroll_y = (int16_t)v_bgscrposy_vdp;

    /* Sprite list metadata. */
    uint8_t *table = &ram[v_spritetablebuffer];
    int n = v_spritecount;
    if (n > 80) n = 80;
    if (n > sprites_max) n = sprites_max;

    int sy[80], sh[80], sw[80];
    for (int i = 0; i < n; i++) {
        uint8_t *entry = &table[i * 8];
        /* Decodifica Y como int16 con signo: evita el wrap-around de
         *      9 bits para objetos que están por encima o por debajo de la
         *      pantalla. Mismo valor que el original dentro de [-128, 383]. */
        sy[i] = (int)(int16_t)(entry[0] | (entry[1] << 8)) - 0x80;
        sh[i] = ((entry[2] & 0x0F) + 1) * 8;
        sw[i] = ((entry[2] >> 4) + 1) * 8;
    }

    for (int row = 0; row < SCREEN_HEIGHT; row++) {
        const uint8_t *h = &ram[v_hscrolltablebuffer + row * 4];
        int16_t fg_scroll_x = (int16_t)((uint16_t)h[0] | ((uint16_t)h[1] << 8));
        int16_t bg_scroll_x = (int16_t)((uint16_t)h[2] | ((uint16_t)h[3] << 8));

        /* Sprites que cruzan este scanline, en orden de tabla. El límite de
           20 sprites/línea y 320px se mantiene IGUAL al hardware (no lo
           ensanchamos con widescreen, igual que en una Genesis real). */
        int list[20], list_len = 0;
        int px_budget = 0;
        for (int i = 0; i < n && list_len < 20; i++) {
            if (row >= sy[i] && row < sy[i] + sh[i]) {
                if (px_budget + sw[i] > 320) break;
                px_budget += sw[i];
                list[list_len++] = i;
            }
        }

        /* Stack de prioridad MD (bajo → alto):
             1. Plane B pri 0
             2. Plane A pri 0
             3. Sprites pri 0
             4. Plane B pri 1
             5. Plane A pri 1
             6. Sprites pri 1
           Nota: en el ASM "FG"/"BG" están invertidos respecto a plane A/B. */
        render_plane_scanline(&vdp.vram[plane_b_addr], palette_main,
                              bg_scroll_x, bg_scroll_y, row, pix, pitch, 0, 1);
        render_plane_scanline(&vdp.vram[plane_a_addr], palette_main,
                              fg_scroll_x, fg_scroll_y, row, pix, pitch, 0, 0);

        /* Pass 0: sprites no-priority, blit tail-to-head para que la primera
           entrada de la tabla quede encima de las siguientes. */
        for (int k = list_len - 1; k >= 0; k--) {
            int i = list[k];
            uint8_t *entry = &table[i * 8];
            uint16_t pattern = (uint16_t)(entry[4] | (entry[5] << 8));
            if (((pattern >> 15) & 1) != 0) continue;
            int y = sy[i];
            int height_tiles = (entry[2] & 0x0F) + 1;
            int width_tiles  = (entry[2] >> 4) + 1;
            int tile         = pattern & 0x7FF;
            int pal_line     = (pattern >> 13) & 3;
            int x            = (int)(int16_t)(entry[6] | (entry[7] << 8)) - 0x80;

            int xflip = (pattern >> 11) & 1;
            int yflip = (pattern >> 12) & 1;

            int dy = row - y;
            int ty, prow;
            if (yflip) {
                dy = height_tiles * 8 - 1 - dy;
                ty = dy >> 3;
                prow = dy & 7;
            } else {
                ty = dy >> 3;
                prow = dy & 7;
            }
            if (ty < 0 || ty >= height_tiles) continue;

            for (int tx = 0; tx < width_tiles; tx++) {
                int txx = xflip ? (width_tiles - 1 - tx) : tx;
                int tile_idx = tile + txx * height_tiles + ty;
                if (tile_idx >= 0x800) continue;
                const uint8_t *r = &vdp.vram[tile_idx * 32 + prow * 4];

                /* Desplazamos el sprite al área widescreen. El "centro" (los
                   320 px originales) empieza en x = g_render_left. */
                int sx0 = x + tx * 8 + g_render_left;

                for (int col = 0; col < 8; col++) {
                    int color_idx = (col & 1) ? (r[col >> 1] & 0xF)
                                              : ((r[col >> 1] >> 4) & 0xF);
                    if (color_idx == 0) continue;
                    int sx = xflip ? (sx0 + 7 - col) : (sx0 + col);
                    if (sx < 0 || sx >= g_render_w) continue;
                    pix[row * stride + sx] =
                        MD_ColorToRGBA(palette_main[pal_line * 16 + color_idx]);
                }
            }
        }

        render_plane_scanline(&vdp.vram[plane_b_addr], palette_main,
                              bg_scroll_x, bg_scroll_y, row, pix, pitch, 1, 1);
        render_plane_scanline(&vdp.vram[plane_a_addr], palette_main,
                              fg_scroll_x, fg_scroll_y, row, pix, pitch, 1, 0);

        /* Pass 1: sprites priority, encima de todo. */
        for (int k = list_len - 1; k >= 0; k--) {
            int i = list[k];
            uint8_t *entry = &table[i * 8];
            uint16_t pattern = (uint16_t)(entry[4] | (entry[5] << 8));
            if (((pattern >> 15) & 1) != 1) continue;
            int y = sy[i];
            int height_tiles = (entry[2] & 0x0F) + 1;
            int width_tiles  = (entry[2] >> 4) + 1;
            int tile         = pattern & 0x7FF;
            int pal_line     = (pattern >> 13) & 3;
            int x            = (int)(int16_t)(entry[6] | (entry[7] << 8)) - 0x80;

            int xflip = (pattern >> 11) & 1;
            int yflip = (pattern >> 12) & 1;

            int dy = row - y;
            int ty, prow;
            if (yflip) {
                dy = height_tiles * 8 - 1 - dy;
                ty = dy >> 3;
                prow = dy & 7;
            } else {
                ty = dy >> 3;
                prow = dy & 7;
            }
            if (ty < 0 || ty >= height_tiles) continue;

            for (int tx = 0; tx < width_tiles; tx++) {
                int txx = xflip ? (width_tiles - 1 - tx) : tx;
                int tile_idx = tile + txx * height_tiles + ty;
                if (tile_idx >= 0x800) continue;
                const uint8_t *r = &vdp.vram[tile_idx * 32 + prow * 4];
                int sx0 = x + tx * 8 + g_render_left;

                for (int col = 0; col < 8; col++) {
                    int color_idx = (col & 1) ? (r[col >> 1] & 0xF)
                                              : ((r[col >> 1] >> 4) & 0xF);
                    if (color_idx == 0) continue;
                    int sx = xflip ? (sx0 + 7 - col) : (sx0 + col);
                    if (sx < 0 || sx >= g_render_w) continue;
                    pix[row * stride + sx] =
                        MD_ColorToRGBA(palette_main[pal_line * 16 + color_idx]);
                }
            }
        }
    }

    /* Optional test overlay (barras verdes de debug) */
    if (vdp_test_counter >= 0) {
        int w = g_render_w - 20;
        int bar_w = (vdp_test_counter * w) / 600;
        if (bar_w > w) bar_w = w;
        for (int y = 20; y < 60 && y < SCREEN_HEIGHT; y++) {
            for (int x = 10; x < 10 + bar_w && x < g_render_w; x++) {
                pix[y * stride + x] = 0xFF00FF00;
            }
        }
        if (vdp_test_counter % 60 < 30) {
            pix[0] = 0xFFFFFFFF;
        }
    }

    /* Copia persistente para VDP_SaveScreenshot (row-by-row por stride). */
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        memcpy(&last_frame[y * g_render_w],
               &pix[y * stride],
               (size_t)g_render_w * 4);
    }

    /* Overlay de depuración de colisión (usa SCREEN_WIDTH internamente;
       con widescreen pinta solo los 320 px centrales, lo cual es correcto
       porque las coords de Sonic son relativas al viewport original). */
    debug_collision_overlay(pix);

        /* Post-proceso: aplicar filtros. */
    const uint32_t *final = PP_Apply(pix, g_render_w, SCREEN_HEIGHT, &g_settings);

    /* Upload al texture SDL row-by-row (por si pitch de SDL no coincide
       con g_render_w * 4). */
    void *pixels;
    int sdl_pitch;
    SDL_LockTexture(vdp.framebuffer, NULL, &pixels, &sdl_pitch);
    {
        int dst_stride = sdl_pitch / 4;
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            memcpy((uint32_t *)pixels + (size_t)y * dst_stride,
                   final + (size_t)y * g_render_w,
                   (size_t)g_render_w * 4);
        }
    }
    SDL_UnlockTexture(vdp.framebuffer);

    /* Present */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, vdp.framebuffer, NULL, NULL);
    SDL_RenderPresent(renderer);

    /* Visores de depuración (no-op si están cerrados) */
    render_vram_viewer();
    PlaneView_Render();
    extern void ObjView_Render(void);
    extern void RamView_Render(void);
    ObjView_Render();
    RamView_Render();
}

void VDP_SaveScreenshot(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", g_render_w, SCREEN_HEIGHT);
    for (int i = 0; i < g_render_w * SCREEN_HEIGHT; i++) {
        uint32_t c = last_frame[i];
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >> 8) & 0xFF;
        uint8_t b = (c >> 0) & 0xFF;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
}
