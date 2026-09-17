#include "ramview.h"
#include "ram.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ===========================================================================
   Minimal 8x8 bitmap font, ASCII 32..127. Same glyphs as objview.c;
   lowercase (ASCII 96-127) is blank — we uppercase all strings we render.
   =========================================================================== */
static const uint8_t rv_font[96][8] = {
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
    /* 96..127: blank (lowercase and braces not needed — we uppercase) */
};

/* ---------------------------------------------------------------------------
   Dimensions and window state
   ------------------------------------------------------------------------- */
#define RV_CHAR_W   8
#define RV_CHAR_H   10
#define RV_COLS     56
#define RV_VIS_ROWS 54                                  /* visible rows */
#define RV_W      (RV_COLS * RV_CHAR_W)
#define RV_H      (RV_VIS_ROWS * RV_CHAR_H)

static SDL_Window   *rv_win = NULL;
static SDL_Renderer *rv_ren = NULL;
static SDL_Texture  *rv_tex = NULL;

static int rv_scroll = 0;

/* ---------------------------------------------------------------------------
   Watch entries. Name is UPPERCASE. addr = offset into ram[]. fmt controls
   how the value is decoded and printed.
   ------------------------------------------------------------------------- */
enum { F_U8, F_S8, F_U16, F_S16, F_U32, F_S32 };

typedef struct {
    const char *name;
    uint32_t    addr;
    uint8_t     fmt;
} RV_Watch;

typedef struct {
    const char     *title;
    const RV_Watch *items;
    int             count;
} RV_Section;

/* --- Game state --- */
static const RV_Watch sec_game[] = {
    { "V_GAMEMODE",   0xF600, F_U8  },
    { "V_ZONE_ACT",   0xFE10, F_U16 },
    { "V_LIVES",      0xFE12, F_U8  },
    { "V_RINGS",      0xFE20, F_U16 },
    { "V_SCORE",      0xFE26, F_U32 },
    { "V_TIME",       0xFE22, F_U32 },
    { "V_AIR",        0xFE14, F_U16 },
    { "V_CONTINUES",  0xFE18, F_U8  },
};

/* --- Level bounds & camera --- */
static const RV_Watch sec_bounds[] = {
    { "V_LIMITLEFT2",  0xF728, F_U16 },
    { "V_LIMITRIGHT2", 0xF72A, F_U16 },
    { "V_LIMITTOP2",   0xF72C, F_U16 },
    { "V_LIMITBTM2",   0xF72E, F_U16 },
    { "V_SCREENPOSX",  0xF700, F_S32 },
    { "V_SCREENPOSY",  0xF704, F_S32 },
    { "V_BGSCRPOSX",   0xF708, F_S32 },
    { "V_BGSCRPOSY",   0xF70C, F_S32 },
    { "V_LOOKSHIFT",   0xF73E, F_S16 },
};

/* --- Sonic (v_player + its fields) --- */
static const RV_Watch sec_sonic[] = {
    { "SONIC.ID",       0xD000, F_U8  },
    { "SONIC.ROUTINE",  0xD024, F_U8  },
    { "SONIC.STATUS",   0xD022, F_U8  },
    { "SONIC.ANIM",     0xD01C, F_U8  },
    { "SONIC.FRAME",    0xD01A, F_U8  },
    { "SONIC.SUBTYPE",  0xD028, F_U8  },
    { "SONIC.X",        0xD008, F_S16 },
    { "SONIC.Y",        0xD00C, F_S16 },
    { "SONIC.VELX",     0xD010, F_S16 },
    { "SONIC.VELY",     0xD012, F_S16 },
    { "SONIC.INERTIA",  0xD014, F_S16 },
    { "SONIC.ANGLE",    0xD026, F_U8  },
    { "SONIC.FLASHTIME",0xD030, F_S16 },
    { "SONIC.INVTIME",  0xD032, F_S16 },
    { "SONIC.SHOETIME", 0xD034, F_S16 },
    { "SONIC.JUMPING",  0xD03C, F_U8  },
    { "SONIC.LOCKTIME", 0xD03E, F_U8  },
};

/* --- Loop / roll chunk IDs and tracking --- */
static const RV_Watch sec_loop[] = {
    { "V_256LOOP1",   0xF7AC, F_U8  },
    { "V_256LOOP2",   0xF7AD, F_U8  },
    { "V_256ROLL1",   0xF7AE, F_U8  },
    { "V_256ROLL2",   0xF7AF, F_U8  },
    { "V_TRACKPOS",   0xF7A8, F_U16 },
    { "V_TRACKBYTE",  0xF7A9, F_U8  },
};

/* --- Flags --- */
static const RV_Watch sec_flags[] = {
    { "F_RESTART",    0xFE02, F_U16 },
    { "F_PAUSE",      0xF63A, F_U16 },
    { "F_TIMEOVER",   0xFE1A, F_U8  },
    { "V_INVINC",     0xFE2D, F_U8  },
    { "V_SHIELD",     0xFE2C, F_U8  },
    { "V_SHOES",      0xFE2E, F_U8  },
    { "F_TIMECOUNT",  0xFE1E, F_U8  },
    { "F_SCORECOUNT", 0xFE1F, F_U8  },
    { "F_RINGCOUNT",  0xFE1D, F_U8  },
    { "F_LOCKCTRL",   0xF7CC, F_U8  },
    { "F_PLAYERCTRL", 0xF7C8, F_U8  },
    { "F_DEBUGMODE",  0xFFFA, F_U16 },
    { "F_DEMO",       0xFFF0, F_S16 },
    { "V_DEBUGUSE",   0xFE08, F_U16 },
};

/* --- Sync timers + oscillation reads --- */
static const RV_Watch sec_sync[] = {
    { "V_ANI1_TIME",  0xFEC2, F_U8  },
    { "V_ANI1_FRAME", 0xFEC3, F_U8  },
    { "V_ANI3_TIME",  0xFEC6, F_U8  },
    { "V_ANI3_FRAME", 0xFEC7, F_U8  },
    { "V_ANI3_BUF",   0xFEC8, F_U16 },
    { "V_OSC_BITS",   0xFE5E, F_U16 },  /* bitfield word */
    { "V_OSC+$1A",    0xFE78, F_U8  },  /* Plat_ChangeMotion reads this byte */
    { "V_OSC+$0E",    0xFE6C, F_U8  },  /* slow platforms read this byte */
};

/* --- Misc useful things --- */
static const RV_Watch sec_misc[] = {
    { "V_RANDOM",      0xF636, F_U32 },
    { "V_PLC_PATLEFT", 0xF6F8, F_U16 },
    { "V_PLC_BUF0",    0xF680, F_U32 },
    { "V_FRAMECOUNT",  0xFE04, F_U16 },
    { "V_FRAMEBYTE",   0xFE05, F_U8  },
    { "V_VBLANK_BYTE", 0xFE0F, F_U8  },
    { "V_VBLANK_ROUT", 0xF62A, F_U8  },
    { "V_SONSPEEDMAX", 0xF760, F_U16 },
    { "V_SONSPEEDACC", 0xF762, F_U16 },
    { "V_SONSPEEDDEC", 0xF764, F_U16 },
    { "V_SONFRAMENUM", 0xF766, F_U8  },
    { "F_SONFRAMECHG", 0xF767, F_U8  },
    { "V_SOUNDQ0",     0xF00A, F_U8  },
    { "V_SOUNDQ1",     0xF00B, F_U8  },
    { "V_SOUNDQ2",     0xF00C, F_U8  },
};

static const RV_Section rv_sections[] = {
    { "GAME STATE",       sec_game,   sizeof(sec_game)   / sizeof(sec_game[0])   },
    { "BOUNDS & CAMERA",  sec_bounds, sizeof(sec_bounds) / sizeof(sec_bounds[0]) },
    { "SONIC",            sec_sonic,  sizeof(sec_sonic)  / sizeof(sec_sonic[0])  },
    { "LOOP/ROLL/TRACK",  sec_loop,   sizeof(sec_loop)   / sizeof(sec_loop[0])   },
    { "FLAGS",            sec_flags,  sizeof(sec_flags)  / sizeof(sec_flags[0])  },
    { "SYNC/OSCILLATE",   sec_sync,   sizeof(sec_sync)   / sizeof(sec_sync[0])   },
    { "MISC",             sec_misc,   sizeof(sec_misc)   / sizeof(sec_misc[0])   },
};
#define RV_SECTION_COUNT (sizeof(rv_sections) / sizeof(rv_sections[0]))

/* ---------------------------------------------------------------------------
   Scroll helpers (need rv_sections + RV_SECTION_COUNT, so they live below).
   ------------------------------------------------------------------------- */
static int rv_total_rows(void) {
    int rows = 2;                                  /* title + separator */
    for (size_t s = 0; s < RV_SECTION_COUNT; s++) {
        if (rv_sections[s].title) rows++;
        rows += rv_sections[s].count;
        rows += 1;                                 /* blank line between sections */
    }
    return rows;
}

static int rv_max_scroll(void) {
    int m = rv_total_rows() - RV_VIS_ROWS;
    return m < 0 ? 0 : m;
}

static void rv_clamp_scroll(void) {
    if (rv_scroll < 0) rv_scroll = 0;
    int m = rv_max_scroll();
    if (rv_scroll > m) rv_scroll = m;
}

static void rv_update_title(void) {
    if (!rv_win) return;
    char buf[64];
    snprintf(buf, sizeof buf, "RAM View  rows %d-%d / %d",
             rv_scroll, rv_scroll + RV_VIS_ROWS - 1, rv_total_rows());
    SDL_SetWindowTitle(rv_win, buf);
}

static int rv_event_watch(void *userdata, SDL_Event *e) {
    (void)userdata;
    if (!rv_win) return 1;
    Uint32 wid = SDL_GetWindowID(rv_win);

    if (e->type == SDL_MOUSEWHEEL && e->wheel.windowID == wid) {
        if (e->wheel.y > 0)      rv_scroll -= 3;
        else if (e->wheel.y < 0) rv_scroll += 3;
        rv_clamp_scroll();
        rv_update_title();
    } else if (e->type == SDL_KEYDOWN && e->key.windowID == wid) {
        switch (e->key.keysym.sym) {
            case SDLK_UP:       rv_scroll -= 1;              break;
            case SDLK_DOWN:     rv_scroll += 1;              break;
            case SDLK_PAGEUP:   rv_scroll -= RV_VIS_ROWS;    break;
            case SDLK_PAGEDOWN: rv_scroll += RV_VIS_ROWS;    break;
            case SDLK_HOME:     rv_scroll = 0;               break;
            case SDLK_END:      rv_scroll = rv_max_scroll(); break;
            default: return 1;
        }
        rv_clamp_scroll();
        rv_update_title();
    }
    return 1;
}

/* ---------------------------------------------------------------------------
   Rendering primitives
   ------------------------------------------------------------------------- */
static inline void rv_put_char(uint32_t *px, int pitch, int cx, int cy,
                               char ch, uint32_t fg, uint32_t bg) {
    int idx = (unsigned char)ch - 32;
    if (idx < 0 || idx >= 96) return;
    const uint8_t *g = rv_font[idx];
    int ox = cx * RV_CHAR_W;
    int oy = cy * RV_CHAR_H;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            px[(oy + row) * pitch + ox + col] =
                (bits & (0x80 >> col)) ? fg : bg;
        }
    }
    for (int row = 8; row < RV_CHAR_H; row++) {
        for (int col = 0; col < 8; col++) {
            px[(oy + row) * pitch + ox + col] = bg;
        }
    }
}

static void rv_put_string(uint32_t *px, int pitch, int cx, int cy,
                          const char *s, uint32_t fg, uint32_t bg) {
    while (*s && cx < RV_COLS) {
        rv_put_char(px, pitch, cx, cy, *s, fg, bg);
        cx++;
        s++;
    }
}

/* Read a value using the same endianness as RAM_WORD/RAM_LONG
   (host-native little-endian on x86). */
static void rv_format_value(char *hexbuf, char *decbuf,
                            uint32_t addr, uint8_t fmt) {
    switch (fmt) {
    case F_U8: {
        uint8_t v = ram[addr];
        sprintf(hexbuf, "0x%02X", v);
        sprintf(decbuf, "%3u", v);
        break;
    }
    case F_S8: {
        int8_t v = (int8_t)ram[addr];
        sprintf(hexbuf, "0x%02X", (uint8_t)v);
        sprintf(decbuf, "%4d", v);
        break;
    }
    case F_U16: {
        uint16_t v = (uint16_t)ram[addr] | ((uint16_t)ram[addr+1] << 8);
        sprintf(hexbuf, "0x%04X", v);
        sprintf(decbuf, "%5u", v);
        break;
    }
    case F_S16: {
        int16_t v = (int16_t)((uint16_t)ram[addr]
                            | ((uint16_t)ram[addr+1] << 8));
        sprintf(hexbuf, "0x%04X", (uint16_t)v);
        sprintf(decbuf, "%6d", v);
        break;
    }
    case F_U32: {
        uint32_t v = (uint32_t)ram[addr]
                   | ((uint32_t)ram[addr+1] << 8)
                   | ((uint32_t)ram[addr+2] << 16)
                   | ((uint32_t)ram[addr+3] << 24);
        sprintf(hexbuf, "0x%08X", v);
        sprintf(decbuf, "%10u", v);
        break;
    }
    case F_S32: {
        int32_t v = (int32_t)((uint32_t)ram[addr]
                            | ((uint32_t)ram[addr+1] << 8)
                            | ((uint32_t)ram[addr+2] << 16)
                            | ((uint32_t)ram[addr+3] << 24));
        sprintf(hexbuf, "0x%08X", (uint32_t)v);
        sprintf(decbuf, "%11d", v);
        break;
    }
    }
}

/* ---------------------------------------------------------------------------
   Public API
   ------------------------------------------------------------------------- */
void RamView_Toggle(void) {
    if (rv_win) {
        SDL_DelEventWatch(rv_event_watch, NULL);
        if (rv_tex) SDL_DestroyTexture(rv_tex);
        if (rv_ren) SDL_DestroyRenderer(rv_ren);
        SDL_DestroyWindow(rv_win);
        rv_tex = NULL; rv_ren = NULL; rv_win = NULL;
        return;
    }
    rv_scroll = 0;
    rv_win = SDL_CreateWindow("RAM View",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              RV_W, RV_H, 0);
    if (!rv_win) return;
    rv_ren = SDL_CreateRenderer(rv_win, -1, 0);
    if (!rv_ren) { SDL_DestroyWindow(rv_win); rv_win = NULL; return; }
    rv_tex = SDL_CreateTexture(rv_ren, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING, RV_W, RV_H);
    if (!rv_tex) {
        SDL_DestroyRenderer(rv_ren);
        SDL_DestroyWindow(rv_win);
        rv_ren = NULL; rv_win = NULL;
        return;
    }
    SDL_AddEventWatch(rv_event_watch, NULL);
    rv_update_title();
}

int RamView_WindowID(void) {
    return rv_win ? (int)SDL_GetWindowID(rv_win) : -1;
}

void RamView_Render(void) {
    if (!rv_win) return;

    void *pixels;
    int pitch;
    SDL_LockTexture(rv_tex, NULL, &pixels, &pitch);
    uint32_t *px = (uint32_t *)pixels;
    int pwords = pitch / 4;

    const uint32_t BG     = 0xFF101018;
    const uint32_t HEADER = 0xFF80FFFF;
    const uint32_t NAME   = 0xFFE0E0E0;
    const uint32_t HEX    = 0xFF90D0FF;
    const uint32_t DEC    = 0xFFFFD080;
    const uint32_t SEP    = 0xFF505060;

    for (int y = 0; y < RV_H; y++)
        for (int x = 0; x < RV_W; x++)
            px[y * pwords + x] = BG;

    int cy = 0;

    /* Title */
    if ((cy - rv_scroll) >= 0 && (cy - rv_scroll) < RV_VIS_ROWS) {
        rv_put_string(px, pwords, 0, cy - rv_scroll,
                      "RAM VIEWER   HEX   DEC", HEADER, BG);
    }
    cy++;

    /* Separator */
    if ((cy - rv_scroll) >= 0 && (cy - rv_scroll) < RV_VIS_ROWS) {
        for (int x = 0; x < RV_COLS; x++)
            rv_put_char(px, pwords, x, cy - rv_scroll, '-', SEP, BG);
    }
    cy++;

    /* Sections */
    for (size_t s = 0; s < RV_SECTION_COUNT; s++) {
        const RV_Section *sec = &rv_sections[s];

        if (sec->title) {
            int y = cy - rv_scroll;
            if (y >= 0 && y < RV_VIS_ROWS) {
                rv_put_string(px, pwords, 0, y, sec->title, HEADER, BG);
            }
            cy++;
        }

        for (int i = 0; i < sec->count; i++) {
            int y = cy - rv_scroll;
            if (y >= 0 && y < RV_VIS_ROWS) {
                const RV_Watch *w = &sec->items[i];
                char hexbuf[16], decbuf[24];
                rv_format_value(hexbuf, decbuf, w->addr, w->fmt);

                int col = 2;
                rv_put_string(px, pwords, col, y, w->name, NAME, BG);
                col += (int)strlen(w->name);
                while (col < 24) { rv_put_char(px, pwords, col, y, ' ', NAME, BG); col++; }
                rv_put_string(px, pwords, col, y, hexbuf, HEX, BG);
                col += (int)strlen(hexbuf);
                while (col < 38) { rv_put_char(px, pwords, col, y, ' ', NAME, BG); col++; }
                rv_put_string(px, pwords, col, y, decbuf, DEC, BG);
            }
            cy++;
        }

        cy++;  /* blank line between sections */
    }

    SDL_UnlockTexture(rv_tex);
    SDL_SetRenderDrawColor(rv_ren, 0, 0, 0, 255);
    SDL_RenderClear(rv_ren);
    SDL_RenderCopy(rv_ren, rv_tex, NULL, NULL);
    SDL_RenderPresent(rv_ren);
}
