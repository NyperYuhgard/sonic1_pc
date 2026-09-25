#include "tas_editor.h"
#include "tas.h"
#include "constants.h"
#include "font8x8.h"
#include <stdio.h>
#include <string.h>

/* -------- Layout -------- */
#define TOOLBAR_H   28
#define RULER_H     16
#define FOOTER_H    22
#define SCROLL_H     8
#define LABEL_W     36
#define N_ROWS       8

/* -------- Paleta -------- */
#define C_BG        0xFF181818
#define C_ROW_BG    0xFF242424
#define C_ROW_ALT   0xFF282828
#define C_LABEL_BG  0xFF101010
#define C_RULER_BG  0xFF0A0A0A
#define C_TOOLBAR   0xFF2A2A2A
#define C_FOOTER    0xFF2A2A2A
#define C_SCROLL_BG 0xFF101010
#define C_SCROLL_FG 0xFF606060
#define C_BORDER    0xFF404040
#define C_GRID      0xFF1A1A1A
#define C_CELL_OFF  0xFF2E2E2E
#define C_CELL_P1   0xFF30D030
#define C_CELL_P2   0xFFC060D0
#define C_CURSOR    0xFFFFFFFF
#define C_HEAD      0xFFFFD000
#define C_SEL_BD    0xFFFF8800
#define C_SEL_BG    0xFF4A2800
#define C_TEXT      0xFFE8E8E8
#define C_TEXT_DIM  0xFF909090
#define C_RED       0xFFFF4040
#define C_GREEN     0xFF40FF40
#define C_YELLOW    0xFFFFD000
#define C_ORANGE    0xFFFF8000

/* -------- Estado ventana -------- */
static SDL_Window   *ed_win = NULL;
static SDL_Renderer *ed_ren = NULL;
static SDL_Texture  *ed_tex = NULL;
static int tex_w = 0, tex_h = 0;

/* -------- Estado editor -------- */
static int   view_start  = 0;
static float ppf         = 4.0f;
static int   edit_cursor = 0;
static int   sel_start   = -1, sel_end = -1;
static int   left_dragging  = 0;
static int   right_dragging = 0;
static int   drag_bit   = 0;
static int   drag_value = 0;
static int   scrollbar_dragging = 0;
static int   scrollbar_drag_offset = 0;
static int   prev_playing = 0;
static int   blink_counter = 0;

static const uint8_t btn_bits[N_ROWS] = {
    btnUp, btnDn, btnL, btnR, btnA, btnB, btnC, btnStart
};
static const char *btn_names[N_ROWS] = { "U","D","L","R","A","B","C","S" };

/* -------- Ventana -------- */
void TASEditor_Toggle(void) {
    if (ed_win) {
        /* cerrar */
        SDL_DestroyTexture(ed_tex);
        SDL_DestroyRenderer(ed_ren);
        SDL_DestroyWindow(ed_win);
        ed_tex = NULL; ed_ren = NULL; ed_win = NULL;
        tex_w = 0; tex_h = 0;
        return;
    }

    /* abrir: pausa automática para que el usuario pueda trabajar */
    TAS_SetPaused(1);

    ed_win = SDL_CreateWindow("TAS Editor",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1100, 420, SDL_WINDOW_RESIZABLE);
    if (!ed_win) {
        TAS_SetPaused(0);   /* si falla, no dejar el juego colgado */
        return;
    }
    ed_ren = SDL_CreateRenderer(ed_win, -1, SDL_RENDERER_ACCELERATED);
    if (!ed_ren) {
        SDL_DestroyWindow(ed_win);
        ed_win = NULL;
        TAS_SetPaused(0);
    }
}

int TASEditor_WindowID(void) {
    return ed_win ? (int)SDL_GetWindowID(ed_win) : -1;
}

int TASEditor_IsOpen(void) { return ed_win != NULL; }

int TASEditor_HasFocus(void) {
    return ed_win && SDL_GetKeyboardFocus() == ed_win;
}

/* -------- Geometría -------- */

static int timeline_h(int wh) {
    int h = wh - TOOLBAR_H - RULER_H - FOOTER_H - SCROLL_H;
    return h > 0 ? h : 0;
}
static int row_h_from(int wh) {
    int th = timeline_h(wh);
    int rh = th / N_ROWS;
    return rh > 2 ? rh : 2;
}

static int frame_to_x(int f) {
    /* (f - view_start) * ppf, con redondeo al más cercano */
    float px = (float)(f - view_start) * ppf;
    return LABEL_W + (int)(px + (px >= 0.0f ? 0.5f : -0.5f));
}
static int x_to_frame(int x) {
    /* (x - LABEL_W) / ppf, truncado hacia 0 */
    float f = (float)(x - LABEL_W) / ppf;
    if (f < 0.0f) f = 0.0f;
    return view_start + (int)f;
}
static int frame_width_px(void) {
    int w = (int)(ppf + 0.5f);
    return w < 1 ? 1 : w;
}

/* -------- Scrollbar -------- */

static int scrollbar_visible(int ww) {
    return (TAS_GetLength() > 0) && (ww - LABEL_W > 20);
}

/* Posición del thumb en píxeles, dado el viewport */
static void scrollbar_thumb(int ww, int *x0, int *x1) {
    int track_x = LABEL_W;
    int track_w = ww - LABEL_W;
    uint32_t len = TAS_GetLength();
    if (track_w <= 0 || len == 0) { *x0 = *x1 = 0; return; }
    /* cuántos frames caben en el viewport */
    int frames_visible = (int)((ww - LABEL_W) / ppf);
    if (frames_visible < 1) frames_visible = 1;
    if (frames_visible >= (int)len) {
        *x0 = track_x;
        *x1 = track_x + track_w;
        return;
    }
    int t0 = track_x + (int)((float)view_start / len * track_w);
    int t1 = track_x + (int)((float)(view_start + frames_visible) / len * track_w);
    if (t1 - t0 < 12) t1 = t0 + 12;
    if (t1 > track_x + track_w) t1 = track_x + track_w;
    *x0 = t0;
    *x1 = t1;
}

/* -------- Eventos -------- */

void TASEditor_HandleEvent(SDL_Event *e) {
    if (!ed_win) return;
    int win_id = (int)SDL_GetWindowID(ed_win);

    if (e->type == SDL_WINDOWEVENT &&
        (int)e->window.windowID == win_id &&
        e->window.event == SDL_WINDOWEVENT_CLOSE) {
        TASEditor_Toggle();
        return;
    }

    /* Wheel: acepta eventos con windowID == win_id O windowID == 0 (SDL puede
       reportar 0 cuando el cursor está fuera de cualquier ventana). */
    if (e->type == SDL_MOUSEWHEEL) {
        int wid = (int)e->wheel.windowID;
        if (wid != 0 && wid != win_id) return;

        SDL_Keymod km = SDL_GetModState();
        if (km & KMOD_CTRL) {
            /* zoom */
            ppf += (float)e->wheel.y * 0.5f;
            if (ppf < 1.0f) ppf = 1.0f;
            if (ppf > 64.0f) ppf = 64.0f;
        } else {
            /* scroll: 32 px de "unidad", pero al menos 1 frame */
            float delta = -(float)e->wheel.y * 32.0f / ppf;
            if (delta == 0.0f) delta = (e->wheel.y > 0) ? -1.0f : 1.0f;
            view_start += (int)delta;
        }
        return;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && (int)e->button.windowID == win_id) {
        int ww, wh;
        SDL_GetWindowSize(ed_win, &ww, &wh);
        uint32_t len = TAS_GetLength();
        int rh = row_h_from(wh);
        int tl_top = TOOLBAR_H + RULER_H;
        int tl_h = rh * N_ROWS;

        /* Scrollbar? */
        if (scrollbar_visible(ww) && e->button.y >= tl_top + tl_h &&
            e->button.y < tl_top + tl_h + SCROLL_H) {
            int x0, x1;
            scrollbar_thumb(ww, &x0, &x1);
            if (e->button.x >= x0 && e->button.x < x1) {
                scrollbar_dragging = 1;
                scrollbar_drag_offset = e->button.x - x0;
            } else if (len > 0) {
                /* jump */
                int track_x = LABEL_W;
                int track_w = ww - LABEL_W;
                int frames_visible = (int)((ww - LABEL_W) / ppf);
                if (frames_visible < 1) frames_visible = 1;
                float ratio = (float)(e->button.x - track_x) / track_w;
                int new_start = (int)(ratio * len) - frames_visible / 2;
                view_start = new_start < 0 ? 0 : new_start;
            }
            return;
        }

        if (e->button.button == SDL_BUTTON_RIGHT) {
            if (e->button.y < tl_top || e->button.y >= tl_top + tl_h) return;
            int f = x_to_frame(e->button.x);
            if (f < 0) f = 0;
            if (len > 0 && f >= (int)len) f = (int)len - 1;
            sel_start = sel_end = f;
            right_dragging = 1;
            return;
        }
        if (e->button.button == SDL_BUTTON_LEFT) {
            /* Click en el ruler: mover el cursor */
            if (e->button.y >= TOOLBAR_H && e->button.y < TOOLBAR_H + RULER_H) {
                int f = x_to_frame(e->button.x);
                if (f < 0) f = 0;
                if (len > 0 && f >= (int)len) f = (int)len - 1;
                edit_cursor = f;
                return;
            }
            /* Click en la timeline */
            if (e->button.y < tl_top || e->button.y >= tl_top + tl_h) return;
            int row = (e->button.y - tl_top) / rh;
            if (row < 0 || row >= N_ROWS) return;
            if (len == 0) return;
            int f = x_to_frame(e->button.x);
            if (f < 0) f = 0;
            if (f >= (int)len) f = (int)len - 1;
            uint8_t bit = btn_bits[row];
            uint8_t cur = TAS_GetInputP1((uint32_t)f);
            int val = (cur & bit) ? 0 : 1;
            TAS_SetInputP1((uint32_t)f, (uint8_t)((cur & ~bit) | (val ? bit : 0)));
            left_dragging = 1;
            drag_bit = bit;
            drag_value = val;
            return;
        }
    }

    if (e->type == SDL_MOUSEBUTTONUP && (int)e->button.windowID == win_id) {
        if (e->button.button == SDL_BUTTON_LEFT)  left_dragging  = 0;
        if (e->button.button == SDL_BUTTON_RIGHT) right_dragging = 0;
        scrollbar_dragging = 0;
        return;
    }

    if (e->type == SDL_MOUSEMOTION && (int)e->motion.windowID == win_id) {
        int ww, wh;
        SDL_GetWindowSize(ed_win, &ww, &wh);
        uint32_t len = TAS_GetLength();

        if (scrollbar_dragging && len > 0) {
            int track_x = LABEL_W;
            int track_w = ww - LABEL_W;
            int x0, x1;
            scrollbar_thumb(ww, &x0, &x1);
            int thumb_w = x1 - x0;
            int px = e->motion.x - scrollbar_drag_offset;
            float ratio = (float)(px - track_x) / (track_w - thumb_w);
            if (ratio < 0) ratio = 0;
            if (ratio > 1) ratio = 1;
            int frames_visible = (int)((ww - LABEL_W) / ppf);
            if (frames_visible < 1) frames_visible = 1;
            int max_start = (int)len - frames_visible;
            if (max_start < 0) max_start = 0;
            view_start = (int)(ratio * max_start);
            return;
        }

        if (left_dragging && len > 0) {
            int f = x_to_frame(e->motion.x);
            if (f < 0) f = 0;
            if (f >= (int)len) f = (int)len - 1;
            uint8_t cur = TAS_GetInputP1((uint32_t)f);
            TAS_SetInputP1((uint32_t)f, (uint8_t)((cur & ~drag_bit) | (drag_value ? drag_bit : 0)));
            return;
        }
        if (right_dragging) {
            int f = x_to_frame(e->motion.x);
            if (f < 0) f = 0;
            if (len > 0 && f >= (int)len) f = (int)len - 1;
            sel_end = f;
            return;
        }
    }
}

/* -------- Teclado -------- */

void TASEditor_PollKeys(void) {
    if (!ed_win || !TASEditor_HasFocus()) return;

    const uint8_t *k = SDL_GetKeyboardState(NULL);
    SDL_Keymod km = SDL_GetModState();
    static uint8_t pk[SDL_NUM_SCANCODES] = {0};

    #define DOWN(sc) (k[sc] && !pk[sc])

    if (DOWN(SDL_SCANCODE_ESCAPE)) { TASEditor_Toggle(); return; }
    if (DOWN(SDL_SCANCODE_SPACE))  TAS_SetPaused(!TAS_IsPaused());
    if (DOWN(SDL_SCANCODE_R))     { TAS_SetPaused(1); TAS_ReplayFromStart(); }

    int step = (km & KMOD_SHIFT) ? 10 : 1;
    if (DOWN(SDL_SCANCODE_RIGHT)) edit_cursor += step;
    if (DOWN(SDL_SCANCODE_LEFT))  edit_cursor -= step;
    if (DOWN(SDL_SCANCODE_HOME))  edit_cursor = 0;
    if (DOWN(SDL_SCANCODE_END))   edit_cursor = (int)TAS_GetLength() - 1;

    /* Scroll de página */
    int ww, wh;
    SDL_GetWindowSize(ed_win, &ww, &wh);
    int frames_visible = (int)((ww - LABEL_W) / ppf);
    if (frames_visible < 1) frames_visible = 1;
    if (DOWN(SDL_SCANCODE_PAGEUP))   view_start -= frames_visible;
    if (DOWN(SDL_SCANCODE_PAGEDOWN)) view_start += frames_visible;

    if (TAS_GetLength() > 0) {
        if (edit_cursor < 0) edit_cursor = 0;
        if (edit_cursor >= (int)TAS_GetLength())
            edit_cursor = (int)TAS_GetLength() - 1;
    } else {
        edit_cursor = 0;
    }

    if (DOWN(SDL_SCANCODE_PERIOD)) { TAS_SetPaused(1); TAS_RequestStep(); }

    if (DOWN(SDL_SCANCODE_DELETE) || DOWN(SDL_SCANCODE_BACKSPACE)) {
        if (sel_start >= 0 && sel_end >= 0) {
            int a = sel_start, b = sel_end;
            if (a > b) { int t = a; a = b; b = t; }
            TAS_DeleteRange((uint32_t)a, (uint32_t)b);
            edit_cursor = a;
            sel_start = sel_end = -1;
        } else if (TAS_GetLength() > 0) {
            TAS_DeleteFrame((uint32_t)edit_cursor);
        }
    }
    if (DOWN(SDL_SCANCODE_INSERT)) TAS_InsertFrame((uint32_t)edit_cursor);

    if (DOWN(SDL_SCANCODE_A) && !(km & KMOD_CTRL)) {
        /* Seleccionar todo */
        if (TAS_GetLength() > 0) {
            sel_start = 0;
            sel_end = (int)TAS_GetLength() - 1;
        }
    }
    if ((km & KMOD_CTRL) && DOWN(SDL_SCANCODE_S)) TAS_SaveFile_Action();
    if ((km & KMOD_CTRL) && DOWN(SDL_SCANCODE_O)) TAS_LoadFile_Action();

    if (DOWN(SDL_SCANCODE_F5)) TAS_SaveNextState();
    if (DOWN(SDL_SCANCODE_F6)) TAS_LoadPrevState();

    memcpy(pk, k, SDL_NUM_SCANCODES);
    #undef DOWN
}

/* -------- Helpers de dibujo -------- */

static void fill(uint32_t *px, int stride, int ww, int wh,
                 int x, int y, int w, int h, uint32_t col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ww) w = ww - x;
    if (y + h > wh) h = wh - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        uint32_t *row = px + (y + j) * stride + x;
        for (int i = 0; i < w; i++) row[i] = col;
    }
}

static void vline(uint32_t *px, int stride, int ww, int wh,
                  int x, int y, int h, uint32_t col) {
    if (x < 0 || x >= ww) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > wh) h = wh - y;
    if (h <= 0) return;
    for (int j = 0; j < h; j++) px[(y + j) * stride + x] = col;
}

static void hline(uint32_t *px, int stride, int ww, int wh,
                  int x, int y, int w, uint32_t col) {
    if (y < 0 || y >= wh) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > ww) w = ww - x;
    if (w <= 0) return;
    uint32_t *row = px + y * stride + x;
    for (int i = 0; i < w; i++) row[i] = col;
}

/* -------- Render -------- */

void TASEditor_Render(void) {
    if (!ed_win) return;

    int ww, wh;
    SDL_GetWindowSize(ed_win, &ww, &wh);
    if (ww < 160 || wh < 100) return;

    if (!ed_tex || tex_w != ww || tex_h != wh) {
        if (ed_tex) SDL_DestroyTexture(ed_tex);
        ed_tex = SDL_CreateTexture(ed_ren, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, ww, wh);
        tex_w = ww; tex_h = wh;
        if (!ed_tex) return;
    }

    void *raw;
    int pitch;
    SDL_LockTexture(ed_tex, NULL, &raw, &pitch);
    uint32_t *px = (uint32_t *)raw;
    int stride = pitch / 4;

    fill(px, stride, ww, wh, 0, 0, ww, wh, C_BG);

    uint32_t len = TAS_GetLength();
    uint32_t cur = TAS_GetFrame();
    TasMode mode = TAS_GetMode();
    int playing = (mode == TAS_MODE_PLAYBACK);

    int rh = row_h_from(wh);
    int tl_top = TOOLBAR_H + RULER_H;
    int tl_h = rh * N_ROWS;
    int frames_visible = (int)((ww - LABEL_W) / ppf);
    if (frames_visible < 1) frames_visible = 1;

    /* ============================ TOOLBAR ============================ */
    fill(px, stride, ww, wh, 0, 0, ww, TOOLBAR_H, C_TOOLBAR);
    hline(px, stride, ww, wh, 0, TOOLBAR_H - 1, ww, C_BORDER);

    {
        int paused = TAS_IsPaused();

        /* [REC] [PLAY] [PAUSE] indicadores */
        int x = 6;
        char modebuf[16];
        uint32_t modecol;
        if (mode == TAS_MODE_RECORD) {
            snprintf(modebuf, sizeof modebuf, " REC ");
            modecol = C_RED;
        } else if (mode == TAS_MODE_PLAYBACK) {
            snprintf(modebuf, sizeof modebuf, " PLAY ");
            modecol = C_GREEN;
        } else {
            snprintf(modebuf, sizeof modebuf, " LIVE ");
            modecol = C_TEXT_DIM;
        }
        fill(px, stride, ww, wh, x, 5, 44, TOOLBAR_H - 11, modecol);
        font8x8_blit_shadow(px, pitch, ww, wh, x + 2, 10, modebuf, 0xFF000000);
        x += 50;

        if (paused) {
            fill(px, stride, ww, wh, x, 5, 56, TOOLBAR_H - 11, C_YELLOW);
            font8x8_blit_shadow(px, pitch, ww, wh, x + 2, 10, " PAUSE ", 0xFF000000);
            x += 62;
        }

        /* Info textual */
        char buf[256];
        snprintf(buf, sizeof buf,
                 "Frame %u/%u   Cursor %d   Zoom %.1f   %s",
                 cur, len, edit_cursor, ppf,
                 (sel_start >= 0 && sel_end >= 0) ? "SEL" : "");
        font8x8_blit_shadow(px, pitch, ww, wh, x, 10, buf, C_TEXT);
    }

    /* ============================ RULER ============================ */
    fill(px, stride, ww, wh, 0, TOOLBAR_H, ww, RULER_H, C_RULER_BG);
    hline(px, stride, ww, wh, 0, TOOLBAR_H + RULER_H - 1, ww, C_BORDER);

    /* Ruler ticks: cada 30 frames (marca pequeña), cada 60 frames (marca con número). */
    {
        int step_minor = 30;
        int step_major = 60;
        /* Si ppf es muy chico, reducir densidad para no tapar */
        int px_per_60 = (int)(step_major * ppf);
        while (px_per_60 < 40 && step_major < 3600) {
            step_major *= 2;
            step_minor = step_major / 2;
        }
        int f_start = view_start;
        int f_end = view_start + frames_visible;
        int first_minor = ((f_start + step_minor - 1) / step_minor) * step_minor;
        for (int f = first_minor; f <= f_end; f += step_minor) {
            if (f < 0) continue;
            int x = frame_to_x(f);
            if (x < LABEL_W || x >= ww) continue;
            int is_major = (f % step_major) == 0;
            if (is_major) {
                vline(px, stride, ww, wh, x, TOOLBAR_H + 2, RULER_H - 2, C_TEXT_DIM);
                if ((int)len > 0 || f == 0) {
                    char nb[16];
                    snprintf(nb, sizeof nb, "%d", f);
                    font8x8_blit_shadow(px, pitch, ww, wh, x + 2, TOOLBAR_H + 3,
                                        nb, C_TEXT);
                }
            } else {
                vline(px, stride, ww, wh, x, TOOLBAR_H + RULER_H - 5, 4, C_TEXT_DIM);
            }
        }
        /* Playback head en el ruler, solo durante playback real */
        if (playing &&
            (int)cur >= view_start && (int)cur < view_start + frames_visible) {
            int x = frame_to_x((int)cur);
            vline(px, stride, ww, wh, x, TOOLBAR_H, RULER_H, C_HEAD);
        }
    }

    /* ============================ ROWS ============================ */
    for (int r = 0; r < N_ROWS; r++) {
        int ry = tl_top + r * rh;
        uint32_t bg = (r & 1) ? C_ROW_BG : C_ROW_ALT;
        fill(px, stride, ww, wh, LABEL_W, ry, ww - LABEL_W, rh, bg);
        hline(px, stride, ww, wh, LABEL_W, ry + rh - 1, ww - LABEL_W, C_GRID);
    }
    /* Columna de labels */
    fill(px, stride, ww, wh, 0, tl_top, LABEL_W, tl_h, C_LABEL_BG);
    vline(px, stride, ww, wh, LABEL_W - 1, tl_top, tl_h, C_BORDER);

    for (int r = 0; r < N_ROWS; r++) {
        int ry = tl_top + r * rh;
        int ty = ry + (rh - 8) / 2;
        font8x8_blit_shadow(px, pitch, ww, wh, 6, ty, btn_names[r], C_TEXT);
    }

    /* ============================ CELDAS ============================ */
    if (len > 0) {
        int fw = frame_width_px();
        int f_end = view_start + frames_visible + 2;
        for (int f = view_start; f < f_end; f++) {
            if (f < 0) continue;
            if ((uint32_t)f >= len) break;
            int x0 = frame_to_x(f);
            int x1 = x0 + fw;
            if (x1 <= LABEL_W) continue;
            if (x0 < LABEL_W) x0 = LABEL_W;
            if (x0 >= ww) break;
            if (x1 > ww) x1 = ww;
            int cw = x1 - x0;
            if (cw <= 0) continue;

            uint8_t p1 = TAS_GetInputP1((uint32_t)f);
            uint8_t p2 = TAS_GetInputP2((uint32_t)f);
            for (int r = 0; r < N_ROWS; r++) {
                uint8_t bit = btn_bits[r];
                if (!(p1 & bit) && !(p2 & bit)) continue;
                uint32_t col = (p2 & bit) ? C_CELL_P2 : C_CELL_P1;
                int ry = tl_top + r * rh;
                fill(px, stride, ww, wh, x0, ry + 1, cw, rh - 2, col);
            }
        }
    }

    /* ============================ SELECCIÓN ============================ */
    if (sel_start >= 0 && sel_end >= 0 && len > 0) {
        int a = sel_start, b = sel_end;
        if (a > b) { int t = a; a = b; b = t; }
        int xa = frame_to_x(a);
        int xb = frame_to_x(b + 1);
        int lo = xa < LABEL_W ? LABEL_W : xa;
        int hi = xb > ww ? ww : xb;
        if (hi > lo) {
            /* tinte sutil sobre las celdas */
            for (int y = tl_top; y < tl_top + tl_h; y++) {
                uint32_t *row = px + y * stride;
                for (int x = lo; x < hi; x++) {
                    uint32_t c = row[x];
                    int r1 = (c >> 16) & 0xFF;
                    int g1 = (c >> 8) & 0xFF;
                    int b1 = c & 0xFF;
                    int r2 = (C_ORANGE >> 16) & 0xFF;
                    int g2 = (C_ORANGE >> 8) & 0xFF;
                    int b2 = C_ORANGE & 0xFF;
                    row[x] = 0xFF000000u
                           | (((r1 + r2) / 2) << 16)
                           | (((g1 + g2) / 2) << 8)
                           |  ((b1 + b2) / 2);
                }
            }
            /* bordes verticales */
            if (xa >= LABEL_W && xa < ww)
                vline(px, stride, ww, wh, xa, tl_top, tl_h, C_SEL_BD);
            if (xb - 1 >= LABEL_W && xb - 1 < ww)
                vline(px, stride, ww, wh, xb - 1, tl_top, tl_h, C_SEL_BD);
        }
    }

    /* ============================ PLAYBACK HEAD ============================
       Solo durante playback real. En LIVE/RECORD no tiene sentido, porque el
       frame counter del TAS avanza con el reloj del juego y la línea
       "bailaría" sola sin que haya nada que reproducir. */
    if (playing &&
        (int)cur >= view_start && (int)cur < view_start + frames_visible + 1) {
        int x = frame_to_x((int)cur);
        if (x >= LABEL_W && x < ww) {
            vline(px, stride, ww, wh, x, tl_top, tl_h, C_HEAD);
            if (x + 1 < ww) vline(px, stride, ww, wh, x + 1, tl_top, tl_h, C_HEAD);
        }
    }

    /* ============================ CURSOR ============================ */
    if (edit_cursor >= view_start && edit_cursor < view_start + frames_visible + 1) {
        int x = frame_to_x(edit_cursor);
        if (x >= LABEL_W && x < ww) {
            vline(px, stride, ww, wh, x, tl_top, tl_h, C_CURSOR);
        }
    }

    /* ============================ SCROLLBAR ============================ */
    {
        int sy = tl_top + tl_h;
        fill(px, stride, ww, wh, 0, sy, ww, SCROLL_H, C_SCROLL_BG);
        if (scrollbar_visible(ww)) {
            int x0, x1;
            scrollbar_thumb(ww, &x0, &x1);
            if (x1 > x0)
                fill(px, stride, ww, wh, x0, sy + 1, x1 - x0, SCROLL_H - 2, C_SCROLL_FG);
        }
        hline(px, stride, ww, wh, 0, sy, ww, C_BORDER);
    }

    /* ============================ FOOTER ============================ */
    {
        int fy = wh - FOOTER_H;
        fill(px, stride, ww, wh, 0, fy, ww, FOOTER_H, C_FOOTER);
        hline(px, stride, ww, wh, 0, fy, ww, C_BORDER);
        char buf[300];
        if (sel_start >= 0 && sel_end >= 0) {
            int a = sel_start, b = sel_end;
            if (a > b) { int t = a; a = b; b = t; }
            snprintf(buf, sizeof buf,
                     "SEL %d..%d (%d fr)   Del cut   Ins insert   Esc clear sel   A select all",
                     a, b, b - a + 1);
        } else {
            snprintf(buf, sizeof buf,
                     "L-click toggle   R-drag select   Wheel scroll   Ctrl+Wheel zoom   "
                     "Space play/pause   R replay   .  step   ^S save   ^O load");
        }
        font8x8_blit_shadow(px, pitch, ww, wh, 6, fy + 6, buf, C_TEXT);
    }

    SDL_UnlockTexture(ed_tex);

    /* Clamp view_start tras todo (por si wheel/scroll lo dejó fuera) */
    if (len > 0) {
        int max_start = (int)len - frames_visible;
        if (max_start < 0) max_start = 0;
        if (view_start > max_start) view_start = max_start;
        if (view_start < 0) view_start = 0;
    } else {
        view_start = 0;
    }

    SDL_SetRenderDrawColor(ed_ren, 0, 0, 0, 255);
    SDL_RenderClear(ed_ren);
    SDL_RenderCopy(ed_ren, ed_tex, NULL, NULL);
    SDL_RenderPresent(ed_ren);

    /* Auto-pausa al terminar playback */
    if (prev_playing == TAS_MODE_PLAYBACK && mode != TAS_MODE_PLAYBACK) {
        TAS_SetPaused(1);
    }
    prev_playing = mode;
    blink_counter++;
}