#include "data.h"
#include "assets.h"
#include "palette.h"
#include "constants.h"
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

/* Object mapping pointers are stored in 32-bit fields (obMap), so mapping
   data must live below 4 GB. MAP_32BIT asks the kernel for such an address. */
static uint8_t *alloc_32bit(size_t n) {
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    return (p == MAP_FAILED) ? NULL : (uint8_t *)p;
}

/* ============================================================================
   Asset pointers
   ============================================================================ */
uint8_t *Pal_SegaBG = NULL;
size_t   Pal_SegaBG_len = 0;

uint8_t *Pal_Sega1 = NULL;
size_t   Pal_Sega1_len = 0;

uint8_t *Pal_Sega2 = NULL;
size_t   Pal_Sega2_len = 0;

uint8_t *Nem_SegaLogo = NULL;
size_t   Nem_SegaLogo_len = 0;

uint8_t *Eni_SegaLogo = NULL;
size_t   Eni_SegaLogo_len = 0;

uint8_t *Pal_Title = NULL;
size_t   Pal_Title_len = 0;

uint8_t *Pal_LevelSel = NULL;
size_t   Pal_LevelSel_len = 0;

uint8_t *Pal_Sonic = NULL;
size_t   Pal_Sonic_len = 0;

uint8_t *Nem_JapNames = NULL;
size_t   Nem_JapNames_len = 0;

uint8_t *Eni_JapNames = NULL;
size_t   Eni_JapNames_len = 0;

uint8_t *Nem_CreditText = NULL;
size_t   Nem_CreditText_len = 0;

uint8_t *Nem_TitleFg = NULL;
size_t   Nem_TitleFg_len = 0;

uint8_t *Nem_TitleSonic = NULL;
size_t   Nem_TitleSonic_len = 0;

uint8_t *Nem_TitleTM = NULL;
size_t   Nem_TitleTM_len = 0;

uint8_t *Art_Text = NULL;
size_t   Art_Text_len = 0;

uint8_t *Blk16_GHZ = NULL;
size_t   Blk16_GHZ_len = 0;

uint8_t *Blk256_GHZ = NULL;
size_t   Blk256_GHZ_len = 0;

uint8_t *Eni_Title = NULL;
size_t   Eni_Title_len = 0;

uint8_t *Nem_GHZ_1st = NULL;
size_t   Nem_GHZ_1st_len = 0;

/* ============================================================================
   Asset loading
   ============================================================================ */

static int load_asset(const char *name, uint8_t **out_ptr, size_t *out_len);

static int load_asm_asset(const char *name, uint8_t **out_ptr, size_t *out_len, int is_map);

static int load_asset(const char *name, uint8_t **out_ptr, size_t *out_len) {
    char path[512];
    snprintf(path, sizeof(path), "./assets/%s", name);
    uint8_t *buf = Assets_Load(path, out_len);
    if (!buf) {
        fprintf(stderr, "[Data] Failed to load asset: %s\n", path);
        return -1;
    }
    *out_ptr = buf;
    return 0;
}

int Data_Init(void) {
    Pal_SegaBG = NULL;
    Pal_SegaBG_len = 0;
    Pal_Sega1 = NULL;
    Pal_Sega1_len = 0;
    Pal_Sega2 = NULL;
    Pal_Sega2_len = 0;
    Nem_SegaLogo = NULL;
    Nem_SegaLogo_len = 0;
    Eni_SegaLogo = NULL;
    Eni_SegaLogo_len = 0;
    Pal_Title = NULL;
    Pal_Title_len = 0;
    Pal_LevelSel = NULL;
    Pal_LevelSel_len = 0;
    Pal_Sonic = NULL;
    Pal_Sonic_len = 0;
    Nem_JapNames = NULL;
    Nem_JapNames_len = 0;
    Eni_JapNames = NULL;
    Eni_JapNames_len = 0;
    Nem_CreditText = NULL;
    Nem_CreditText_len = 0;
    Nem_TitleFg = NULL;
    Nem_TitleFg_len = 0;
    Nem_TitleSonic = NULL;
    Nem_TitleSonic_len = 0;
    Nem_TitleTM = NULL;
    Nem_TitleTM_len = 0;
    Art_Text = NULL;
    Art_Text_len = 0;
    Blk16_GHZ = NULL;
    Blk16_GHZ_len = 0;
    Blk256_GHZ = NULL;
    Blk256_GHZ_len = 0;
    Eni_Title = NULL;
    Eni_Title_len = 0;
    Nem_GHZ_1st = NULL;
    Nem_GHZ_1st_len = 0;

    if (load_asset("palette/sega_bg.bin", &Pal_SegaBG, &Pal_SegaBG_len) != 0) {
        Pal_SegaBG = NULL;
        Pal_SegaBG_len = 0;
    }

    if (load_asset("palette/sega1.bin", &Pal_Sega1, &Pal_Sega1_len) != 0) {
        Pal_Sega1 = NULL;
        Pal_Sega1_len = 0;
    }

    if (load_asset("palette/sega2.bin", &Pal_Sega2, &Pal_Sega2_len) != 0) {
        Pal_Sega2 = NULL;
        Pal_Sega2_len = 0;
    }

    if (load_asset("artnem/sega_logo.nem", &Nem_SegaLogo, &Nem_SegaLogo_len) != 0) {
        Nem_SegaLogo = NULL;
        Nem_SegaLogo_len = 0;
    }

    if (load_asset("tilemaps/sega_logo.eni", &Eni_SegaLogo, &Eni_SegaLogo_len) != 0) {
        Eni_SegaLogo = NULL;
        Eni_SegaLogo_len = 0;
    }

    if (load_asset("palette/title.bin", &Pal_Title, &Pal_Title_len) != 0) {
        Pal_Title = NULL;
        Pal_Title_len = 0;
    }

    if (load_asset("palette/level_select.bin", &Pal_LevelSel, &Pal_LevelSel_len) != 0) {
        Pal_LevelSel = NULL;
        Pal_LevelSel_len = 0;
    }

    if (load_asset("palette/sonic.bin", &Pal_Sonic, &Pal_Sonic_len) != 0) {
        Pal_Sonic = NULL;
        Pal_Sonic_len = 0;
    }

    if (load_asset("artnem/title_fg.nem", &Nem_TitleFg, &Nem_TitleFg_len) != 0) {
        Nem_TitleFg = NULL;
        Nem_TitleFg_len = 0;
    }

    if (load_asset("artnem/title_sonic.nem", &Nem_TitleSonic, &Nem_TitleSonic_len) != 0) {
        Nem_TitleSonic = NULL;
        Nem_TitleSonic_len = 0;
    }

    if (load_asset("artnem/title_tm.nem", &Nem_TitleTM, &Nem_TitleTM_len) != 0) {
        Nem_TitleTM = NULL;
        Nem_TitleTM_len = 0;
    }

    if (load_asset("artnem/ghz1.nem", &Nem_GHZ_1st, &Nem_GHZ_1st_len) != 0) {
        Nem_GHZ_1st = NULL;
        Nem_GHZ_1st_len = 0;
    }

    if (load_asm_asset("anim/titlesonic.asm", &Ani_TSon, &Ani_TSon_len, 0) != 0) {
        Ani_TSon = NULL;
        Ani_TSon_len = 0;
    }

    if (load_asm_asset("anim/psbtm.asm", &Ani_PSBTM, &Ani_PSBTM_len, 0) != 0) {
        Ani_PSBTM = NULL;
        Ani_PSBTM_len = 0;
    }

    if (load_asm_asset("maps/titlesonic.asm", &Map_TSon, &Map_TSon_len, 1) != 0) {
        Map_TSon = NULL;
        Map_TSon_len = 0;
    }

    if (load_asm_asset("maps/psbtm.asm", &Map_PSB, &Map_PSB_len, 1) != 0) {
        Map_PSB = NULL;
        Map_PSB_len = 0;
    }

    if (load_asset("tilemaps/title.eni", &Eni_Title, &Eni_Title_len) != 0) {
        Eni_Title = NULL;
        Eni_Title_len = 0;
    }

    if (load_asset("map16/ghz.eni", &Blk16_GHZ, &Blk16_GHZ_len) != 0) {
        Blk16_GHZ = NULL;
        Blk16_GHZ_len = 0;
    }

    if (load_asset("map256/ghz.kos", &Blk256_GHZ, &Blk256_GHZ_len) != 0) {
        Blk256_GHZ = NULL;
        Blk256_GHZ_len = 0;
    }

    if (load_asset("artnem/jap_credits.nem", &Nem_JapNames, &Nem_JapNames_len) != 0) {
        Nem_JapNames = NULL;
        Nem_JapNames_len = 0;
    }

    if (load_asset("tilemaps/jap_credits.eni", &Eni_JapNames, &Eni_JapNames_len) != 0) {
        Eni_JapNames = NULL;
        Eni_JapNames_len = 0;
    }

    if (load_asset("artnem/credit_text.nem", &Nem_CreditText, &Nem_CreditText_len) != 0) {
        Nem_CreditText = NULL;
        Nem_CreditText_len = 0;
    }

    if (load_asset("artunc/Level Select & Debug Text.unc", &Art_Text, &Art_Text_len) != 0) {
        Art_Text = NULL;
        Art_Text_len = 0;
    }

    Palette_Init();
    return 0;
}

void Data_Quit(void) {
#define FREE_ASSET(p) do { Assets_Free(p); p = NULL; } while(0)
    FREE_ASSET(Pal_SegaBG);
    FREE_ASSET(Pal_Sega1);
    FREE_ASSET(Pal_Sega2);
    FREE_ASSET(Nem_SegaLogo);
    FREE_ASSET(Eni_SegaLogo);
    FREE_ASSET(Pal_Title);
    FREE_ASSET(Pal_LevelSel);
    FREE_ASSET(Pal_Sonic);
    FREE_ASSET(Nem_JapNames);
    FREE_ASSET(Eni_JapNames);
    FREE_ASSET(Nem_CreditText);
    FREE_ASSET(Nem_TitleFg);
    FREE_ASSET(Nem_TitleSonic);
    FREE_ASSET(Nem_TitleTM);
    FREE_ASSET(Art_Text);
    FREE_ASSET(Blk16_GHZ);
    FREE_ASSET(Blk256_GHZ);
    FREE_ASSET(Eni_Title);
    FREE_ASSET(Nem_GHZ_1st);
#undef FREE_ASSET
}

/* Cheat data remains static for now */
const uint8_t LevSelCode_US[] = {btnUp, btnDn, btnL, btnR, 0, 0xFF};
const uint32_t LevSelCode_US_len = 6;

const uint8_t LevSelCode_J[] = {btnUp, btnDn, btnL, btnR, 0, 0xFF};
const uint32_t LevSelCode_J_len = 6;

const uint16_t LevSel_Ptrs[] = {
  0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
  0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00,
  0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700,
  0x1800, 0x1900, 0x1A00, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x1F00,
  0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700,
  0x2800, 0x2900, 0x2A00, 0x2B00, 0x2C00, 0x2D00, 0x2E00, 0x2F00,
  0x3000, 0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700,
  0x3800, 0x3900, 0x3A00, 0x3B00, 0x3C00, 0x3D00, 0x3E00, 0x3F00,
  0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4700,
  0x4800, 0x4900, 0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00,
  0x5000, 0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5600, 0x5700,
  0x5800, 0x5900, 0x5A00, 0x5B00, 0x5C00, 0x5D00, 0x5E00, 0x5F00,
  0x6000, 0x6100, 0x6200, 0x6300, 0x6400, 0x6500, 0x6600, 0x6700,
  0x6800, 0x6900, 0x6A00, 0x6B00, 0x6C00, 0x6D00, 0x6E00, 0x6F00,
  0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500, 0x7600, 0x7700,
  0x7800, 0x7900, 0x7A00, 0x7B00, 0x7C00, 0x7D00, 0x7E00, 0x7F00,
  0x8000 | id_SS, 0x8100 | id_SS
};
const uint32_t LevSel_Ptrs_len = sizeof(LevSel_Ptrs);

/* ===========================================================================
   Title screen animation scripts (from _anim slash .asm)
   Format: word offset to animation data, then duration + frame IDs/flags
   =========================================================================== */

uint8_t *Ani_TSon = NULL;
size_t   Ani_TSon_len = 0;

uint8_t *Ani_PSBTM = NULL;
size_t   Ani_PSBTM_len = 0;

/* ===========================================================================
   Title screen sprite mappings (from _maps slash .asm)
   These are loaded at runtime from assets/
   =========================================================================== */

uint8_t *Map_TSon = NULL;
size_t   Map_TSon_len = 0;

uint8_t *Map_PSB = NULL;
size_t   Map_PSB_len = 0;

/* ===========================================================================
   ASM parser for original Sonic 1 anim/map assets
   =========================================================================== */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_comments_and_spaces(const char *p) {
    while (*p) {
        if (*p == ';') {
            while (*p && *p != '\n') p++;
        } else if (isspace((unsigned char)*p)) {
            p++;
        } else {
            break;
        }
    }
    return p;
}

static long parse_asm_number(const char *p, const char **end) {
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '-') {
        const char *e2 = NULL;
        long neg = parse_asm_number(p + 1, &e2);
        if (end) *end = e2;
        return -neg;
    }
    if (*p == '$') {
        p++;
        long val = 0;
        while (isxdigit((unsigned char)*p)) {
            val = val * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
            p++;
        }
        if (end) *end = p;
        return val;
    }
    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        long val = 0;
        while (isxdigit((unsigned char)*p)) {
            val = val * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
            p++;
        }
        if (end) *end = p;
        return val;
    }
    /* Symbolic animation flags used in dc.b lines (afBack, afEnd, ...) */
    static const char *af_names[] = { "afBack", "afEnd", "afChange",
                                      "afRoutine", "afReset", "af2ndRoutine", "afWait" };
    static const long  af_vals[]   = { 0xFE, 0xFF, 0xFD, 0xFC, 0xFB, 0xFA, 0x80 };
    const char *sym = p;
    while (*sym && isalnum((unsigned char)*sym)) sym++;
    size_t sym_len = (size_t)(sym - p);
    for (int i = 0; i < 7; i++) {
        size_t n = strlen(af_names[i]);
        if (sym_len == n && strncmp(p, af_names[i], n) == 0) {
            if (end) *end = sym;
            return af_vals[i];
        }
    }
    char *ep = NULL;
    long val = strtol(p, &ep, 10);
    if (ep && end) *end = ep;
    return val;
}

static int is_directive(const char *line, const char *dir) {
    const char *p = skip_comments_and_spaces(line);
    if (p[0] == '\0') return 0;
    size_t len = strlen(dir);
    if (strncmp(p, dir, len) != 0) return 0;
    if (!isspace((unsigned char)p[len]) && p[len] != '\0') return 0;
    return 1;
}

/* Skip a leading "label:" prefix and return the first non-space character of
   the instruction that follows.  Returns the original pointer when the line
   has no label prefix. */
static const char *strip_label(const char *line) {
    const char *p = line;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.')) p++;
    if (*p != ':') return line;
    const char *ins = p + 1;
    while (*ins && isspace((unsigned char)*ins)) ins++;
    return ins;
}

/* Extract the target label and optional signed delta from a table entry
   expression such as ".titlesonic-Ani_TSon" or ".psb+1". */
static void parse_table_expr(const char *s, char *label_out, size_t label_sz, int *delta_out) {
    *delta_out = 0;
    if (label_sz == 0) return;
    size_t n = 0;
    while (*s && isspace((unsigned char)*s)) s++;
    while (*s && (isalnum((unsigned char)*s) || *s == '_' || *s == '.') && n + 1 < label_sz) {
        label_out[n++] = *s++;
    }
    label_out[n] = '\0';
    if (*s == '+' || *s == '-') {
        char *e2 = NULL;
        long d = strtol(s, &e2, 10);
        if (e2 != s) *delta_out = (int)d;
    }
}

typedef struct {
    char name[64];
    uint8_t *bytes;
    size_t len;
} AnimSeg;

static uint8_t *parse_anim_asm(const char *text, size_t text_len, size_t *out_len) {
    (void)text_len;
    const char *p = text;
    AnimSeg segs[16];
    int seg_count = 0;
    char table_name[16][64];
    int table_delta[16];
    int table_count = 0;

    while (*p) {
        p = skip_comments_and_spaces(p);
        if (*p == '\0') break;

        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(lp);
        if (*ins == '\0') continue;

        if (is_directive(ins, "dc.w")) {
            const char *dp = ins + 4;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (table_count < 16) {
                parse_table_expr(dp, table_name[table_count], 64, &table_delta[table_count]);
                table_count++;
            }
            continue;
        }

        if (is_directive(ins, "dc.b")) {
            const int has_label = (strip_label(lp) != lp);
            if (has_label) {
                if (seg_count < 16) {
                    segs[seg_count].bytes = NULL;
                    segs[seg_count].len = 0;
                    const char *q = lp;
                    size_t nn = 0;
                    while (*q && (isalnum((unsigned char)*q) || *q == '_' || *q == '.') && nn + 1 < 64)
                        segs[seg_count].name[nn++] = *q++;
                    segs[seg_count].name[nn] = '\0';
                    seg_count++;
                }
            } else if (seg_count == 0) {
                if (seg_count < 16) {
                    segs[seg_count].name[0] = '\0';
                    segs[seg_count].bytes = NULL;
                    segs[seg_count].len = 0;
                    seg_count++;
                }
            }
            if (seg_count == 0) continue;

            AnimSeg *sg = &segs[seg_count - 1];
            const char *dp = ins + 4;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (*dp == '<') dp++;
            while (*dp) {
                dp = skip_comments_and_spaces(dp);
                if (*dp == '\0' || *dp == ';') break;
                const char *val_end = NULL;
                long val = parse_asm_number(dp, &val_end);
                if (val_end == dp) break;
                long b = val;
                if (b < 0) b = 0;
                uint8_t *nb = (uint8_t *)realloc(sg->bytes, sg->len + 1);
                if (!nb) break;
                sg->bytes = nb;
                sg->bytes[sg->len++] = (uint8_t)(b & 0xFF);
                dp = val_end;
                while (*dp && (isspace((unsigned char)*dp) || *dp == ',')) dp++;
            }
        }
    }

    /* Layout: table of word offsets first, then each referenced frame block. */
    size_t total = 2 * (size_t)table_count;
    size_t seg_pos[16];
    int ref_idx[16];
    size_t cursor = total;

    for (int j = 0; j < seg_count; j++) seg_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < seg_count; j++) {
            if (strcmp(segs[j].name, table_name[k]) == 0) {
                idx = j;
                break;
            }
        }
        if (idx < 0 && k < seg_count) idx = k;
        ref_idx[k] = idx;
        if (idx < 0) continue;
        if (seg_pos[idx] == (size_t)-1) {
            seg_pos[idx] = cursor;
            cursor += segs[idx].len;
        }
    }
    for (int j = 0; j < seg_count; j++) {
        if (seg_pos[j] == (size_t)-1) {
            seg_pos[j] = cursor;
            cursor += segs[j].len;
        }
    }

    *out_len = cursor;
    if (cursor == 0) return NULL;

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) { *out_len = 0; return NULL; }

    for (int k = 0; k < table_count; k++) {
        size_t off = (ref_idx[k] >= 0) ? seg_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < seg_count; j++)
        memcpy(out + seg_pos[j], segs[j].bytes, segs[j].len);

    for (int j = 0; j < seg_count; j++) free(segs[j].bytes);
    return out;
}

typedef struct {
    char name[64];
    uint8_t *pieces;
    size_t count;
} MapFrame;

static uint8_t *parse_map_asm(const char *text, size_t text_len, size_t *out_len) {
    (void)text_len;
    const char *p = text;
    MapFrame frames[32];
    int frame_count = 0;
    char table_name[32][64];
    int table_delta[32];
    int table_count = 0;

    while (*p) {
        p = skip_comments_and_spaces(p);
        if (*p == '\0') break;

        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(lp);
        if (*ins == '\0') continue;

        if (is_directive(ins, "mappingsTableEntry.w")) {
            const char *dp = ins + 20;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (table_count < 32) {
                parse_table_expr(dp, table_name[table_count], 64, &table_delta[table_count]);
                table_count++;
            }
            continue;
        }

        if (is_directive(ins, "spriteHeader")) {
            if (frame_count < 32) {
                frames[frame_count].name[0] = '\0';
                if (strip_label(lp) != lp) {
                    const char *q = lp;
                    size_t nn = 0;
                    while (*q && (isalnum((unsigned char)*q) || *q == '_' || *q == '.') && nn + 1 < 64)
                        frames[frame_count].name[nn++] = *q++;
                    frames[frame_count].name[nn] = '\0';
                }
                frames[frame_count].pieces = NULL;
                frames[frame_count].count = 0;
                frame_count++;
            }
            continue;
        }

        if (is_directive(ins, "spritePiece")) {
            if (frame_count == 0) continue;
            MapFrame *fr = &frames[frame_count - 1];
            const char *dp = ins + 11;
            const char *args[9];
            int arg_idx = 0;
            while (*dp && arg_idx < 9) {
                dp = skip_comments_and_spaces(dp);
                if (*dp == '\0' || *dp == ';') break;
                const char *val_end2 = NULL;
                parse_asm_number(dp, &val_end2);
                if (val_end2 == dp) break;
                args[arg_idx++] = dp;
                if (arg_idx >= 9) break;
                dp = val_end2;
                while (*dp && isspace((unsigned char)*dp)) dp++;
                if (*dp == ',') dp++;
            }
            if (arg_idx < 5) continue;

            long x = parse_asm_number(args[0], NULL);
            long y = parse_asm_number(args[1], NULL);
            long w = parse_asm_number(args[2], NULL);
            long h = parse_asm_number(args[3], NULL);
            long tile = parse_asm_number(args[4], NULL);

            if (w < 1) w = 1;
            if (h < 1) h = 1;
            if (w > 16) w = 16;
            if (h > 16) h = 16;

            uint8_t piece[5];
            piece[0] = (uint8_t)(y & 0xFF);
            piece[1] = (uint8_t)((((w - 1) & 0xF) << 4) | ((h - 1) & 0xF));
            piece[2] = (uint8_t)((tile >> 8) & 0xFF);
            piece[3] = (uint8_t)(tile & 0xFF);
            piece[4] = (uint8_t)(x & 0xFF);

            uint8_t *np = (uint8_t *)realloc(fr->pieces, (fr->count + 1) * 5);
            if (!np) continue;
            fr->pieces = np;
            memcpy(fr->pieces + fr->count * 5, piece, 5);
            fr->count++;
        }
    }

    /* Layout: table of word offsets first, then per-frame [count][pieces]. */
    size_t total = 2 * (size_t)table_count;
    size_t frame_pos[32];
    int ref_idx[32];
    size_t cursor = total;

    for (int j = 0; j < frame_count; j++) frame_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < frame_count; j++) {
            if (strcmp(frames[j].name, table_name[k]) == 0) {
                idx = j;
                break;
            }
        }
        if (idx < 0 && k < frame_count) idx = k;
        ref_idx[k] = idx;
        if (idx < 0) continue;
        if (frame_pos[idx] == (size_t)-1) {
            frame_pos[idx] = cursor;
            cursor += 1 + frames[idx].count * 5;
        }
    }
    for (int j = 0; j < frame_count; j++) {
        if (frame_pos[j] == (size_t)-1) {
            frame_pos[j] = cursor;
            cursor += 1 + frames[j].count * 5;
        }
    }

    *out_len = cursor;
    if (cursor == 0) return NULL;

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) { *out_len = 0; return NULL; }

    for (int k = 0; k < table_count; k++) {
        size_t off = (ref_idx[k] >= 0) ? frame_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < frame_count; j++) {
        MapFrame *fr = &frames[j];
        out[frame_pos[j]] = (uint8_t)fr->count;
        memcpy(out + frame_pos[j] + 1, fr->pieces, fr->count * 5);
    }

    for (int j = 0; j < frame_count; j++) free(frames[j].pieces);
    return out;
}

static int load_asm_asset(const char *name, uint8_t **out_ptr, size_t *out_len, int is_map) {
    char path[512];
    snprintf(path, sizeof(path), "./assets/%s", name);
    size_t text_len = 0;
    char *text = (char *)Assets_Load(path, &text_len);
    if (!text) {
        fprintf(stderr, "[Data] Failed to load ASM asset: %s\n", path);
        return -1;
    }

    uint8_t *data = NULL;
    size_t data_len = 0;
    if (is_map) {
        data = parse_map_asm(text, text_len, &data_len);
    } else {
        data = parse_anim_asm(text, text_len, &data_len);
    }

    free(text);

    if (!data || data_len == 0) {
        fprintf(stderr, "[Data] Failed to parse ASM asset: %s\n", path);
        return -1;
    }

    /* Move the parsed buffer into 32-bit addressable space (needed by obMap). */
    uint8_t *low = alloc_32bit(data_len);
    if (!low) {
        fprintf(stderr, "[Data] mmap MAP_32BIT failed for: %s\n", path);
        free(data);
        return -1;
    }
    memcpy(low, data, data_len);
    free(data);

    *out_ptr = low;
    *out_len = data_len;
    return 0;
}

