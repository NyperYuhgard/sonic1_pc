#include "palette.h"
#include "ram.h"
#include "constants.h"
#include "data.h"
#include <string.h>
#include <stdio.h>

uint16_t palette_main[PALETTE_TOTAL];
uint16_t palette_water[PALETTE_TOTAL];
uint16_t palette_fading[PALETTE_TOTAL];

void Palette_LoadFromData(const uint8_t *data, uint16_t *dest, int count) {
    for (int i = 0; i < count; i++) {
        dest[i] = ((uint16_t)data[i * 2] << 8) | data[i * 2 + 1];
    }
}

/* Palette index entry: ROM data pointer, target RAM address, count (longwords - 1) */
typedef struct {
    const uint8_t *data;
    uint16_t       target_ram_offset; /* offset into ram[] (e.g. v_palette_line_1) */
    uint16_t       count;             /* longwords - 1 */
} PalEntry;

static const PalEntry pal_index[] = {
    /* palid_SegaBG */  { Pal_SegaBG,  v_palette_line_1, (128 / 4) - 1 },
    /* palid_Title */   { NULL,        v_palette_line_1, 0 },
    /* palid_LevelSel */{ NULL,        v_palette_line_1, 0 },
    /* palid_Sonic */   { NULL,        v_palette_line_1, 0 },
};

void PalLoad(int index) {
    if (index < 0 || index >= (int)(sizeof(pal_index) / sizeof(pal_index[0])))
        return;
    const PalEntry *e = &pal_index[index];
    if (!e->data) return;

    uint16_t *dest = (uint16_t *)RAM_ADDR(e->target_ram_offset);
    int longwords = e->count + 1;
    for (int i = 0; i < longwords * 2; i += 2) {
        /* Each longword = 2 colors = 4 bytes */
        uint8_t b0 = e->data[i * 2];
        uint8_t b1 = e->data[i * 2 + 1];
        uint8_t b2 = e->data[i * 2 + 2];
        uint8_t b3 = e->data[i * 2 + 3];
        dest[i]     = ((uint16_t)b0 << 8) | b1;
        dest[i + 1] = ((uint16_t)b2 << 8) | b3;
    }
}

/* Copy v_palette (RAM) to palette_main (rendering buffer) */
void Palette_Update(void) {
    memcpy(palette_main, RAM_ADDR(v_palette), sizeof(palette_main));
}

/* ============================================================================
   Palette Fade Out (from _inc/Palette Fading.asm - PaletteFadeOut)
   Fades the active palette (v_palette) to black over 22 frames.
   Fades red first, then green, then blue per color.
   ============================================================================ */

static void fade_out_dec_color(uint16_t *color) {
    uint16_t c = *color;
    if (c == 0) return; /* already black */

    /* CRAM layout: R=bits1-3, G=bits5-7, B=bits9-11 (0BGR) */
    /* Try to decrease red (bits 1-3) */
    if (c & 0x00E) {
        *color = c - 0x002;
        return;
    }
    /* Try to decrease green (bits 5-7) */
    if (c & 0x0E0) {
        *color = c - 0x020;
        return;
    }
    /* Try to decrease blue (bits 9-11) */
    if (c & 0xE00) {
        *color = c - 0x200;
    }
}

void Palette_FadeOut(void) {
    uint16_t *pal = (uint16_t *)RAM_ADDR(v_palette);
    int num_colors = 64; /* all 4 palette lines */

    for (int frame = 0; frame < 22; frame++) {
        /* Wait for next frame */
        extern void WaitForVBlank(void);
        WaitForVBlank();

        /* Fade each color one step closer to black */
        for (int i = 0; i < num_colors; i++) {
            fade_out_dec_color(&pal[i]);
        }
        /* Also fade water palette */
        uint16_t *pal_water = (uint16_t *)RAM_ADDR(v_palette_water);
        for (int i = 0; i < num_colors; i++) {
            fade_out_dec_color(&pal_water[i]);
        }
    }
}

/* ============================================================================
 *  PalCycle_Sega (from sonic.asm lines 1565-1664)
 *  Two-phase palette animation for the Sega screen:
 *  Phase 1: Light scan effect (Pal_Sega1 colors sweep across palette lines 2-4)
 *  Phase 2: Fade-in (4 color sets from Pal_Sega2 progressively replace all lines)
 *  Returns nonzero while active, 0 when complete.
 *  ============================================================================ */

int PalCycle_Sega(void) {
    uint16_t *line1 = (uint16_t *)RAM_ADDR(v_palette_line_1);
    uint16_t *line2 = (uint16_t *)RAM_ADDR(v_palette_line_2);

    /* The Sega animation color tables are stored as big-endian byte arrays;
     *      read them as native words the same way PalLoad does. */
    #define SEGA_COLOR(tab, i) ((uint16_t)((uint16_t)(tab)[(i)*2] << 8) | (tab)[(i)*2+1])

    /* v_pcyc_num / v_pcyc_time are native lvalues (see ram.h).
     *      v_pcyc_time packs two fields for the fade-in phase:
     *        flag  = (word & 0x00FF)  -> 1 once the light scan is done
     *        delay = (word >> 8)      -> frames to wait between fade-in steps     */
    int scan_done = (v_pcyc_time & 0xFF) != 0;

    if (!scan_done) {
        /* ---------------- Phase 1: light scan effect ---------------- */
        int d1 = 5; /* 6 colors - 1 */

        int d0 = (int16_t)v_pcyc_num;

        /* Find scan start position */
        int a0 = 0; /* index into Pal_Sega1 (uint16_t slots) */
        while (d0 < 0) {
            a0++;
            d1--;
            d0 += 2;
        }

        /* Write light scan colors.
         *          NOTE: a0 only advances when a color is actually consumed/written,
         *          mirroring the original's move.w (a0)+,... postincrement semantics. */
        while (d1 >= 0) {
            int d2 = d0 & 0x1E;
            if (d2 == 0) {
                d0 += 2; /* skip transparent color at start of each line */
            }
            if (d0 < 0x60) {
                line2[d0 / 2] = SEGA_COLOR(Pal_Sega1, a0);
            }
            a0++;
            d0 += 2;
            d1--;
        }

        /* Advance position */
        d0 = (int16_t)v_pcyc_num;
        d0 += 2;
        int d2 = d0 & 0x1E;
        if (d2 == 0) {
            d0 += 2;
        }

        /* Check if scan is done */
        if (d0 >= 0x64) {
            v_pcyc_time = (4 << 8) | 1; /* delay=4 (high), flag=1 (low) */
            d0 = -12; /* starting offset for fade-in */
        }

        v_pcyc_num = (uint16_t)(int16_t)d0;
        return 1; /* still active */
    }

    /* ---------------- Phase 2: fade-in ---------------- */
    uint8_t delay = (v_pcyc_time >> 8) & 0xFF;
    if (delay != 0) {
        /* decrement delay without touching the flag in the low byte */
        v_pcyc_time = (v_pcyc_time & 0x00FF) | ((uint16_t)(delay - 1) << 8);
        return 1; /* waiting for delay */
    }

    /* Reset delay */
    v_pcyc_time = (v_pcyc_time & 0x00FF) | (4 << 8);

    int d0 = (int16_t)v_pcyc_num;
    d0 += 12; /* advance to next color set (6 colors * 2 bytes = 12) */

    if (d0 >= 48) {
        /* All 4 color sets done.
         *          NOTE: the original does NOT write v_pcyc_num here (only sets d0=0
         *          as the return-value signal in a register) - leave it untouched. */
        return 0; /* done */
    }

    v_pcyc_num = (uint16_t)(int16_t)d0;

    /* Pal_Sega2 is 24 colors (48 bytes, 4 sets of 6), stored big-endian */
    int a0 = d0 / 2; /* color index into Pal_Sega2's uint16 slots */

    /* Write 5 colors starting at v_palette_line_1 + 2 (skip transparent and white) */
    uint16_t *a1 = line1 + 2; /* +2 words = skip color 0 (transparent) and color 1 (white) */
    a1[0] = SEGA_COLOR(Pal_Sega2, a0 + 0);
    a1[1] = SEGA_COLOR(Pal_Sega2, a0 + 1);
    a1[2] = SEGA_COLOR(Pal_Sega2, a0 + 2);
    a1[3] = SEGA_COLOR(Pal_Sega2, a0 + 3);
    a1[4] = SEGA_COLOR(Pal_Sega2, a0 + 4);

    /* Fill remaining palette (lines 2-4) with 6th color.
     *      d0 here is relative to v_palette_line_2 (== line2), starting at 0,
     *      and runs for ((line4-line1)/2)-3 = 0x2C+1 = 45 iterations, matching
     *      the original's `dbf` loop (moveq #$2C,d1 -> executes 0x2C+1 times). */
    uint16_t fill_color = SEGA_COLOR(Pal_Sega2, a0 + 5);
    int iterations = 0x2C + 1; /* 45 */
    int offset = 0;
    for (int i = 0; i < iterations; i++) {
        int d2 = offset & 0x1E;
        if (d2 == 0) {
            offset += 2; /* skip transparent color at start of each line */
        }
        line2[offset / 2] = fill_color;
        offset += 2;
    }

    return 1; /* still active */
}
