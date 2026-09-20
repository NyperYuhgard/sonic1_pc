#include "palette.h"
#include "ram.h"
#include "constants.h"
#include "data.h"
#include "vdp.h"
#include "plc.h"
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

/* Palette index entry: data pointer, target RAM address, count (longwords - 1) */
typedef struct {
    uint8_t *data;
    uint16_t       target_ram_offset; /* offset into ram[] (e.g. v_palette_line_1) */
    uint16_t       count;             /* longwords - 1 */
} PalEntry;

static PalEntry pal_index[20];

/* Number of longwords minus 1 for a palette buffer of `len` bytes, exactly
   like the ASM (makePalEntry: "(end-start)/4-1"). 0 for empty data. */
static uint16_t pal_count(size_t len) {
    return len ? (uint16_t)(len / 2 - 1) : 0;
}

void Palette_Init(void) {
    /* Pal_Index — 1:1 with disasm/_inc/Palette Index.asm */
    pal_index[0].data = Pal_SegaBG;
    pal_index[0].target_ram_offset = v_palette_line_1;
    pal_index[0].count = pal_count(Pal_SegaBG_len);

    pal_index[1].data = Pal_Title;
    pal_index[1].target_ram_offset = v_palette_line_1;
    pal_index[1].count = pal_count(Pal_Title_len);

    pal_index[2].data = Pal_LevelSel;
    pal_index[2].target_ram_offset = v_palette_line_1;
    pal_index[2].count = pal_count(Pal_LevelSel_len);

    pal_index[3].data = Pal_Sonic;
    pal_index[3].target_ram_offset = v_palette_line_1;
    pal_index[3].count = pal_count(Pal_Sonic_len);

    pal_index[4].data = Pal_GHZ;
    pal_index[4].target_ram_offset = v_palette_line_2;
    pal_index[4].count = pal_count(Pal_GHZ_len);

    pal_index[5].data = Pal_LZ;
    pal_index[5].target_ram_offset = v_palette_line_2;
    pal_index[5].count = pal_count(Pal_LZ_len);

    pal_index[6].data = Pal_MZ;
    pal_index[6].target_ram_offset = v_palette_line_2;
    pal_index[6].count = pal_count(Pal_MZ_len);

    pal_index[7].data = Pal_SLZ;
    pal_index[7].target_ram_offset = v_palette_line_2;
    pal_index[7].count = pal_count(Pal_SLZ_len);

    pal_index[8].data = Pal_SYZ;
    pal_index[8].target_ram_offset = v_palette_line_2;
    pal_index[8].count = pal_count(Pal_SYZ_len);

    pal_index[9].data = Pal_SBZ1;
    pal_index[9].target_ram_offset = v_palette_line_2;
    pal_index[9].count = pal_count(Pal_SBZ1_len);

    pal_index[10].data = Pal_Special;
    pal_index[10].target_ram_offset = v_palette_line_1;
    pal_index[10].count = pal_count(Pal_Special_len);

    pal_index[11].data = Pal_LZWater;
    pal_index[11].target_ram_offset = v_palette_line_1;
    pal_index[11].count = pal_count(Pal_LZWater_len);

    pal_index[12].data = Pal_SBZ3;
    pal_index[12].target_ram_offset = v_palette_line_2;
    pal_index[12].count = pal_count(Pal_SBZ3_len);

    pal_index[13].data = Pal_SBZ3Water;
    pal_index[13].target_ram_offset = v_palette_line_1;
    pal_index[13].count = pal_count(Pal_SBZ3Water_len);

    pal_index[14].data = Pal_SBZ2;
    pal_index[14].target_ram_offset = v_palette_line_2;
    pal_index[14].count = pal_count(Pal_SBZ2_len);

    pal_index[15].data = Pal_LZSonWater;
    pal_index[15].target_ram_offset = v_palette_line_1;
    pal_index[15].count = pal_count(Pal_LZSonWater_len);

    pal_index[16].data = Pal_SBZ3SonWat;
    pal_index[16].target_ram_offset = v_palette_line_1;
    pal_index[16].count = pal_count(Pal_SBZ3SonWat_len);

    pal_index[17].data = Pal_SSResult;
    pal_index[17].target_ram_offset = v_palette_line_1;
    pal_index[17].count = pal_count(Pal_SSResult_len);

    pal_index[18].data = Pal_Continue;
    pal_index[18].target_ram_offset = v_palette_line_1;
    pal_index[18].count = pal_count(Pal_Continue_len);

    pal_index[19].data = Pal_Ending;
    pal_index[19].target_ram_offset = v_palette_line_1;
    pal_index[19].count = pal_count(Pal_Ending_len);
}

void PalLoad(int index) {
    if (index < 0 || index >= (int)(sizeof(pal_index) / sizeof(pal_index[0])))
        return;
    const PalEntry *e = &pal_index[index];
    if (!e->data) return;

    uint16_t *dest = (uint16_t *)RAM_ADDR(e->target_ram_offset);
    int words = e->count + 1;
    for (int i = 0; i < words; i++) {
        dest[i] = ((uint16_t)e->data[i * 2] << 8) | e->data[i * 2 + 1];
    }

    int cram_offset = (int)(e->target_ram_offset - v_palette);
    if (cram_offset >= 0 && cram_offset <= CRAM_SIZE) {
        int n = words;
        if (cram_offset + n > CRAM_SIZE) n = CRAM_SIZE - cram_offset;
        if (n > 0) memcpy(&vdp.cram[cram_offset], dest, n * sizeof(uint16_t));
    }
}

void PalLoad_Fade(int index) {
    if (index < 0 || index >= (int)(sizeof(pal_index) / sizeof(pal_index[0])))
        return;
    const PalEntry *e = &pal_index[index];
    if (!e->data) return;

    /* Mirror the ASM: a3 = target RAM address, then add $80 to land in the
       matching line of the fade-in buffer (v_palette_fading). Level palettes
       target v_palette_line_2, so they must land in the fade buffer line 2. */
    int line_off = (int)(e->target_ram_offset - v_palette);
    uint16_t *dest = (uint16_t *)RAM_ADDR(v_palette_fading + line_off);
    int words = e->count + 1;
    for (int i = 0; i < words; i++) {
        dest[i] = ((uint16_t)e->data[i * 2] << 8) | e->data[i * 2 + 1];
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

        memcpy(vdp.cram, pal, num_colors * sizeof(uint16_t));
    }
}

/* ============================================================================
     Palette Fade In
     ============================================================================ */

static void fade_inc_color(uint16_t *color, uint16_t target) {
    uint16_t c = *color;
    if (c == target) return;

    if ((c & 0x00E) != (target & 0x00E)) {
        if ((c & 0x00E) < (target & 0x00E)) {
            *color = c + 0x002;
        } else {
            *color = c - 0x002;
        }
        return;
    }
    if ((c & 0x0E0) != (target & 0x0E0)) {
        if ((c & 0x0E0) < (target & 0x0E0)) {
            *color = c + 0x020;
        } else {
            *color = c - 0x020;
        }
        return;
    }
    if ((c & 0xE00) != (target & 0xE00)) {
        if ((c & 0xE00) < (target & 0xE00)) {
            *color = c + 0x200;
        } else {
            *color = c - 0x200;
        }
    }
}

void Palette_FadeIn(void) {
    uint16_t *dst = (uint16_t *)RAM_ADDR(v_palette);
    uint16_t *src = (uint16_t *)RAM_ADDR(v_palette_fading);
    int num_colors = 64;

    for (int frame = 0; frame < 22; frame++) {
        extern void WaitForVBlank(void);
        WaitForVBlank();
        for (int i = 0; i < num_colors; i++) {
            fade_inc_color(&dst[i], src[i]);
        }
        memcpy(vdp.cram, dst, num_colors * sizeof(uint16_t));
    }
}

/* ============================================================================
    Palette White Fades (from _inc/Palette Fading.asm — PaletteWhiteIn / Out)

    The Special Stage fades to/from WHITE instead of black:
    - PaletteWhiteOut: brightens every color toward cWhite over 22 frames.
    - PaletteWhiteIn: pre-fills v_palette with white, then darkens each color
      toward its target in v_palette_fading over 22 frames.
    Both set the VBlank fade routine ($12) per frame (CRAM transfer happens
    there) and poll RunPLC, exactly like the ASM.
    ============================================================================ */

/* WhiteIn_DecColor (_inc/Palette Fading.asm lines 271-305): darken one active
   color by one channel-step toward its target. Blue first, then green; red is
   always decremented by $002 (no target check — ASM quirk, ported as-is). */
static void WhiteIn_DecColor(uint16_t *a0, uint16_t target) {
    uint16_t d3 = *a0;                    /* move.w (a0),d3 */
    if (d3 == target) return;             /* cmp.w d2,d3 / beq.s .nextColor */

    /* .decBlue: decrease blue value by one step */
    if (d3 >= 0x200) {                    /* subi.w #$200,d1 / blo.s .decGreen */
        uint16_t d1 = (uint16_t)(d3 - 0x200);
        if (d1 >= target) {               /* cmp.w d2,d1 / blo.s .decGreen */
            *a0 = d1;                     /* move.w d1,(a0)+ */
            return;
        }
    }
    /* .decGreen: decrease green value by one step */
    if (d3 >= 0x020) {                    /* subi.w #$020,d1 / blo.s .decRed */
        uint16_t d1 = (uint16_t)(d3 - 0x020);
        if (d1 >= target) {               /* cmp.w d2,d1 / blo.s .decRed */
            *a0 = d1;                     /* move.w d1,(a0)+ */
            return;
        }
    }
    /* .decRed: decrease red value by one step & update active color */
    *a0 = (uint16_t)(d3 - 0x002);         /* subq.w #$002,(a0)+ */
}

/* WhiteIn_FromWhite (lines 239-268): fade all affected colors one step toward
   their targets; also fades the underwater palette, but only in Labyrinth. */
void WhiteIn_FromWhite(void) {
    uint16_t *a0 = (uint16_t *)RAM_ADDR(v_palette);          /* lea (v_palette).w,a0 */
    uint16_t *a1 = (uint16_t *)RAM_ADDR(v_palette_fading);   /* lea (v_palette_fading).w,a1 */
    int start = v_pfade_start;                               /* move.b (v_pfade_start).w,d0 / adda.w d0,a0/a1 */
    int size  = v_pfade_size;                                /* move.b (v_pfade_size).w,d0 */

    for (int i = 0; i <= size; i++) {
        WhiteIn_DecColor(&a0[start + i], a1[start + i]);     /* .fadeColors: bsr.s WhiteIn_DecColor / dbf d0 */
    }

    if (v_zone == id_LZ) {                                   /* cmpi.b #id_LZ,(v_zone).w / bne.s .return */
        uint16_t *w0 = (uint16_t *)RAM_ADDR(v_palette_water);         /* lea (v_palette_water).w,a0 */
        uint16_t *w1 = (uint16_t *)RAM_ADDR(v_palette_water_fading);  /* lea (v_palette_water_fading).w,a1 */
        for (int i = 0; i <= size; i++) {
            WhiteIn_DecColor(&w0[start + i], w1[start + i]);  /* .fadeColorsWater: bsr.s WhiteIn_DecColor / dbf d0 */
        }
    }
}

/* PaletteWhiteIn (lines 212-236): fill the active palette with white, then
   22 frames of WhiteIn_FromWhite toward the fade-in buffer. */
void PaletteWhiteIn(void) {
    extern void WaitForVBlank(void);

    /* move.w #$003F,(v_pfade_start).w: start=0, size=$3F (affect all $40 colors) */
    v_pfade_start = 0;
    v_pfade_size  = 0x3F;

    /* (PalWhiteIn_Alt) fill palette with white */
    uint16_t *a0 = (uint16_t *)RAM_ADDR(v_palette);          /* lea (v_palette).w,a0 */
    for (int i = 0; i <= v_pfade_size; i++) {                 /* .fillWhite: move.w d1,(a0)+ / dbf d0 */
        a0[i] = cWhite;                                       /* move.w #cWhite,d1 */
    }

    for (int d4 = 22 - 1; d4 >= 0; d4--) {                    /* move.w #22-1,d4 / .fadeMainLoop */
        v_vblank_routine = id_VBlank_PaletteFade;             /* move.b #id_VBlank_PaletteFade,(v_vblank_routine) */
        WaitForVBlank();                                      /* bsr.w WaitForVBlank */
        WhiteIn_FromWhite();                                  /* bsr.s WhiteIn_FromWhite */
        RunPLC();                                             /* bsr.w RunPLC */
        /* Port sync: VBlank_StandardTransfers already does Palette_Update()
           (v_palette -> palette_main); keep vdp.cram in step too, like the
           black Palette_FadeIn/FadeOut do (writeCRAM v_palette,0). */
        memcpy(vdp.cram, RAM_ADDR(v_palette), 64 * sizeof(uint16_t));
    }                                                         /* dbf d4,.fadeMainLoop */
}

/* WhiteOut_AddColor (lines 352-387): brighten one active color by one
   channel-step toward white. Red first, then green, then blue. */
static void WhiteOut_AddColor(uint16_t *a0) {
    uint16_t d2 = *a0;                                        /* move.w (a0),d2 */
    if (d2 == cWhite) return;                                 /* cmpi.w #cWhite,d2 / beq.s .nextColor */

    /* .addRed: red channel already full? */
    if ((d2 & 0x00E) != cRed) {                               /* andi.w #$00E,d1 / cmpi.w #cRed,d1 / beq.s .addGreen */
        *a0 = (uint16_t)(d2 + 0x002);                         /* addq.w #$002,(a0)+ */
        return;
    }
    /* .addGreen */
    if ((d2 & 0x0E0) != cGreen) {                             /* andi.w #$0E0,d1 / cmpi.w #cGreen,d1 / beq.s .addBlue */
        *a0 = (uint16_t)(d2 + 0x020);                         /* addi.w #$020,(a0)+ */
        return;
    }
    /* .addBlue */
    if ((d2 & 0xE00) != cBlue) {                              /* andi.w #$E00,d1 / cmpi.w #cBlue,d1 / beq.s .nextColor */
        *a0 = (uint16_t)(d2 + 0x200);                         /* addi.w #$200,(a0)+ */
    }
    /* else: all channels full == cWhite, already returned above */
}

/* WhiteOut_ToWhite (lines 328-349): brighten all affected colors a bit more.
   The underwater palette is faded to white even in non-LZ levels. */
void WhiteOut_ToWhite(void) {
    uint16_t *a0 = (uint16_t *)RAM_ADDR(v_palette);           /* lea (v_palette).w,a0 */
    int start = v_pfade_start;                                /* move.b (v_pfade_start).w,d0 / adda.w d0,a0 */
    int size  = v_pfade_size;                                 /* move.b (v_pfade_size).w,d0 */

    for (int i = 0; i <= size; i++) {                         /* .fadeColors: bsr.s WhiteOut_AddColor / dbf d0 */
        WhiteOut_AddColor(&a0[start + i]);
    }

    /* Underwater palette is faded out to white even in non-LZ levels */
    uint16_t *w0 = (uint16_t *)RAM_ADDR(v_palette_water);     /* lea (v_palette_water).w,a0 */
    for (int i = 0; i <= size; i++) {                         /* .fadeColorsWater: bsr.s WhiteOut_AddColor / dbf d0 */
        WhiteOut_AddColor(&w0[start + i]);
    }
}

/* PaletteWhiteOut (lines 313-325): 22 frames of WhiteOut_ToWhite. */
void PaletteWhiteOut(void) {
    extern void WaitForVBlank(void);

    /* move.w #$003F,(v_pfade_start).w */
    v_pfade_start = 0;
    v_pfade_size  = 0x3F;

    for (int d4 = 22 - 1; d4 >= 0; d4--) {                    /* move.w #22-1,d4 / .fadeMainLoop */
        v_vblank_routine = id_VBlank_PaletteFade;             /* move.b #id_VBlank_PaletteFade,(v_vblank_routine) */
        WaitForVBlank();                                      /* bsr.w WaitForVBlank */
        WhiteOut_ToWhite();                                   /* bsr.s WhiteOut_ToWhite */
        RunPLC();                                             /* bsr.w RunPLC */
        /* Port sync: same as PaletteWhiteIn (writeCRAM v_palette,0) */
        memcpy(vdp.cram, RAM_ADDR(v_palette), 64 * sizeof(uint16_t));
    }                                                         /* dbf d4,.fadeMainLoop */
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

/* ============================================================================
 *  PalCycle_Title (from _inc/PaletteCycle.asm, PalCycle_Title/PCycGHZ_Go)
 *  Cycles the background water colours on the title screen: every 6 frames,
 *  4 colours (palette line 3, colours 8-B) are replaced with the next block
 *  of 4 colours from Pal_TitleCycWater (4 blocks of 4, big-endian words).
 *  ============================================================================ */
void PalCycle_Title(void) {
    if (!Pal_TitleCycWater || Pal_TitleCycWater_len < 32) return;

    const uint8_t *tab = Pal_TitleCycWater;
    #define TITLE_CYC_COLOR(idx) ((uint16_t)((uint16_t)tab[(idx)*2] << 8) | tab[(idx)*2 + 1])

    /* Decrementar temporizador */
    v_pcyc_time = (uint16_t)(v_pcyc_time - 1);
    if ((v_pcyc_time & 0x8000) == 0) return; /* Timer aún positivo (>= 0) */

    v_pcyc_time = 6 - 1; /* Reset timer */

    /* Obtener el número de ciclo (0..3) */
    uint16_t block = v_pcyc_num & 3;
    v_pcyc_num = (uint16_t)(v_pcyc_num + 1);

    /* Cada bloque tiene 4 colores (uint16_t) */
    int base_color_idx = block * 4; 

    /* Escribir en la paleta RAM (Línea 3, colores 8 a 11) */
    uint16_t *line3 = (uint16_t *)RAM_ADDR(v_palette_line_3);
    for (int i = 0; i < 4; i++) {
        uint16_t color = TITLE_CYC_COLOR(base_color_idx + i);
        line3[8 + i] = color;
        vdp.cram[40 + i] = color; /* Sincronizar CRAM (Línea 3 = offset 32, 32+8 = 40) */
    }

    #undef TITLE_CYC_COLOR
}
