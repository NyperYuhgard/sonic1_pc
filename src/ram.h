#ifndef SONIC1_RAM_H
#define SONIC1_RAM_H

#include <stddef.h>   /* offsetof, size_t */
#include "types.h"
#include "constants.h"

/* ===========================================================================
   RAM — translated from _Variables.asm (+ s1.sounddriver.ram.asm)

   The 68000 has 64KB of RAM mapped at $FFFF0000..$FFFFFFFF. In C we model it
   as a flat byte array `ram[0x10000]`. Every variable declared in the ASM is
   an offset into this array (the ASM base $FFFF0000 maps to ram[0x0000]).

   ---------------------------------------------------------------------------
   Layout model — struct + offsetof
   ---------------------------------------------------------------------------
   The RAM layout is described by the `RamLayout` struct below, which is
   NEVER instantiated. It exists purely so that the compiler computes field
   offsets for us — exactly the same job that the assembler does when it walks
   `ds.b` / `ds.w` / `ds.l` directives and bumps the location counter.

   Why this is better than hardcoded numeric offsets:
     * Inserting a new variable is a ONE-LINE change. Everything after it
       shifts automatically, exactly like inserting a `ds.b` in ASM.
     * The compiler catches overlapping or mis-sized fields via the final
       `_Static_assert(sizeof(RamLayout) == 0x10000)`.
     * There is exactly one source of truth for the layout.

   How to read/write RAM:
     * Every variable v_xxx is a macro that expands to a native typed lvalue
       over &ram[offsetof(RamLayout, field)]. You use it just like before:
           v_gamemode = 8;
           obX(player) += 3;
     * Byte aliases (v_framebyte, v_airbyte, ...) point at the byte with the
       correct ASM offset within their parent word; the delta is preserved
       from the original header.
     * Unused gaps are modelled as `_pad_XXXX[N]` fields and exposed as
       v_unused_XXXX with a companion v_unused_XXXX_SIZE constant.

   What is NOT in the struct:
     * The Special Stage phase at $FF0000 and the error-handler phase at
       v_objstate overlap the main layout; those are kept as raw offset
       defines further down.
     * SMPS sound-driver variables are exposed with their own offsets into
       the snddriver_ram block, unchanged.
   =========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* The entire 64KB RAM. Defined in ram.c (or wherever ram[] is instantiated). */
extern uint8_t ram[0x10000];

/* ---------------------------------------------------------------------------
   RAM accessors — native typed lvalues (little-endian on PC)
   --------------------------------------------------------------------------- */
#define RAM_BYTE(addr)  (*(uint8_t  *)(&ram[(addr)]))
#define RAM_WORD(addr)  (*(uint16_t *)(&ram[(addr)]))
#define RAM_LONG(addr)  (*(uint32_t *)(&ram[(addr)]))

#define RAM_SBYTE(addr) (*(int8_t   *)(&ram[(addr)]))
#define RAM_SWORD(addr) (*(int16_t  *)(&ram[(addr)]))

#define RAM_ADDR(addr)  (&ram[(addr)])

/* Size of a struct field without needing an instance (constant expression). */
#define RAM_FIELD_SIZE(field)  (sizeof(((RamLayout *)0)->field))

/* Free-form size helper for cases where you only have two raw offsets. */
#define RAM_UNUSED_SIZE(first_off, next_off)  ((size_t)((next_off) - (first_off)))
#define RAM_UNUSED_BYTES(name)                (name##_SIZE)

/* ---------------------------------------------------------------------------
   Big-endian RAM accessors (for data reloaded from the ROM, VDP words, etc.)
   These do NOT create lvalues; use the RAM_SET_* forms to write.
   --------------------------------------------------------------------------- */
static inline uint16_t RAM_U16(uint32_t addr) {
    return (uint16_t)(((uint16_t)ram[addr] << 8) | ram[addr + 1]);
}
static inline int16_t RAM_S16(uint32_t addr) {
    return (int16_t)RAM_U16(addr);
}
static inline uint32_t RAM_U32(uint32_t addr) {
    return ((uint32_t)ram[addr]     << 24) |
           ((uint32_t)ram[addr + 1] << 16) |
           ((uint32_t)ram[addr + 2] << 8)  |
            (uint32_t)ram[addr + 3];
}
static inline void RAM_SET_U16(uint32_t addr, uint16_t v) {
    ram[addr]     = (uint8_t)(v >> 8);
    ram[addr + 1] = (uint8_t)(v & 0xFF);
}
static inline void RAM_SET_S16(uint32_t addr, int16_t v) {
    RAM_SET_U16(addr, (uint16_t)v);
}
static inline void RAM_SET_U32(uint32_t addr, uint32_t v) {
    ram[addr]     = (uint8_t)(v >> 24);
    ram[addr + 1] = (uint8_t)((v >> 16) & 0xFF);
    ram[addr + 2] = (uint8_t)((v >> 8)  & 0xFF);
    ram[addr + 3] = (uint8_t)(v & 0xFF);
}

/* ===========================================================================
   RamLayout — the RAM map.
   Never instantiated. Field order = address order.
   Each field is named after the corresponding v_xxx (without the v_ prefix);
   blob fields are prefixed `blob_` to avoid colliding with macros.
   =========================================================================== */
#pragma pack(push, 1)
typedef struct {

    /* ---- $0000: 256x256 tile mappings ($A400 = chunk_size * $52) ---- */
    uint8_t  blob_256x256[chunk_size * 0x52];

    /* ---- $A400: level layouts (layout_row * 8) ---- */
    uint8_t  blob_lvllayout[layout_row * 8];

    /* ---- $A800: background scroll buffer ---- */
    uint8_t  blob_bgscroll_buffer[0x200];

    /* ---- $AA00: Nemesis graphics decompression buffer ---- */
    uint8_t  blob_ngfx_buffer[0x200];

    /* ---- $AC00: sprite display queue (spritelayer_num * spritelayer_size) ---- */
    uint8_t  blob_spritequeue[spritelayer_num * spritelayer_size];

    /* ---- $B000: 16x16 tile mappings ---- */
    uint8_t  blob_16x16[0x1800];

    /* ---- $C800: buffered Sonic graphics (tile_size * 23) ---- */
    uint8_t  blob_sgfx_buffer[tile_size * 23];

    /* ---- $CAE0: unused 0x20 ---- */
    uint8_t  _pad_CAE0[0x20];

    /* ---- $CB00: position tracking data for Sonic ---- */
    uint8_t  blob_tracksonic[0x100];

    /* ---- $CC00: scrolling table ($380 + $80 spill-over pad) ---- */
    uint8_t  blob_hscrolltablebuffer[0x400];

    /* ---- $D000: object variable space (object_size * $80) ---- */
    uint8_t  blob_objspace[object_size * 0x80];

    /* ---- $F000: SMPS sound driver RAM (0x600 bytes incl. $40 unused) ---- */
    uint8_t  blob_snddriver_ram[0x600];

    /* ==== $F600: game mode + VDP + misc small variables ==== */
    uint8_t  gamemode;                    /* $F600 */
    uint8_t  _pad_F601[1];                /* $F601 */
    uint8_t  jpadhold2;                   /* $F602 */
    uint8_t  jpadpress2;                  /* $F603 */
    uint8_t  jpadhold1;                   /* $F604 */
    uint8_t  jpadpress1;                  /* $F605 */
    uint8_t  _pad_F606[6];                /* $F606..$F60B */
    uint16_t vdp_buffer1;                 /* $F60C */
    uint8_t  _pad_F60E[6];                /* $F60E..$F613 */
    uint16_t generictimer;                /* $F614 */
    uint16_t scrposy_vdp;                 /* $F616 */
    uint16_t bgscrposy_vdp;               /* $F618 */
    uint16_t scrposx_vdp;                 /* $F61A */
    uint16_t bgscrposx_vdp;               /* $F61C */
    uint16_t bg3scrposy_vdp;              /* $F61E */
    uint16_t bg3scrposx_vdp;              /* $F620 */
    uint8_t  _pad_F622[2];                /* $F622..$F623 */
    uint16_t hblank_hreg;                 /* $F624 */
    uint8_t  pfade_start;                 /* $F626 */
    uint8_t  pfade_size;                  /* $F627 */

    /* ---- v_misc_variables ---- */
    uint8_t  vblank_0e_counter;           /* $F628 */
    uint8_t  _pad_F629[1];                /* $F629 */
    uint8_t  vblank_routine;              /* $F62A */
    uint8_t  _pad_F62B[1];                /* $F62B */
    uint8_t  spritecount;                 /* $F62C */
    uint8_t  _pad_F62D[5];                /* $F62D..$F631 */
    uint16_t pcyc_num;                    /* $F632 */
    uint16_t pcyc_time;                   /* $F634 */
    uint32_t random;                      /* $F636 */
    uint16_t pause;                       /* $F63A */
    uint8_t  _pad_F63C[4];                /* $F63C..$F63F */
    uint16_t vdp_buffer2;                 /* $F640 */
    uint8_t  _pad_F642[2];                /* $F642..$F643 */
    uint16_t hblank_pal;                  /* $F644 */
    uint16_t waterpos1;                   /* $F646 */
    uint16_t waterpos2;                   /* $F648 */
    uint16_t waterpos3;                   /* $F64A */
    uint8_t  water;                       /* $F64C */
    uint8_t  wtr_routine;                 /* $F64D */
    uint8_t  wtr_state;                   /* $F64E */
    uint8_t  doupdatesinhblank;           /* $F64F */

    /* ---- palette cycling buffer ($30 bytes) ---- */
    uint8_t  pal_buffer[0x30];            /* $F650..$F67F */

    /* ---- pattern load cues buffer (plc_slot_size * 16 = $60) ---- */
    uint8_t  plc_buffer[plc_slot_size * 16];       /* $F680..$F6DF */
    uint32_t plc_ptrnemcode;              /* $F6E0 */
    uint32_t plc_repeatcount;             /* $F6E4 */
    uint32_t plc_paletteindex;            /* $F6E8 */
    uint32_t plc_previousrow;             /* $F6EC */
    uint32_t plc_dataword;                /* $F6F0 */
    uint32_t plc_shiftvalue;              /* $F6F4 */
    uint16_t plc_patternsleft;            /* $F6F8 */
    uint16_t plc_framepatternsleft;       /* $F6FA */
    uint8_t  _pad_F6FC[4];                /* $F6FC..$F6FF */

    /* ==== Level variables (v_levelvariables) ==== */
    uint32_t screenposx;                  /* $F700 */
    uint32_t screenposy;                  /* $F704 */
    uint32_t bgscreenposx;                /* $F708 */
    uint32_t bgscreenposy;                /* $F70C */
    uint32_t bg2screenposx;               /* $F710 */
    uint32_t bg2screenposy;               /* $F714 */
    uint32_t bg3screenposx;               /* $F718 */
    uint32_t bg3screenposy;               /* $F71C */

    uint16_t limitleft1;                  /* $F720 */
    uint16_t limitright1;                 /* $F722 */
    uint16_t limittop1;                   /* $F724 */
    uint16_t limitbtm1;                   /* $F726 */
    uint16_t limitleft2;                  /* $F728 */
    uint16_t limitright2;                 /* $F72A */
    uint16_t limittop2;                   /* $F72C */
    uint16_t limitbtm2;                   /* $F72E */
    uint16_t unused11;                    /* $F730 */
    uint16_t limitleft3;                  /* $F732 */
    uint8_t  _pad_F734[6];                /* $F734..$F739 */

    uint16_t scrshiftx;                   /* $F73A */
    uint16_t scrshifty;                   /* $F73C */
    uint16_t lookshift;                   /* $F73E */
    uint8_t  unused7;                     /* $F740 */
    uint8_t  unused8;                     /* $F741 */
    uint8_t  dle_routine;                 /* $F742 */
    uint8_t  _pad_F743[1];                /* $F743 */
    uint8_t  nobgscroll;                  /* $F744 */
    uint8_t  _pad_F745[1];                /* $F745 */
    uint8_t  unused9;                     /* $F746 */
    uint8_t  _pad_F747[1];                /* $F747 */
    uint8_t  unused10;                    /* $F748 */
    uint8_t  _pad_F749[1];                /* $F749 */

    uint8_t  fg_xblock;                   /* $F74A */
    uint8_t  fg_yblock;                   /* $F74B */
    uint8_t  bg1_xblock;                  /* $F74C */
    uint8_t  bg1_yblock;                  /* $F74D */
    uint8_t  bg2_xblock;                  /* $F74E */
    uint8_t  bg2_yblock;                  /* $F74F */
    uint8_t  bg3_xblock;                  /* $F750 */
    uint8_t  bg3_yblock;                  /* $F751 */
    uint8_t  _pad_F752[2];                /* $F752..$F753 */
    uint16_t fg_scroll_flags;             /* $F754 */
    uint16_t bg1_scroll_flags;            /* $F756 */
    uint16_t bg2_scroll_flags;            /* $F758 */
    uint16_t bg3_scroll_flags;            /* $F75A */
    uint8_t  bgscrollvert;                /* $F75C */
    uint8_t  _pad_F75D[3];                /* $F75D..$F75F */

    uint16_t sonspeedmax;                 /* $F760 */
    uint16_t sonspeedacc;                 /* $F762 */
    uint16_t sonspeeddec;                 /* $F764 */
    uint8_t  sonframenum;                 /* $F766 */
    uint8_t  sonframechg;                 /* $F767 */
    uint8_t  anglebuffer;                 /* $F768 */
    uint8_t  _pad_F769[1];                /* $F769 */
    uint8_t  anglebuffer2;                /* $F76A */
    uint8_t  _pad_F76B[1];                /* $F76B */
    uint8_t  opl_routine;                 /* $F76C */
    uint8_t  _pad_F76D[1];                /* $F76D */
    uint16_t opl_screen;                  /* $F76E */
    uint8_t  opl_data[0x10];              /* $F770..$F77F */
    uint16_t ssangle;                     /* $F780 */
    uint16_t ssrotate;                    /* $F782 */
    uint8_t  _pad_F784[0xC];              /* $F784..$F78F */

    uint16_t btnpushtime1;                /* $F790 */
    uint16_t btnpushtime2;                /* $F792 */
    uint16_t palchgspeed;                 /* $F794 */
    uint32_t collindex;                   /* $F796 */
    uint16_t palss_num;                   /* $F79A */
    uint16_t palss_time;                  /* $F79C */
    uint16_t palss_index;                 /* $F79E */
    uint16_t ssbganim;                    /* $F7A0 */
    uint8_t  _pad_F7A2[2];                /* $F7A2..$F7A3 */
    uint16_t obj31ypos;                   /* $F7A4 */
    uint8_t  _pad_F7A6[1];                /* $F7A6 */
    uint8_t  bossstatus;                  /* $F7A7 */
    uint16_t trackpos;                    /* $F7A8 */
    uint8_t  lockscreen;                  /* $F7AA */
    uint8_t  _pad_F7AB[1];                /* $F7AB */

    uint8_t  loop1_256;                   /* $F7AC */
    uint8_t  loop2_256;                   /* $F7AD */
    uint8_t  roll1_256;                   /* $F7AE */
    uint8_t  roll2_256;                   /* $F7AF */
    uint8_t  lani0_frame;                 /* $F7B0 */
    uint8_t  lani0_time;                  /* $F7B1 */
    uint8_t  lani1_frame;                 /* $F7B2 */
    uint8_t  lani1_time;                  /* $F7B3 */
    uint8_t  lani2_frame;                 /* $F7B4 */
    uint8_t  lani2_time;                  /* $F7B5 */
    uint8_t  lani3_frame;                 /* $F7B6 */
    uint8_t  lani3_time;                  /* $F7B7 */
    uint8_t  lani4_frame;                 /* $F7B8 */
    uint8_t  lani4_time;                  /* $F7B9 */
    uint8_t  lani5_frame;                 /* $F7BA */
    uint8_t  lani5_time;                  /* $F7BB */
    uint8_t  _pad_F7BC[2];                /* $F7BC..$F7BD */
    uint16_t gfxbigring;                  /* $F7BE */
    uint8_t  conveyrev;                   /* $F7C0 */
    uint8_t  obj63[6];                    /* $F7C1..$F7C6 */
    uint8_t  wtunnelmode;                 /* $F7C7 */
    uint8_t  playerctrl;                  /* $F7C8 */
    uint8_t  wtunneldisallow;             /* $F7C9 */
    uint8_t  slidemode;                   /* $F7CA */
    uint8_t  obj6B;                       /* $F7CB */
    uint8_t  lockctrl;                    /* $F7CC */
    uint8_t  bigring;                     /* $F7CD */
    uint8_t  obj56;                       /* $F7CE */
    uint8_t  _pad_F7CF[1];                /* $F7CF */
    uint16_t itembonus;                   /* $F7D0 */
    uint16_t timebonus;                   /* $F7D2 */
    uint16_t ringbonus;                   /* $F7D4 */
    uint8_t  endactbonus;                 /* $F7D6 */
    uint8_t  sonicend;                    /* $F7D7 */
    uint16_t lz_deform;                   /* $F7D8 */
    /* Start:$F7DA */
    uint8_t  cam_x_delay[2];
    uint8_t  cam_y_delay;
    uint8_t  spindash_sfx_flag;
    uint8_t  spindash_sfx_timer;
    uint8_t  spindash_sfx_pitch;
    /* End:$F7DF */
    uint8_t  switch_flags[0x10];          /* $F7E0..$F7EF */
    uint16_t scroll_block_1_size;         /* $F7F0 */
    uint16_t scroll_block_2_size;         /* $F7F2 */
    uint16_t scroll_block_3_size;         /* $F7F4 */
    uint16_t scroll_block_4_size;         /* $F7F6 */
    uint8_t  _pad_F7F8[8];                /* $F7F8..$F7FF */

    /* ==== Sprite table + palettes ==== */
    uint8_t  spritetablebuffer[spritetable_entrysize * sprites_max]; /* $F800..$FA7F */
    uint8_t  palette_water[0x80];         /* $FA80..$FAFF */
    uint8_t  palette[0x80];               /* $FB00..$FB7F */
    uint8_t  palette_fading[0x80];        /* $FB80..$FBFF */
    uint8_t  objstate[0xC0];              /* $FC00..$FCBF */
    uint8_t  systemstack_end[0x140];      /* $FCC0..$FDFF */

    /* ==== $FE00: cold-boot RAM ==== */
    uint8_t  _pad_FE00[2];                /* $FE00..$FE01 */
    uint16_t restart;                     /* $FE02 */
    uint16_t framecount;                  /* $FE04 */
    uint8_t  debugitem;                   /* $FE06 */
    uint8_t  _pad_FE07[1];                /* $FE07 */
    uint16_t debuguse;                    /* $FE08 */
    uint8_t  debugspeedtimer;             /* $FE0A */
    uint8_t  debugspeed;                  /* $FE0B */
    uint32_t vblank_count;                /* $FE0C..$FE0F */
    uint8_t  zone;                        /* $FE10 */
    uint8_t  act;                         /* $FE11 */
    uint8_t  lives;                       /* $FE12 */
    uint8_t  _pad_FE13[1];                /* $FE13 */
    uint16_t air;                         /* $FE14 */
    uint8_t  lastspecial;                 /* $FE16 */
    uint8_t  _pad_FE17[1];                /* $FE17 */
    uint8_t  continues;                   /* $FE18 */
    uint8_t  _pad_FE19[1];                /* $FE19 */
    uint8_t  timeover;                    /* $FE1A */
    uint8_t  lifecount;                   /* $FE1B */
    uint8_t  lifecount_flag;              /* $FE1C */
    uint8_t  ringcount_flag;              /* $FE1D */
    uint8_t  timecount_flag;              /* $FE1E */
    uint8_t  scorecount_flag;             /* $FE1F */
    uint16_t rings;                       /* $FE20 */
    uint32_t time;                        /* $FE22 */
    uint32_t score;                       /* $FE26 */
    uint8_t  _pad_FE2A[2];                /* $FE2A..$FE2B */
    uint8_t  shield;                      /* $FE2C */
    uint8_t  invinc;                      /* $FE2D */
    uint8_t  shoes;                       /* $FE2E */
    uint8_t  unused1;                     /* $FE2F */
    uint8_t  lastlamp[2];                 /* $FE30..$FE31 */
    uint16_t lamp_xpos;                   /* $FE32 */
    uint16_t lamp_ypos;                   /* $FE34 */
    uint16_t lamp_rings;                  /* $FE36 */
    uint32_t lamp_time;                   /* $FE38 */
    uint8_t  lamp_dle;                    /* $FE3C */
    uint8_t  _pad_FE3D[1];                /* $FE3D */
    uint16_t lamp_limitbtm;               /* $FE3E */
    uint16_t lamp_scrx;                   /* $FE40 */
    uint16_t lamp_scry;                   /* $FE42 */
    uint16_t lamp_bgscrx;                 /* $FE44 */
    uint16_t lamp_bgscry;                 /* $FE46 */
    uint16_t lamp_bg2scrx;                /* $FE48 */
    uint16_t lamp_bg2scry;                /* $FE4A */
    uint16_t lamp_bg3scrx;                /* $FE4C */
    uint16_t lamp_bg3scry;                /* $FE4E */
    uint16_t lamp_wtrpos;                 /* $FE50 */
    uint8_t  lamp_wtrrout;                /* $FE52 */
    uint8_t  lamp_wtrstat;                /* $FE53 */
    uint8_t  lamp_lives;                  /* $FE54 */
    uint8_t  _pad_FE55[2];                /* $FE55..$FE56 */
    uint8_t  emeralds;                    /* $FE57 */
    uint8_t  emldlist[6];                 /* $FE58..$FE5D */
    uint16_t oscillate;                   /* $FE5E */
    uint8_t  oscillate_values[0x40];      /* $FE60..$FE9F */
    uint8_t  _pad_FEA0[0x20];             /* $FEA0..$FEBF */
    uint8_t  ani0_time;                   /* $FEC0 */
    uint8_t  ani0_frame;                  /* $FEC1 */
    uint8_t  ani1_time;                   /* $FEC2 */
    uint8_t  ani1_frame;                  /* $FEC3 */
    uint8_t  ani2_time;                   /* $FEC4 */
    uint8_t  ani2_frame;                  /* $FEC5 */
    uint8_t  ani3_time;                   /* $FEC6 */
    uint8_t  ani3_frame;                  /* $FEC7 */
    uint16_t ani3_buf;                    /* $FEC8 */
    uint8_t  _pad_FECA[0x26];             /* $FECA..$FEEF */
    uint16_t limittopdb;                  /* $FEF0 */
    uint16_t limitbtmdb;                  /* $FEF2 */
    uint8_t  _pad_FEF4[0xC];              /* $FEF4..$FEFF */
    uint16_t chunk0collision;             /* $FF00 */
    uint8_t  _pad_FF02[0xE];              /* $FF02..$FF0F */
    uint32_t screenposx_dup;              /* $FF10 */
    uint32_t screenposy_dup;              /* $FF14 */
    uint32_t bgscreenposx_dup;            /* $FF18 */
    uint32_t bgscreenposy_dup;            /* $FF1C */
    uint32_t bg2screenposx_dup;           /* $FF20 */
    uint32_t bg2screenposy_dup;           /* $FF24 */
    uint32_t bg3screenposx_dup;           /* $FF28 */
    uint32_t bg3screenposy_dup;           /* $FF2C */
    uint16_t fg_scroll_flags_dup;         /* $FF30 */
    uint16_t bg1_scroll_flags_dup;        /* $FF32 */
    uint16_t bg2_scroll_flags_dup;        /* $FF34 */
    uint16_t bg3_scroll_flags_dup;        /* $FF36 */
    uint8_t  _pad_FF38[0x48];             /* $FF38..$FF7F */
    uint16_t levseldelay;                 /* $FF80 */
    uint16_t levselitem;                  /* $FF82 */
    uint16_t levselsound;                 /* $FF84 */
    uint8_t  _pad_FF86[0x3A];             /* $FF86..$FFBF */
    uint32_t scorecopy;                   /* $FFC0 (v_scorecopy / v_scorelife) */
    uint8_t  _pad_FFC4[0x1C];             /* $FFC4..$FFDF */
    uint8_t  levselcheat;                 /* $FFE0 */
    uint8_t  slomocheat;                  /* $FFE1 */
    uint8_t  debugcheat;                  /* $FFE2 */
    uint8_t  creditscheat;                /* $FFE3 */
    uint16_t title_dcount;                /* $FFE4 */
    uint16_t title_ccount;                /* $FFE6 */
    uint8_t  _pad_FFE8[2];                /* $FFE8..$FFE9 */
    uint16_t unused2;                     /* $FFEA */
    uint8_t  unused3;                     /* $FFEC */
    uint8_t  unused4;                     /* $FFED */
    uint8_t  unused5;                     /* $FFEE */
    uint8_t  unused6;                     /* $FFEF */
    uint16_t demo;                        /* $FFF0 */
    uint16_t demonum;                     /* $FFF2 */
    uint16_t creditsnum;                  /* $FFF4 */
    uint8_t  _pad_FFF6[2];                /* $FFF6..$FFF7 */
    uint8_t  megadrive;                   /* $FFF8 */
    uint8_t  _pad_FFF9[1];                /* $FFF9 */
    uint16_t debugmode;                   /* $FFFA */
    uint32_t init;                        /* $FFFC..$FFFF */
} RamLayout;
#pragma pack(pop)

/* Guarantee the layout exactly fills the 64KB window. If you insert a field
   and forget to add a matching `_pad_*`, or if a size constant drifts, this
   fires and points you at the culprit. */
_Static_assert(sizeof(RamLayout) == 0x10000, "RamLayout size mismatch");

/* ===========================================================================
   Variable definitions — one per field of RamLayout
   =========================================================================== */

/* ---- large blobs ---- */
#define v_ram_start_def         (offsetof(RamLayout, blob_256x256))
#define v_ram_start             (v_ram_start_def)

#define v_256x256               (offsetof(RamLayout, blob_256x256))
#define v_256x256_end           (v_256x256 + chunk_size * 0x52)

#define v_lvllayout             (offsetof(RamLayout, blob_lvllayout))
#define v_lvllayout_fg          (v_lvllayout)
#define v_lvllayout_bg          (v_lvllayout + layout_row_interlaced)
#define v_lvllayout_end         (v_lvllayout + layout_row * 8)

#define v_bgscroll_buffer       (offsetof(RamLayout, blob_bgscroll_buffer))

#define v_ngfx_buffer           (offsetof(RamLayout, blob_ngfx_buffer))
#define v_ngfx_buffer_end       (v_ngfx_buffer + 0x200)

#define v_spritequeue           (offsetof(RamLayout, blob_spritequeue))

#define v_16x16                 (offsetof(RamLayout, blob_16x16))

#define v_sgfx_buffer           (offsetof(RamLayout, blob_sgfx_buffer))
#define v_sgfx_buffer_end       (v_sgfx_buffer + tile_size * 23)

#define v_tracksonic            (offsetof(RamLayout, blob_tracksonic))

#define v_hscrolltablebuffer    (offsetof(RamLayout, blob_hscrolltablebuffer))
#define v_hscrolltablebuffer_end        (v_hscrolltablebuffer + 0x380)
#define v_hscrolltablebuffer_end_padded (v_hscrolltablebuffer + 0x400)

#define v_objspace              (offsetof(RamLayout, blob_objspace))

/* Object aliases within v_objspace (unchanged formulas) */
#define v_sonicteam             (v_objspace + object_size * 2)
#define v_titlesonic            (v_objspace + object_size * 1)
#define v_pressstart            (v_objspace + object_size * 2)
#define v_titletm               (v_objspace + object_size * 3)
#define v_ttlsonichide          (v_objspace + object_size * 4)

#define v_player                (v_objspace + object_size * 0)
#define v_hud                   (v_objspace + object_size * 1)

#define v_titlecard             (v_objspace + object_size * 2)
#define v_ttlcardname           (v_titlecard + object_size * 0)
#define v_ttlcardzone           (v_titlecard + object_size * 1)
#define v_ttlcardact            (v_titlecard + object_size * 2)
#define v_ttlcardoval           (v_titlecard + object_size * 3)

#define v_gameovertext1         (v_objspace + object_size * 2)
#define v_gameovertext2         (v_objspace + object_size * 3)

#define v_shieldobj             (v_objspace + object_size * 6)
#define v_starsobj1             (v_objspace + object_size * 8)
#define v_starsobj2             (v_objspace + object_size * 9)
#define v_starsobj3             (v_objspace + object_size * 10)
#define v_starsobj4             (v_objspace + object_size * 11)

#define v_splash                (v_objspace + object_size * 12)
#define v_sonicbubbles          (v_objspace + object_size * 13)
#define v_watersurface1         (v_objspace + object_size * 30)
#define v_watersurface2         (v_objspace + object_size * 31)

#define v_endcard               (v_objspace + object_size * 23)
#define v_endcardsonic          (v_endcard + object_size * 0)
#define v_endcardpassed         (v_endcard + object_size * 1)
#define v_endcardact            (v_endcard + object_size * 2)
#define v_endcardscore          (v_endcard + object_size * 3)
#define v_endcardtime           (v_endcard + object_size * 4)
#define v_endcardring           (v_endcard + object_size * 5)
#define v_endcardoval           (v_endcard + object_size * 6)

#define v_lvlobjspace           (v_objspace + object_size * 32)
#define v_lvlobjend             (v_lvlobjspace + object_size * 96)
#define v_objspace_end          (v_lvlobjend)

#define v_ssrescard             (v_objspace + object_size * 23)
#define v_ssrestext             (v_ssrescard + object_size * 0)
#define v_ssresscore            (v_ssrescard + object_size * 1)
#define v_ssresring             (v_ssrescard + object_size * 2)
#define v_ssresoval             (v_ssrescard + object_size * 3)
#define v_ssrescontinue         (v_ssrescard + object_size * 4)
#define v_ssresemeralds         (v_objspace + object_size * 32)

#define v_continuetext          (v_objspace + object_size * 1)
#define v_continuelight         (v_objspace + object_size * 2)
#define v_continueicon          (v_objspace + object_size * 3)

#define v_endemeralds           (v_objspace + object_size * 16)
#define v_endemeralds_end       (v_objspace + object_size * 32)
#define v_endlogo               (v_objspace + object_size * 16)

#define v_credits               (v_objspace + object_size * 2)
#define v_endeggman             (v_objspace + object_size * 2)
#define v_tryagain              (v_objspace + object_size * 3)
#define v_eggmanchaos           (v_objspace + object_size * 32)

/* ---- sound driver base ---- */
#define v_snddriver_ram         (offsetof(RamLayout, blob_snddriver_ram))

/* ---- $F600 small variables ---- */
#define v_gamemode_def          (offsetof(RamLayout, gamemode))
#define v_gamemode              (RAM_BYTE(v_gamemode_def))

#define v_unused_F601           (RAM_BYTE(offsetof(RamLayout, _pad_F601)))
#define v_unused_F601_SIZE      RAM_FIELD_SIZE(_pad_F601)

#define v_jpadhold2             (RAM_BYTE(offsetof(RamLayout, jpadhold2)))
#define v_jpadpress2            (RAM_BYTE(offsetof(RamLayout, jpadpress2)))
#define v_jpadhold1             (RAM_BYTE(offsetof(RamLayout, jpadhold1)))
#define v_jpadpress1            (RAM_BYTE(offsetof(RamLayout, jpadpress1)))

#define v_unused_F606           (RAM_BYTE(offsetof(RamLayout, _pad_F606)))
#define v_unused_F606_SIZE      RAM_FIELD_SIZE(_pad_F606)

#define v_vdp_buffer1           (RAM_WORD(offsetof(RamLayout, vdp_buffer1)))

#define v_unused_F60E           (RAM_BYTE(offsetof(RamLayout, _pad_F60E)))
#define v_unused_F60E_SIZE      RAM_FIELD_SIZE(_pad_F60E)

#define v_generictimer          (RAM_WORD(offsetof(RamLayout, generictimer)))
#define v_scrposy_vdp           (RAM_WORD(offsetof(RamLayout, scrposy_vdp)))
#define v_bgscrposy_vdp         (RAM_WORD(offsetof(RamLayout, bgscrposy_vdp)))
#define v_scrposx_vdp           (RAM_WORD(offsetof(RamLayout, scrposx_vdp)))
#define v_bgscrposx_vdp         (RAM_WORD(offsetof(RamLayout, bgscrposx_vdp)))
#define v_bg3scrposy_vdp        (RAM_WORD(offsetof(RamLayout, bg3scrposy_vdp)))
#define v_bg3scrposx_vdp        (RAM_WORD(offsetof(RamLayout, bg3scrposx_vdp)))

#define v_unused_F622           (RAM_WORD(offsetof(RamLayout, _pad_F622)))
#define v_unused_F622_SIZE      RAM_FIELD_SIZE(_pad_F622)

#define v_hblank_hreg           (RAM_WORD(offsetof(RamLayout, hblank_hreg)))
#define v_hblank_line           (RAM_BYTE(offsetof(RamLayout, hblank_hreg) + 1))
#define v_pfade_start           (RAM_BYTE(offsetof(RamLayout, pfade_start)))
#define v_pfade_size            (RAM_BYTE(offsetof(RamLayout, pfade_size)))

/* ---- v_misc_variables ---- */
#define v_vblank_0e_counter     (RAM_BYTE(offsetof(RamLayout, vblank_0e_counter)))

#define v_unused_F629           (RAM_BYTE(offsetof(RamLayout, _pad_F629)))
#define v_unused_F629_SIZE      RAM_FIELD_SIZE(_pad_F629)

#define v_vblank_routine        (RAM_BYTE(offsetof(RamLayout, vblank_routine)))

#define v_unused_F62B           (RAM_BYTE(offsetof(RamLayout, _pad_F62B)))
#define v_unused_F62B_SIZE      RAM_FIELD_SIZE(_pad_F62B)

#define v_spritecount           (RAM_BYTE(offsetof(RamLayout, spritecount)))

#define v_unused_F62D           (RAM_BYTE(offsetof(RamLayout, _pad_F62D)))
#define v_unused_F62D_SIZE      RAM_FIELD_SIZE(_pad_F62D)

#define v_pcyc_num              (RAM_WORD(offsetof(RamLayout, pcyc_num)))
#define v_pcyc_time             (RAM_WORD(offsetof(RamLayout, pcyc_time)))
#define v_random                (RAM_LONG(offsetof(RamLayout, random)))
#define f_pause                 (RAM_WORD(offsetof(RamLayout, pause)))

#define v_unused_F63C           (RAM_LONG(offsetof(RamLayout, _pad_F63C)))
#define v_unused_F63C_SIZE      RAM_FIELD_SIZE(_pad_F63C)

#define v_vdp_buffer2           (RAM_WORD(offsetof(RamLayout, vdp_buffer2)))

#define v_unused_F642           (RAM_WORD(offsetof(RamLayout, _pad_F642)))
#define v_unused_F642_SIZE      RAM_FIELD_SIZE(_pad_F642)

#define f_hblank_pal            (RAM_WORD(offsetof(RamLayout, hblank_pal)))
#define v_waterpos1             (RAM_WORD(offsetof(RamLayout, waterpos1)))
#define v_waterpos2             (RAM_WORD(offsetof(RamLayout, waterpos2)))
#define v_waterpos3             (RAM_WORD(offsetof(RamLayout, waterpos3)))
#define f_water                 (RAM_BYTE(offsetof(RamLayout, water)))
#define v_wtr_routine           (RAM_BYTE(offsetof(RamLayout, wtr_routine)))
#define f_wtr_state             (RAM_BYTE(offsetof(RamLayout, wtr_state)))
#define f_doupdatesinhblank     (RAM_BYTE(offsetof(RamLayout, doupdatesinhblank)))

#define v_pal_buffer            (offsetof(RamLayout, pal_buffer))

/* ---- PLC buffer ---- */
#define v_plc_buffer            (offsetof(RamLayout, plc_buffer))
#define v_plc_buffer_dest       (v_plc_buffer + 4)
#define v_plc_buffer_only_end   (v_plc_buffer + plc_slot_size * 16)
#define v_plc_ptrnemcode        (RAM_LONG(offsetof(RamLayout, plc_ptrnemcode)))
#define v_plc_repeatcount       (RAM_LONG(offsetof(RamLayout, plc_repeatcount)))
#define v_plc_paletteindex      (RAM_LONG(offsetof(RamLayout, plc_paletteindex)))
#define v_plc_previousrow       (RAM_LONG(offsetof(RamLayout, plc_previousrow)))
#define v_plc_dataword          (RAM_LONG(offsetof(RamLayout, plc_dataword)))
#define v_plc_shiftvalue        (RAM_LONG(offsetof(RamLayout, plc_shiftvalue)))
#define v_plc_patternsleft      (RAM_WORD(offsetof(RamLayout, plc_patternsleft)))
#define v_plc_framepatternsleft (RAM_WORD(offsetof(RamLayout, plc_framepatternsleft)))

#define v_unused_F6FC           (RAM_LONG(offsetof(RamLayout, _pad_F6FC)))
#define v_unused_F6FC_SIZE      RAM_FIELD_SIZE(_pad_F6FC)

#define v_plc_buffer_end        (offsetof(RamLayout, screenposx))

/* ---- Level variables ---- */
#define v_screenposx            (RAM_LONG(offsetof(RamLayout, screenposx)))
#define v_screenposy            (RAM_LONG(offsetof(RamLayout, screenposy)))
#define v_bgscreenposx          (RAM_LONG(offsetof(RamLayout, bgscreenposx)))
#define v_bgscreenposy          (RAM_LONG(offsetof(RamLayout, bgscreenposy)))
#define v_bg2screenposx         (RAM_LONG(offsetof(RamLayout, bg2screenposx)))
#define v_bg2screenposy         (RAM_LONG(offsetof(RamLayout, bg2screenposy)))
#define v_bg3screenposx         (RAM_LONG(offsetof(RamLayout, bg3screenposx)))
#define v_bg3screenposy         (RAM_LONG(offsetof(RamLayout, bg3screenposy)))

#define v_limitleft1            (RAM_WORD(offsetof(RamLayout, limitleft1)))
#define v_limitright1           (RAM_WORD(offsetof(RamLayout, limitright1)))
#define v_limittop1             (RAM_WORD(offsetof(RamLayout, limittop1)))
#define v_limitbtm1             (RAM_WORD(offsetof(RamLayout, limitbtm1)))
#define v_limitleft2            (RAM_WORD(offsetof(RamLayout, limitleft2)))
#define v_limitright2           (RAM_WORD(offsetof(RamLayout, limitright2)))
#define v_limittop2             (RAM_WORD(offsetof(RamLayout, limittop2)))
#define v_limitbtm2             (RAM_WORD(offsetof(RamLayout, limitbtm2)))
#define v_unused11              (RAM_WORD(offsetof(RamLayout, unused11)))
#define v_limitleft3            (RAM_WORD(offsetof(RamLayout, limitleft3)))

#define v_unused_F734           (RAM_WORD(offsetof(RamLayout, _pad_F734)))
#define v_unused_F734_SIZE      RAM_FIELD_SIZE(_pad_F734)

#define v_scrshiftx             (RAM_WORD(offsetof(RamLayout, scrshiftx)))
#define v_scrshifty             (RAM_WORD(offsetof(RamLayout, scrshifty)))
#define v_lookshift             (RAM_WORD(offsetof(RamLayout, lookshift)))
#define v_unused7               (RAM_BYTE(offsetof(RamLayout, unused7)))
#define v_unused8               (RAM_BYTE(offsetof(RamLayout, unused8)))
#define v_dle_routine           (RAM_BYTE(offsetof(RamLayout, dle_routine)))

#define v_unused_F743           (RAM_BYTE(offsetof(RamLayout, _pad_F743)))
#define v_unused_F743_SIZE      RAM_FIELD_SIZE(_pad_F743)

#define f_nobgscroll            (RAM_BYTE(offsetof(RamLayout, nobgscroll)))

#define v_unused_F745           (RAM_BYTE(offsetof(RamLayout, _pad_F745)))
#define v_unused_F745_SIZE      RAM_FIELD_SIZE(_pad_F745)

#define v_unused9               (RAM_BYTE(offsetof(RamLayout, unused9)))

#define v_unused_F747           (RAM_BYTE(offsetof(RamLayout, _pad_F747)))
#define v_unused_F747_SIZE      RAM_FIELD_SIZE(_pad_F747)

#define v_unused10              (RAM_BYTE(offsetof(RamLayout, unused10)))

#define v_unused_F749           (RAM_BYTE(offsetof(RamLayout, _pad_F749)))
#define v_unused_F749_SIZE      RAM_FIELD_SIZE(_pad_F749)

#define v_fg_xblock             (RAM_BYTE(offsetof(RamLayout, fg_xblock)))
#define v_fg_yblock             (RAM_BYTE(offsetof(RamLayout, fg_yblock)))
#define v_bg1_xblock            (RAM_BYTE(offsetof(RamLayout, bg1_xblock)))
#define v_bg1_yblock            (RAM_BYTE(offsetof(RamLayout, bg1_yblock)))
#define v_bg2_xblock            (RAM_BYTE(offsetof(RamLayout, bg2_xblock)))
#define v_bg2_yblock            (RAM_BYTE(offsetof(RamLayout, bg2_yblock)))
#define v_bg3_xblock            (RAM_BYTE(offsetof(RamLayout, bg3_xblock)))
#define v_bg3_yblock            (RAM_BYTE(offsetof(RamLayout, bg3_yblock)))

#define v_unused_F752           (RAM_WORD(offsetof(RamLayout, _pad_F752)))
#define v_unused_F752_SIZE      RAM_FIELD_SIZE(_pad_F752)

#define v_fg_scroll_flags       (RAM_WORD(offsetof(RamLayout, fg_scroll_flags)))
#define v_bg1_scroll_flags      (RAM_WORD(offsetof(RamLayout, bg1_scroll_flags)))
#define v_bg2_scroll_flags      (RAM_WORD(offsetof(RamLayout, bg2_scroll_flags)))
#define v_bg3_scroll_flags      (RAM_WORD(offsetof(RamLayout, bg3_scroll_flags)))
#define f_bgscrollvert          (RAM_BYTE(offsetof(RamLayout, bgscrollvert)))

#define v_unused_F75D           (RAM_BYTE(offsetof(RamLayout, _pad_F75D)))
#define v_unused_F75D_SIZE      RAM_FIELD_SIZE(_pad_F75D)

#define v_sonspeedmax           (RAM_WORD(offsetof(RamLayout, sonspeedmax)))
#define v_sonspeedacc           (RAM_WORD(offsetof(RamLayout, sonspeedacc)))
#define v_sonspeeddec           (RAM_WORD(offsetof(RamLayout, sonspeeddec)))
#define v_sonframenum           (RAM_BYTE(offsetof(RamLayout, sonframenum)))
#define f_sonframechg           (RAM_BYTE(offsetof(RamLayout, sonframechg)))
#define v_anglebuffer           (RAM_BYTE(offsetof(RamLayout, anglebuffer)))

#define v_unused_F769           (RAM_BYTE(offsetof(RamLayout, _pad_F769)))
#define v_unused_F769_SIZE      RAM_FIELD_SIZE(_pad_F769)

#define v_anglebuffer2          (RAM_BYTE(offsetof(RamLayout, anglebuffer2)))

#define v_unused_F76B           (RAM_BYTE(offsetof(RamLayout, _pad_F76B)))
#define v_unused_F76B_SIZE      RAM_FIELD_SIZE(_pad_F76B)

#define v_opl_routine           (RAM_BYTE(offsetof(RamLayout, opl_routine)))

#define v_unused_F76D           (RAM_BYTE(offsetof(RamLayout, _pad_F76D)))
#define v_unused_F76D_SIZE      RAM_FIELD_SIZE(_pad_F76D)

#define v_opl_screen            (RAM_WORD(offsetof(RamLayout, opl_screen)))
#define v_opl_data              (offsetof(RamLayout, opl_data))

#define v_ssangle               (RAM_WORD(offsetof(RamLayout, ssangle)))
#define v_ssrotate              (RAM_WORD(offsetof(RamLayout, ssrotate)))

#define v_unused_F784           (RAM_LONG(offsetof(RamLayout, _pad_F784)))
#define v_unused_F784_SIZE      RAM_FIELD_SIZE(_pad_F784)

#define v_btnpushtime1          (RAM_WORD(offsetof(RamLayout, btnpushtime1)))
#define v_btnpushtime2          (RAM_WORD(offsetof(RamLayout, btnpushtime2)))
#define v_palchgspeed           (RAM_WORD(offsetof(RamLayout, palchgspeed)))
#define v_collindex             (RAM_LONG(offsetof(RamLayout, collindex)))
#define v_palss_num             (RAM_WORD(offsetof(RamLayout, palss_num)))
#define v_palss_time            (RAM_WORD(offsetof(RamLayout, palss_time)))
#define v_palss_index           (RAM_WORD(offsetof(RamLayout, palss_index)))
#define v_ssbganim              (RAM_WORD(offsetof(RamLayout, ssbganim)))

#define v_unused_F7A2           (RAM_WORD(offsetof(RamLayout, _pad_F7A2)))
#define v_unused_F7A2_SIZE      RAM_FIELD_SIZE(_pad_F7A2)

#define v_obj31ypos             (RAM_WORD(offsetof(RamLayout, obj31ypos)))

#define v_unused_F7A6           (RAM_BYTE(offsetof(RamLayout, _pad_F7A6)))
#define v_unused_F7A6_SIZE      RAM_FIELD_SIZE(_pad_F7A6)

#define v_bossstatus            (RAM_BYTE(offsetof(RamLayout, bossstatus)))
#define v_trackpos              (RAM_WORD(offsetof(RamLayout, trackpos)))
#define v_trackbyte             (RAM_BYTE(offsetof(RamLayout, trackpos) + 1))
#define f_lockscreen            (RAM_BYTE(offsetof(RamLayout, lockscreen)))

#define v_unused_F7AB           (RAM_BYTE(offsetof(RamLayout, _pad_F7AB)))
#define v_unused_F7AB_SIZE      RAM_FIELD_SIZE(_pad_F7AB)

#define v_256loop1              (RAM_BYTE(offsetof(RamLayout, loop1_256)))
#define v_256loop2              (RAM_BYTE(offsetof(RamLayout, loop2_256)))
#define v_256roll1              (RAM_BYTE(offsetof(RamLayout, roll1_256)))
#define v_256roll2              (RAM_BYTE(offsetof(RamLayout, roll2_256)))

#define v_lani0_frame           (RAM_BYTE(offsetof(RamLayout, lani0_frame)))
#define v_lani0_time            (RAM_BYTE(offsetof(RamLayout, lani0_time)))
#define v_lani1_frame           (RAM_BYTE(offsetof(RamLayout, lani1_frame)))
#define v_lani1_time            (RAM_BYTE(offsetof(RamLayout, lani1_time)))
#define v_lani2_frame           (RAM_BYTE(offsetof(RamLayout, lani2_frame)))
#define v_lani2_time            (RAM_BYTE(offsetof(RamLayout, lani2_time)))
#define v_lani3_frame           (RAM_BYTE(offsetof(RamLayout, lani3_frame)))
#define v_lani3_time            (RAM_BYTE(offsetof(RamLayout, lani3_time)))
#define v_lani4_frame           (RAM_BYTE(offsetof(RamLayout, lani4_frame)))
#define v_lani4_time            (RAM_BYTE(offsetof(RamLayout, lani4_time)))
#define v_lani5_frame           (RAM_BYTE(offsetof(RamLayout, lani5_frame)))
#define v_lani5_time            (RAM_BYTE(offsetof(RamLayout, lani5_time)))

#define v_unused_F7BC           (RAM_WORD(offsetof(RamLayout, _pad_F7BC)))
#define v_unused_F7BC_SIZE      RAM_FIELD_SIZE(_pad_F7BC)

#define v_gfxbigring            (RAM_WORD(offsetof(RamLayout, gfxbigring)))
#define f_conveyrev             (RAM_BYTE(offsetof(RamLayout, conveyrev)))
#define v_obj63                 (offsetof(RamLayout, obj63))
#define f_wtunnelmode           (RAM_BYTE(offsetof(RamLayout, wtunnelmode)))
#define f_playerctrl            (RAM_BYTE(offsetof(RamLayout, playerctrl)))
#define f_wtunneldisallow       (RAM_BYTE(offsetof(RamLayout, wtunneldisallow)))
#define f_slidemode             (RAM_BYTE(offsetof(RamLayout, slidemode)))
#define v_obj6B                 (RAM_BYTE(offsetof(RamLayout, obj6B)))
#define f_lockctrl              (RAM_BYTE(offsetof(RamLayout, lockctrl)))
#define f_bigring               (RAM_BYTE(offsetof(RamLayout, bigring)))
#define f_obj56                 (RAM_BYTE(offsetof(RamLayout, obj56)))

#define v_unused_F7CF           (RAM_BYTE(offsetof(RamLayout, _pad_F7CF)))
#define v_unused_F7CF_SIZE      RAM_FIELD_SIZE(_pad_F7CF)

#define v_itembonus             (RAM_WORD(offsetof(RamLayout, itembonus)))
#define v_timebonus             (RAM_WORD(offsetof(RamLayout, timebonus)))
#define v_ringbonus             (RAM_WORD(offsetof(RamLayout, ringbonus)))
#define f_endactbonus           (RAM_BYTE(offsetof(RamLayout, endactbonus)))
#define v_sonicend              (RAM_BYTE(offsetof(RamLayout, sonicend)))
#define v_lz_deform             (RAM_WORD(offsetof(RamLayout, lz_deform)))

#define  v_cam_x_delay          (RAM_WORD(offsetof(RamLayout, cam_x_delay)))
#define  v_cam_y_delay          (RAM_BYTE(offsetof(RamLayout, cam_y_delay)))
#define  v_spindash_sfx_flag    (RAM_BYTE(offsetof(RamLayout, spindash_sfx_flag)))
#define  v_spindash_sfx_timer   (RAM_BYTE(offsetof(RamLayout, spindash_sfx_timer)))
#define  v_spindash_sfx_pitch   (RAM_BYTE(offsetof(RamLayout, spindash_sfx_pitch)))

#define f_switch                (offsetof(RamLayout, switch_flags))

#define v_scroll_block_1_size   (RAM_WORD(offsetof(RamLayout, scroll_block_1_size)))
#define v_scroll_block_2_size   (RAM_WORD(offsetof(RamLayout, scroll_block_2_size)))
#define v_scroll_block_3_size   (RAM_WORD(offsetof(RamLayout, scroll_block_3_size)))
#define v_scroll_block_4_size   (RAM_WORD(offsetof(RamLayout, scroll_block_4_size)))

#define v_unused_F7F8           (RAM_LONG(offsetof(RamLayout, _pad_F7F8)))
#define v_unused_F7F8_SIZE      RAM_FIELD_SIZE(_pad_F7F8)

/* ---- Sprite table + palettes ---- */
#define v_spritetablebuffer      (offsetof(RamLayout, spritetablebuffer))
#define v_spritetablebuffer_end  (v_spritetablebuffer + spritetable_entrysize * sprites_max)
#define v_palette_water_fading   (v_spritetablebuffer_end - 0x80)

#define v_palette_water          (offsetof(RamLayout, palette_water))
#define v_palette_water_line_1   (v_palette_water + 0x00)
#define v_palette_water_line_2   (v_palette_water + 0x20)
#define v_palette_water_line_3   (v_palette_water + 0x40)
#define v_palette_water_line_4   (v_palette_water + 0x60)
#define v_palette_water_end      (v_palette_water + 0x80)

#define v_palette                (offsetof(RamLayout, palette))
#define v_palette_line_1         (v_palette + 0x00)
#define v_palette_line_2         (v_palette + 0x20)
#define v_palette_line_3         (v_palette + 0x40)
#define v_palette_line_4         (v_palette + 0x60)
#define v_palette_end            (v_palette + 0x80)

#define v_palette_fading         (offsetof(RamLayout, palette_fading))
#define v_palette_fading_line_1  (v_palette_fading + 0x00)
#define v_palette_fading_line_2  (v_palette_fading + 0x20)
#define v_palette_fading_line_3  (v_palette_fading + 0x40)
#define v_palette_fading_line_4  (v_palette_fading + 0x60)
#define v_palette_fading_end     (v_palette_fading + 0x80)

#define v_objstate               (offsetof(RamLayout, objstate))
#define v_objstate_end           (v_objstate + 0xC0)

#define v_systemstack_end        (offsetof(RamLayout, systemstack_end))
#define v_systemstack            (offsetof(RamLayout, _pad_FE00))

/* ---- v_crossresetram (cold boot) ---- */
#define v_crossresetram          (offsetof(RamLayout, _pad_FE00))

#define v_unused_FE00            (RAM_WORD(offsetof(RamLayout, _pad_FE00)))
#define v_unused_FE00_SIZE       RAM_FIELD_SIZE(_pad_FE00)

#define f_restart                (RAM_WORD(offsetof(RamLayout, restart)))
#define v_framecount             (RAM_WORD(offsetof(RamLayout, framecount)))
/* 68k: v_framebyte: equ v_framecount+1, the LOW byte of the word (big-endian).
   On native little-endian the low byte lives at offset + 0. */
#define v_framebyte              (RAM_BYTE(offsetof(RamLayout, framecount)))
#define v_debugitem              (RAM_BYTE(offsetof(RamLayout, debugitem)))

#define v_unused_FE07            (RAM_BYTE(offsetof(RamLayout, _pad_FE07)))
#define v_unused_FE07_SIZE       RAM_FIELD_SIZE(_pad_FE07)

#define v_debuguse               (RAM_WORD(offsetof(RamLayout, debuguse)))
#define v_debugspeedtimer        (RAM_BYTE(offsetof(RamLayout, debugspeedtimer)))
#define v_debugspeed             (RAM_BYTE(offsetof(RamLayout, debugspeed)))
#define v_vblank_count           (RAM_LONG(offsetof(RamLayout, vblank_count)))
#define v_vblank_word            (RAM_WORD(offsetof(RamLayout, vblank_count) + 2))
#define v_vblank_byte            (RAM_BYTE(offsetof(RamLayout, vblank_count) + 3))

#define v_zone                   (RAM_BYTE(offsetof(RamLayout, zone)))
#define v_act                    (RAM_BYTE(offsetof(RamLayout, act)))
#define v_zone_act               (RAM_WORD(offsetof(RamLayout, zone)))
#define v_lives                  (RAM_BYTE(offsetof(RamLayout, lives)))

#define v_unused_FE13            (RAM_BYTE(offsetof(RamLayout, _pad_FE13)))
#define v_unused_FE13_SIZE       RAM_FIELD_SIZE(_pad_FE13)

#define v_air                    (RAM_WORD(offsetof(RamLayout, air)))
#define v_airbyte                (RAM_BYTE(offsetof(RamLayout, air) + 1))
#define v_lastspecial            (RAM_BYTE(offsetof(RamLayout, lastspecial)))

#define v_unused_FE17            (RAM_BYTE(offsetof(RamLayout, _pad_FE17)))
#define v_unused_FE17_SIZE       RAM_FIELD_SIZE(_pad_FE17)

#define v_continues              (RAM_BYTE(offsetof(RamLayout, continues)))

#define v_unused_FE19            (RAM_BYTE(offsetof(RamLayout, _pad_FE19)))
#define v_unused_FE19_SIZE       RAM_FIELD_SIZE(_pad_FE19)

#define f_timeover               (RAM_BYTE(offsetof(RamLayout, timeover)))
#define v_lifecount              (RAM_BYTE(offsetof(RamLayout, lifecount)))
#define f_lifecount              (RAM_BYTE(offsetof(RamLayout, lifecount_flag)))
#define f_ringcount              (RAM_BYTE(offsetof(RamLayout, ringcount_flag)))
#define f_timecount              (RAM_BYTE(offsetof(RamLayout, timecount_flag)))
#define f_scorecount             (RAM_BYTE(offsetof(RamLayout, scorecount_flag)))
#define v_rings                  (RAM_WORD(offsetof(RamLayout, rings)))
#define v_ringbyte               (RAM_BYTE(offsetof(RamLayout, rings) + 1))
#define v_time                   (RAM_LONG(offsetof(RamLayout, time)))
#define v_timemin                (RAM_BYTE(offsetof(RamLayout, time) + 1))
#define v_timesec                (RAM_BYTE(offsetof(RamLayout, time) + 2))
#define v_timecent               (RAM_BYTE(offsetof(RamLayout, time) + 3))
#define v_score                  (RAM_LONG(offsetof(RamLayout, score)))

#define v_unused_FE2A            (RAM_WORD(offsetof(RamLayout, _pad_FE2A)))
#define v_unused_FE2A_SIZE       RAM_FIELD_SIZE(_pad_FE2A)

#define v_shield                 (RAM_BYTE(offsetof(RamLayout, shield)))
#define v_invinc                 (RAM_BYTE(offsetof(RamLayout, invinc)))
#define v_shoes                  (RAM_BYTE(offsetof(RamLayout, shoes)))
#define v_unused1                (RAM_BYTE(offsetof(RamLayout, unused1)))

#define v_lastlamp               (offsetof(RamLayout, lastlamp))
#define v_lamp_xpos              (RAM_WORD(offsetof(RamLayout, lamp_xpos)))
#define v_lamp_ypos              (RAM_WORD(offsetof(RamLayout, lamp_ypos)))
#define v_lamp_rings             (RAM_WORD(offsetof(RamLayout, lamp_rings)))
#define v_lamp_time              (RAM_LONG(offsetof(RamLayout, lamp_time)))
#define v_lamp_dle               (RAM_BYTE(offsetof(RamLayout, lamp_dle)))

#define v_unused_FE3D            (RAM_BYTE(offsetof(RamLayout, _pad_FE3D)))
#define v_unused_FE3D_SIZE       RAM_FIELD_SIZE(_pad_FE3D)

#define v_lamp_limitbtm          (RAM_WORD(offsetof(RamLayout, lamp_limitbtm)))
#define v_lamp_scrx              (RAM_WORD(offsetof(RamLayout, lamp_scrx)))
#define v_lamp_scry              (RAM_WORD(offsetof(RamLayout, lamp_scry)))
#define v_lamp_bgscrx            (RAM_WORD(offsetof(RamLayout, lamp_bgscrx)))
#define v_lamp_bgscry            (RAM_WORD(offsetof(RamLayout, lamp_bgscry)))
#define v_lamp_bg2scrx           (RAM_WORD(offsetof(RamLayout, lamp_bg2scrx)))
#define v_lamp_bg2scry           (RAM_WORD(offsetof(RamLayout, lamp_bg2scry)))
#define v_lamp_bg3scrx           (RAM_WORD(offsetof(RamLayout, lamp_bg3scrx)))
#define v_lamp_bg3scry           (RAM_WORD(offsetof(RamLayout, lamp_bg3scry)))
#define v_lamp_wtrpos            (RAM_WORD(offsetof(RamLayout, lamp_wtrpos)))
#define v_lamp_wtrrout           (RAM_BYTE(offsetof(RamLayout, lamp_wtrrout)))
#define v_lamp_wtrstat           (RAM_BYTE(offsetof(RamLayout, lamp_wtrstat)))
#define v_lamp_lives             (RAM_BYTE(offsetof(RamLayout, lamp_lives)))

#define v_unused_FE55            (RAM_WORD(offsetof(RamLayout, _pad_FE55)))
#define v_unused_FE55_SIZE       RAM_FIELD_SIZE(_pad_FE55)

#define v_emeralds               (RAM_BYTE(offsetof(RamLayout, emeralds)))
#define v_emldlist               (offsetof(RamLayout, emldlist))
#define v_oscillate              (offsetof(RamLayout, oscillate))
#define v_oscillate_values       (offsetof(RamLayout, oscillate_values))

#define v_unused_FEA0            (RAM_LONG(offsetof(RamLayout, _pad_FEA0)))
#define v_unused_FEA0_SIZE       RAM_FIELD_SIZE(_pad_FEA0)

#define v_ani0_time              (RAM_BYTE(offsetof(RamLayout, ani0_time)))
#define v_ani0_frame             (RAM_BYTE(offsetof(RamLayout, ani0_frame)))
#define v_ani1_time              (RAM_BYTE(offsetof(RamLayout, ani1_time)))
#define v_ani1_frame             (RAM_BYTE(offsetof(RamLayout, ani1_frame)))
#define v_ani2_time              (RAM_BYTE(offsetof(RamLayout, ani2_time)))
#define v_ani2_frame             (RAM_BYTE(offsetof(RamLayout, ani2_frame)))
#define v_ani3_time              (RAM_BYTE(offsetof(RamLayout, ani3_time)))
#define v_ani3_frame             (RAM_BYTE(offsetof(RamLayout, ani3_frame)))
#define v_ani3_buf               (RAM_WORD(offsetof(RamLayout, ani3_buf)))

#define v_unused_FECA            (RAM_WORD(offsetof(RamLayout, _pad_FECA)))
#define v_unused_FECA_SIZE       RAM_FIELD_SIZE(_pad_FECA)

#define v_limittopdb             (RAM_WORD(offsetof(RamLayout, limittopdb)))
#define v_limitbtmdb             (RAM_WORD(offsetof(RamLayout, limitbtmdb)))

#define v_unused_FEF4            (RAM_LONG(offsetof(RamLayout, _pad_FEF4)))
#define v_unused_FEF4_SIZE       RAM_FIELD_SIZE(_pad_FEF4)

#define v_chunk0collision        (RAM_WORD(offsetof(RamLayout, chunk0collision)))

#define v_unused_FF02            (RAM_WORD(offsetof(RamLayout, _pad_FF02)))
#define v_unused_FF02_SIZE       RAM_FIELD_SIZE(_pad_FF02)

#define v_screenposx_dup         (RAM_LONG(offsetof(RamLayout, screenposx_dup)))
#define v_screenposy_dup         (RAM_LONG(offsetof(RamLayout, screenposy_dup)))
#define v_bgscreenposx_dup       (RAM_LONG(offsetof(RamLayout, bgscreenposx_dup)))
#define v_bgscreenposy_dup       (RAM_LONG(offsetof(RamLayout, bgscreenposy_dup)))
#define v_bg2screenposx_dup      (RAM_LONG(offsetof(RamLayout, bg2screenposx_dup)))
#define v_bg2screenposy_dup      (RAM_LONG(offsetof(RamLayout, bg2screenposy_dup)))
#define v_bg3screenposx_dup      (RAM_LONG(offsetof(RamLayout, bg3screenposx_dup)))
#define v_bg3screenposy_dup      (RAM_LONG(offsetof(RamLayout, bg3screenposy_dup)))
#define v_fg_scroll_flags_dup    (RAM_WORD(offsetof(RamLayout, fg_scroll_flags_dup)))
#define v_bg1_scroll_flags_dup   (RAM_WORD(offsetof(RamLayout, bg1_scroll_flags_dup)))
#define v_bg2_scroll_flags_dup   (RAM_WORD(offsetof(RamLayout, bg2_scroll_flags_dup)))
#define v_bg3_scroll_flags_dup   (RAM_WORD(offsetof(RamLayout, bg3_scroll_flags_dup)))

#define v_unused_FF38            (RAM_LONG(offsetof(RamLayout, _pad_FF38)))
#define v_unused_FF38_SIZE       RAM_FIELD_SIZE(_pad_FF38)

#define v_levseldelay            (RAM_WORD(offsetof(RamLayout, levseldelay)))
#define v_levselitem             (RAM_WORD(offsetof(RamLayout, levselitem)))
#define v_levselsound            (RAM_WORD(offsetof(RamLayout, levselsound)))

#define v_unused_FF86            (RAM_WORD(offsetof(RamLayout, _pad_FF86)))
#define v_unused_FF86_SIZE       RAM_FIELD_SIZE(_pad_FF86)

#define v_scorecopy              (RAM_LONG(offsetof(RamLayout, scorecopy)))
#define v_scorelife              (RAM_LONG(offsetof(RamLayout, scorecopy)))

#define v_unused_FFC4            (RAM_LONG(offsetof(RamLayout, _pad_FFC4)))
#define v_unused_FFC4_SIZE       RAM_FIELD_SIZE(_pad_FFC4)

#define f_levselcheat            (RAM_BYTE(offsetof(RamLayout, levselcheat)))
#define f_slomocheat             (RAM_BYTE(offsetof(RamLayout, slomocheat)))
#define f_debugcheat             (RAM_BYTE(offsetof(RamLayout, debugcheat)))
#define f_creditscheat           (RAM_BYTE(offsetof(RamLayout, creditscheat)))
#define v_title_dcount           (RAM_WORD(offsetof(RamLayout, title_dcount)))
#define v_title_ccount           (RAM_WORD(offsetof(RamLayout, title_ccount)))

#define v_unused_FFE8            (RAM_WORD(offsetof(RamLayout, _pad_FFE8)))
#define v_unused_FFE8_SIZE       RAM_FIELD_SIZE(_pad_FFE8)

#define v_unused2                (RAM_WORD(offsetof(RamLayout, unused2)))
#define v_unused3                (RAM_BYTE(offsetof(RamLayout, unused3)))
#define v_unused4                (RAM_BYTE(offsetof(RamLayout, unused4)))
#define v_unused5                (RAM_BYTE(offsetof(RamLayout, unused5)))
#define v_unused6                (RAM_BYTE(offsetof(RamLayout, unused6)))

#define f_demo                   (RAM_WORD(offsetof(RamLayout, demo)))
#define v_demonum                (RAM_WORD(offsetof(RamLayout, demonum)))
#define v_creditsnum             (RAM_WORD(offsetof(RamLayout, creditsnum)))

#define v_unused_FFF6            (RAM_WORD(offsetof(RamLayout, _pad_FFF6)))
#define v_unused_FFF6_SIZE       RAM_FIELD_SIZE(_pad_FFF6)

#define v_megadrive              (RAM_BYTE(offsetof(RamLayout, megadrive)))

#define v_unused_FFF9            (RAM_BYTE(offsetof(RamLayout, _pad_FFF9)))
#define v_unused_FFF9_SIZE       RAM_FIELD_SIZE(_pad_FFF9)

#define f_debugmode              (RAM_WORD(offsetof(RamLayout, debugmode)))
#define v_init                   (RAM_LONG(offsetof(RamLayout, init)))

#define v_ram_end                0x10000

/* Anchor addresses for a handful of well-known variables, so any accidental
   layout change is caught at compile time rather than at runtime. */
_Static_assert(offsetof(RamLayout, gamemode)          == 0xF600, "v_gamemode drifted");
_Static_assert(offsetof(RamLayout, blob_objspace)     == 0xD000, "v_objspace drifted");
_Static_assert(offsetof(RamLayout, objstate)          == 0xFC00, "v_objstate drifted");
_Static_assert(offsetof(RamLayout, zone)              == 0xFE10, "v_zone drifted");
_Static_assert(offsetof(RamLayout, init)              == 0xFFFC, "v_init drifted");

/* ===========================================================================
   Object field accessors — unchanged from before.
   =========================================================================== */
#undef obID
#undef obRender
#undef obGfx
#undef obMap
#undef obX
#undef obSubpixelX
#undef obScreenY
#undef obY
#undef obSubpixelY
#undef obVelX
#undef obVelY
#undef obInertia
#undef obHeight
#undef obWidth
#undef obPriority
#undef obActWid
#undef obFrame
#undef obAniFrame
#undef obAnim
#undef obPrevAni
#undef obTimeFrame
#undef obDelayAni
#undef obColType
#undef obColProp
#undef obStatus
#undef obRespawnNo
#undef obRoutine
#undef ob2ndRout
#undef obSolid
#undef obAngle
#undef obSubtype
#undef flashtime
#undef invtime
#undef shoetime
#undef angleright
#undef angleleft
#undef sticktoconvex
#undef spindash_flag
#undef restartime
#undef spindash_count
#undef spindash_decay
#undef jumping
#undef standonobject
#undef locktime

#define obID(obj)        (*(uint8_t *)((uint8_t *)(obj) + 0))
#define obRender(obj)    (*(uint8_t *)((uint8_t *)(obj) + 1))
#define obGfx(obj)       (*(uint16_t*)((uint8_t *)(obj) + 2))
#define obMap(obj)       (*(uint32_t*)((uint8_t *)(obj) + 4))
#define obX(obj)         (*(int16_t *)((uint8_t *)(obj) + 8))
#define obSubpixelX(obj) (*(int16_t *)((uint8_t *)(obj) + 0xA))
#define obScreenY(obj)   (*(int16_t *)((uint8_t *)(obj) + 0xA))
#define obY(obj)         (*(int16_t *)((uint8_t *)(obj) + 0xC))
#define obSubpixelY(obj) (*(int16_t *)((uint8_t *)(obj) + 0xE))
#define obVelX(obj)      (*(int16_t *)((uint8_t *)(obj) + 0x10))
#define obVelY(obj)      (*(int16_t *)((uint8_t *)(obj) + 0x12))
#define obInertia(obj)   (*(int16_t *)((uint8_t *)(obj) + 0x14))
#define obHeight(obj)    (*(uint8_t *)((uint8_t *)(obj) + 0x16))
#define obWidth(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x17))
#define obPriority(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x18))
#define obActWid(obj)    (*(uint8_t *)((uint8_t *)(obj) + 0x19))
#define obFrame(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x1A))
#define obAniFrame(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x1B))
#define obAnim(obj)      (*(uint8_t *)((uint8_t *)(obj) + 0x1C))
#define obPrevAni(obj)   (*(uint8_t *)((uint8_t *)(obj) + 0x1D))
#define obTimeFrame(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x1E))
#define obDelayAni(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x1F))
#define obColType(obj)   (*(uint8_t *)((uint8_t *)(obj) + 0x20))
#define obColProp(obj)   (*(uint8_t *)((uint8_t *)(obj) + 0x21))
#define obStatus(obj)    (*(uint8_t *)((uint8_t *)(obj) + 0x22))
#define obRespawnNo(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x23))
#define obRoutine(obj)   (*(uint8_t *)((uint8_t *)(obj) + 0x24))
#define ob2ndRout(obj)   (*(uint8_t *)((uint8_t *)(obj) + 0x25))
#define obSolid(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x25))
#define obAngle(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x26))
#define obSubtype(obj)   (*(uint8_t *)((uint8_t *)(obj) + 0x28))

#define flashtime(obj)      (*(int16_t *)((uint8_t *)(obj) + 0x30))
#define invtime(obj)        (*(int16_t *)((uint8_t *)(obj) + 0x32))
#define shoetime(obj)       (*(int16_t *)((uint8_t *)(obj) + 0x34))
#define angleright(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x36))
#define angleleft(obj)      (*(uint8_t *)((uint8_t *)(obj) + 0x37))
#define sticktoconvex(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x38))
#define spindash_flag(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x39))
#define restartime(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x3A))
#define spindash_count(obj)  (*(uint16_t *)((uint8_t *)(obj) + 0x3A))
#define spindash_decay(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x3B))
#define jumping(obj)        (*(uint8_t *)((uint8_t *)(obj) + 0x3C))
#define standonobject(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x3D))
#define locktime(obj)       (*(uint8_t *)((uint8_t *)(obj) + 0x3E))

#define animal_doublehop(obj)   (*(uint8_t  *)((uint8_t *)(obj) + 0x29))
#define animal_id(obj)          (*(uint8_t  *)((uint8_t *)(obj) + 0x30))
#define animal_speedX(obj)      (*(int16_t  *)((uint8_t *)(obj) + 0x32))
#define animal_speedY(obj)      (*(int16_t  *)((uint8_t *)(obj) + 0x34))
#define animal_prisondelay(obj) (*(uint16_t *)((uint8_t *)(obj) + 0x36))
#define exitem_pointsframe(obj) (*(uint16_t *)((uint8_t *)(obj) + 0x3E))
#define animal_pointsframe(obj) (*(uint16_t *)((uint8_t *)(obj) + 0x3E))

#define cardMainX(obj)   (*(int16_t *)((uint8_t *)(obj) + 0x30))
#define cardFinalX(obj)  (*(int16_t *)((uint8_t *)(obj) + 0x32))

#define got_mainX(obj)   (*(int16_t *)((uint8_t *)(obj) + 0x30))
#define got_finalX(obj)  (*(int16_t *)((uint8_t *)(obj) + 0x32))
#define got_timeframe(obj) (*(uint16_t *)((uint8_t *)(obj) + 0x1E))

#define ssr_mainX(obj)     (*(int16_t *)((uint8_t *)(obj) + 0x30))
#define ssr_timeframe(obj) (*(uint16_t *)((uint8_t *)(obj) + 0x1E))

#define ring_origX(obj)      (*(int16_t *)((uint8_t *)(obj) + 0x32))
#define ring_respawnbit(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x34))

/* ===========================================================================
   Sound driver RAM (SMPS)
   The snddriver_ram block lives at offsetof(RamLayout, blob_snddriver_ram)
   and is 0x600 bytes long (0x5C0 driver + $40 unused). SMPS variables are
   exposed by adding their intra-block offsets to that base.
   =========================================================================== */
#define v_snd_ram_base           (offsetof(RamLayout, blob_snddriver_ram))

#define v_sndprio                (RAM_BYTE(v_snd_ram_base + 0x00))
#define v_main_tempo_timeout     (RAM_BYTE(v_snd_ram_base + 0x01))
#define v_main_tempo             (RAM_BYTE(v_snd_ram_base + 0x02))
#define f_pausemusic             (RAM_BYTE(v_snd_ram_base + 0x03))
#define v_fadeout_counter        (RAM_BYTE(v_snd_ram_base + 0x04))
#define v_fadeout_delay          (RAM_BYTE(v_snd_ram_base + 0x06))
#define v_communication_byte     (RAM_BYTE(v_snd_ram_base + 0x07))
#define f_updating_dac           (RAM_BYTE(v_snd_ram_base + 0x08))
#define v_sound_id               (RAM_BYTE(v_snd_ram_base + 0x09))
#define v_soundqueue0            (RAM_BYTE(v_snd_ram_base + 0x0A))
#define v_soundqueue1            (RAM_BYTE(v_snd_ram_base + 0x0B))
#define v_soundqueue2            (RAM_BYTE(v_snd_ram_base + 0x0C))
#define f_voice_selector         (RAM_BYTE(v_snd_ram_base + 0x0E))
#define v_voice_ptr              (RAM_LONG(v_snd_ram_base + 0x18))
#define v_special_voice_ptr      (RAM_LONG(v_snd_ram_base + 0x20))
#define f_fadein_flag            (RAM_BYTE(v_snd_ram_base + 0x24))
#define v_fadein_delay           (RAM_BYTE(v_snd_ram_base + 0x25))
#define v_fadein_counter         (RAM_BYTE(v_snd_ram_base + 0x26))
#define f_1up_playing            (RAM_BYTE(v_snd_ram_base + 0x27))
#define v_tempo_mod              (RAM_BYTE(v_snd_ram_base + 0x28))
#define v_speeduptempo           (RAM_BYTE(v_snd_ram_base + 0x29))
#define f_speedup                (RAM_BYTE(v_snd_ram_base + 0x2A))
#define v_ring_speaker           (RAM_BYTE(v_snd_ram_base + 0x2B))
#define f_push_playing           (RAM_BYTE(v_snd_ram_base + 0x2C))

/* ===========================================================================
   Special Stage phase — $FF0000 base, overlays the main layout.
   Kept as raw offsets since they don't participate in the RamLayout struct.
   =========================================================================== */
#define v_sslayout_base         0x0000
#define v_sslayout_actual       0x1020
#define v_sslayout_end          0x3020
#define v_ss_spritesettings     0x4000
#define v_sslayout_decompress   0x4000
#define v_ss_animations         0x4400
#define v_ss_animations_end     0x4500

#define v_ss_rotationmatrix     0x8000
#define v_ss_scroll_bubbles     0xAA00
#define v_ss_scroll_clouds      0xAB00

/* ===========================================================================
   Error handler phase — overlays v_objstate.
   =========================================================================== */
#define v_regbuffer             0xFC00   /* 0x40 bytes */
#define v_spbuffer              0xFC40
#define v_errortype             0xFC44

#ifdef __cplusplus
}
#endif

#endif /* SONIC1_RAM_H */
