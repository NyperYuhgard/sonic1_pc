#include "tas.h"
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

/* Banco de escritura actual. Los writers públicos lo consultan; las
   variantes *_Bank lo ignoran. Default 0 = MD-visible. */
int g_vdp_write_bank = 0;

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
    if (VRAM_BANK_COUNT > 1 && vdp.vram_ext == NULL) {
        size_t sz = (size_t)(VRAM_BANK_COUNT - 1) * VRAM_BANK_SIZE;
        vdp.vram_ext = (uint8_t *)calloc(1, sz);
        if (!vdp.vram_ext) {
            fprintf(stderr,
                    "[VDP] WARN: calloc(%zu) failed for %d VRAM banks. "
                    "Falling back to single-bank mode.\n",
                    sz, VRAM_BANK_COUNT);
        } else {
            fprintf(stderr,
                    "[VDP] Allocated %d VRAM banks (%zu bytes)\n",
                    VRAM_BANK_COUNT, sz);
        }
    }
    g_vdp_write_bank = 0;
    VDP_Reset();
}

void VDP_Reset(void) {
    /* Default VDP register values (from VDPSetupArray in sonic.asm)
       These are the RAW register values, NOT VDP command words. */
    vdp.registers[0]  = 0x0004;  /* mode 1: 8-colour mode */
    vdp.registers[1]  = 0x0074;  /* Mega Drive mode, DMA enable, display on */
    vdp.registers[2]  = 0x0030;  /* FG nametable at $C000 */
    vdp.registers[3]  = 0x0028;  /* Window nametable at $A000 */
    vdp.registers[4]  = 0x0007;  /* BG nametable at $E000 */
    vdp.registers[5]  = 0x007F;  /* Sprite table at $F800 */
    vdp.registers[6]  = 0x0000;
    vdp.registers[7]  = 0x0000;  /* BG colour */
    vdp.registers[8]  = 0x0000;
    vdp.registers[9]  = 0x0000;
    vdp.registers[10] = 0x00FF;  /* HBlank rate */
    vdp.registers[11] = 0x0000;  /* Full screen scroll */
    vdp.registers[12] = 0x0081;  /* 40 cell display */
    vdp.registers[13] = 0x0037;  /* H-scroll table at $DC00 */
    vdp.registers[14] = 0x0000;
    vdp.registers[15] = 0x0001;  /* VDP increment = 2 */
    vdp.registers[16] = 0x0001;  /* 64 cell h-scroll size */
    vdp.registers[17] = 0x0000;
    vdp.registers[18] = 0x0000;
    vdp.registers[19] = 0x00FF;  /* DMA length */
    vdp.registers[20] = 0x0000;  /* DMA source low */
    vdp.registers[21] = 0x0000;  /* DMA source high */
    vdp.registers[22] = 0x0080;  /* DMA mode */

    memset(vdp.vram, 0, VRAM_BANK_SIZE);
    memset(vdp.cram, 0, sizeof(vdp.cram));
    memset(vdp.vsram, 0, sizeof(vdp.vsram));
    vdp.status = 0;
    vdp.vdp_cmd = 0;
}

/* ======================================================================
   VRAM BANKS: accesores + estado de banco de escritura
   ====================================================================== */

void VDP_SetWriteBank(int bank) {
    if (bank < 0) bank = 0;
    if (bank >= VRAM_BANK_COUNT) bank = VRAM_BANK_COUNT - 1;
    g_vdp_write_bank = bank;
}

int VDP_GetWriteBank(void) {
    return g_vdp_write_bank;
}

uint8_t *VDP_VRAM_BankW(int bank) {
    if (bank <= 0) return vdp.vram;
    if (VRAM_BANK_COUNT <= 1 || vdp.vram_ext == NULL) return vdp.vram;
    if (bank >= VRAM_BANK_COUNT) bank = VRAM_BANK_COUNT - 1;
    return &vdp.vram_ext[(size_t)(bank - 1) * VRAM_BANK_SIZE];
}
const uint8_t *VDP_VRAM_Bank(int bank) {
    return VDP_VRAM_BankW(bank);
}

void VDP_WriteTile(int bank, int tile_index, const uint8_t tile[32]) {
    if (tile_index < 0 || tile_index >= (int)(VRAM_BANK_SIZE / 32)) return;
    memcpy(VDP_VRAM_BankW(bank) + (size_t)tile_index * 32, tile, 32);
}

/* ======================================================================
   Writers con banco explícito (ignoran g_vdp_write_bank)
   ====================================================================== */

void VDP_WriteVRAM_Bank(int bank, const uint8_t *src,
                        uint32_t vram_addr, uint32_t len) {
    uint8_t *base = VDP_VRAM_BankW(bank);
    if (vram_addr + len > VRAM_BANK_SIZE) {
        len = VRAM_BANK_SIZE - vram_addr;
    }
    memcpy(base + vram_addr, src, len);
}

void VDP_FillVRAM_Bank(int bank, uint8_t byte,
                       uint32_t vram_addr, uint32_t len) {
    uint8_t *base = VDP_VRAM_BankW(bank);
    if (vram_addr + len > VRAM_BANK_SIZE) {
        len = VRAM_BANK_SIZE - vram_addr;
    }
    memset(base + vram_addr, byte, len);
}

void VDP_CopyTilemapToVRAM_Bank(int bank, const uint16_t *source,
                                uint32_t vram_dest, int width, int height) {
    uint8_t *base = VDP_VRAM_BankW(bank);
    for (int row = 0; row < height; row++) {
        uint32_t dest = vram_dest + (row * 128);
        for (int col = 0; col < width; col++) {
            uint32_t addr = dest + (uint32_t)col * 2;
            if (addr + 1 >= VRAM_BANK_SIZE) continue;
            uint16_t tile_entry = source[row * width + col];
            base[addr]     = (uint8_t)(tile_entry >> 8);
            base[addr + 1] = (uint8_t)(tile_entry & 0xFF);
        }
    }
}

/* ======================================================================
   Writers públicos: escriben en g_vdp_write_bank (default 0).
   Si quieres forzar un banco concreto sin tocar el estado global, usa
   las variantes *_Bank de arriba.
   ====================================================================== */

void VDP_WriteVRAM(const uint8_t *src, uint32_t vram_addr, uint32_t len) {
    VDP_WriteVRAM_Bank(g_vdp_write_bank, src, vram_addr, len);
}

void VDP_WriteCRAM(const uint8_t *src, uint32_t cram_addr, uint32_t len) {
    uint32_t words = len / 2;
    for (uint32_t i = 0; i < words && (cram_addr + i) < CRAM_SIZE; i++) {
        uint16_t color = ((uint16_t)src[i * 2] << 8) | src[i * 2 + 1];
        vdp.cram[cram_addr + i] = color;
    }
}

void VDP_FillVRAM(uint8_t byte, uint32_t vram_addr, uint32_t len) {
    VDP_FillVRAM_Bank(g_vdp_write_bank, byte, vram_addr, len);
}

void VDP_CopyTilemapToVRAM(const uint16_t *source, uint32_t vram_dest,
                            int width, int height) {
    VDP_CopyTilemapToVRAM_Bank(g_vdp_write_bank, source, vram_dest, width, height);
}

void VDP_SetRegister(uint8_t reg, uint16_t value) {
    if (reg < 24) {
        vdp.registers[reg] = value;
    }
}

/* ClearScreen siempre toca el banco 0: los nametables (vram_fg, vram_bg)
   y los buffers de sprite/hscroll viven ahí. Si esto consultara
   g_vdp_write_bank, cambiar el banco de escritura rompería la pantalla. */
void VDP_ClearScreen(void) {
    VDP_FillVRAM_Bank(0, 0, vram_fg, 64 * 32 * 2);
    VDP_FillVRAM_Bank(0, 0, vram_bg, 64 * 32 * 2);

    v_scrposy_vdp    = 0;
    v_bgscrposy_vdp  = 0;
    v_scrposx_vdp    = 0;
    v_bgscrposx_vdp  = 0;
    v_bg3scrposy_vdp = 0;
    v_bg3scrposx_vdp = 0;

    memset(&ram[v_spritetablebuffer], 0, 0x400);
    memset(&ram[v_hscrolltablebuffer], 0, 0x400);
}

void VDP_TransferPalette(void) {
    memcpy(palette_main, RAM_ADDR(v_palette), sizeof(palette_main));
}

int VDP_PlaneRows(int plane_b) {
    int height_bits = (vdp.registers[16] >> (plane_b ? 6 : 4)) & 0x3;
    return (height_bits == 1) ? 64
         : (height_bits == 3) ? 128
         : 32;
}
static int vdp_plane_height_mask(int plane_b) {
    return VDP_PlaneRows(plane_b) * 8 - 1;
}

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

/* =================================================================== */
/* Real-time VRAM viewer window                                       */
/* =================================================================== */
#define VRAM_VIEW_TILES  2048 /* tiles $000..$7FF (por banco)              */
#define VRAM_VIEW_LABEL  56   /* left margin for VRAM address labels       */
#define VRAM_VIEW_HDR    16   /* OSD band (debajo de las pestañas)         */
#define VRAM_VIEW_TABS   24   /* altura de la barra de tabs                */
#define VRAM_VIEW_STRIP  67   /* palette: 4 square-swatch palette lines    */

static SDL_Window   *g_vram_win = NULL;
static SDL_Renderer *g_vram_ren = NULL;
static SDL_Texture  *g_vram_tex = NULL;
static int g_vram_tex_w = 0, g_vram_tex_h = 0;

static int g_vram_view_bank = 0;

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

    g_vram_view_bank = 0;
}

int VDP_ViewerWindowID(void) {
    return g_vram_win ? (int)SDL_GetWindowID(g_vram_win) : -1;
}

static void render_pal_section(uint32_t *px, int stride, int pitch, int ww,
                               int wh, int sx, int sy, int cell,
                               const uint32_t rgba[64], const char *cap,
                               char lch) {
    int pitch_h = cell + 1;
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
        if (li < 3) {
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

static int vram_tab_geometry(int ww, int *tab_w, int *x0) {
    if (VRAM_BANK_COUNT <= 0) { *tab_w = 0; *x0 = 0; return 0; }
    int tw = ww / VRAM_BANK_COUNT;
    if (tw > 90) tw = 90;
    if (tw < 20) tw = 20;
    *tab_w = tw;
    int row_w = tw * VRAM_BANK_COUNT;
    *x0 = (row_w < ww) ? (ww - row_w) / 2 : 0;
    return 1;
}

static void draw_vram_tabs(uint32_t *px, int stride, int pitch, int ww, int wh) {
    if (VRAM_BANK_COUNT <= 0) return;
    int tab_w, x0;
    if (!vram_tab_geometry(ww, &tab_w, &x0)) return;

    int tab_h = VRAM_VIEW_TABS - 2;

    for (int y = 0; y < VRAM_VIEW_TABS; y++) {
        for (int x = 0; x < ww; x++) {
            px[y * stride + x] = 0xFF1A1A1A;
        }
    }

    for (int b = 0; b < VRAM_BANK_COUNT; b++) {
        int tx     = x0 + b * tab_w;
        int tx_end = tx + tab_w;
        if (tx >= ww) break;
        if (tx_end > ww) tx_end = ww;

        int active = (b == g_vram_view_bank);
        uint32_t face = active ? 0xFF5050A0 : 0xFF303030;
        uint32_t top  = active ? 0xFFA0A0FF : 0xFF606060;

        for (int y = 0; y < tab_h; y++) {
            for (int x = tx; x < tx_end; x++) {
                px[y * stride + x] = face;
            }
        }
        for (int x = tx; x < tx_end; x++) px[0 * stride + x] = top;
        if (tx_end < ww) {
            for (int y = 0; y < tab_h; y++) px[y * stride + tx_end] = 0xFF101010;
        }

        char lab[16];
        snprintf(lab, sizeof lab, "B%d", b);
        int lw = (int)strlen(lab) * 8;
        int lx = tx + (tab_w - lw) / 2;
        int ly = (tab_h - 8) / 2;
        if (lx < 0) lx = 0;
        font8x8_blit_shadow(px, pitch, ww, wh, lx, ly, lab,
                            active ? 0xFFFFFFFF : 0xFFC0C0C0);
    }
}

static void VDP_ViewerPollClick(void) {
    if (!g_vram_win) return;

    static Uint32 prev_buttons = 0;

    int gx = 0, gy = 0;
    Uint32 buttons = SDL_GetGlobalMouseState(&gx, &gy);

    int now_left  = (buttons & SDL_BUTTON_LMASK) ? 1 : 0;
    int prev_left = (prev_buttons & SDL_BUTTON_LMASK) ? 1 : 0;
    prev_buttons  = buttons;

    /* Solo en el flanco de bajada (transición 0 -> 1) */
    if (!now_left || prev_left) return;

    int wx = 0, wy = 0;
    SDL_GetWindowPosition(g_vram_win, &wx, &wy);
    int mx = gx - wx;
    int my = gy - wy;

    int ww = 0, wh = 0;
    SDL_GetWindowSize(g_vram_win, &ww, &wh);
    if (mx < 0 || mx >= ww || my < 0 || my >= wh) return;

    /* ¿El click cae en la barra de pestañas? */
    if (my >= VRAM_VIEW_TABS) return;

    int tab_w, x0;
    if (!vram_tab_geometry(ww, &tab_w, &x0)) return;

    int b = (mx - x0) / tab_w;
    if (b < 0 || b >= VRAM_BANK_COUNT) return;

    g_vram_view_bank = b;
}

static void render_vram_viewer(void) {
    if (!g_vram_win) return;

    VDP_ViewerPollClick();

    int ww, wh;
    SDL_GetWindowSize(g_vram_win, &ww, &wh);
    if (ww < 120 || wh < 120) return;

    if (g_vram_view_bank >= VRAM_BANK_COUNT) g_vram_view_bank = VRAM_BANK_COUNT - 1;
    if (g_vram_view_bank < 0) g_vram_view_bank = 0;

    int avail_w = ww - VRAM_VIEW_LABEL;
    int avail_h = wh - VRAM_VIEW_TABS - VRAM_VIEW_HDR - VRAM_VIEW_STRIP;
    if (avail_w < 16 || avail_h < 16) return;

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
    int gy = VRAM_VIEW_TABS + VRAM_VIEW_HDR + (avail_h - grid_h) / 2;
    if (gx < VRAM_VIEW_LABEL) gx = VRAM_VIEW_LABEL;
    if (gy < VRAM_VIEW_TABS + VRAM_VIEW_HDR) gy = VRAM_VIEW_TABS + VRAM_VIEW_HDR;

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

    draw_vram_tabs(px, stride, pitch, ww, wh);

    /* OSD band */
    {
        uint32_t plane_a = ((uint32_t)(vdp.registers[2] & 0x38)) << 10;
        uint32_t plane_b = ((uint32_t)(vdp.registers[4] & 0x07)) << 13;
        plane_a &= VRAM_BANK_SIZE - 1;
        plane_b &= VRAM_BANK_SIZE - 1;
        const uint8_t *h = &ram[v_hscrolltablebuffer];
        int16_t ax = (int16_t)((uint16_t)h[0] | ((uint16_t)h[1] << 8));
        int16_t bx = (int16_t)((uint16_t)h[2] | ((uint16_t)h[3] << 8));
        char buf[160];
        snprintf(buf, sizeof buf,
                 "BANK %d/%d   A=$%04X B=$%04X R10=$%02X  AX=%d AY=%d BX=%d BY=%d",
                 g_vram_view_bank, VRAM_BANK_COUNT - 1,
                 plane_a, plane_b, vdp.registers[16] & 0xFF,
                 ax, (int16_t)v_scrposy_vdp, bx, (int16_t)v_bgscrposy_vdp);
        font8x8_blit_shadow(px, pitch, ww, wh, 2, VRAM_VIEW_TABS + 2, buf,
                            0xFFC0C0C0);
    }

    /* Checker background inside the grid area */
    for (int y = gy; y < gy + grid_h && y < wh; y++) {
        for (int x = gx; x < gx + grid_w && x < ww; x++) {
            int cc = ((x >> 2) + (y >> 2)) & 1;
            px[y * stride + x] = cc ? 0xFF3A3A3A : 0xFF525252;
        }
    }

    /* Todos los tiles del banco seleccionado */
    const uint8_t *bank_data = VDP_VRAM_Bank(g_vram_view_bank);
    int scale = cell / 8;
    for (int t = 0; t < VRAM_VIEW_TILES; t++) {
        int col = t % cols;
        int row = t / cols;
        int ox0 = gx + col * cell;
        int oy0 = gy + row * cell;
        const uint8_t *td = &bank_data[t * 32];

        for (int ty = 0; ty < 8; ty++) {
            for (int tx = 0; tx < 8; tx++) {
                int idx = (tx & 1) ? (td[ty * 4 + (tx >> 1)] & 0xF)
                                   : ((td[ty * 4 + (tx >> 1)] >> 4) & 0xF);
                if (idx == 0) continue;

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
    for (int y = VRAM_VIEW_TABS + VRAM_VIEW_HDR; y < wh; y++) {
        px[y * stride + (VRAM_VIEW_LABEL - 2)] = 0xFF4A4A4A;
    }

    /* Palette (bottom) */
    {
        uint32_t pal_rgba[64], cram_rgba[64];
        for (int i = 0; i < 64; i++) {
            pal_rgba[i] = MD_ColorToRGBA(palette_main[i]);
            cram_rgba[i] = MD_ColorToRGBA(vdp.cram[i]);
        }
        int cell2 = 16;
        while (cell2 > 8 && 2 * 16 * cell2 + 52 > avail_w) cell2 -= 4;
        int sec_w = 16 * cell2;
        int gap = avail_w - 2 * sec_w;
        if (gap > 80) gap = 80;
        if (gap < 52) gap = 52;
        int total = 2 * sec_w + gap;
        int left = VRAM_VIEW_LABEL + (avail_w - total) / 2;
        int sec_h = 4 * (cell2 + 1) - 1;
        int strip_top = wh - VRAM_VIEW_STRIP;
        int sy = strip_top + (VRAM_VIEW_STRIP - sec_h) / 2;
        render_pal_section(px, stride, pitch, ww, wh, left, sy, cell2,
                           pal_rgba, "PAL", 'P');
        render_pal_section(px, stride, pitch, ww, wh,
                           left + sec_w + gap, sy, cell2,
                           cram_rgba, "CRAM", 'C');
    }

    SDL_UnlockTexture(g_vram_tex);

    SDL_SetRenderDrawColor(g_vram_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_vram_ren);
    SDL_RenderCopy(g_vram_ren, g_vram_tex, NULL, NULL);
    SDL_RenderPresent(g_vram_ren);
}

void VDP_ViewerHandleEvent(const union SDL_Event *ev) {
    if (!g_vram_win) return;
    Uint32 wid = SDL_GetWindowID(g_vram_win);

    if (ev->type == SDL_WINDOWEVENT &&
        ev->window.windowID == wid &&
        ev->window.event == SDL_WINDOWEVENT_CLOSE) {
        VDP_ToggleVRAMViewer();
        return;
    }

    if (ev->type == SDL_KEYDOWN && ev->key.windowID == wid) {
        switch (ev->key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
                VDP_ToggleVRAMViewer();
                break;
            case SDL_SCANCODE_LEFT:
                if (g_vram_view_bank > 0) g_vram_view_bank--;
                break;
            case SDL_SCANCODE_RIGHT:
                if (g_vram_view_bank < VRAM_BANK_COUNT - 1) g_vram_view_bank++;
                break;
            case SDL_SCANCODE_TAB:
                if (VRAM_BANK_COUNT > 0) {
                    g_vram_view_bank = (g_vram_view_bank + 1) % VRAM_BANK_COUNT;
                }
                break;
            default: break;
        }
        return;
    }

    if (ev->type == SDL_MOUSEBUTTONDOWN &&
        ev->button.windowID == wid &&
        ev->button.button == SDL_BUTTON_LEFT) {
        int ww, wh;
        SDL_GetWindowSize(g_vram_win, &ww, &wh);
        int mx = ev->button.x;
        int my = ev->button.y;
        if (my < VRAM_VIEW_TABS) {
            int tab_w, x0;
            if (vram_tab_geometry(ww, &tab_w, &x0)) {
                int b = (mx - x0) / tab_w;
                if (b >= 0 && b < VRAM_BANK_COUNT) {
                    g_vram_view_bank = b;
                }
            }
        }
    }
}

/* ============================================================================
   Debug collision overlay. Activar con SONIC_DEBUG_COLLISION=1.
   ============================================================================ */
static int _dbg_col_init = -1;
static int _dbg_col_on = 0;
static int _dbg_col_fill = 0;

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

            uint8_t hmap_id = col_index_ptr[block_id];
            if (hmap_id == 0) continue;

            uint8_t angle = Col_AngleMap[hmap_id];
            if (xflip) angle = (uint8_t)(0 - angle);
            if (yflip) {
                angle = (uint8_t)(angle + 0x40);
                angle = (uint8_t)(0 - angle);
                angle = (uint8_t)(angle - 0x40);
            }

            uint32_t line_color;
            if (angle == 0)          line_color = 0xFFFFFF00;
            else if (angle == 0x80)  line_color = 0xFFFF4040;
            else                     line_color = 0xFF00FFFF;

            if (s_top) {
                const uint8_t *hmap = &Col_CollArray1[hmap_id * 16];
                for (int col = 0; col < 16; col++) {
                    int c = xflip ? (15 - col) : col;
                    int8_t h = (int8_t)hmap[c];
                    int py;
                    if (h == 0x10) {
                        py = screen_y0;
                    } else if (h > 0) {
                        py = screen_y0 + (0xF - h);
                    } else {
                        continue;
                    }
                    int px = screen_x0 + col;
                    if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT)
                        pix[py * SCREEN_WIDTH + px] = line_color;
                }
            }

            if (s_lr) {
                const uint8_t *hmap = &Col_CollArray2[hmap_id * 16];
                for (int row = 0; row < 16; row++) {
                    int r = yflip ? (15 - row) : row;
                    int8_t h = (int8_t)hmap[r];
                    int px;
                    if (h == 0x10) {
                        px = screen_x0;
                    } else if (h > 0) {
                        px = screen_x0 + (0xF - h);
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

    if (son_x >= 0 && son_x < SCREEN_WIDTH && son_y >= 0 && son_y < SCREEN_HEIGHT)
        pix[son_y * SCREEN_WIDTH + son_x] = 0xFF00FF00;
}

void VDP_RenderFrame(SDL_Renderer *renderer) {
    static int tex_w = 0;
    if (!vdp.framebuffer || tex_w != g_render_w) {
        if (vdp.framebuffer) SDL_DestroyTexture(vdp.framebuffer);
        vdp.framebuffer = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            g_render_w, SCREEN_HEIGHT);
        tex_w = g_render_w;
    }

    static uint32_t frame_buf[PP_MAX_W * PP_MAX_H];
    uint32_t *pix = frame_buf;
    int pitch = g_render_w * 4;
    int stride = g_render_w;

    Palette_Water_Update();

    uint16_t bg_cram_index = vdp.registers[7] & 0x3F;
    uint32_t bg_color = MD_ColorToRGBA(vdp.cram[bg_cram_index]);

    int total_px = g_render_w * SCREEN_HEIGHT;
    for (int i = 0; i < total_px; i++) pix[i] = bg_color;

    uint32_t plane_a_addr = ((uint32_t)(vdp.registers[2] & 0x38)) << 10;
    uint32_t plane_b_addr = ((uint32_t)(vdp.registers[4] & 0x07)) << 13;
    if (plane_a_addr >= VRAM_BANK_SIZE) plane_a_addr &= (VRAM_BANK_SIZE - 1);
    if (plane_b_addr >= VRAM_BANK_SIZE) plane_b_addr &= (VRAM_BANK_SIZE - 1);

    int16_t fg_scroll_y = (int16_t)v_scrposy_vdp;
    int16_t bg_scroll_y = (int16_t)v_bgscrposy_vdp;

    uint8_t *table = &ram[v_spritetablebuffer];
    int n = v_spritecount;
    if (n > 80) n = 80;
    if (n > sprites_max) n = sprites_max;

    int sy[80], sh[80], sw[80];
    for (int i = 0; i < n; i++) {
        uint8_t *entry = &table[i * 8];
        sy[i] = (int)(int16_t)(entry[0] | (entry[1] << 8)) - 0x80;
        sh[i] = ((entry[2] & 0x0F) + 1) * 8;
        sw[i] = ((entry[2] >> 4) + 1) * 8;
    }

    for (int row = 0; row < SCREEN_HEIGHT; row++) {
        const uint8_t *h = &ram[v_hscrolltablebuffer + row * 4];
        int16_t fg_scroll_x = (int16_t)((uint16_t)h[0] | ((uint16_t)h[1] << 8));
        int16_t bg_scroll_x = (int16_t)((uint16_t)h[2] | ((uint16_t)h[3] << 8));

        const uint16_t *pal_row = palette_main;
        if (v_zone == id_LZ) {
             if (f_wtr_state) {
                pal_row = palette_water_main;
            } else if (row >= (int)v_hblank_line) {
                pal_row = palette_water_main;
            }
        }

        int list[20], list_len = 0;
        int px_budget = 0;
        for (int i = 0; i < n && list_len < 20; i++) {
            if (row >= sy[i] && row < sy[i] + sh[i]) {
                if (px_budget + sw[i] > 320) break;
                px_budget += sw[i];
                list[list_len++] = i;
            }
        }

        render_plane_scanline(&vdp.vram[plane_b_addr], (uint16_t *)pal_row,
                              bg_scroll_x, bg_scroll_y, row, pix, pitch, 0, 1);
        render_plane_scanline(&vdp.vram[plane_a_addr], (uint16_t *)pal_row,
                              fg_scroll_x, fg_scroll_y, row, pix, pitch, 0, 0);

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
            if (yflip) { dy = height_tiles * 8 - 1 - dy; ty = dy >> 3; prow = dy & 7; }
            else       { ty = dy >> 3; prow = dy & 7; }
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
                        MD_ColorToRGBA(pal_row[pal_line * 16 + color_idx]);
                }
            }
        }

        render_plane_scanline(&vdp.vram[plane_b_addr], (uint16_t *)pal_row,
                              bg_scroll_x, bg_scroll_y, row, pix, pitch, 1, 1);
        render_plane_scanline(&vdp.vram[plane_a_addr], (uint16_t *)pal_row,
                              fg_scroll_x, fg_scroll_y, row, pix, pitch, 1, 0);

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
            if (yflip) { dy = height_tiles * 8 - 1 - dy; ty = dy >> 3; prow = dy & 7; }
            else       { ty = dy >> 3; prow = dy & 7; }
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
                        MD_ColorToRGBA(pal_row[pal_line * 16 + color_idx]);
                }
            }
        }
    }

    if (vdp_test_counter >= 0) {
        int w = g_render_w - 20;
        int bar_w = (vdp_test_counter * w) / 600;
        if (bar_w > w) bar_w = w;
        for (int y = 20; y < 60 && y < SCREEN_HEIGHT; y++) {
            for (int x = 10; x < 10 + bar_w && x < g_render_w; x++) {
                pix[y * stride + x] = 0xFF00FF00;
            }
        }
        if (vdp_test_counter % 60 < 30) pix[0] = 0xFFFFFFFF;
    }

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        memcpy(&last_frame[y * g_render_w],
               &pix[y * stride],
               (size_t)g_render_w * 4);
    }

    debug_collision_overlay(pix);

    const uint32_t *final = PP_Apply(pix, g_render_w, SCREEN_HEIGHT, &g_settings);

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

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, vdp.framebuffer, NULL, NULL);
    SDL_RenderPresent(renderer);

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
