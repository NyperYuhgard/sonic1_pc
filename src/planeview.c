#include "planeview.h"
#include "ram.h"
#include "vdp.h"
#include "palette.h"
#include "font8x8.h"
#include <SDL2/SDL.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 *  Plane A/B viewer window [G]
 *
 *  Shows the FULL nametables of Plane A and Plane B decoded exactly like the
 *  renderer: same big-endian tile entries, same flips, same palette_main and
 *  the same plane-height wrapping (register $10). A red rectangle marks the
 *  320x224 region the screen currently samples (from hscroll row 0 and
 *  v_scrposy_vdp / v_bgscrposy_vdp).
 *
 *  The layout adapts to the window size WITHOUT stretching anything:
 *   - wide windows: panels anchored near the left/right edges with a flexible
 *     gap between them;
 *   - narrow windows: the panels stack vertically;
 *   - the whole block is centered when it fits, and scrolls (mouse wheel +
 *     left-drag pan) when it does not.
 *  ------------------------------------------------------------------------- */

#define PV_COLS      64    /* nametable columns (reg $10 bit 2) */
#define PV_STRIP     28    /* per-panel left ruler strip */
#define PV_PANEL_W   (PV_COLS * 8)          /* 512 px */
#define PV_PAD       8
#define PV_GAP_MIN   20
#define PV_HEADER    20                     /* header text + top ruler */
#define PV_FOOT      12
#define SIDE_MIN     (PV_PAD + PV_STRIP + PV_PANEL_W + PV_GAP_MIN + \
                      PV_PANEL_W + PV_STRIP + PV_PAD)               /* 1116 */

static SDL_Window   *g_win = NULL;
static SDL_Renderer *g_ren = NULL;
static SDL_Texture  *g_tex = NULL;
static int g_tex_w = 0, g_tex_h = 0;

static int g_ox = 0, g_oy = 0;              /* scroll offset (content - view) */
static int g_drag = 0, g_last_mx = 0, g_last_my = 0;

typedef struct {
    int ww, wh;                 /* window client size */
    int content_w, content_h;   /* panels-group size (units = px) */
    int ox, oy;                 /* draw offset of the panels group */
    int pa_x, pa_y;             /* panel A origin (y relative to the group) */
    int pb_x, pb_y;             /* panel B origin */
    int ha_x, hb_x;             /* header label x for each panel */
    int sa_x, sb_x;             /* ruler strip x for each panel */
} PVLayout;

/* Compute the placement of everything for the current window. The header
   band and the top ruler stay pinned at the window top; only the panels
   group is centered (when it fits) or scrolled (mouse wheel / drag) inside
   the area below the header. Nothing is ever stretched. */
static void pv_layout(int ww, int wh, int rowsA, int rowsB, PVLayout *L) {
    int maxr = rowsA > rowsB ? rowsA : rowsB;
    L->ww = ww;
    L->wh = wh;

    if (ww >= SIDE_MIN) {
        /* side by side; block centered with a bounded gap between panels */
        int gap = ww - 2 * (PV_PAD + PV_STRIP + PV_PANEL_W);
        if (gap > 80) gap = 80;                 /* keep wide windows tidy */
        int block_w = 2 * (PV_PAD + PV_STRIP) + 2 * PV_PANEL_W + gap;
        int left = (ww - block_w) / 2;
        L->pa_x = left + PV_PAD + PV_STRIP;
        L->pb_x = L->pa_x + PV_PANEL_W + gap;
        L->sa_x = L->pa_x - PV_STRIP;
        L->sb_x = L->pb_x - PV_STRIP;
        L->ha_x = L->sa_x;
        L->hb_x = L->sb_x;
        L->pa_y = 0;
        L->pb_y = 0;
        L->content_w = ww;
        L->content_h = maxr * 8 + PV_FOOT;
    } else {
        /* stacked vertically, column centered */
        int col_w = PV_STRIP + PV_PANEL_W;
        int start = (ww - col_w) / 2;
        if (start < PV_PAD) start = PV_PAD;
        L->pa_x = start + PV_STRIP;
        L->sa_x = start;
        L->ha_x = start;
        L->pb_x = L->pa_x;
        L->sb_x = start;
        L->hb_x = start;
        L->pa_y = 0;
        L->pb_y = rowsA * 8 + 16;
        L->content_w = ww;
        L->content_h = L->pb_y + rowsB * 8 + PV_FOOT;
    }

    /* Center the panels group in the area below the header, or scroll it.
       Draw offset is applied as:  window_y = PV_HEADER + rel_y - oy.
       Scrolling down means oy > 0 (content moves up); centering a group
       that fits means oy < 0 (content moves down into the middle). */
    int avail_h = wh - PV_HEADER;
    int max_oy = L->content_h > avail_h ? L->content_h - avail_h : 0;
    if (max_oy == 0) g_oy = 0;
    else {
        if (g_oy < 0) g_oy = 0;
        if (g_oy > max_oy) g_oy = max_oy;
    }
    if (max_oy == 0 && L->content_h < avail_h) {
        g_oy = -(avail_h - L->content_h) / 2;
    }
    g_ox = 0;
    L->ox = g_ox;
    L->oy = g_oy;
}

void PlaneView_Toggle(void) {
    if (g_win) {
        SDL_DestroyTexture(g_tex);
        SDL_DestroyRenderer(g_ren);
        SDL_DestroyWindow(g_win);
        g_tex = NULL;
        g_ren = NULL;
        g_win = NULL;
        g_tex_w = g_tex_h = 0;
        g_ox = g_oy = 0;
        g_drag = 0;
        return;
    }
    int rowsA = VDP_PlaneRows(0), rowsB = VDP_PlaneRows(1);
    int maxr = rowsA > rowsB ? rowsA : rowsB;
    int ww = SIDE_MIN, wh = PV_HEADER + maxr * 8 + PV_FOOT;
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0 && dm.w > 0) {
        int m = dm.w * 3 / 4;           /* small screens start stacked */
        if (m < ww) ww = m;
    }
    g_win = SDL_CreateWindow("Plane Viewer [G]",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             ww, wh, SDL_WINDOW_RESIZABLE);
    if (!g_win) return;
    g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) {
        SDL_DestroyWindow(g_win);
        g_win = NULL;
        return;
    }
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, ww, wh);
    g_tex_w = ww;
    g_tex_h = wh;
}

int PlaneView_WindowID(void) {
    return g_win ? (int)SDL_GetWindowID(g_win) : -1;
}

void PlaneView_Scroll(int dx, int dy) {
    g_ox += dx;
    g_oy += dy;
}

void PlaneView_DragStart(int mx, int my) {
    g_drag = 1;
    g_last_mx = mx;
    g_last_my = my;
}

void PlaneView_DragMove(int mx, int my) {
    if (!g_drag) return;
    g_ox -= mx - g_last_mx;             /* grab-and-pan: content follows */
    g_oy -= my - g_last_my;
    g_last_mx = mx;
    g_last_my = my;
}

void PlaneView_DragEnd(void) {
    g_drag = 0;
}

/* Draw one full nametable panel. wx/wy = window coords of the panel origin
   (already scrolled/centered); every write is clipped to [0,ww)x[0,wh). */
static void planeview_draw_panel(uint32_t *px, int stride, int ww, int wh,
                                 int wx, int wy, int rows, uint32_t base) {
    /* Checker background so empty VRAM is obvious */
    for (int y = 0; y < rows * 8; y++) {
        int yy = wy + y;
        if (yy < 0 || yy >= wh) continue;
        for (int x = 0; x < PV_PANEL_W; x++) {
            int xx = wx + x;
            if (xx < 0 || xx >= ww) continue;
            int cc = ((x >> 2) + (y >> 2)) & 1;
            px[yy * stride + xx] = cc ? 0xFF3A3A3A : 0xFF525252;
        }
    }

    /* Tiles, decoded exactly like render_plane_scanline (all priorities) */
    const uint8_t *nt = &vdp.vram[base];
    for (int ty = 0; ty < rows; ty++) {
        int y0 = wy + ty * 8;
        for (int tx = 0; tx < PV_COLS; tx++) {
            const uint8_t *e = &nt[(ty * PV_COLS + tx) * 2];
            uint16_t entry = (uint16_t)((e[0] << 8) | e[1]);
            if (entry == 0) continue;   /* truly empty cell */
            uint16_t tile_num = entry & 0x7FF;
            int x_flip = (entry >> 11) & 1;
            int y_flip = (entry >> 12) & 1;
            int pal_line = (entry >> 13) & 3;

            for (int py = 0; py < 8; py++) {
                const uint8_t *row =
                    &vdp.vram[tile_num * 32 + (y_flip ? (7 - py) : py) * 4];
                int yy = y0 + py;
                if (yy < 0 || yy >= wh) continue;
                for (int pxi = 0; pxi < 8; pxi++) {
                    int xx = wx + tx * 8 + pxi;
                    if (xx < 0 || xx >= ww) continue;
                    int src_x = x_flip ? (7 - pxi) : pxi;
                    int ci = (src_x & 1) ? (row[src_x >> 1] & 0x0F)
                                         : ((row[src_x >> 1] >> 4) & 0x0F);
                    if (ci == 0) continue;              /* transparent */
                    px[yy * stride + xx] =
                        MD_ColorToRGBA(palette_main[pal_line * 16 + ci]);
                }
            }
        }
    }

    /* Grid: 1px per tile, stronger every 8 tiles */
    for (int y = 0; y < rows * 8; y++) {
        int yy = wy + y;
        if (yy < 0 || yy >= wh) continue;
        for (int x = 0; x < PV_PANEL_W; x++) {
            int xx = wx + x;
            if (xx < 0 || xx >= ww) continue;
            if (x % 8 == 0 || y % 8 == 0) {
                int strong = (x % 64 == 0) || (y % 64 == 0);
                px[yy * stride + xx] = strong ? 0xFF3F3F3F : 0xFF2A2A2A;
            }
        }
    }
}

/* Red rectangle: the 320x224 viewport the screen samples, clipped to the
   panel. sx/sy = plane scroll values from hscroll row 0 (x) and VSRAM (y). */
static void planeview_draw_viewport(uint32_t *px, int stride, int ww, int wh,
                                    int px0, int py0, int rows,
                                    int16_t sx, int16_t sy) {
    int vx = sx & 0x1FF;                              /* wrap 0..511 px */
    int vy = sy & (rows * 8 - 1);
    int x0 = px0 + vx, y0 = py0 + vy;
    int x1 = x0 + 320, y1 = y0 + 224;
    int l = px0, r = px0 + PV_PANEL_W;
    int t = py0, b = py0 + rows * 8;
    if (x0 < l) x0 = l;
    if (x1 > r) x1 = r;
    if (y0 < t) y0 = t;
    if (y1 > b) y1 = b;
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t col = 0xFFFF4040;
    for (int xx = x0; xx < x1; xx++) {
        if (xx >= 0 && xx < ww && y0 >= 0 && y0 < wh) px[y0 * stride + xx] = col;
        if (xx >= 0 && xx < ww && y1 - 1 >= 0 && y1 - 1 < wh)
            px[(y1 - 1) * stride + xx] = col;
    }
    for (int yy = y0; yy < y1; yy++) {
        if (yy >= 0 && yy < wh && x0 >= 0 && x0 < ww) px[yy * stride + x0] = col;
        if (yy >= 0 && yy < wh && x1 - 1 >= 0 && x1 - 1 < ww)
            px[yy * stride + (x1 - 1)] = col;
    }
}

void PlaneView_Render(void) {
    if (!g_win) return;

    int rowsA = VDP_PlaneRows(0);
    int rowsB = VDP_PlaneRows(1);
    int ww, wh;
    SDL_GetWindowSize(g_win, &ww, &wh);
    if (ww < 60 || wh < 60) return;

    PVLayout L;
    pv_layout(ww, wh, rowsA, rowsB, &L);

    /* Backing texture at window size => RenderCopy stays 1:1. */
    if (g_tex_w != ww || g_tex_h != wh) {
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING, ww, wh);
        g_tex_w = ww;
        g_tex_h = wh;
        if (!g_tex) return;
    }

    void *pixels;
    int pitch;
    SDL_LockTexture(g_tex, NULL, &pixels, &pitch);
    uint32_t *px = (uint32_t *)pixels;
    int stride = pitch / 4;

    for (int i = 0; i < ww * wh; i++) px[i] = 0xFF202020;

    /* Plane addresses, decoded like the renderer */
    uint32_t plane_a = ((uint32_t)(vdp.registers[2] & 0x38)) << 10;
    uint32_t plane_b = ((uint32_t)(vdp.registers[4] & 0x07)) << 13;
    plane_a &= VRAM_SIZE - 1;
    plane_b &= VRAM_SIZE - 1;

    /* Scroll values the renderer consumes */
    const uint8_t *h = &ram[v_hscrolltablebuffer];
    int16_t ax = (int16_t)((uint16_t)h[0] | ((uint16_t)h[1] << 8));
    int16_t bx = (int16_t)((uint16_t)h[2] | ((uint16_t)h[3] << 8));
    int16_t ay = (int16_t)v_scrposy_vdp;
    int16_t by = (int16_t)v_bgscrposy_vdp;

    int rows[2] = { rowsA, rowsB };
    int pan_x[2] = { L.pa_x, L.pb_x };
    int pan_y[2] = { L.pa_y, L.pb_y };
    int hdr_x[2] = { L.ha_x, L.hb_x };
    int strp_x[2] = { L.sa_x, L.sb_x };
    const char *names[2] = { "A", "B" };
    char buf[96];

    /* Window y of the panels-group top: the header band stays pinned. */
    int py_base = PV_HEADER - L.oy;

    /* Header labels (pinned at the window top) */
    snprintf(buf, sizeof buf, "PLANE %s BASE=$%04X  %dX%d",
             names[0], plane_a, PV_COLS, rowsA);
    font8x8_blit_shadow(px, pitch, ww, wh, hdr_x[0] - L.ox, 1,
                        buf, 0xFF9FDF9F);
    snprintf(buf, sizeof buf, "PLANE %s BASE=$%04X  %dX%d  R10=$%02X",
             names[1], plane_b, PV_COLS, rowsB, vdp.registers[16] & 0xFF);
    font8x8_blit_shadow(px, pitch, ww, wh, hdr_x[1] - L.ox, 1,
                        buf, 0xFF9FDFDF);

    /* Top column ruler (repeated per panel) + separator, pinned */
    for (int i = 0; i < 2; i++) {
        for (int c = 0; c <= PV_COLS; c += 8) {
            snprintf(buf, sizeof buf, "%X", c);
            int x = pan_x[i] - L.ox + c * 8 + 2;
            if (c == PV_COLS) x -= 10;
            font8x8_blit_shadow(px, pitch, ww, wh, x, 11,
                                buf, 0xFFA0A0A0);
        }
        int ps = pan_x[i] - L.ox, pe = pan_x[i] - L.ox + PV_PANEL_W;
        int sep_y = PV_HEADER - 1;
        for (int xx = ps; xx <= pe; xx++) {
            if (xx >= 0 && xx < ww && sep_y >= 0 && sep_y < wh) {
                px[sep_y * stride + xx] = 0xFF4A4A4A;
            }
        }
    }

    /* Panels + per-panel left ruler */
    for (int i = 0; i < 2; i++) {
        int rws = rows[i];
        planeview_draw_panel(px, stride, ww, wh,
                             pan_x[i] - L.ox, py_base + pan_y[i], rws,
                             i == 0 ? plane_a : plane_b);

        int rs = strp_x[i] - L.ox, re = strp_x[i] - L.ox + PV_STRIP - 1;
        int y0 = py_base + pan_y[i], y1 = y0 + rws * 8;
        for (int yy = y0; yy <= y1; yy++) {
            if (yy >= 0 && yy < wh && re >= 0 && re < ww) {
                px[yy * stride + re] = 0xFF4A4A4A;
            }
        }
        for (int r = 0; r <= rws; r += 8) {
            snprintf(buf, sizeof buf, "%X", r);
            font8x8_blit_shadow(px, pitch, ww, wh, rs + 4,
                                y0 + r * 8, buf, 0xFFA0A0A0);
        }
    }

    /* Viewport rectangles */
    planeview_draw_viewport(px, stride, ww, wh,
                            pan_x[0] - L.ox, py_base + pan_y[0], rowsA, ax, ay);
    planeview_draw_viewport(px, stride, ww, wh,
                            pan_x[1] - L.ox, py_base + pan_y[1], rowsB, bx, by);

    /* Legend */
    int maxr = rowsA > rowsB ? rowsA : rowsB;
    font8x8_blit_shadow(px, pitch, ww, wh, PV_PAD,
                        py_base + maxr * 8 + 4,
                        "RED RECT = SCREEN VIEWPORT 320X224 (HSCROLL ROW 0)",
                        0xFFC0C0C0);

    SDL_UnlockTexture(g_tex);

    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    SDL_RenderPresent(g_ren);
}