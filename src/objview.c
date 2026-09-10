#include "objview.h"
#include "ram.h"
#include "objects.h"
#include "constants.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
   Minimal 8x8 bitmap font (ASCII 32-127). Each glyph is 8 bytes (one per
   row, MSB-left). Only the glyphs we need are filled in; unused slots are
   zero (blank). This avoids any external font dependency.
   -------------------------------------------------------------------------- */
static const uint8_t font8x8[96][8] = {
    /* 32 ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 33 '!' */ {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    /* 34 '"' */ {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00},
    /* 35 '#' */ {0x6C,0xFE,0x6C,0x6C,0xFE,0x6C,0x00,0x00},
    /* 36 '$' */ {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00},
    /* 37 '%' */ {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00},
    /* 38 '&' */ {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00},
    /* 39 ''' */ {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00},
    /* 40 '(' */ {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00},
    /* 41 ')' */ {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00},
    /* 42 '*' */ {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    /* 43 '+' */ {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    /* 44 ',' */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
    /* 45 '-' */ {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    /* 46 '.' */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    /* 47 '/' */ {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00},
    /* 48 '0' */ {0x7C,0xC6,0xCE,0xDE,0xF6,0xE6,0x7C,0x00},
    /* 49 '1' */ {0x18,0x38,0x78,0x18,0x18,0x18,0x7E,0x00},
    /* 50 '2' */ {0x7C,0xC6,0x06,0x1C,0x30,0x60,0xFE,0x00},
    /* 51 '3' */ {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00},
    /* 52 '4' */ {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00},
    /* 53 '5' */ {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00},
    /* 54 '6' */ {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00},
    /* 55 '7' */ {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00},
    /* 56 '8' */ {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00},
    /* 57 '9' */ {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00},
    /* 58 ':' */ {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    /* 59 ';' */ {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    /* 60 '<' */ {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00},
    /* 61 '=' */ {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
    /* 62 '>' */ {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00},
    /* 63 '?' */ {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00},
    /* 64 '@' */ {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00},
    /* 65 'A' */ {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00},
    /* 66 'B' */ {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00},
    /* 67 'C' */ {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00},
    /* 68 'D' */ {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00},
    /* 69 'E' */ {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00},
    /* 70 'F' */ {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00},
    /* 71 'G' */ {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00},
    /* 72 'H' */ {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00},
    /* 73 'I' */ {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    /* 74 'J' */ {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00},
    /* 75 'K' */ {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00},
    /* 76 'L' */ {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00},
    /* 77 'M' */ {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00},
    /* 78 'N' */ {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00},
    /* 79 'O' */ {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    /* 80 'P' */ {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},
    /* 81 'Q' */ {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06},
    /* 82 'R' */ {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00},
    /* 83 'S' */ {0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00},
    /* 84 'T' */ {0x7E,0x7E,0x5A,0x18,0x18,0x18,0x3C,0x00},
    /* 85 'U' */ {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    /* 86 'V' */ {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00},
    /* 87 'W' */ {0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00},
    /* 88 'X' */ {0xC6,0x6C,0x38,0x38,0x6C,0xC6,0xC6,0x00},
    /* 89 'Y' */ {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00},
    /* 90 'Z' */ {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00},
    /* 91 '[' */ {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00},
    /* 92 '\' */ {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00},
    /* 93 ']' */ {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00},
    /* 94 '^' */ {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},
    /* 95 '_' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
};

/* ---------------------------------------------------------------------------
   Window state
   -------------------------------------------------------------------------- */
#define COL_W       7     /* chars per field column                  */
#define HDR_ROWS    2     /* title + header line                     */
#define OBJ_ROWS    128   /* one row per object slot                 */
#define WIN_COLS    78    /* total chars wide                        */
#define CHAR_W      8     /* glyph width in pixels                  */
#define CHAR_H      10    /* glyph height in pixels (8 + 2 spacing) */
#define WIN_W       (WIN_COLS * CHAR_W)
#define WIN_H       ((HDR_ROWS + OBJ_ROWS) * CHAR_H)

static SDL_Window   *g_win = NULL;
static SDL_Renderer *g_ren = NULL;
static SDL_Texture  *g_tex = NULL;

/* Known object ID -> name map (only IDs registered in obj_dispatch) */
static const char *obj_id_name(uint8_t id) {
    switch (id) {
    case 0x00: return "----";          /* free slot */
    case id_SonicPlayer:  return "Soni";
    case id_HUD:          return " HUD";
    case id_TitleCard:    return "TtlC";
    case id_GameOverCard: return "GOvr";
    case id_Rings:        return "Ring";
    case id_RingLoss:     return "RngL";
    case id_TitleSonic:   return "TtSn";
    case id_PSBTM:        return "PSBT";
    case id_CreditsText:  return "Crdt";
    default:              return NULL;
    }
}

/* ---------------------------------------------------------------------------
   Toggle / window-ID
   -------------------------------------------------------------------------- */
void ObjView_Toggle(void) {
    if (g_win) {
        SDL_DestroyTexture(g_tex);
        SDL_DestroyRenderer(g_ren);
        SDL_DestroyWindow(g_win);
        g_tex = NULL;
        g_ren = NULL;
        g_win = NULL;
        return;
    }
    g_win = SDL_CreateWindow("Object RAM [O]",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             WIN_W, WIN_H, 0);
    if (!g_win) return;
    g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) { SDL_DestroyWindow(g_win); g_win = NULL; return; }
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, WIN_W, WIN_H);
}

int ObjView_WindowID(void) {
    return g_win ? (int)SDL_GetWindowID(g_win) : -1;
}

/* ---------------------------------------------------------------------------
   Glyph renderer: draw one 8x8 character into a 32-bit pixel buffer
   -------------------------------------------------------------------------- */
static inline void put_char(uint32_t *px, int pitch, int cx, int cy,
                            char ch, uint32_t fg, uint32_t bg) {
    int idx = (unsigned char)ch - 32;
    if (idx < 0 || idx >= 96) return;
    const uint8_t *glyph = font8x8[idx];
    int ox = cx * CHAR_W;
    int oy = cy * CHAR_H;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            px[(oy + row) * pitch + ox + col] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
    /* spacing row */
    for (int col = 0; col < 8; col++) {
        px[(oy + 8) * pitch + ox + col] = bg;
    }
}

static void put_string(uint32_t *px, int pitch, int cx, int cy,
                       const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        put_char(px, pitch, cx, cy, *s, fg, bg);
        cx++;
        s++;
    }
}

/* Format a signed 16-bit value as 5-char decimal string (e.g. "-1234") */
static void fmt_i16(char *buf, int16_t v) {
    sprintf(buf, "%5d", v);
}

/* Format a uint8_t as 2-char hex */
static void fmt_hex8(char *buf, uint8_t v) {
    sprintf(buf, "%02X", v);
}

/* Format a uint16_t as 4-char hex */
static void fmt_hex16(char *buf, uint16_t v) {
    sprintf(buf, "%04X", v);
}

/* ---------------------------------------------------------------------------
   Render the full table
   -------------------------------------------------------------------------- */
static const uint32_t COL_BG     = 0xFF1A1A2E;  /* dark navy            */
static const uint32_t COL_HDR_FG = 0xFFE0E0FF;  /* bright white-blue    */
static const uint32_t COL_ACT_FG = 0xFF50FF50;  /* green for active obj */
static const uint32_t COL_FREE   = 0xFF505050;  /* dim grey for empty   */
static const uint32_t COL_SONIC  = 0xFF5090FF;  /* orange for Sonic     */
static const uint32_t COL_HUD    = 0xFFFFFF50;  /* yellow for HUD       */
static const uint32_t COL_GRID   = 0xFF252540;  /* gridline             */

void ObjView_Render(void) {
    if (!g_win) return;

    void *pixels;
    int pitch;
    SDL_LockTexture(g_tex, NULL, &pixels, &pitch);
    uint32_t *px = (uint32_t *)pixels;
    int pwords = pitch / 4;

    /* Fill background */
    for (int y = 0; y < WIN_H; y++)
        for (int x = 0; x < WIN_W; x++)
            px[y * pwords + x] = COL_BG;

    /* Header line 1: title */
    put_string(px, pwords, 0, 0,
               " #  ID  Rtn   X     Y     Vx    Vy   St  An Fr  Map  Name",
               COL_HDR_FG, COL_BG);

    /* Separator */
    for (int x = 0; x < WIN_COLS; x++)
        put_char(px, pwords, x, 1, '-', COL_GRID, COL_BG);

    /* Object rows */
    uint8_t *base = ObjRAM;
    for (int i = 0; i < NUM_OBJECTS; i++) {
        uint8_t *o = &base[i * OBJECT_SIZE];
        uint8_t id = obID(o);

        uint32_t fg;
        if (id == 0) {
            fg = COL_FREE;
        } else if (id == id_SonicPlayer) {
            fg = COL_SONIC;
        } else if (id == id_HUD) {
            fg = COL_HUD;
        } else {
            fg = COL_ACT_FG;
        }

        int row = HDR_ROWS + i;
        char line[WIN_COLS + 1];
        memset(line, ' ', sizeof(line));
        line[WIN_COLS] = '\0';

        /* Slot number (2 digits) */
        char tmp[8];
        sprintf(tmp, "%2d", i);
        memcpy(&line[0], tmp, 2);

        /* ID (hex) */
        fmt_hex8(tmp, id);
        memcpy(&line[3], tmp, 2);

        /* Routine */
        fmt_hex8(tmp, obRoutine(o));
        memcpy(&line[6], tmp, 2);

        /* X */
        fmt_i16(tmp, obX(o));
        memcpy(&line[9], tmp, 5);

        /* Y */
        fmt_i16(tmp, obY(o));
        memcpy(&line[15], tmp, 5);

        /* VelX */
        fmt_i16(tmp, obVelX(o));
        memcpy(&line[21], tmp, 5);

        /* VelY */
        fmt_i16(tmp, obVelY(o));
        memcpy(&line[27], tmp, 5);

        /* Status (hex byte) */
        fmt_hex8(tmp, obStatus(o));
        memcpy(&line[33], tmp, 2);

        /* Animation */
        fmt_hex8(tmp, obAnim(o));
        memcpy(&line[36], tmp, 2);

        /* Frame */
        fmt_hex8(tmp, obFrame(o));
        memcpy(&line[39], tmp, 2);

        /* Map pointer (low 16 bits hex) */
        fmt_hex16(tmp, (uint16_t)((uintptr_t)obMap(o) & 0xFFFF));
        memcpy(&line[42], tmp, 4);

        /* Name (if known) */
        const char *name = obj_id_name(id);
        if (name) {
            memcpy(&line[47], name, 4);
        }

        /* Render each character */
        for (int c = 0; c < WIN_COLS; c++) {
            if (line[c] != ' ')
                put_char(px, pwords, c, row, line[c], fg, COL_BG);
        }
    }

    SDL_UnlockTexture(g_tex);

    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    SDL_RenderPresent(g_ren);
}
