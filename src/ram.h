#ifndef SONIC1_RAM_H
#define SONIC1_RAM_H

#include "types.h"
#include "constants.h"

/* ===========================================================================
   RAM — translated from _Variables.asm (+ s1.sounddriver.ram.asm)

   The 68000 has 64KB of RAM mapped at $FFFF0000..$FFFFFFFF. In C we model it
   as a flat byte array `ram[0x10000]`. Every variable declared in the ASM is
   an offset into this array (the ASM base $FFFF0000 maps to ram[0x0000]).

   Addresses below are the REAL addresses used by the original game, verified
   against the canonical Sonic 1 disassembly (e.g. v_gamemode = $FFFFF600,
   v_objspace = $FFFFD000, v_objstate = $FFFFFC00, v_systemstack = $FFFFFE00).
   They fit exactly in the 64KB window (last variable, v_init, ends at 0x10000).

   Endianness: the Mega Drive stores words big-endian. RAM is just a byte
   array here, and for live game logic we read/write using native (little-
   endian on PC) typed lvalues, which is correct for a port. The BE helpers
   are provided for loading big-endian data from the ROM when required.
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

/* Convenience: raw pointer into the RAM array */
#define RAM_ADDR(addr)  (&ram[(addr)])

/* ---------------------------------------------------------------------------
   Big-endian RAM accessors (for reading/writing big-endian data, e.g. data
   reloaded from the ROM or VDP-formatted words). These do NOT create lvalues;
   use the RAM_SET_* forms to write.
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
   Large fixed blocks (offsets into ram[])
   =========================================================================== */
#define v_ram_start_def         0x0000
#define v_ram_start             0x0000

/* 256x256 tile mappings ($A400 = chunk_size * $52 chunks) */
#define v_256x256               0x0000
#define v_256x256_end           (v_256x256 + chunk_size * 0x52)

/* Level layouts (FG/BG rows interlaced, 8 rows, $400 bytes total) */
#define v_lvllayout             0xA400
#define v_lvllayout_fg          (v_lvllayout)
#define v_lvllayout_bg          (v_lvllayout + layout_row_interlaced)
#define v_lvllayout_end         (v_lvllayout + layout_row * 8)

/* Background scroll buffer */
#define v_bgscroll_buffer       0xA800

/* Nemesis graphics decompression buffer */
#define v_ngfx_buffer           0xAA00
#define v_ngfx_buffer_end       (v_ngfx_buffer + 0x200)

/* Sprite display queue, in priority order (8 * $80 = $400 bytes) */
#define v_spritequeue           0xAC00

/* 16x16 tile mappings */
#define v_16x16                 0xB000

/* Buffered Sonic graphics ($17 cells) */
#define v_sgfx_buffer           0xC800
#define v_sgfx_buffer_end       (v_sgfx_buffer + tile_size * 23)

/* Position tracking data for Sonic */
#define v_tracksonic            0xCB00

/* Scrolling table data */
#define v_hscrolltablebuffer    0xCC00
#define v_hscrolltablebuffer_end        (v_hscrolltablebuffer + 0x380)
#define v_hscrolltablebuffer_end_padded (v_hscrolltablebuffer + 0x400)

/* Object variable space ($40 bytes per object, $2000 bytes total) */
#define v_objspace              0xD000

/* Title screen objects */
#define v_sonicteam             (v_objspace + object_size * 2)
#define v_titlesonic            (v_objspace + object_size * 1)
#define v_pressstart            (v_objspace + object_size * 2)
#define v_titletm               (v_objspace + object_size * 3)
#define v_ttlsonichide          (v_objspace + object_size * 4)

/* Level objects */
#define v_player                (v_objspace + object_size * 0)
#define v_hud                   (v_objspace + object_size * 1)

/* Title card objects ($100 = 4 objects) */
#define v_titlecard             (v_objspace + object_size * 2)
#define v_ttlcardname           (v_titlecard + object_size * 0)
#define v_ttlcardzone           (v_titlecard + object_size * 1)
#define v_ttlcardact            (v_titlecard + object_size * 2)
#define v_ttlcardoval           (v_titlecard + object_size * 3)

/* Game over / time over text */
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

/* Level results card ($1C0 = 7 objects) */
#define v_endcard               (v_objspace + object_size * 23)
#define v_endcardsonic          (v_endcard + object_size * 0)
#define v_endcardpassed         (v_endcard + object_size * 1)
#define v_endcardact            (v_endcard + object_size * 2)
#define v_endcardscore          (v_endcard + object_size * 3)
#define v_endcardtime           (v_endcard + object_size * 4)
#define v_endcardring           (v_endcard + object_size * 5)
#define v_endcardoval           (v_endcard + object_size * 6)

/* Level object variable space ($1800 bytes = 96 objects) */
#define v_lvlobjspace           (v_objspace + object_size * 32)
#define v_lvlobjend             (v_lvlobjspace + object_size * 96)
#define v_objspace_end          (v_lvlobjend)

/* Special Stage objects */
#define v_ssrescard             (v_objspace + object_size * 23)   /* $140 */
#define v_ssrestext             (v_ssrescard + object_size * 0)
#define v_ssresscore            (v_ssrescard + object_size * 1)
#define v_ssresring             (v_ssrescard + object_size * 2)
#define v_ssresoval             (v_ssrescard + object_size * 3)
#define v_ssrescontinue         (v_ssrescard + object_size * 4)
#define v_ssresemeralds         (v_objspace + object_size * 32)   /* $180 */

/* Continue screen objects */
#define v_continuetext          (v_objspace + object_size * 1)
#define v_continuelight         (v_objspace + object_size * 2)
#define v_continueicon          (v_objspace + object_size * 3)

/* Ending objects */
#define v_endemeralds           (v_objspace + object_size * 16)   /* $180 */
#define v_endemeralds_end       (v_objspace + object_size * 32)
#define v_endlogo               (v_objspace + object_size * 16)

/* Credits objects */
#define v_credits               (v_objspace + object_size * 2)
#define v_endeggman             (v_objspace + object_size * 2)
#define v_tryagain              (v_objspace + object_size * 3)
#define v_eggmanchaos           (v_objspace + object_size * 32)   /* $180 */

/* Sound driver state (SMPS). Base of the sound driver RAM block. */
#define v_snddriver_ram         0xF000

/* ---------------------------------------------------------------------------
   Game-mode / VDP registers and misc small variables (all single/lite RAM)
   Offsets are relative to ram[] (base $FFFF0000).
   --------------------------------------------------------------------------- */

#define v_gamemode_def          0xF600
#define v_gamemode              (RAM_BYTE(v_gamemode_def))

#define v_jpadhold2             (RAM_BYTE(0xF602))
#define v_jpadpress2            (RAM_BYTE(0xF603))
#define v_jpadhold1             (RAM_BYTE(0xF604))
#define v_jpadpress1            (RAM_BYTE(0xF605))

#define v_vdp_buffer1           (RAM_WORD(0xF60C))

#define v_generictimer          (RAM_WORD(0xF614))
#define v_scrposy_vdp           (RAM_WORD(0xF616))
#define v_bgscrposy_vdp         (RAM_WORD(0xF618))
#define v_scrposx_vdp           (RAM_WORD(0xF61A))
#define v_bgscrposx_vdp         (RAM_WORD(0xF61C))
#define v_bg3scrposy_vdp        (RAM_WORD(0xF61E))
#define v_bg3scrposx_vdp        (RAM_WORD(0xF620))

#define v_hblank_hreg           (RAM_WORD(0xF624))
#define v_hblank_line           (RAM_BYTE(0xF625))
#define v_pfade_start           (RAM_BYTE(0xF626))
#define v_pfade_size            (RAM_BYTE(0xF627))

/* Misc variables (v_misc_variables) */
#define v_vblank_0e_counter     (RAM_BYTE(0xF628))
#define v_vblank_routine        (RAM_BYTE(0xF62A))
#define v_spritecount           (RAM_BYTE(0xF62C))
#define v_pcyc_num              (RAM_WORD(0xF632))
#define v_pcyc_time             (RAM_WORD(0xF634))
#define v_random                (RAM_LONG(0xF636))
#define f_pause                 (RAM_WORD(0xF63A))
#define v_vdp_buffer2           (RAM_WORD(0xF640))
#define f_hblank_pal            (RAM_WORD(0xF644))
#define v_waterpos1             (RAM_WORD(0xF646))
#define v_waterpos2             (RAM_WORD(0xF648))
#define v_waterpos3             (RAM_WORD(0xF64A))
#define f_water                 (RAM_BYTE(0xF64C))
#define v_wtr_routine           (RAM_BYTE(0xF64D))
#define f_wtr_state             (RAM_BYTE(0xF64E))
#define f_doupdatesinhblank     (RAM_BYTE(0xF64F))

/* Palette data buffer (used for palette cycling, $30 bytes) */
#define v_pal_buffer            0xF650

/* Pattern load cues buffer ($60 bytes = plc_slot_size * 16) */
#define v_plc_buffer            0xF680
#define v_plc_buffer_dest       (v_plc_buffer + 4)
#define v_plc_buffer_only_end   (v_plc_buffer + plc_slot_size * 16)
#define v_plc_ptrnemcode        (RAM_LONG(0xF6E0))
#define v_plc_repeatcount       (RAM_LONG(0xF6E4))
#define v_plc_paletteindex      (RAM_LONG(0xF6E8))
#define v_plc_previousrow       (RAM_LONG(0xF6EC))
#define v_plc_dataword          (RAM_LONG(0xF6F0))
#define v_plc_shiftvalue        (RAM_LONG(0xF6F4))
#define v_plc_patternsleft      (RAM_WORD(0xF6F8))
#define v_plc_framepatternsleft (RAM_WORD(0xF6FA))
#define v_plc_buffer_end        0xF700

/* ---------------------------------------------------------------------------
   Level variables (v_levelvariables) — reset between levels
   --------------------------------------------------------------------------- */
#define v_screenposx            (RAM_LONG(0xF700))
#define v_screenposy            (RAM_LONG(0xF704))
#define v_bgscreenposx          (RAM_LONG(0xF708))
#define v_bgscreenposy          (RAM_LONG(0xF70C))
#define v_bg2screenposx         (RAM_LONG(0xF710))
#define v_bg2screenposy         (RAM_LONG(0xF714))
#define v_bg3screenposx         (RAM_LONG(0xF718))
#define v_bg3screenposy         (RAM_LONG(0xF71C))

#define v_limitleft1            (RAM_WORD(0xF720))
#define v_limitright1           (RAM_WORD(0xF722))
#define v_limittop1             (RAM_WORD(0xF724))
#define v_limitbtm1             (RAM_WORD(0xF726))
#define v_limitleft2            (RAM_WORD(0xF728))
#define v_limitright2           (RAM_WORD(0xF72A))
#define v_limittop2             (RAM_WORD(0xF72C))
#define v_limitbtm2             (RAM_WORD(0xF72E))
#define v_unused11              (RAM_WORD(0xF730))
#define v_limitleft3            (RAM_WORD(0xF732))

#define v_scrshiftx             (RAM_WORD(0xF73A))
#define v_scrshifty             (RAM_WORD(0xF73C))
#define v_lookshift             (RAM_WORD(0xF73E))
#define v_unused7               (RAM_BYTE(0xF740))
#define v_unused8               (RAM_BYTE(0xF741))
#define v_dle_routine           (RAM_BYTE(0xF742))
#define f_nobgscroll            (RAM_BYTE(0xF744))
#define v_unused9               (RAM_BYTE(0xF746))
#define v_unused10              (RAM_BYTE(0xF748))

#define v_fg_xblock             (RAM_BYTE(0xF74A))
#define v_fg_yblock             (RAM_BYTE(0xF74B))
#define v_bg1_xblock            (RAM_BYTE(0xF74C))
#define v_bg1_yblock            (RAM_BYTE(0xF74D))
#define v_bg2_xblock            (RAM_BYTE(0xF74E))
#define v_bg2_yblock            (RAM_BYTE(0xF74F))
#define v_bg3_xblock            (RAM_BYTE(0xF750))
#define v_bg3_yblock            (RAM_BYTE(0xF751))

#define v_fg_scroll_flags       (RAM_WORD(0xF754))
#define v_bg1_scroll_flags      (RAM_WORD(0xF756))
#define v_bg2_scroll_flags      (RAM_WORD(0xF758))
#define v_bg3_scroll_flags      (RAM_WORD(0xF75A))
#define f_bgscrollvert          (RAM_BYTE(0xF75C))

#define v_sonspeedmax           (RAM_WORD(0xF760))
#define v_sonspeedacc           (RAM_WORD(0xF762))
#define v_sonspeeddec           (RAM_WORD(0xF764))
#define v_sonframenum           (RAM_BYTE(0xF766))
#define f_sonframechg           (RAM_BYTE(0xF767))
#define v_anglebuffer           (RAM_BYTE(0xF768))
#define v_anglebuffer2          (RAM_BYTE(0xF76A))

#define v_opl_routine           (RAM_BYTE(0xF76C))
#define v_opl_screen            (RAM_WORD(0xF76E))
#define v_opl_data              0xF770

#define v_ssangle               (RAM_WORD(0xF780))
#define v_ssrotate              (RAM_WORD(0xF782))

#define v_btnpushtime1          (RAM_WORD(0xF790))
#define v_btnpushtime2          (RAM_WORD(0xF792))
#define v_palchgspeed           (RAM_WORD(0xF794))
#define v_collindex             (RAM_LONG(0xF796))
#define v_palss_num             (RAM_WORD(0xF79A))
#define v_palss_time            (RAM_WORD(0xF79C))
#define v_palss_index           (RAM_WORD(0xF79E))
#define v_ssbganim              (RAM_WORD(0xF7A0))

#define v_obj31ypos             (RAM_WORD(0xF7A4))
#define v_bossstatus            (RAM_BYTE(0xF7A7))
#define v_trackpos              (RAM_WORD(0xF7A8))
#define v_trackbyte             (RAM_BYTE(0xF7A9))
#define f_lockscreen            (RAM_BYTE(0xF7AA))

#define v_256loop1              (RAM_BYTE(0xF7AC))
#define v_256loop2              (RAM_BYTE(0xF7AD))
#define v_256roll1              (RAM_BYTE(0xF7AE))
#define v_256roll2              (RAM_BYTE(0xF7AF))

#define v_lani0_frame           (RAM_BYTE(0xF7B0))
#define v_lani0_time            (RAM_BYTE(0xF7B1))
#define v_lani1_frame           (RAM_BYTE(0xF7B2))
#define v_lani1_time            (RAM_BYTE(0xF7B3))
#define v_lani2_frame           (RAM_BYTE(0xF7B4))
#define v_lani2_time            (RAM_BYTE(0xF7B5))
#define v_lani3_frame           (RAM_BYTE(0xF7B6))
#define v_lani3_time            (RAM_BYTE(0xF7B7))
#define v_lani4_frame           (RAM_BYTE(0xF7B8))
#define v_lani4_time            (RAM_BYTE(0xF7B9))
#define v_lani5_frame           (RAM_BYTE(0xF7BA))
#define v_lani5_time            (RAM_BYTE(0xF7BB))

#define v_gfxbigring            (RAM_WORD(0xF7BE))
#define f_conveyrev             (RAM_BYTE(0xF7C0))
#define v_obj63                 0xF7C1            /* 6 bytes */
#define f_wtunnelmode           (RAM_BYTE(0xF7C7))
#define f_playerctrl            (RAM_BYTE(0xF7C8))
#define f_wtunneldisallow       (RAM_BYTE(0xF7C9))
#define f_slidemode             (RAM_BYTE(0xF7CA))
#define v_obj6B                 (RAM_BYTE(0xF7CB))
#define f_lockctrl              (RAM_BYTE(0xF7CC))
#define f_bigring               (RAM_BYTE(0xF7CD))
#define f_obj56                 (RAM_BYTE(0xF7CE))

#define v_itembonus             (RAM_WORD(0xF7D0))
#define v_timebonus             (RAM_WORD(0xF7D2))
#define v_ringbonus             (RAM_WORD(0xF7D4))
#define f_endactbonus           (RAM_BYTE(0xF7D6))
#define v_sonicend              (RAM_BYTE(0xF7D7))
#define v_lz_deform             (RAM_WORD(0xF7D8))

#define f_switch                0xF7E0            /* 16 bytes */

#define v_scroll_block_1_size   (RAM_WORD(0xF7F0))
#define v_scroll_block_2_size   (RAM_WORD(0xF7F2))
#define v_scroll_block_3_size   (RAM_WORD(0xF7F4))
#define v_scroll_block_4_size   (RAM_WORD(0xF7F6))

/* ---------------------------------------------------------------------------
   Sprite table + palettes
   --------------------------------------------------------------------------- */
/* Sprite table ($280 bytes; last $80 are overwritten by v_palette_water_fading) */
#define v_spritetablebuffer     0xF800
#define v_spritetablebuffer_end (v_spritetablebuffer + spritetable_entrysize * sprites_max)
#define v_palette_water_fading  (v_spritetablebuffer_end - 0x80)

/* Main underwater palette, 4 palette lines of $20 bytes each */
#define v_palette_water         0xFA80
#define v_palette_water_line_1  (v_palette_water + 0x00)
#define v_palette_water_line_2  (v_palette_water + 0x20)
#define v_palette_water_line_3  (v_palette_water + 0x40)
#define v_palette_water_line_4  (v_palette_water + 0x60)
#define v_palette_water_end     (v_palette_water + 0x80)

/* Main palette */
#define v_palette               0xFB00
#define v_palette_line_1        (v_palette + 0x00)
#define v_palette_line_2        (v_palette + 0x20)
#define v_palette_line_3        (v_palette + 0x40)
#define v_palette_line_4        (v_palette + 0x60)
#define v_palette_end           (v_palette + 0x80)

/* Palette buffer used for fade-in */
#define v_palette_fading        0xFB80
#define v_palette_fading_line_1 (v_palette_fading + 0x00)
#define v_palette_fading_line_2 (v_palette_fading + 0x20)
#define v_palette_fading_line_3 (v_palette_fading + 0x40)
#define v_palette_fading_line_4 (v_palette_fading + 0x60)
#define v_palette_fading_end    (v_palette_fading + 0x80)

/* Object state list */
#define v_objstate              0xFC00
#define v_objstate_end          (v_objstate + 0xC0)

/* System stack */
#define v_systemstack_end       0xFCC0
#define v_systemstack           0xFE00

/* ---------------------------------------------------------------------------
   RAM that is only cleared on a cold boot (v_crossresetram)
   --------------------------------------------------------------------------- */
#define v_crossresetram         0xFE00

#define f_restart               (RAM_WORD(0xFE02))
#define v_framecount            (RAM_WORD(0xFE04))
#define v_framebyte             (RAM_BYTE(0xFE05))
#define v_debugitem             (RAM_BYTE(0xFE06))
#define v_debuguse              (RAM_WORD(0xFE08))
#define v_debugspeedtimer       (RAM_BYTE(0xFE0A))
#define v_debugspeed            (RAM_BYTE(0xFE0B))
#define v_vblank_count          (RAM_LONG(0xFE0C))
#define v_vblank_word           (RAM_WORD(0xFE0E))
#define v_vblank_byte           (RAM_BYTE(0xFE0F))

#define v_zone                  (RAM_BYTE(0xFE10))
#define v_act                   (RAM_BYTE(0xFE11))
#define v_zone_act              (RAM_WORD(0xFE10))
#define v_lives                 (RAM_BYTE(0xFE12))
#define v_air                   (RAM_WORD(0xFE14))
#define v_airbyte               (RAM_BYTE(0xFE15))
#define v_lastspecial           (RAM_BYTE(0xFE16))
#define v_continues             (RAM_BYTE(0xFE18))
#define f_timeover              (RAM_BYTE(0xFE1A))
#define v_lifecount             (RAM_BYTE(0xFE1B))
#define f_lifecount             (RAM_BYTE(0xFE1C))
#define f_ringcount             (RAM_BYTE(0xFE1D))
#define f_timecount             (RAM_BYTE(0xFE1E))
#define f_scorecount            (RAM_BYTE(0xFE1F))

#define v_rings                 (RAM_WORD(0xFE20))
#define v_ringbyte              (RAM_BYTE(0xFE21))
#define v_time                  (RAM_LONG(0xFE22))
#define v_timemin               (RAM_BYTE(0xFE23))
#define v_timesec               (RAM_BYTE(0xFE24))
#define v_timecent              (RAM_BYTE(0xFE25))
#define v_score                 (RAM_LONG(0xFE26))

#define v_shield                (RAM_BYTE(0xFE2C))
#define v_invinc                (RAM_BYTE(0xFE2D))
#define v_shoes                 (RAM_BYTE(0xFE2E))
#define v_unused1               (RAM_BYTE(0xFE2F))

#define v_lastlamp              0xFE30            /* 2 bytes */
#define v_lamp_xpos             (RAM_WORD(0xFE32))
#define v_lamp_ypos             (RAM_WORD(0xFE34))
#define v_lamp_rings            (RAM_WORD(0xFE36))
#define v_lamp_time             (RAM_LONG(0xFE38))
#define v_lamp_dle              (RAM_BYTE(0xFE3C))
#define v_lamp_limitbtm         (RAM_WORD(0xFE3E))
#define v_lamp_scrx             (RAM_WORD(0xFE40))
#define v_lamp_scry             (RAM_WORD(0xFE42))
#define v_lamp_bgscrx           (RAM_WORD(0xFE44))
#define v_lamp_bgscry           (RAM_WORD(0xFE46))
#define v_lamp_bg2scrx          (RAM_WORD(0xFE48))
#define v_lamp_bg2scry          (RAM_WORD(0xFE4A))
#define v_lamp_bg3scrx          (RAM_WORD(0xFE4C))
#define v_lamp_bg3scry          (RAM_WORD(0xFE4E))
#define v_lamp_wtrpos           (RAM_WORD(0xFE50))
#define v_lamp_wtrrout          (RAM_BYTE(0xFE52))
#define v_lamp_wtrstat          (RAM_BYTE(0xFE53))
#define v_lamp_lives            (RAM_BYTE(0xFE54))

#define v_emeralds              (RAM_BYTE(0xFE57))
#define v_emldlist              0xFE58            /* 6 bytes */
#define v_oscillate             (RAM_WORD(0xFE5E))

/* Timing and screen variables */
#define v_ani0_time             (RAM_BYTE(0xFEC0))
#define v_ani0_frame            (RAM_BYTE(0xFEC1))
#define v_ani1_time             (RAM_BYTE(0xFEC2))
#define v_ani1_frame            (RAM_BYTE(0xFEC3))
#define v_ani2_time             (RAM_BYTE(0xFEC4))
#define v_ani2_frame            (RAM_BYTE(0xFEC5))
#define v_ani3_time             (RAM_BYTE(0xFEC6))
#define v_ani3_frame            (RAM_BYTE(0xFEC7))
#define v_ani3_buf              (RAM_WORD(0xFEC8))

#define v_limittopdb            (RAM_WORD(0xFEF0))
#define v_limitbtmdb            (RAM_WORD(0xFEF2))

/* Duplicate screen positions */
#define v_screenposx_dup        (RAM_LONG(0xFF10))
#define v_screenposy_dup        (RAM_LONG(0xFF14))
#define v_bgscreenposx_dup      (RAM_LONG(0xFF18))
#define v_bgscreenposy_dup      (RAM_LONG(0xFF1C))
#define v_bg2screenposx_dup     (RAM_LONG(0xFF20))
#define v_bg2screenposy_dup     (RAM_LONG(0xFF24))
#define v_bg3screenposx_dup     (RAM_LONG(0xFF28))
#define v_bg3screenposy_dup     (RAM_LONG(0xFF2C))
#define v_fg_scroll_flags_dup   (RAM_WORD(0xFF30))
#define v_bg1_scroll_flags_dup  (RAM_WORD(0xFF32))
#define v_bg2_scroll_flags_dup  (RAM_WORD(0xFF34))
#define v_bg3_scroll_flags_dup  (RAM_WORD(0xFF36))

/* Level select */
#define v_levseldelay           (RAM_WORD(0xFF80))
#define v_levselitem            (RAM_WORD(0xFF82))
#define v_levselsound           (RAM_WORD(0xFF84))

/* Score duplicate (REV00) / extra life score (REV01) — same address */
#define v_scorecopy             (RAM_LONG(0xFFC0))
#define v_scorelife             (RAM_LONG(0xFFC0))

/* Cheat flags / title counters */
#define f_levselcheat           (RAM_BYTE(0xFFE0))
#define f_slomocheat            (RAM_BYTE(0xFFE1))
#define f_debugcheat            (RAM_BYTE(0xFFE2))
#define f_creditscheat          (RAM_BYTE(0xFFE3))
#define v_title_dcount          (RAM_WORD(0xFFE4))
#define v_title_ccount          (RAM_WORD(0xFFE6))
#define v_unused2               (RAM_WORD(0xFFEA))
#define v_unused3               (RAM_BYTE(0xFFEC))
#define v_unused4               (RAM_BYTE(0xFFED))
#define v_unused5               (RAM_BYTE(0xFFEE))
#define v_unused6               (RAM_BYTE(0xFFEF))

/* End-of-RAM control */
#define f_demo                  (RAM_WORD(0xFFF0))
#define v_demonum               (RAM_WORD(0xFFF2))
#define v_creditsnum            (RAM_WORD(0xFFF4))
#define v_megadrive             (RAM_BYTE(0xFFF8))
#define f_debugmode             (RAM_WORD(0xFFFA))
#define v_init                  (RAM_LONG(0xFFFC))

#define v_ram_end               0x10000

/* ===========================================================================
   Object field accessors
   ---------------------------------------------------------------------------
   Each accessor takes a pointer to a 64-byte object block (an offset into
   ram[], e.g. &ram[v_objspace + object_size*N]) and reads/writes the field at
   the matching ASM byte offset inside that block.

   These supersede the plain numeric offset constants of the same names in
   constants.h — game code should use these function-like accessors.
   --------------------------------------------------------------------------- */
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
#undef restartime
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

/* Sonic-specific object fields */
#define flashtime(obj)      (*(int16_t *)((uint8_t *)(obj) + 0x30))
#define invtime(obj)        (*(int16_t *)((uint8_t *)(obj) + 0x32))
#define shoetime(obj)       (*(int16_t *)((uint8_t *)(obj) + 0x34))
#define angleright(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x36))
#define angleleft(obj)      (*(uint8_t *)((uint8_t *)(obj) + 0x37))
#define sticktoconvex(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x38))
#define restartime(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x3A))
#define jumping(obj)        (*(uint8_t *)((uint8_t *)(obj) + 0x3C))
#define standonobject(obj)  (*(uint8_t *)((uint8_t *)(obj) + 0x3D))
#define locktime(obj)       (*(uint8_t *)((uint8_t *)(obj) + 0x3E))

/* Zone title card specific fields (objoff_30/objoff_32) */
#define cardMainX(obj)   (*(int16_t *)((uint8_t *)(obj) + 0x30))  /* target X while moving in */
#define cardFinalX(obj)  (*(int16_t *)((uint8_t *)(obj) + 0x32))  /* target X while moving out */

/* ---------------------------------------------------------------------------
   Sound driver RAM (SMPS) — within the block starting at v_snddriver_ram.
   Offsets relative to v_snddriver_ram. (Audio is a stub; see sound.h/.c)
   --------------------------------------------------------------------------- */
#define v_sndprio               (RAM_BYTE(0xF000 + 0x00))
#define v_main_tempo_timeout    (RAM_BYTE(0xF000 + 0x01))
#define v_main_tempo            (RAM_BYTE(0xF000 + 0x02))
#define f_pausemusic            (RAM_BYTE(0xF000 + 0x03))
#define v_fadeout_counter       (RAM_BYTE(0xF000 + 0x04))
#define v_fadeout_delay         (RAM_BYTE(0xF000 + 0x06))
#define v_communication_byte    (RAM_BYTE(0xF000 + 0x07))
#define f_updating_dac          (RAM_BYTE(0xF000 + 0x08))
#define v_sound_id              (RAM_BYTE(0xF000 + 0x09))
#define v_soundqueue0           (RAM_BYTE(0xF000 + 0x0A))
#define v_soundqueue1           (RAM_BYTE(0xF000 + 0x0B))
#define v_soundqueue2           (RAM_BYTE(0xF000 + 0x0C))
#define f_voice_selector        (RAM_BYTE(0xF000 + 0x0E))
#define v_voice_ptr             (RAM_LONG(0xF000 + 0x18))
#define v_special_voice_ptr     (RAM_LONG(0xF000 + 0x20))
#define f_fadein_flag           (RAM_BYTE(0xF000 + 0x24))
#define v_fadein_delay          (RAM_BYTE(0xF000 + 0x25))
#define v_fadein_counter        (RAM_BYTE(0xF000 + 0x26))
#define f_1up_playing           (RAM_BYTE(0xF000 + 0x27))
#define v_tempo_mod             (RAM_BYTE(0xF000 + 0x28))
#define v_speeduptempo          (RAM_BYTE(0xF000 + 0x29))
#define f_speedup               (RAM_BYTE(0xF000 + 0x2A))
#define v_ring_speaker          (RAM_BYTE(0xF000 + 0x2B))
#define f_push_playing          (RAM_BYTE(0xF000 + 0x2C))

#ifdef __cplusplus
}
#endif

#endif /* SONIC1_RAM_H */
