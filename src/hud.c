#include "hud.h"
#include "data.h"
#include "ram.h"
#include "constants.h"
#include "vdp.h"
#include "decomp.h"
#include "objects.h"
#include <string.h>
#include <stdint.h>

/* ===========================================================================
   HUD digit writing.

   The Mega Drive HUD tiles live at ArtTile_HUD*2 .. . The 8x16 glyphs
   (score / time / rings) are two stacked 8x8 tiles (tile N, tile N+1), i.e.
   64 bytes each; the 8x8 glyphs (lives) are one tile each (32 bytes). The
   source fonts are stored byte-for-byte in VRAM order:
     - Art_Hud: 8x16 digits, one glyph per 64 bytes. The static HUD labels
       ("E", ":") are indexed differently from the digits (see below).
     - Art_LivesNums: 8x8 digits, one glyph per 32 bytes.
   =========================================================================== */

#define HUD_8X16_BYTES  64   /* two tiles per 8x16 digit */
#define HUD_8X8_BYTES   32   /* one tile per 8x8 digit  */

static void hud_write_8x16(uint32_t vram_byte, int digit) {
    if (Art_Hud && digit >= 0 && (size_t)(digit * 64 + 64) <= Art_Hud_len) {
        VDP_WriteVRAM(Art_Hud + digit * 64, vram_byte, 64);
    } else {
        VDP_FillVRAM(0, vram_byte, 64);
    }
}

static void hud_write_8x8(uint32_t vram_byte, int digit) {
    if (Art_LivesNums && digit >= 0 && (size_t)(digit * 32 + 32) <= Art_LivesNums_len) {
        VDP_WriteVRAM(Art_LivesNums + digit * 32, vram_byte, 32);
    } else {
        VDP_FillVRAM(0, vram_byte, 32);
    }
}

/* Reverse-engineer one decimal digit from a value+place pair. */
static int hud_extract_digit(uint32_t *value, uint32_t place) {
    uint32_t q = *value / place;
    *value -= q * place;
    return (int)q;
}

/* Write a sequence of 8x16 digits.
   mode: 0 = skip leading zeroes (leave previous VRAM content),
         1 = clear leading zeroes,
         2 = always write (with leading zeroes).
   places: per-digit place values (Hud_100000 .. Hud_1). */
static void hud_write_8x16_seq(uint32_t tile, int count,
                               const uint32_t *places, uint32_t value, int mode) {
    uint32_t vram_byte = tile * tile_size;
    int first_nz_seen = (mode == 2);

    for (int i = 0; i < count; i++) {
        int d = hud_extract_digit(&value, places[i]);

        if (d != 0) first_nz_seen = 1;

        if (first_nz_seen) {
            hud_write_8x16(vram_byte, d);
        } else if (mode == 1) {
            VDP_FillVRAM(0, vram_byte, 64);
        }
        /* mode 0: skip — leave whatever was there before */

        vram_byte += 64;   /* each 8x16 digit advances two tiles */
    }
}

/* Write the lives counter (8x8 digits, always incl. a leading zero so game
   overs show "0"). */
static void hud_write_lives(void) {
    uint32_t vram_byte = ArtTile_Lives_Counter_Num * tile_size;
    uint32_t lives = v_lives & 0xFF;
    const uint32_t places[2] = { 10, 1 };

    for (int i = 0; i < 2; i++) {
        int d = hud_extract_digit(&lives, places[i]);
        hud_write_8x8(vram_byte, d);
        vram_byte += 32;
    }
}

/* Write the static "E______0" / "0:00" / "__0" patterns. Each init byte
   indexes Art_Hud in 8x8-tile-pair units ($14 = ":", $16 = "E"), which is a
   different indexing than the digit glyphs used above (ASM HUD Init.asm:
   lsl.w #5 on the byte, then a 64-byte copy). A negative byte = blank. */
static void hud_init_8x16(uint32_t tile, const int8_t *data, int count) {
    uint32_t vram_byte = tile * tile_size;

    for (int i = 0; i < count; i++) {
        int8_t v = data[i];
        if (v >= 0 && Art_Hud && (uint32_t)v * 32 + 64 <= Art_Hud_len) {
            VDP_WriteVRAM(Art_Hud + v * 32, vram_byte, 64);
        } else {
            VDP_FillVRAM(0, vram_byte, 64);
        }
        vram_byte += 64;
    }
}

/* ===========================================================================
   Subroutines ported one-to-one from _inc/HUD Update.asm
   =========================================================================== */

static void Hud_ResetRings(void) {
    static const int8_t hud_base_rings[3] = { -1, -1, 0 };  /* "__0" */
    hud_init_8x16(ArtTile_HUDRings, hud_base_rings, 3);
}

static void Hud_Rings(void) {
    static const uint32_t places[3] = { 100, 10, 1 };
    hud_write_8x16_seq(ArtTile_HUDRings, 3, places, v_rings & 0xFFFF, 0);
}

static void Hud_Score(void) {
    static const uint32_t places[6] = { 100000, 10000, 1000, 100, 10, 1 };
    hud_write_8x16_seq(ArtTile_HUDScore, 6, places, v_score, 0);
}

static void Hud_Mins(void) {
    static const uint32_t places[1] = { 1 };
    hud_write_8x16_seq(ArtTile_HUDTimeMins, 1, places, v_timemin & 0xFF, 2);
}

static void Hud_Secs(uint8_t secs) {
    static const uint32_t places[2] = { 10, 1 };
    hud_write_8x16_seq(ArtTile_HUDTimeSecs, 2, places, secs, 2);
}

static void Hud_TimeRingBonus(uint16_t value) {
    static const uint32_t places[4] = { 1000, 100, 10, 1 };
    hud_write_8x16_seq(ArtTile_Bonuses, 4, places, value, 1);
}

/* ===========================================================================
   Hud_Base: load basic HUD graphics + static digits (sonic.asm:2857)
   =========================================================================== */
void Hud_Base(void) {
    if (Nem_Hud) {
        NemDecToVRAM(Nem_Hud, ArtTile_HUD * tile_size);
    }

    if (Nem_Lives) {
        NemDecToVRAM(Nem_Lives, ArtTile_Lives_Counter * tile_size);
    }

    hud_write_lives();

    static const int8_t hud_base_score[8] = { 0x16, -1, -1, -1, -1, -1, -1, 0 };  /* E______0 */
    static const int8_t hud_base_time[4]  = { 0, 0x14, 0, 0 };                    /* 0:00    */
    static const int8_t hud_base_rings[3] = { -1, -1, 0 };                        /* __0     */

    hud_init_8x16(ArtTile_HUDScore_E, hud_base_score, 8);
    hud_init_8x16(ArtTile_HUDTimeMins, hud_base_time, 4);
    hud_init_8x16(ArtTile_HUDRings, hud_base_rings, 3);
}

/* ===========================================================================
   TimeOver: 9:59:59 reached. Stops the clock; kill Sonic + set flag.
   Ported from _inc/HUD Update.asm TimeOver (lines 103-112).
   =========================================================================== */
static void TimeOver(void) {
    f_timecount = 0;

    KillSonic(Object_GetSlot(0));

    f_timeover = 1;
}

/* ===========================================================================
   Hud_Lives — ported from _inc/HUD Update.asm Hud_Lives (lines 537-543).
   Writes lives count at ArtTile_Lives_Counter_Num using 8x8 digits.
   Always writes both digits so "0" shows for game overs (last digit forced).
   =========================================================================== */
static void Hud_Lives(void) {
    uint32_t vram_byte = ArtTile_Lives_Counter_Num * tile_size;
    uint32_t lives = v_lives & 0xFF;
    const uint32_t places[2] = { 10, 1 };

    for (int i = 0; i < 2; i++) {
        int d = hud_extract_digit(&lives, places[i]);
        hud_write_8x8(vram_byte, d);
        vram_byte += 32;
    }
}

/* ===========================================================================
   HudDb_XY_Write — write 8 hex nibbles (two 16-bit words) to VRAM.
   Ported from _inc/HUD Update.asm HudDb_XY_Write (lines 255-274).

   Each hex digit is one 8x8 tile from Art_Text (32 bytes each).
   input: value = upper 16 bits = first word, lower 16 bits = second word.
   =========================================================================== */
static void hud_write_hex_word(uint32_t vram_byte, uint16_t value) {
    for (int i = 3; i >= 0; i--) {
        int nibble = (value >> (i * 4)) & 0xF;
        if (nibble >= 0xA) nibble += 7;   /* A-F → letter tile offsets */
        uint32_t src_off = (uint32_t)nibble * 32;
        if (Art_Text && src_off + 32 <= Art_Text_len) {
            VDP_WriteVRAM(Art_Text + src_off, vram_byte, 32);
        } else {
            VDP_FillVRAM(0, vram_byte, 32);
        }
        vram_byte += 32;
    }
}

/* ===========================================================================
   HudDb_XY — display camera + Sonic X/Y positions in hex, replacing the
   score area.  Ported from _inc/HUD Update.asm HudDb_XY (lines 242-252).

   Layout at ArtTile_HUDScore_E:
     [CamX_hi CamX_lo SonX_hi SonX_lo]  — 8 tiles (two 16-bit hex words)
     [CamY_hi CamY_lo SonY_hi SonY_lo]  — 8 tiles

   d1 upper word = camera position, lower word = Sonic position.
   =========================================================================== */
static void HudDb_XY(void) {
    uint32_t vram = ArtTile_HUDScore_E * tile_size;

    /* X row: camera X (upper word) | Sonic X (lower word) */
    hud_write_hex_word(vram,                  (uint16_t)RAM_WORD(0xF700));  /* cam X */
    hud_write_hex_word(vram + 4 * 32,         (uint16_t)RAM_WORD(v_player + 8));  /* Sonic X */

    /* Y row: camera Y (upper word) | Sonic Y (lower word) */
    hud_write_hex_word(vram + 8 * 32,         (uint16_t)RAM_WORD(0xF704));  /* cam Y */
    hud_write_hex_word(vram + 12 * 32,        (uint16_t)RAM_WORD(v_player + 0xC)); /* Sonic Y */
}

/* ===========================================================================
   HUD_Update (VBlank): refresh whichever HUD counters are dirty.
   Ported from _inc/HUD Update.asm HUD_Update (lines 6-96).
   =========================================================================== */
void HUD_Update(void) {
    /* Debug mode: show camera + Sonic X/Y in hex (HudDebug path). */
    if (f_debugmode) {
        HudDb_XY();

        /* --- rings --- */
        if (f_ringcount) {
            if ((int8_t)f_ringcount < 0) {
                Hud_ResetRings();
            }
            f_ringcount = 0;
            Hud_Rings();
        }

        /* --- sprite counter → time seconds area --- */
        {
            uint8_t sc = v_spritecount;
            uint32_t p[2] = { 10, 1 };
            uint32_t v = sc;
            uint32_t vram = ArtTile_HUDTimeSecs * tile_size;
            for (int i = 0; i < 2; i++) {
                int d = hud_extract_digit(&v, p[i]);
                hud_write_8x16(vram, d);
                vram += 64;
            }
        }

        /* --- lives --- */
        if (f_lifecount) {
            f_lifecount = 0;
            Hud_Lives();
        }

        /* --- time/ring bonuses --- */
        if (f_endactbonus) {
            f_endactbonus = 0;
            Hud_TimeRingBonus(v_timebonus);
            Hud_TimeRingBonus(v_ringbonus);
        }
        return;
    }

    /* --- score --- */
    if (f_scorecount) {
        f_scorecount = 0;
        Hud_Score();
    }

    /* --- rings --- */
    if (f_ringcount) {
        if ((int8_t)f_ringcount < 0) {
            Hud_ResetRings();   /* flag >= $80: Sonic got hit, clear digits */
        }
        f_ringcount = 0;
        Hud_Rings();
    }

    /* --- time --- */
    if (f_timecount) {
        if (f_pause) {
            /* game paused: leave the flag set, don't advance the clock */
        } else if (v_timemin == 9 && v_timesec == 59 && v_timecent == 59) {
            f_timecount = 0;
            TimeOver();   /* stops the clock; no VRAM update */
        } else {
            uint8_t min  = v_timemin;
            uint8_t sec  = v_timesec;
            uint8_t cent = v_timecent + 1;   /* increment 1/60s counter */

            if (cent >= 60) {
                cent = 0;
                sec++;                        /* increment seconds */
                if (sec >= 60) {
                    sec = 0;
                    min++;                    /* increment minutes */
                    if (min > 9) min = 9;     /* never exceed 9 */
                }
            }

            v_timecent = cent;
            v_timesec  = sec;
            v_timemin  = min;

            if (cent == 0) {
                /* seconds ticked: rewrite mins + secs in VRAM (ASM .updatetime) */
                Hud_Mins();
                Hud_Secs(sec);
            }
        }
    }

    /* --- lives --- */
    if (f_lifecount) {
        f_lifecount = 0;
        Hud_Lives();
    }

    /* --- time/ring bonuses (end-of-level cards) --- */
    if (f_endactbonus) {
        f_endactbonus = 0;
        Hud_TimeRingBonus(v_timebonus);
        Hud_TimeRingBonus(v_ringbonus);
    }
}