#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdlib.h>

#include "types.h"
#include "constants.h"
#include "ram.h"
#include "objects.h"
#include "sound.h"
#include "deform.h"

/* Game Mode ID for the title-screen check in Deform_GHZ (from sonic.asm). */
#define GM_Title 0x04

/* ===========================================================================
   Faithful C port of the Sonic 1 background deformation subsystem.
   Built from the disassembly REV01 sources (Revision=1, FixBugs=0):
     - _inc/DeformLayers (REV01).asm
     - _inc/ScrollHoriz & ScrollVertical.asm
     - _inc/DynamicLevelEvents.asm
   =========================================================================== */

/* ---------------------------------------------------------------------------
   Camera 16.16 fixed-point helpers.

   A 68k long is stored in RAM as two little-endian words:
     [addr+0] integer part, [addr+2] fraction (like the original object RAM,
   whose "integer" is the high word and consequently RAM_WORD(addr) gives the
   integer that consume.rs/sprites.c read). This keeps all existing RAM_LONG
   consumers (which truncate to (int16_t)) valid.
   ------------------------------------------------------------------------- */
static int32_t cam_getl(uint16_t addr) {
    return ((int32_t)((int16_t)RAM_WORD(addr)) << 16) | (uint16_t)RAM_WORD(addr + 2);
}

static void cam_setl(uint16_t addr, int32_t v) {
    RAM_WORD(addr)     = (uint16_t)((uint32_t)v >> 16);
    RAM_WORD(addr + 2) = (uint16_t)(uint32_t)v;
}

static int16_t cam_int(uint16_t addr) {
    return (int16_t)RAM_WORD(addr);
}

/* Forward declarations of the ripple tables (defined below Deform_LZ). */
static const int8_t Lz_Scroll_Data[256];
static const int8_t Drown_WobbleData[256];

/* 68k "swap" on a 32-bit register. */
static int32_t rot16(int32_t v) {
    return (int32_t)(((uint32_t)v >> 16) | ((uint32_t)v << 16));
}

/* ---------------------------------------------------------------------------
   Background scroll buffer (shared scratch, REV01 zones).
   ------------------------------------------------------------------------- */
#define DEFORM_BUF   (0xA800u)      /* v_bgscroll_buffer */
#define DEFORM_BUFW(i) (RAM_WORD(DEFORM_BUF + 2 * (i)))

/* ---------------------------------------------------------------------------
   ScrollHoriz / MoveScreenHoriz  (REV00/REV01, FixBugs=0)
   _inc/ScrollHoriz & ScrollVertical.asm:6-100
   ------------------------------------------------------------------------- */
static void SetScreenX(int16_t old_sx, int32_t d0w) {
    /* SH_SetScreen: d1 = (d0 - old) << 8 (asl.w #8, 16-bit wrap) */
    int32_t d1 = (int32_t)(uint16_t)((uint16_t)d0w - (uint16_t)(uint16_t)old_sx) << 8;
    RAM_WORD(0xF700) = (uint16_t)d0w;             /* v_screenposx */
    RAM_WORD(0xF73A) = (uint16_t)d1;              /* v_scrshiftx */
}

static void MoveScreenHoriz(void) {
    int16_t spx = cam_int(0xF700);                /* v_screenposx */
    uint16_t a = (uint16_t)((int)obX(&ram[v_player]) - (int)spx);

    /* subi.w #(320/2)-16,d0 ; bcs.s SH_MoveCameraLeft -> Sonic < 144px from edge */
    if (a < 144u) {
        /* SH_MoveCameraLeft (FixBugs=0 has no -16 cap) */
        uint16_t d0 = (uint16_t)((a - 144u) + (uint16_t)spx);
        int16_t l = (int16_t)RAM_WORD(0xF728);   /* v_limitleft2 */
        if ((int16_t)d0 <= l) d0 = (uint16_t)l;  /* bgt -> keep d0 */
        SetScreenX(spx, d0);
        return;
    }

    /* subi.w #16,d0 ; bcc.s SH_MoveCameraRight -> Sonic >= 160px from edge */
    if (a >= 160u) {
        uint16_t d0 = (uint16_t)(a - 160u);      /* after 2nd subi (a - 144 - 16) */
        if (d0 >= 16u) d0 = 16u;                 /* cmpi.w #16,d0 ; blo keeps d0 */
        d0 = (uint16_t)(d0 + (uint16_t)spx);
        int16_t r = (int16_t)RAM_WORD(0xF72A);   /* v_limitright2 */
        if ((int16_t)d0 >= r) d0 = (uint16_t)r;  /* blt -> keep d0 */
        SetScreenX(spx, d0);
        return;
    }

    /* sweet spot: camera does not move this frame */
    RAM_WORD(0xF73A) = 0;                         /* v_scrshiftx */
}

static void ScrollHoriz(void) {
    int16_t old = cam_int(0xF700);
    MoveScreenHoriz();
    int16_t sx = cam_int(0xF700);

    if (((uint16_t)sx & 0x10) != v_fg_xblock) return;  /* no block boundary crossed */
    v_fg_xblock ^= 0x10;
    if (sx - old >= 0)
        RAM_WORD(0xF754) |= (1u << 3);            /* bset #3 (right edge column) */
    else
        RAM_WORD(0xF754) |= (1u << 2);            /* bset #2 (left edge column) */
}

/* ---------------------------------------------------------------------------
   ScrollVertical  (from _inc/ScrollHoriz & ScrollVertical.asm:127-305)
   ------------------------------------------------------------------------- */
static void SetScreenY(int16_t newy) {
    int16_t oldy = cam_int(0xF704);

    /* v_scrshifty = (new-old) << 8 (ror.l #8 of the 16.16 delta) */
    RAM_WORD(0xF73C) = (uint16_t)((uint16_t)((uint16_t)newy - (uint16_t)oldy) << 8);
    RAM_WORD(0xF704) = (uint16_t)newy;            /* v_screenposy (fraction preserved) */

    /* redraw a row of blocks every $10px */
    if (((uint16_t)newy & 0x10) != v_fg_yblock) return;
    v_fg_yblock ^= 0x10;
    if (newy - oldy >= 0)
        RAM_WORD(0xF754) |= (1u << 1);            /* bset #1 (bottom edge row) */
    else
        RAM_WORD(0xF754) |= (1u << 0);            /* bset #0 (top edge row) */
}

/* TopBoundary (incl. vertical wrap). d1 in = new integer Y position. */
static void SVTopBoundary(int16_t d1) {
    int16_t top = (int16_t)RAM_WORD(0xF72C);     /* v_limittop2 */
    if (d1 > top) { SetScreenY(d1); return; }    /* bgt -> set as-is */
    if (d1 > -0x100) {                             /* cmpi.w #-$100,d1 ; bgt -> no wrap */
        SetScreenY(top);                           /* clamp to top boundary */
        return;
    }
    /* wrap vertically */
    d1 = (int16_t)(d1 & 0x7FF);
    obY(&ram[v_player]) = (int16_t)(obY(&ram[v_player]) & 0x7FF);
    RAM_WORD(0xF704)    = RAM_WORD(0xF704) & 0x7FF;
    RAM_WORD(0xF70C)    = RAM_WORD(0xF70C) & 0x3FF;  /* v_bgscreenposy */
    SetScreenY(d1);
}

/* BottomBoundary (incl. vertical wrap). d1 in = new integer Y position. */
static void SVBottomBoundary(int16_t d1) {
    int16_t btm = (int16_t)RAM_WORD(0xF72E);     /* v_limitbtm2 */
    if (d1 < btm) { SetScreenY(d1); return; }    /* blt -> set as-is */
    if ((uint16_t)d1 >= 0x800u) {                  /* subi.w #$7FF+1 ; bcc -> wrap */
        obY(&ram[v_player]) = (int16_t)(obY(&ram[v_player]) & 0x7FF);
        RAM_WORD(0xF704)    = (uint16_t)((uint16_t)RAM_WORD(0xF704) - 0x800u);
        RAM_WORD(0xF70C)    = RAM_WORD(0xF70C) & 0x3FF;  /* v_bgscreenposy */
        d1 = (int16_t)((uint16_t)d1 - 0x800u);
        SetScreenY(d1);
        return;
    }
    SetScreenY(btm);                              /* clamp to bottom boundary */
}

static void SVSweetSpot(int16_t d0dist) {
    int16_t d1 = (int16_t)((uint16_t)d0dist + (uint16_t)cam_int(0xF704));
    if (d0dist >= 0) SVBottomBoundary(d1);
    else             SVTopBoundary(d1);
}

static void SVScroll(int16_t d0dist, uint16_t speed8) {
    /* Cmp +/-N against signed d0dist; move up/down or park in sweet spot */
    if (d0dist > (int16_t)speed8) {              /* bgt -> down */
        /* SV_MoveCameraDown */
        int32_t d1 = ((int32_t)(int16_t)speed8) << 8;
        int32_t newl = cam_getl(0xF704) + d1;   /* add.l old long */
        SVBottomBoundary((int16_t)((uint32_t)newl >> 16));
    } else if (d0dist < -(int16_t)speed8) {      /* blt -> up */
        /* SV_MoveCameraUp */
        int32_t d1 = ((int32_t)(int16_t)(-(int16_t)speed8)) << 8;
        int32_t newl = cam_getl(0xF704) + d1;
        SVTopBoundary((int16_t)((uint32_t)newl >> 16));
    } else {
        SVSweetSpot(d0dist);
    }
}

static void ScrollVertical(void) {
    uint16_t d0;
    int16_t d0s;
    uint8_t *ob = &ram[v_player];

    d0 = (uint16_t)((int)obY(ob) - (int)cam_int(0xF704));   /* move.w obY ; sub.w screenposy */

    if (obStatus(ob) & 0x04)                    /* btst #2 (rolling) */
        d0 = (uint16_t)(d0 - (sonic_height - sonic_roll_height));

    if (obStatus(ob) & 0x02) {                  /* btst #1 (in air) */
        uint16_t lk = v_lookshift;
        uint16_t a = (uint16_t)(d0 + 32u);      /* addi.w #32,d0 */
        uint16_t r = (uint16_t)(a - lk);        /* sub.w lookshift */
        if (a < lk) {                            /* bcs -> below sweet spot */
            d0s = (int16_t)r;
            SVScroll(d0s, 16u);                  /* SV_ScrollFast */
            return;
        }
        d0 = (uint16_t)(r - 64u);                /* subi.w #32*2,d0 */
        if (r >= 64u) {                          /* bcc -> above sweet spot */
            d0s = (int16_t)d0;
            SVScroll(d0s, 16u);                  /* SV_ScrollFast */
            return;
        }
        ((void)0);
        if (RAM_BYTE(0xF75C) != 0)             /* f_bgscrollvert */
            { RAM_BYTE(0xF75C) = 0; d0s = 0; SVSweetSpot(d0s); return; }  /* SV_BottomBoundaryMoving */
        RAM_WORD(0xF73C) = 0;                   /* SV_NoUpdate: scrshifty=0 */
        return;
    }

    /* SV_OnGround */
    {
        uint16_t lk = v_lookshift;
        uint16_t r = (uint16_t)(d0 - lk);       /* sub.w lookshift */
        if (r != 0u) {                           /* bne -> SV_OutsideMid */
            d0s = (int16_t)r;
            if (v_lookshift != 0x0060u)      /* cmpi.w #$60 -> slow */
                SVScroll(d0s, 2u);
            else {
                int16_t inr = obInertia(ob);    /* |inertia| for fast check */
                if (inr < 0) inr = (int16_t)(-inr);
                if ((uint16_t)inr >= 0x800u)
                    SVScroll(d0s, 16u);
                else
                    SVScroll(d0s, 6u);
            }
            return;
        }
        if (RAM_BYTE(0xF75C) != 0)             /* f_bgscrollvert */
            { RAM_BYTE(0xF75C) = 0; d0s = 0; SVSweetSpot(d0s); return; }
        RAM_WORD(0xF73C) = 0;                   /* SV_NoUpdate */
    }
}

/* ---------------------------------------------------------------------------
   DynamicLevelEvents  (_inc/DynamicLevelEvents.asm)
   ------------------------------------------------------------------------- */
static void dle_AddPLC(int plc_id) {
    (void)plc_id;
    /* TODO: the port's PLC runner only handles the main queue (NewPLC/RunPLC
       in main.c); boss art load cues are not decompressed yet. */
}

static void DLE_SBZ2_SetBoundary(void) {
    RAM_WORD(0xF728) = (uint16_t)cam_int(0xF700);       /* v_limitleft2 */
}

static void DLE_Ending(void) { }

static void DLE_FZ(void) {
    switch (v_dle_routine) {          /* DLE_FZ_Index, stepped by +2 */
    case 0x00: {                                 /* DLE_FZ_Main */
        if (cam_int(0xF700) < boss_fz_x - 0x308) { DLE_SBZ2_SetBoundary(); return; }
        v_dle_routine += 2;
        dle_AddPLC(plcid_FZBoss);
        DLE_SBZ2_SetBoundary();
        return;
    }
    case 0x02: {                                 /* DLE_FZ_Boss */
        if (cam_int(0xF700) >= boss_fz_x - 0x150) {
            if (FindFreeObj()) {
                obID(FindFreeObj()) = id_BossFinal;
                v_dle_routine += 2;
                f_lockscreen = 1;
            }
        }
        DLE_SBZ2_SetBoundary();
        return;
    }
    case 0x04:                                   /* DLE_FZ_Arena */
        if (cam_int(0xF700) >= boss_fz_x) v_dle_routine += 2;
        DLE_SBZ2_SetBoundary();
        return;
    default:                                     /* DLE_FZ_Wait / End */
        return;
    }
}

static void DLE_SBZ(void) {
    switch (v_act) {
    case act1: {                                 /* DLE_SBZ1 */
        if (cam_int(0xF700) >= 0x1880) {
            RAM_WORD(0xF726) = (uint16_t)(cam_int(0xF700) >= 0x2000 ? 0x2A0 : 0x620);
        } else {
            RAM_WORD(0xF726) = 0x720;            /* v_limitbtm1 */
        }
        return;
    }
    case act2: {                                 /* DLE_SBZ2 */
        switch (v_dle_routine) {
        case 0x00: {                             /* DLE_SBZ2_Main */
            int16_t sx = cam_int(0xF700);
            RAM_WORD(0xF726) = (uint16_t)(sx >= 0x1800 ? boss_sbz2_y : 0x800);
            if (sx >= 0x1E00) v_dle_routine += 2;
            return;
        }
        case 0x02: {                             /* DLE_SBZ2_Blocks */
            if (cam_int(0xF700) >= boss_sbz2_x - 0x1A0) {
                if (FindFreeObj()) {
                    obID(FindFreeObj()) = id_FalseFloor;
                    v_dle_routine += 2;
                    dle_AddPLC(plcid_EggmanSBZ2);
                }
            }
            return;
        }
        case 0x04:                               /* DLE_SBZ2_Eggman */
            if (cam_int(0xF700) >= boss_sbz2_x - 0xF0) {
                if (FindFreeObj()) {
                    obID(FindFreeObj()) = id_ScrapEggman;
                    v_dle_routine += 2;
                }
                f_lockscreen = 1;
            }
            DLE_SBZ2_SetBoundary();
            return;
        default:                                 /* DLE_SBZ2_End */
            if (cam_int(0xF700) < boss_sbz2_x) DLE_SBZ2_SetBoundary();
            return;
        }
    }
    default:                                     /* DLE_FZ (SBZ act3) */
        DLE_FZ();
        return;
    }
}

static void DLE_SYZ(void) {
    switch (v_act) {
    case act1:
        return;
    case act2: {                                 /* DLE_SYZ2 */
        if (cam_int(0xF700) >= 0x25A0) {
            if (obY(&ram[v_player]) < 0x4D0) { RAM_WORD(0xF726) = 0x420; return; }
            RAM_WORD(0xF726) = 0x520;
        } else {
            RAM_WORD(0xF726) = 0x520;
        }
        return;
    }
    default:                                     /* DLE_SYZ3 */
        switch (v_dle_routine) {
        case 0x00: {                             /* DLE_SYZ3_Main */
            if (cam_int(0xF700) >= boss_syz_x - 0x140) {
                if (FindFreeObj()) {
                    obID(FindFreeObj()) = id_BossBlock;
                    v_dle_routine += 2;
                }
            }
            return;
        }
        case 0x02: {                             /* DLE_SYZ3_Boss */
            if (cam_int(0xF700) >= boss_syz_x) {
                RAM_WORD(0xF726) = boss_syz_y;   /* v_limitbtm1 */
                if (FindFreeObj()) {
                    obID(FindFreeObj()) = id_BossSpringYard;
                    v_dle_routine += 2;
                }
                Sound_Queue(bgm_Boss);
                f_lockscreen = 1;
                dle_AddPLC(plcid_Boss);
            }
            return;
        }
        default:                                 /* DLE_SYZ3_End */
            DLE_SBZ2_SetBoundary();
            return;
        }
    }
}

static void DLE_SLZ(void) {
    switch (v_act) {
    case act1:
    case act2:
        return;                                  /* DLE_SLZ12 */
    default:                                     /* DLE_SLZ3 */
        switch (v_dle_routine) {
        case 0x00:                               /* DLE_SLZ3_Main */
            if (cam_int(0xF700) >= boss_slz_x - 0x190) {
                RAM_WORD(0xF726) = boss_slz_y;   /* v_limitbtm1 */
                v_dle_routine += 2;
            }
            return;
        case 0x02: {                             /* DLE_SLZ3_Boss */
            if (cam_int(0xF700) >= boss_slz_x) {
                if (FindFreeObj()) obID(FindFreeObj()) = id_BossStarLight;
                Sound_Queue(bgm_Boss);
                f_lockscreen = 1;
                v_dle_routine += 2;
                dle_AddPLC(plcid_Boss);
            }
            return;
        }
        default:                                 /* DLE_SLZ3_End */
            DLE_SBZ2_SetBoundary();
            return;
        }
    }
}

static void DLE_MZ(void) {
    switch (v_act) {
    case act1: {                                 /* DLE_MZ1 */
        switch (v_dle_routine) {
        case 0x00: {                             /* DLE_MZ1_0 */
            int16_t sx = cam_int(0xF700);
            int16_t sy = cam_int(0xF704);
            if (sx >= 0x700) RAM_WORD(0xF726) = (uint16_t)(sx >= 0xD00 ? 0x340 : 0x220);
            else              RAM_WORD(0xF726) = 0x1D0;
            if (sx >= 0xD00 && sy >= 0x340) v_dle_routine += 2;
            return;
        }
        case 0x02: {                             /* DLE_MZ1_2 */
            if (cam_int(0xF704) < 0x340) { v_dle_routine -= 2; return; }
            RAM_WORD(0xF72C) = 0;                /* v_limittop2 = 0 */
            if (cam_int(0xF700) < 0xE00) {
                RAM_WORD(0xF72C) = 0x340;
                RAM_WORD(0xF726) = 0x340;        /* v_limitbtm1 */
                if (cam_int(0xF700) < 0xA90) {
                    RAM_WORD(0xF726) = 0x500;
                    if (cam_int(0xF704) >= 0x370) v_dle_routine += 2;
                }
            }
            return;
        }
        case 0x04: {                             /* DLE_MZ1_4 */
            if (cam_int(0xF704) < 0x370) { v_dle_routine -= 2; return; }
            int16_t sx = cam_int(0xF700);
            int16_t sy = cam_int(0xF704);
            if (sy >= 0x500 && sx >= 0xB80) {   /* REV01 adds the $B80 check */
                RAM_WORD(0xF72C) = 0x500;        /* v_limittop2 */
                v_dle_routine += 2;
            }
            return;
        }
        default: {                               /* DLE_MZ1_6 */
            int16_t sx = cam_int(0xF700);
            int16_t sy = cam_int(0xF704);
            if (sx < 0xB80) {                    /* REV01 mid-section block */
                if (RAM_WORD(0xF72C) != 0x340)
                    RAM_WORD(0xF72C) -= 2;       /* move top boundary up 2px */
            } else {
                if (RAM_WORD(0xF72C) != 0x500 && sy >= 0x500)
                    RAM_WORD(0xF72C) = 0x500;
            }
            if (sx >= 0xE70) {
                RAM_WORD(0xF72C) = 0;            /* v_limittop2 cleared */
                RAM_WORD(0xF726) = 0x500;        /* v_limitbtm1 */
                if (sx >= 0x1430) RAM_WORD(0xF726) = 0x210;
            }
            return;
        }
        }
        return;
    }
    case act2: {                                 /* DLE_MZ2 */
        if (cam_int(0xF700) >= 0x1700) RAM_WORD(0xF726) = 0x200;
        else                            RAM_WORD(0xF726) = 0x520;
        return;
    }
    default: {                                   /* DLE_MZ3 */
        switch (v_dle_routine) {
        case 0x00: {                             /* DLE_MZ3_Boss */
            int16_t sx = cam_int(0xF700);
            RAM_WORD(0xF726) = (uint16_t)(sx >= boss_mz_x - 0x10 ? boss_mz_y : 0x720);
            if (sx >= boss_mz_x - 0x10) {
                if (FindFreeObj()) {
                    uint8_t *boss = FindFreeObj();
                    obID(boss) = id_BossMarble;
                    obX(boss) = (int16_t)(boss_mz_x + 0x1F0);
                    obY(boss) = (int16_t)(boss_mz_y + 0x1C);
                }
                Sound_Queue(bgm_Boss);
                f_lockscreen = 1;
                v_dle_routine += 2;
                dle_AddPLC(plcid_Boss);
            }
            return;
        }
        default:                                 /* DLE_MZ3_End */
            DLE_SBZ2_SetBoundary();
            return;
        }
    }
    }
}

static void DLE_LZ(void) {
    switch (v_act) {
    case act1:
    case act2:
        return;                                  /* DLE_LZ12 */
    case act3: {                                 /* DLE_LZ3 */
        /* switch $F pressed -> reveal hidden route */
        if (RAM_BYTE(0xF7EF)) {                  /* f_switch+$F */
            uint8_t *chunk = &ram[v_lvllayout_fg + (layout_row * 2) + 6];
            if (*chunk != 7) {
                *chunk = 7;
                Sound_Queue(sfx_Rumbling);
            }
        }
        if (v_dle_routine != 0) return;   /* boss already loaded */
        if (cam_int(0xF700) >= boss_lz_x - 0x140 /* FixBugs=0 */
            && cam_int(0xF704) < boss_lz_y + 0x540) {
            if (FindFreeObj()) obID(FindFreeObj()) = id_BossLabyrinth;
            Sound_Queue(bgm_Boss);
            f_lockscreen = 1;
            v_dle_routine += 2;
            dle_AddPLC(plcid_Boss);
        }
        return;
    }
    default: {                                   /* DLE_SBZ3 (LZ act4) */
        if (cam_int(0xF700) >= 0xD00 && obY(&ram[v_player]) < 0x18) {
            RAM_BYTE(v_lastlamp) = 0;
            f_restart = 1;
            v_zone_act = id_FZ;
            f_playerctrl = 1;
        }
        return;
    }
    }
}

static void DLE_GHZ(void) {
    switch (v_act) {
    case act1: {                                 /* DLE_GHZ1 (no FixBugs title guard) */
        RAM_WORD(0xF726) = (uint16_t)(cam_int(0xF700) >= 0x1780 ? 0x400 : 0x300);
        return;
    }
    case act2: {                                 /* DLE_GHZ2 */
        int16_t sx = cam_int(0xF700);
        uint16_t btm = 0x300;
        if (sx >= 0xED0)  btm = 0x200;
        if (sx >= 0x1600) btm = 0x400;
        if (sx >= 0x1D60) btm = 0x300;
        RAM_WORD(0xF726) = btm;
        return;
    }
    default: {                                   /* DLE_GHZ3 */
        switch (v_dle_routine) {
        case 0x00: {                             /* DLE_GHZ3_Main */
            int16_t sx = cam_int(0xF700);
            int16_t sy = cam_int(0xF704);
            if (sx < 0x380) { RAM_WORD(0xF726) = 0x300; return; }
            if (sx < 0x960) { RAM_WORD(0xF726) = 0x310; return; }
            if (sy < 0x280) {
                RAM_WORD(0xF726) = 0x400;
                if (sx >= 0x1700) v_dle_routine += 2;
                return;
            }
            /* underground section */
            RAM_WORD(0xF726) = 0x400;
            if (sx < 0x1380) {
                RAM_WORD(0xF726) = 0x4C0;
                RAM_WORD(0xF72E) = 0x4C0;        /* v_limitbtm2 */
            }
            return;
        }
        case 0x02: {                             /* DLE_GHZ3_Boss */
            if (cam_int(0xF700) < 0x960) {
                v_dle_routine -= 2;    /* go back to Main */
                return;
            }
            if (cam_int(0xF700) >= boss_ghz_x) {
                if (FindFreeObj()) {
                    uint8_t *boss = FindFreeObj();
                    obID(boss) = id_BossGreenHill;
                    obX(boss) = (int16_t)(boss_ghz_x + 0x100);
                    obY(boss) = (int16_t)(boss_ghz_y - 0x80);
                }
                Sound_Queue(bgm_Boss);
                f_lockscreen = 1;
                v_dle_routine += 2;
                dle_AddPLC(plcid_Boss);
            }
            return;
        }
        default:                                 /* DLE_GHZ3_End */
            RAM_WORD(0xF728) = (uint16_t)cam_int(0xF700);  /* v_limitleft2 */
            return;
        }
    }
    }
}

void DynamicLevelEvents(void) {
    /* DLE_Index dispatch */
    switch (v_zone) {
    case id_GHZ:  DLE_GHZ(); break;
    case id_LZ:   DLE_LZ();  break;
    case id_MZ:   DLE_MZ();  break;
    case id_SLZ:  DLE_SLZ(); break;
    case id_SYZ:  DLE_SYZ(); break;
    case id_SBZ:  DLE_SBZ(); break;
    default:      DLE_Ending(); break;            /* zonewarning -> DLE_Ending */
    }

    /* Common boundary-follow logic */
    int32_t d1 = 2;
    int16_t b1 = (int16_t)RAM_WORD(0xF726);      /* v_limitbtm1 */
    int16_t b2 = (int16_t)RAM_WORD(0xF72E);      /* v_limitbtm2 */
    int32_t d0 = b1 - b2;
    if (d0 == 0) return;                          /* boundary is where it should be */

    /* bhs (JAE): target boundary Y is numerically >= current (incl. both above 0x800) */
    if ((uint16_t)b1 >= (uint16_t)b2) {
        /* move_boundary_down */
        int16_t sy = cam_int(0xF704);
        int16_t chk = (int16_t)(sy + 8);
        if (!((uint16_t)chk < (uint16_t)b2)) {    /* blo -> down_2px */
            if (obStatus(&ram[v_player]) & 0x02)  /* btst #1, Sonic in air */
                d1 = 8;                           /* boundary moves 8px */
        }
        RAM_WORD(0xF72E) = (uint16_t)((uint16_t)b2 + (uint16_t)d1);  /* v_limitbtm2 */
        RAM_BYTE(0xF75C) = 1;                     /* f_bgscrollvert */
        return;
    }

    /* moving boundary up */
    d1 = -2;
    {
        int16_t sy = cam_int(0xF704);
        if (!((uint16_t)sy <= (uint16_t)b1))      /* bls -> camera_below: match to camera */
            RAM_WORD(0xF72E) = (uint16_t)sy & 0xFFFEu;
        RAM_WORD(0xF72E) = (uint16_t)((uint16_t)RAM_WORD(0xF72E) + (uint16_t)d1);
    }
    RAM_BYTE(0xF75C) = 1;                         /* f_bgscrollvert */
}

/* ---------------------------------------------------------------------------
   Background-scroll helpers (BGScroll_*) from _inc/DeformLayers (REV01).asm
   ------------------------------------------------------------------------- */
/* Block 1/2/3: update one bg x-position by d4 (16.16), flag redraw on 16px
   boundary crossing. d6 selects which bits to set. */
static void BGScroll_Block(uint16_t xaddr, uint16_t flagsaddr, uint16_t xblockaddr, int32_t d4, uint8_t d6) {
    int32_t d2 = cam_getl(xaddr);
    int32_t d0 = d2 + d4;
    cam_setl(xaddr, d0);

    uint16_t d1 = (uint16_t)((uint32_t)d0 >> 16) & 0x10u;
    uint8_t xb = RAM_BYTE(xblockaddr);
    if ((uint8_t)(d1 ^ xb) != 0) return;          /* eor.b ; bne -> no boundary crossed */
    RAM_BYTE(xblockaddr) = (uint8_t)(xb ^ 0x10);

    if (d0 - d2 < 0) {                            /* bpl -> scrollRight */
        RAM_WORD(flagsaddr) |= (uint16_t)(1u << d6);
    } else {
        RAM_WORD(flagsaddr) |= (uint16_t)(1u << ((uint16_t)d6 + 1u));
    }
}

/* BGScroll_XY: update bg x and y (both relative), plus BGScroll_YRelative. */
static void BGScroll_XY(int32_t d4, int32_t d5) {
    int32_t d2 = cam_getl(0xF708);                /* v_bgscreenposx */
    int32_t d0 = d2 + d4;
    cam_setl(0xF708, d0);

    uint16_t d1 = (uint16_t)((uint32_t)d0 >> 16) & 0x10u;
    uint8_t xb = RAM_BYTE(0xF74C);                /* v_bg1_xblock */
    if ((uint8_t)(d1 ^ xb) != 0) goto do_y;       /* bne -> no x redraw */
    RAM_BYTE(0xF74C) = (uint8_t)(xb ^ 0x10);
    if (d0 - d2 < 0)                              /* sub.l ; bpl */
        RAM_WORD(0xF756) |= (1u << 2);            /* bset #2 */
    else
        RAM_WORD(0xF756) |= (1u << 3);            /* bset #3 */

do_y:
    /* BGScroll_YRelative */
    {
        int32_t d3 = cam_getl(0xF70C);            /* v_bgscreenposy */
        int32_t d0b = d3 + d5;
        cam_setl(0xF70C, d0b);
        uint16_t d1b = (uint16_t)((uint32_t)d0b >> 16) & 0x10u;
        uint8_t yb = RAM_BYTE(0xF74D);            /* v_bg1_yblock */
        if ((uint8_t)(d1b ^ yb) != 0) return;     /* bne -> return */
        RAM_BYTE(0xF74D) = (uint8_t)(yb ^ 0x10);
        if (d0b - d3 < 0)
            RAM_WORD(0xF756) |= (1u << 0);        /* bset #0 */
        else
            RAM_WORD(0xF756) |= (1u << 1);        /* bset #1 */
    }
}

/* BGScroll_Y: vertical relative scroll; sets bits #4/#5. */
static void BGScroll_Y(int32_t d5) {
    int32_t d3 = cam_getl(0xF70C);
    int32_t d0 = d3 + d5;
    cam_setl(0xF70C, d0);

    uint16_t d1 = (uint16_t)((uint32_t)d0 >> 16) & 0x10u;
    uint8_t yb = RAM_BYTE(0xF74D);                /* v_bg1_yblock */
    if ((uint8_t)(d1 ^ yb) != 0) return;
    RAM_BYTE(0xF74D) = (uint8_t)(yb ^ 0x10);
    if (d0 - d3 < 0)
        RAM_WORD(0xF756) |= (1u << 4);
    else
        RAM_WORD(0xF756) |= (1u << 5);
}

/* BGScroll_YAbsolute: set bg y to an absolute integer, flag redraw. */
static void BGScroll_YAbsolute(int16_t d0int) {
    int16_t d3 = cam_int(0xF70C);                 /* save old bg y int */
    RAM_WORD(0xF70C) = (uint16_t)d0int;            /* frac untouched */

    uint16_t d1 = (uint16_t)d0int & 0x10u;
    uint8_t yb = RAM_BYTE(0xF74D);                /* v_bg1_yblock */
    if ((uint8_t)(d1 ^ yb) != 0) return;
    RAM_BYTE(0xF74D) = (uint8_t)(yb ^ 0x10);
    if ((int32_t)(int16_t)d0int - d3 < 0)         /* sub.w d3,d0 ; bpl */
        RAM_WORD(0xF756) |= (1u << 0);
    else
        RAM_WORD(0xF756) |= (1u << 1);
}

/* BGScroll_X: copy the shared buffer's values into the hscroll table for all
   256 rows (skipping the bg-y-nybble aligned offset). d2-nybble rotates. */
static void BGScroll_X(int16_t d2, uint16_t buf_byte_off) {
    uint16_t skip = ((uint16_t)d2 & 0x0Fu) >> 1;  /* (nybble*2 bytes) / 4-byte entry */
    int16_t fg = (int16_t)(-cam_int(0xF700));     /* fg x = -screenposx */
    uint16_t a1 = v_hscrolltablebuffer;

    for (int g = 0; g < 16; g++) {
        int16_t bg = (int16_t)DEFORM_BUFW(buf_byte_off / 2 + g);
        int first = (g == 0) ? (int)skip : 0;
        for (int r = first; r < 16; r++) {
            RAM_WORD(a1)     = (uint16_t)fg;      /* high word (fg) */
            RAM_WORD(a1 + 2) = (uint16_t)bg;      /* low word (bg) */
            a1 += 4;
        }
    }
}

/* ---------------------------------------------------------------------------
   Per-zone deformation (Deform_GHZ/LZ/MZ/SLZ/SYZ/SBZ, REV01)
   ------------------------------------------------------------------------- */
static void hscroll_row(uint16_t row, int16_t fg, int16_t bg) {
    RAM_WORD(v_hscrolltablebuffer + row * 4)     = (uint16_t)fg;
    RAM_WORD(v_hscrolltablebuffer + row * 4 + 2) = (uint16_t)bg;
}

static void Deform_GHZ(void) {
    int16_t sx = cam_int(0xF700);
    int16_t sh = (int16_t)v_scrshiftx;

    /* block 3 - distant mountains: sh * $60 */
    {
        int32_t d4 = (int32_t)sh * 96;
        BGScroll_Block(0xF718, 0xF75A, 0xF750, d4, 0);
    }
    /* block 2 - hills & waterfalls: sh * $80 */
    {
        int32_t d4 = (int32_t)sh * 128;
        BGScroll_Block(0xF710, 0xF758, 0xF74E, d4, 0);
    }

    /* Y position */
    {
        int32_t d0 = ((uint16_t)cam_int(0xF704) & 0x7FFu) >> 5;   /* &$7FF, lsr #5 */
        d0 = 0x20 - d0;                            /* neg.w + addi.w #$20 (signed) */
        if (d0 < 0) d0 = 0;                        /* bpl -> keep, else 0 */
        RAM_WORD(0xF618) = (uint16_t)d0;           /* v_bgscrposy_vdp */

        /* FG X (forced to 0 on the title screen) */
        int16_t fgx = (v_gamemode == GM_Title) ? 0 : sx;

        /* autoscroll clouds: += $10000 / $C000 / $8000 per frame (REV01) */
        cam_setl(0xA800 + 0, cam_getl(0xA800 + 0) + 0x10000);
        cam_setl(0xA800 + 4, cam_getl(0xA800 + 4) + 0xC000);
        cam_setl(0xA800 + 8, cam_getl(0xA800 + 8) + 0x8000);

        uint16_t row = 0;

        /* upper clouds: (32 - d4) rows */
        uint16_t n_c1 = ((uint32_t)d0 < 32u) ? (uint32_t)(32 - (uint16_t)d0) : 0;
        if (n_c1) {                                /* sub.w d4,d1 ; bcs skip */
            int16_t bg = (int16_t)(-(cam_getl(0xA800) >> 16) - cam_int(0xF718));
            for (int i = 0; i < n_c1; i++) hscroll_row(row++, -fgx, bg);
        }
        /* middle clouds: 16 rows */
        {
            int16_t bg = (int16_t)(-(cam_getl(0xA800 + 4) >> 16) - cam_int(0xF718));
            for (int i = 0; i < 16; i++) hscroll_row(row++, -fgx, bg);
        }
        /* lower clouds: 16 rows */
        {
            int16_t bg = (int16_t)(-(cam_getl(0xA800 + 8) >> 16) - cam_int(0xF718));
            for (int i = 0; i < 16; i++) hscroll_row(row++, -fgx, bg);
        }
        /* mountains: 48 rows of -bg3x */
        for (int i = 0; i < 48; i++) hscroll_row(row++, -fgx, (int16_t)(-cam_int(0xF718)));
        /* hills & waterfalls: 40 rows of -bg2x */
        for (int i = 0; i < 40; i++) hscroll_row(row++, -fgx, (int16_t)(-cam_int(0xF710)));

        /* water region: 72 + d4 rows */
        int16_t bg2x = cam_int(0xF710);
        int32_t d3 = (int32_t)(uint16_t)bg2x;      /* moveq#0 ; move.w bg2x */
        /* d2 = ((sx - bg2x) << 8) / $68, then << 8 (16.16) */
        int32_t d2 = (int32_t)(int16_t)((uint16_t)sx - (uint16_t)bg2x);
        d2 <<= 8;
        d2 = (int32_t)(int16_t)(d2 / 0x68);        /* divs.w #$68 (16-bit quotient) */
        d2 <<= 8;
        for (int i = 0; i < (int)(72 + d0); i++) {
            int16_t bg = (int16_t)(-(int16_t)(uint16_t)(uint32_t)d3 & 0xFFFF);  /* move.w d3,d0 ; neg.w */
            hscroll_row(row++, -fgx, bg);
            d3 = rot16(d3);                        /* swap d3 */
d3 += d2;                               /* add.l d2,d3 */
            d3 = rot16(d3);                         /* swap d3 */
        }
    }
}

static void Deform_LZ(void) {
    int32_t d4 = (int32_t)(int16_t)v_scrshiftx << 7;
    int32_t d5 = (int32_t)(int16_t)v_scrshifty << 7;
    BGScroll_XY(d4, d5);
    RAM_WORD(0xF618) = (uint16_t)cam_int(0xF70C); /* v_bgscrposy_vdp */

    /* REV01 water ripples */
    uint8_t d2 = (uint8_t)v_lz_deform;
    uint8_t d3 = d2;
    v_lz_deform = (uint16_t)(v_lz_deform + 0x80);

    uint16_t d2i = (uint16_t)(d2 + (uint16_t)cam_int(0xF70C)) & 0xFFu;  /* +bgy, &$FF */
    uint16_t d3i = (uint16_t)(d3 + (uint16_t)cam_int(0xF704)) & 0xFFu;  /* +screenposy, &$FF */

    int16_t bgx = (int16_t)cam_int(0xF708);       /* v_bgscreenposx */
    uint16_t d4w = RAM_WORD(0xF646);              /* v_waterpos1 */
    int16_t sy = cam_int(0xF704);

    uint16_t row = 0;
    int d1 = 224 - 1;                             /* dbf 224 rows */
    int16_t fg = (int16_t)(-cam_int(0xF700));

    /* normal rows until water position */
    for (;;) {
        if ((int16_t)(uint16_t)((uint16_t)sy + row) >= (int16_t)d4w) break;  /* bge -> underwater */
        RAM_WORD(v_hscrolltablebuffer + row * 4)     = (uint16_t)fg;
        RAM_WORD(v_hscrolltablebuffer + row * 4 + 2) = (uint16_t)(-bgx);
        row++;
        d2i++; d3i++;
        if (row > (uint16_t)d1) return;
    }

    /* underwater ripples */
    for (;;) {
        int16_t rfg = (int16_t)(fg + (int16_t)(int8_t)Lz_Scroll_Data[d3i]);
        int16_t rbg = (int16_t)((int16_t)(-bgx) + (int16_t)(int8_t)Drown_WobbleData[d2i]);
        RAM_WORD(v_hscrolltablebuffer + row * 4)     = (uint16_t)rfg;
        RAM_WORD(v_hscrolltablebuffer + row * 4 + 2) = (uint16_t)rbg;
        row++;
        d2i++; d3i++;
        if (row > (uint16_t)d1) return;
    }
}

static void Deform_MZ(void) {
    int16_t sh = (int16_t)v_scrshiftx;

    /* block 1 - dungeon interior: sh * $C0 */
    {
        int32_t d4 = (int32_t)sh * 192;
        BGScroll_Block(0xF708, 0xF756, 0xF74C, d4, 2);
    }
    /* block 3 - mountains: sh * $40 */
    BGScroll_Block(0xF718, 0xF75A, 0xF750, (int32_t)sh * 64, 6);
    /* block 2 - bushes: sh * $80 */
    BGScroll_Block(0xF710, 0xF758, 0xF74E, (int32_t)sh * 128, 4);

    /* Y position */
    {
        int32_t d0 = 512;
        int16_t d1 = cam_int(0xF704);
        if ((uint16_t)d1 >= 456u) {                /* subi.w #456 ; bcs -> no scroll */
            int16_t dd = (int16_t)(d1 - 456);      /* d1 in signed */
            int32_t d2 = dd;                       /* d2 = d1 */
            dd = (int16_t)(dd + dd);               /* d1 += d1 */
            dd = (int16_t)(dd + (int16_t)d2);      /* d1 += d2  -> 3*d1 */
            d0 += (dd >> 2);                       /* asr.w #2 (arith) */
        }
        RAM_WORD(0xF714) = (uint16_t)d0;           /* v_bg2screenposy */
        RAM_WORD(0xF71C) = (uint16_t)d0;           /* v_bg3screenposy */
    }
    BGScroll_YAbsolute((int16_t)cam_int(0xF70C));  /* uses v_bgscreenposy */
    RAM_WORD(0xF618) = (uint16_t)cam_int(0xF70C);

    /* merge redraw flags: bg3 |= bg1|bg2 ; clear bg1/bg2 low byte */
    v_bg3_scroll_flags |= (uint16_t)(v_bg1_scroll_flags | v_bg2_scroll_flags);
    v_bg1_scroll_flags = 0;
    v_bg2_scroll_flags = 0;

    /* background scroll buffer */
    {
        int16_t d2 = (int16_t)(-cam_int(0xF700)); /* d2 = -screenposx */
        int32_t d0 = (int32_t)(d2 >> 2) - d2;      /* asr.w #2 ; sub.w d2 (word) */
        d0 <<= 3;                                  /* asl.l #3 */
        d0 = (int32_t)(int16_t)(d0 / 5);           /* divs.w #5 */
        d0 <<= 4; d0 <<= 8;                        /* asl.l #4 ; asl.l #8 */
        /* clouds: 5 words */
        int32_t d3 = (int32_t)(uint16_t)(int16_t)((int16_t)d2 >> 1);  /* moveq#0 + move.w d2 + asr.w #1 */
        for (int i = 0; i < 5; i++) {
            DEFORM_BUFW(i) = (uint16_t)(uint32_t)d3;
            d3 = rot16(d3); d3 += d0; d3 = rot16(d3);
        }
        int16_t bg3x = cam_int(0xF718), bg2x = cam_int(0xF710), bg1x = cam_int(0xF708);
        for (int i = 0; i < 2;  i++) DEFORM_BUFW(5 + i)  = (uint16_t)(-bg3x);   /* mountains */
        for (int i = 0; i < 9;  i++) DEFORM_BUFW(7 + i)  = (uint16_t)(-bg2x);   /* bushes */
        for (int i = 0; i < 16; i++) DEFORM_BUFW(16 + i) = (uint16_t)(-bg1x);   /* interior */

        /* BGScroll_X entry, offset by bg y */
        int16_t d2save = cam_int(0xF70C);          /* original bgscreenposy int */
        int32_t offd0 = ((int32_t)cam_int(0xF70C) - 512);
        if (offd0 >= 0x100) offd0 = 0x100;
        offd0 &= 0x1F0; offd0 >>= 3;
        BGScroll_X(d2save, (uint16_t)offd0);
    }
}

static void Deform_SLZ(void) {
    BGScroll_Y((int32_t)(int16_t)v_scrshifty << 7);
    RAM_WORD(0xF618) = (uint16_t)cam_int(0xF70C);

    int16_t d2 = (int16_t)(-cam_int(0xF700));
    int32_t d0 = (int32_t)(d2 >> 3) - d2;          /* asr.w #3 ; sub.w */
    d0 <<= 4;                                      /* asl.l #4 */
    d0 = (int32_t)(int16_t)(d0 / 0x1C);            /* divs.w #$1C */
    d0 <<= 4; d0 <<= 8;

    int32_t d3 = (int32_t)(uint16_t)d2;            /* stars: moveq#0 + move.w d2 */
    for (int i = 0; i < 28; i++) {                 /* starLoop */
        DEFORM_BUFW(i) = (uint16_t)(uint32_t)d3;
        d3 = rot16(d3); d3 += d0; d3 = rot16(d3);
    }
    /* buildings */
    for (int i = 0; i < 5; i++)                    /* distant: d2/8 + (d2/8)/2 */
        DEFORM_BUFW(28 + i) = (uint16_t)(int16_t)((d2 >> 3) + ((d2 >> 3) >> 1));
    for (int i = 0; i < 5; i++)                    /* closer: d2/4 */
        DEFORM_BUFW(33 + i) = (uint16_t)(d2 >> 2);
    for (int i = 0; i < 30; i++)                   /* bottom: d2/2 */
        DEFORM_BUFW(38 + i) = (uint16_t)(d2 >> 1);

    int16_t d2save = cam_int(0xF70C);              /* original bg y */
    int32_t offd0 = ((int32_t)cam_int(0xF70C) - 0xC0);
    offd0 &= 0x3F0; offd0 >>= 3;
    BGScroll_X(d2save, (uint16_t)offd0);
}

static void Deform_SYZ(void) {
    int32_t d5 = (int32_t)(int16_t)v_scrshifty;
    d5 <<= 4; { int32_t dd = d5; d5 <<= 1; d5 += dd; }   /* * $30 */
    BGScroll_Y(d5);
    RAM_WORD(0xF618) = (uint16_t)cam_int(0xF70C);

    int16_t d2 = (int16_t)(-cam_int(0xF700));
    int32_t d0 = (int32_t)(d2 >> 3) - d2;
    d0 <<= 3;
    d0 = (int32_t)(int16_t)(d0 / 8);
    d0 <<= 4; d0 <<= 8;

    int32_t d3 = (int32_t)(uint16_t)(int16_t)(d2 >> 1);   /* clouds: d2/2 */
    for (int i = 0; i < 8; i++) {
        DEFORM_BUFW(i) = (uint16_t)(uint32_t)d3;
        d3 = rot16(d3); d3 += d0; d3 = rot16(d3);
    }
    for (int i = 0; i < 5; i++) DEFORM_BUFW(8 + i)  = (uint16_t)(d2 >> 3);   /* mountains */
    for (int i = 0; i < 6; i++) DEFORM_BUFW(13 + i) = (uint16_t)(d2 >> 2);   /* buildings */

    /* bushes: d2/2 + accumulated */
    d0 = (int32_t)((d2) - (d2 >> 1));              /* d2 - d2/2 */
    d0 <<= 4;
    d0 = (int32_t)(int16_t)(d0 / 0x0E);
    d0 <<= 4; d0 <<= 8;
    d3 = (int32_t)(uint16_t)(int16_t)(d2 >> 1);
    for (int i = 0; i < 14; i++) {
        DEFORM_BUFW(19 + i) = (uint16_t)(uint32_t)d3;
        d3 = rot16(d3); d3 += d0; d3 = rot16(d3);
    }

    int16_t d2save = cam_int(0xF70C);
    int32_t offd0 = (int32_t)cam_int(0xF70C) & 0x1F0;
    offd0 >>= 3;
    BGScroll_X(d2save, (uint16_t)offd0);
}

static void Deform_SBZ(void) {
    if (v_act != act1) {                  /* Deform_SBZ2 */
        int32_t d4 = (int32_t)(int16_t)v_scrshiftx << 6;
        int32_t d5 = (int32_t)(int16_t)v_scrshifty << 5;
        BGScroll_XY(d4, d5);
        RAM_WORD(0xF618) = (uint16_t)cam_int(0xF70C);
        int16_t fg = (int16_t)(-cam_int(0xF700));
        int16_t bg = (int16_t)(-cam_int(0xF708));
        for (int r = 0; r < 224; r++) hscroll_row(r, fg, bg);
        return;
    }

    /* act 1 */
    int16_t sh = (int16_t)v_scrshiftx;
    BGScroll_Block(0xF708, 0xF756, 0xF74C, (int32_t)sh * 128, 2);   /* block 1 */
    BGScroll_Block(0xF718, 0xF75A, 0xF750, (int32_t)sh * 64,  6);   /* block 3 */
    BGScroll_Block(0xF710, 0xF758, 0xF74E, (int32_t)sh * 96,  4);   /* block 2 */

    /* vertical scroll (relative) */
    {
        int32_t d5 = (int32_t)(int16_t)v_scrshifty << 5;
        /* BGScroll_YRelative with d4 = 0 */
        int32_t d3 = cam_getl(0xF70C);
        int32_t d0b = d3 + d5;
        cam_setl(0xF70C, d0b);
        uint16_t d1 = (uint16_t)((uint32_t)d0b >> 16) & 0x10u;
        uint8_t yb = RAM_BYTE(0xF74D);
        if ((uint8_t)(d1 ^ yb) != 0) {
        } else {
            RAM_BYTE(0xF74D) = (uint8_t)(yb ^ 0x10);
            if (d0b - d3 < 0) RAM_WORD(0xF756) |= (1u << 0);
            else               RAM_WORD(0xF756) |= (1u << 1);
        }
    }
    {
        int16_t d0b = cam_int(0xF70C);
        RAM_WORD(0xF714) = (uint16_t)d0b;           /* bg2y = bgy */
        RAM_WORD(0xF71C) = (uint16_t)d0b;           /* bg3y = bgy */
        RAM_WORD(0xF618) = (uint16_t)d0b;           /* bgscrposy_vdp */
    }
    v_bg2_scroll_flags |= (uint16_t)(v_bg1_scroll_flags | v_bg3_scroll_flags);
    v_bg1_scroll_flags = 0;
    v_bg3_scroll_flags = 0;

    {
        int16_t d2 = (int16_t)(-cam_int(0xF700));
        d2 >>= 2;                                    /* asr.w #2 */
        int16_t d2q = d2;
        int32_t d0 = (int32_t)(d2 >> 1) - d2;        /* d2/2 - d2 */
        d0 <<= 3;
        d0 = (int32_t)(int16_t)(d0 / 4);
        d0 <<= 4; d0 <<= 8;
        int32_t d3 = (int32_t)(uint16_t)d2;         /* clouds */
        for (int i = 0; i < 4; i++) {
            DEFORM_BUFW(i) = (uint16_t)(uint32_t)d3;
            d3 = rot16(d3); d3 += d0; d3 = rot16(d3);
        }
        for (int i = 0; i < 10; i++) DEFORM_BUFW(4 + i)  = (uint16_t)(-cam_int(0xF718));  /* brown buildings */
        for (int i = 0; i < 7;  i++) DEFORM_BUFW(14 + i) = (uint16_t)(-cam_int(0xF710));  /* black buildings */
        for (int i = 0; i < 11; i++) DEFORM_BUFW(21 + i) = (uint16_t)(-cam_int(0xF708));  /* lower buildings */
        int16_t d2save = cam_int(0xF70C);
        int32_t offd0 = (int32_t)cam_int(0xF70C) & 0x1F0;
        offd0 >>= 3;
        BGScroll_X(d2save, (uint16_t)offd0);
        (void)d2q;
    }
}

/* ---------------------------------------------------------------------------
   DeformLayers entry  (_inc/DeformLayers (REV01).asm:6-30)
   ------------------------------------------------------------------------- */
void DeformLayers(void) {
    if (f_nobgscroll) return;                       /* tst.b (f_nobgscroll).w ; beq */

    v_fg_scroll_flags  = 0;                         /* clr.w all four redraw flags */
    v_bg1_scroll_flags = 0;
    v_bg2_scroll_flags = 0;
    v_bg3_scroll_flags = 0;

    ScrollHoriz();                                  /* camera + fg redraw flags */
    ScrollVertical();
    DynamicLevelEvents();                           /* boundaries / bosses */

    /* send integer camera Y positions to the VDP shadow registers */
    v_scrposy_vdp   = (int16_t)cam_int(0xF704);     /* move.w (v_screenposy).w,(v_scrposy_vdp).w */
    v_bgscrposy_vdp = (int16_t)cam_int(0xF70C);     /* move.w (v_bgscreenposy).w,(v_bgscrposy_vdp).w */

    /* Deform_Index dispatch */
    switch (v_zone) {
    case id_GHZ:  Deform_GHZ(); break;
    case id_LZ:   Deform_LZ();  break;
    case id_MZ:   Deform_MZ();  break;
    case id_SLZ:  Deform_SLZ(); break;
    case id_SYZ:  Deform_SYZ(); break;
    case id_SBZ:  Deform_SBZ(); break;
    default:      Deform_GHZ(); break;               /* zonewarning -> Deform_GHZ */
    }
}

/* ---------------------------------------------------------------------------
   Ripple tables.
   ------------------------------------------------------------------------- */
/* Lz_Scroll_Data: 256 bytes of FG water ripple offsets. */
static const int8_t Lz_Scroll_Data[256] = {
    /* 12 lines shifted right */
    1,1,2,2,3,3,3,3,2,2,1,1,
    /* 116 normal lines */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,
    /* 12 lines shifted left */
    -1,-1,-2,-2,-3,-3,-3,-3,-2,-2,-1,-1,
    /* 20 normal lines */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,
    /* 12 shifted right */
    1,1,2,2,3,3,3,3,2,2,1,1,
    /* 84 normal lines */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,
};

/* Drown_WobbleData: 256 bytes (REV01 repeats the 128-byte pattern twice). */
static const int8_t Drown_WobbleData[256] = {
    0,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2, 2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2, 2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,
    0,-1,-1,-1,-1,-1,-2,-2,-2,-2,-2,-3,-3,-3,-3,-3, -3,-3,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,
    -4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-3, -3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,
    /* second copy */
    0,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2, 2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2, 2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,
    0,-1,-1,-1,-1,-1,-2,-2,-2,-2,-2,-3,-3,-3,-3,-3, -3,-3,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,
    -4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-4,-3, -3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,
};
