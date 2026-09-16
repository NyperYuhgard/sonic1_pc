#include "objects.h"
#include "ram.h"
#include "constants.h"
#include "data.h"
#include "sound.h"
#include "collision.h"
#include "plc.h"
#include "debugmode.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Object dispatch table - maps object ID to routine.
   Index = object ID, value = function to execute. */
static ObjFunc obj_dispatch[256];

/* Simple sprite queue for porting BuildSprites gradually */
#define SIMPLE_SPRITE_QUEUE 128
static uint8_t *sprite_queue_data[SIMPLE_SPRITE_QUEUE];
uint8_t **sprite_queue = sprite_queue_data;
int sprite_queue_count = 0;

/* Forward declarations */
static void TitleSonic_Main(void *obj);
static void PSBTM_Main(void *obj);
static void CreditsText_Main(void *obj);
static void SonicPlayer_Main(void *obj);
static void HUD_Main(void *obj);
static void TitleCard_Main(void *obj);
static void GameOverCard_Main(void *obj);
static void Ring_Main(void *obj);
static void RingLoss_Main(void *obj);
static void Signpost_Main(void *obj);
static void GotThroughCard_Main(void *obj);
static void Crabmeat_Main(void *obj);
static void MotoBug_Main(void *obj);
static void BuzzBomber_Main(void *obj);
static void Missile_Main(void *obj);
static void Bridge_Main(void *obj);
static void PurpleRock_Main(void *obj);
static void EdgeWalls_Main(void *obj);
static void ExplosionItem_Main(void *obj);
static void Explosion_Main(void *obj);
static void Animals_Main(void *obj);
static void Points_Main(void *obj);
static void Monitor_Main(void *obj);
static void PowerUp_Main(void *obj);
static void Spikes_ObjectMain(void *obj);
static void Springs_ObjectMain(void *obj);
void AnimateSprite(void *obj, const uint8_t *anim_script);

/* Stub: objects not yet ported do nothing (matches NullObject -> DeleteObject) */
static void NullObject_Main(void *obj) {
    DeleteObject(obj);
}

void Objects_Init(void) {
    /* Default: every unmapped ID self-deletes (matches ASM NullObject) */
    for (int i = 1; i < 256; i++) {
        obj_dispatch[i] = NullObject_Main;
    }

    /* Register title screen objects */
    obj_dispatch[id_TitleSonic]   = TitleSonic_Main;
    obj_dispatch[id_PSBTM]        = PSBTM_Main;
    obj_dispatch[id_CreditsText]  = CreditsText_Main;

    /* Register level objects */
    obj_dispatch[id_SonicPlayer]  = SonicPlayer_Main;
    obj_dispatch[id_HUD]          = HUD_Main;
    obj_dispatch[id_TitleCard]    = TitleCard_Main;
    obj_dispatch[id_GameOverCard] = GameOverCard_Main;
    obj_dispatch[id_Rings]        = Ring_Main;
    obj_dispatch[id_RingLoss]     = RingLoss_Main;
    obj_dispatch[id_Signpost]     = Signpost_Main;
    obj_dispatch[id_GotThroughCard] = GotThroughCard_Main;
    obj_dispatch[id_Crabmeat]     = Crabmeat_Main;
    obj_dispatch[id_MotoBug]      = MotoBug_Main;
    obj_dispatch[id_BuzzBomber]   = BuzzBomber_Main;
    obj_dispatch[id_Missile]      = Missile_Main;
    obj_dispatch[id_Bridge]       = Bridge_Main;
    obj_dispatch[id_PurpleRock]   = PurpleRock_Main;
    obj_dispatch[id_EdgeWalls]    = EdgeWalls_Main;
    obj_dispatch[id_Spikes] = Spikes_ObjectMain;
    obj_dispatch[id_Springs] = Springs_ObjectMain;

    /* Register explosion/gray puff, fiery explosion, animals, and points */
    obj_dispatch[id_ExplosionItem] = ExplosionItem_Main;
    obj_dispatch[id_Explosion]     = Explosion_Main;
    obj_dispatch[id_Animals]       = Animals_Main;
    obj_dispatch[id_Points]        = Points_Main;
    /* Register monitors and power-ups */
    obj_dispatch[id_Monitor] = Monitor_Main;
    obj_dispatch[id_PowerUp] = PowerUp_Main;


    /* Clear all object RAM */
    memset(ObjRAM, 0, NUM_OBJECTS * OBJECT_SIZE);
}

void ExecuteObjects(void) {
    uint8_t *obj = ObjRAM;

    sprite_queue_count = 0;

    for (int i = 0; i < NUM_OBJECTS; i++) {
        uint8_t id = obj[i * OBJECT_SIZE];
        if (id != 0 && obj_dispatch[id]) {
            obj_dispatch[id](&obj[i * OBJECT_SIZE]);
        }
    }
}

void DisplaySprite(void *obj) {
    if (sprite_queue_count < SIMPLE_SPRITE_QUEUE) {
        sprite_queue[sprite_queue_count++] = (uint8_t *)obj;
    }
}

void *FindFreeObj(void) {
    /* ASM: lea (v_lvlobjspace).w,a1 ; move.w #(v_lvlobjend-v_lvlobjspace)/object_size-1,d0 */
    uint8_t *base = RAM_ADDR(v_lvlobjspace);
    int count = (int)((v_lvlobjend - v_lvlobjspace) / OBJECT_SIZE);
    for (int i = 0; i < count; i++) {
        if (base[i * OBJECT_SIZE] == 0) {
            return &base[i * OBJECT_SIZE];
        }
    }
    return NULL;
}

void DeleteObject(void *obj) {
    memset(obj, 0, OBJECT_SIZE);
}

/* ===========================================================================
   CalcSine — port of _incObj/sub CalcSine.asm (REV01, FixBugs=0)
   Input: angle in [0,255]. Output columns (ASM d0/sin, d1/cos) are written
   to *s0 and *s1. Sine_Data is the 320-word table (0xB400 words), where
   words 0x100-0x13F repeat words 0x00-0x3F. The ASM addresses the table
   with BYTE offsets: it doubles the angle (add.w d0,d0) so the sine read is
   the word at byte offset angle*2 = C word index `angle`, and the cosine is
   0x80 bytes later = word index `angle+64` (the overflow repeat at the end
   covers the cos reads for angles 0xC0-0xFF).
   =========================================================================== */

/* Sine_Data: 320 words = 256 unique + 64-word overflow repeat
   (words 0x100-0x13F copy words 0x00-0x3F, so the cosine pick
   index+64 wraps correctly for angles 0xC0-0xFF). Transcribed from
   the disassembly; note the real table is not perfectly symmetric
   around the peak (e.g. word 0x6D = 0x73 but its mirror 0xED
   = -0x75). The ASM addresses this table with BYTE offsets
   (angle*2), so the C word index is just the angle itself; cosine
   is the word 0x80 bytes later = index+64. */
static const int16_t Sine_Data[320] = {
        0,    6,  0xC, 0x12, 0x19, 0x1F, 0x25, 0x2B,
     0x31, 0x38, 0x3E, 0x44, 0x4A, 0x50, 0x56, 0x5C,
     0x61, 0x67, 0x6D, 0x73, 0x78, 0x7E, 0x83, 0x88,
     0x8E, 0x93, 0x98, 0x9D, 0xA2, 0xA7, 0xAB, 0xB0,
     0xB5, 0xB9, 0xBD, 0xC1, 0xC5, 0xC9, 0xCD, 0xD1,
     0xD4, 0xD8, 0xDB, 0xDE, 0xE1, 0xE4, 0xE7, 0xEA,
     0xEC, 0xEE, 0xF1, 0xF3, 0xF4, 0xF6, 0xF8, 0xF9,
     0xFB, 0xFC, 0xFD, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF,
     0x100,0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFD, 0xFC,
     0xFB, 0xF9, 0xF8, 0xF6, 0xF4, 0xF3, 0xF1, 0xEE,
     0xEC, 0xEA, 0xE7, 0xE4, 0xE1, 0xDE, 0xDB, 0xD8,
     0xD4, 0xD1, 0xCD, 0xC9, 0xC5, 0xC1, 0xBD, 0xB9,
     0xB5, 0xB0, 0xAB, 0xA7, 0xA2, 0x9D, 0x98, 0x93,
     0x8E, 0x88, 0x83, 0x7E, 0x78, 0x73, 0x6D, 0x67,
     0x61, 0x5C, 0x56, 0x50, 0x4A, 0x44, 0x3E, 0x38,
     0x31, 0x2B, 0x25, 0x1F, 0x19, 0x12,  0xC,  0x6,
        0,  -6,  -0xC,-0x12,-0x19,-0x1F,-0x25,-0x2B,
    -0x31,-0x38,-0x3E,-0x44,-0x4A,-0x50,-0x56,-0x5C,
    -0x61,-0x67,-0x6D,-0x75,-0x78,-0x7E,-0x83,-0x88,
    -0x8E,-0x93,-0x98,-0x9D,-0xA2,-0xA7,-0xAB,-0xB0,
    -0xB5,-0xB9,-0xBD,-0xC1,-0xC5,-0xC9,-0xCD,-0xD1,
    -0xD4,-0xD8,-0xDB,-0xDE,-0xE1,-0xE4,-0xE7,-0xEA,
    -0xEC,-0xEE,-0xF1,-0xF3,-0xF4,-0xF6,-0xF8,-0xF9,
    -0xFB,-0xFC,-0xFD,-0xFE,-0xFE,-0xFF,-0xFF,-0xFF,
    -0x100,-0xFF,-0xFF,-0xFF,-0xFE,-0xFE,-0xFD,-0xFC,
    -0xFB,-0xF9,-0xF8,-0xF6,-0xF4,-0xF3,-0xF1,-0xEE,
    -0xEC,-0xEA,-0xE7,-0xE4,-0xE1,-0xDE,-0xDB,-0xD8,
    -0xD4,-0xD1,-0xCD,-0xC9,-0xC5,-0xC1,-0xBD,-0xB9,
    -0xB5,-0xB0,-0xAB,-0xA7,-0xA2,-0x9D,-0x98,-0x93,
    -0x8E,-0x88,-0x83,-0x7E,-0x78,-0x75,-0x6D,-0x67,
    -0x61,-0x5C,-0x56,-0x50,-0x4A,-0x44,-0x3E,-0x38,
    -0x31,-0x2B,-0x25,-0x1F,-0x19,-0x12, -0xC,  -6,
    /* overflow repeat: words 0x100-0x13F = words 0x00-0x3F (cosine offset) */
        0,    6,  0xC, 0x12, 0x19, 0x1F, 0x25, 0x2B,
     0x31, 0x38, 0x3E, 0x44, 0x4A, 0x50, 0x56, 0x5C,
     0x61, 0x67, 0x6D, 0x73, 0x78, 0x7E, 0x83, 0x88,
     0x8E, 0x93, 0x98, 0x9D, 0xA2, 0xA7, 0xAB, 0xB0,
     0xB5, 0xB9, 0xBD, 0xC1, 0xC5, 0xC9, 0xCD, 0xD1,
     0xD4, 0xD8, 0xDB, 0xDE, 0xE1, 0xE4, 0xE7, 0xEA,
     0xEC, 0xEE, 0xF1, 0xF3, 0xF4, 0xF6, 0xF8, 0xF9,
     0xFB, 0xFC, 0xFD, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF,
};

void CalcSine(int angle, int16_t *s0, int16_t *s1) {
    int a = angle & 0xFF;

    /* sine   = word at byte offset angle*2 = word index angle */
    *s0 = Sine_Data[a];
    /* cosine = word 0x80 bytes later = word index angle+64 */
    *s1 = Sine_Data[a + 64];
}

/* ===========================================================================
   SynchroAnimate (Sync2 + Sync4) — sonic.asm 3136-3179, FixBugs=0.
   Only the parts used by Ring/RingLoss are ported. Sync1 (spiked log) and
   Sync3 (unused) are not ported yet.
   =========================================================================== */
void SynchroAnimate(void) {
    /* Sync2: rings / giant rings — timer, then frame among 0..3 */
    if ((int8_t)(--v_ani1_time) < 0) {   /* subq.b / bpl */
        v_ani1_time = 8 - 1;
        v_ani1_frame = (v_ani1_frame + 1) & 3;
    }

    /* Sync4: bouncing rings — if the timer is active, derive the frame
       from a buffered accumulate and count down. */
    if (v_ani3_time == 0) {
        return;
    }
    int d0 = v_ani3_time;
    d0 += v_ani3_buf;
    v_ani3_buf = (uint16_t)d0;
    d0 = (int)(uint16_t)((d0 << 7) | ((uint16_t)d0 >> 9));  /* rol.w #7 */
    d0 &= 3;
    v_ani3_frame = (uint8_t)d0;
    v_ani3_time = v_ani3_time - 1;                 /* subq.b */
}

/* ===========================================================================
   SpeedToPos — _incObj/sub ObjectFall & SpeedToPos.asm.
   obX/obY are the low (pixel) words of the 16.16 position; obSubpixelX/Y
   carry the high (fraction) words. Adds asl.l #8 of the signed 16-bit
   velocity to the 32-bit position.
   =========================================================================== */
void SpeedToPos(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    int32_t x = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
    int32_t y = ((uint32_t)obY(o) << 16) | (uint16_t)obSubpixelY(o);

    x += (int32_t)obVelX(o) << 8;
    y += (int32_t)obVelY(o) << 8;

    obX(o)         = (int16_t)((uint32_t)x >> 16);
    obSubpixelX(o) = (int16_t)(x & 0xFFFF);
    obY(o)         = (int16_t)((uint32_t)y >> 16);
    obSubpixelY(o) = (int16_t)(y & 0xFFFF);
}

/* ===========================================================================
   ObjFloorDist — _incObj/sub ObjFloorDist.asm.
   Input: obj = object
   Output: *dist = distance to floor, *angle = floor angle (snapped if bit 0 set)
   =========================================================================== */
void ObjFloorDist(void *obj, int16_t *dist, int16_t *angle) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d1;
    uint8_t d3;
    int16_t y = (int16_t)(obY(o) + (int8_t)obHeight(o));
    int16_t x = obX(o);
    FindFloor(y, x, 0x0D, 0, 0x10, &v_anglebuffer, obj, &d1);
    d3 = v_anglebuffer;
    if (d3 & 0x01)
        d3 = 0;
    if (dist)  *dist  = d1;
    if (angle) *angle = (int16_t)d3;
}

/* ===========================================================================
   ObjFloorDist2 — second entry of _incObj/sub ObjFloorDist.asm.
   Same as ObjFloorDist, but the X-position comes in as a parameter (d3),
   e.g. "16px ahead" for ledge checks. FixBugs=0.
   Ported verbatim from ObjFloorDist.asm lines 21-39.
   =========================================================================== */
void ObjFloorDist2(void *obj, int16_t x, int16_t *dist, int16_t *angle) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d1;
    uint8_t d3;
    int16_t y = (int16_t)(obY(o) + (int8_t)obHeight(o));
    FindFloor(y, x, 0x0D, 0, 0x10, &v_anglebuffer, obj, &d1);
    d3 = v_anglebuffer;
    if (d3 & 0x01)
        d3 = 0;
    if (dist)  *dist  = d1;
    if (angle) *angle = (int16_t)d3;
}

/* ===========================================================================
   RememberState — _incObj/sub RememberState.asm.
   out_of_range.w .offscreen: if the object is on-screen, DisplaySprite;
   otherwise clear its respawn-table bit and delete it so it can respawn. */
void RememberState(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (!OutOfRange(obj, -1)) {            /* out_of_range.w .offscreen (bne) */
        DisplaySprite(obj);                /* bra.w DisplaySprite */
        return;
    }

    /* .offscreen */
    uint8_t d0 = obRespawnNo(o);           /* moveq #0,d0 ; move.b obRespawnNo,d0 */
    if (d0 != 0) {                         /* beq.s .delete */
        RAM_BYTE(v_objstate + 2 + d0) &= ~0x80;  /* bclr #7,2(a2,d0.w) */
    }
    /* .delete */
    DeleteObject(obj);                     /* bra.w DeleteObject */
}

/* ===========================================================================
   OutOfRange — the out_of_range macro (Macros.asm 278-295), FixBugs form:
   cond = ((pos & ~0x7F) - (((v_screenposx - 128) & ~0x7F)));
   delete when the high bit of cond is set OR cond > 128+320+192.
   The ring pass passes ring_origX(a0); pass -1 to mean obX(a0).
   =========================================================================== */
int OutOfRange(void *obj, int16_t ring_origX) {
    uint8_t *o = (uint8_t *)obj;
    int16_t pos = ring_origX;
    if (ring_origX == -1) {
        pos = obX(o);
    }
    uint16_t d0 = (uint16_t)pos & 0xFF80;                      /* andi.w #$FF80 */
    uint16_t d1 = ((uint16_t)RAM_WORD(0xF700) - 128) & 0xFF80; /* v_screenposx */
    int16_t diff = (int16_t)(d0 - d1);                         /* sub.w d1,d0 */

    if (diff < 0) {
        return 1;                                              /* bmi.w exit */
    }
    if ((uint16_t)diff > 128 + 320 + 192u) {                   /* cmpi/bhi (unsigned) */
        return 1;
    }
    return 0;
}

/* ===========================================================================
   RandomNumber — _incObj/sub RandomNumber.asm.
   Generates a pseudo-random number with a 32-bit LCG. Returns the resulting
   word in the low 16 bits (ASM d0); the updated seed is stored in v_random.

   move.l (v_random).w,d1 / bne.s .scramble / move.l #$2A6D365A,d1
   .scramble: d1 = d1*41 (via asl/add); d0 = low(d1) + high(d1); seed = d0<<16
   =========================================================================== */
static uint16_t RandomNumber(void) {
    uint32_t d1 = v_random;                          /* move.l (v_random).w,d1 */
    if (d1 == 0) {                                   /* bne.s .scramble */
        d1 = 0x2A6D365Au;                            /* move.l #$2A6D365A,d1 */
    }

    /* .scramble */
    uint32_t d0 = d1;                                /* move.l d1,d0 */
    d1 = (d1 << 2) + d0;                             /* asl.l #2,d1 / add.l d0,d1 */
    d1 = (d1 << 3) + d0;                             /* asl.l #3,d1 / add.l d0,d1 */

    d0 = (uint32_t)(uint16_t)d1;                     /* move.w d1,d0 (low word) */
    d1 = (d1 >> 16) | (d1 << 16);                    /* swap d1 */
    d0 = ((uint32_t)d0 + (uint16_t)d1) & 0xFFFF;     /* add.w d1,d0 (low+high words) */

    d1 = d0 << 16;                                   /* move.w d0,d1 / swap d1 */
    v_random = d1;                                   /* move.l d1,(v_random).w */

    return (uint16_t)d0;                             /* d0 contains pseudo-random number */
}

/* ===========================================================================
   TitleSonic object (id_TitleSonic = $0E)
   Ported from _incObj/0E, 0F Title Screen - Sonic, Press Start, TM.asm
   =========================================================================== */
static void TitleSonic_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t routine = obRoutine(o);

    switch (routine) {
        case 0: {
            obRoutine(o) = 2;
            obX(o) = (int16_t)(0x80 + 0x70); /* original X (FixBugs: 0x80+0x78) */
            obScreenY(o) = (int16_t)(0x80 + 0x5E);
            obMap(o) = (uint32_t)(uintptr_t)Map_TSon;
            obGfx(o) = (uint16_t)(ArtTile_Title_Sonic | Tile_Pal2);
            obPriority(o) = 1;
            obDelayAni(o) = 30 - 1;
            if (Ani_TSon) {
                AnimateSprite(obj, Ani_TSon); /* matches ASM: lea (Ani_TSon).l,a1 / bsr AnimateSprite */
            }
            return; /* no display (ASM TSon_Main ends after AnimateSprite) */
        }
        case 2: {
            obDelayAni(o)--;
            if ((int8_t)obDelayAni(o) >= 0) {
                return; /* ASM TSon_Delay .wait: rts -> no display while waiting */
            }
            obRoutine(o) = 4;
            DisplaySprite(obj); /* ASM: bra DisplaySprite on delay expiry (no move this frame) */
            return;
        }
        case 4: {
            int16_t y = obScreenY(o);
            y -= 8;
            if (y == 0x80 + 0x16) {
                obRoutine(o) = 6;
            }
            obScreenY(o) = y;
            DisplaySprite(obj);
            return;
        }
        case 6: {
            if (Ani_TSon) {
                AnimateSprite(obj, Ani_TSon);
            }
            DisplaySprite(obj);
            return;
        }
    }
    DisplaySprite(obj);
}

/* ===========================================================================
   PSBTM object (id_PSBTM = $0F)
   Handles Press Start, TM, and masking sprites on title screen
   =========================================================================== */
static void PSBTM_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t routine = obRoutine(o);

    switch (routine) {
        case 0: {
            obRoutine(o) = 2;
            obX(o) = (int16_t)(0x80 + 0x50); /* original X (FixBugs: 0x80+0x58) */
            obScreenY(o) = (int16_t)(0x80 + 0xB0);
            obMap(o) = (uint32_t)(uintptr_t)Map_PSB;
            obGfx(o) = (uint16_t)(ArtTile_Title_Foreground);

            if (obFrame(o) < 2) {
                /* Press Start: animate */
                obRoutine(o) = 2;
            } else {
                /* TM or masking sprites: static */
                obRoutine(o) = 4;
                if (obFrame(o) == 3) {
                    obGfx(o) = (uint16_t)(ArtTile_Title_Trademark | Tile_Pal2);
                    obX(o) = (int16_t)(0x80 + 0xF0); /* FixBugs: 0x80+0xF8 */
                    obScreenY(o) = (int16_t)(0x80 + 0x78);
                }
            }
            break;
        }
        case 2: {
            if (Ani_PSBTM) {
                AnimateSprite(obj, Ani_PSBTM);
            }
            break;
        }
        case 4: {
            /* Static: do nothing */
            break;
        }
    }
    DisplaySprite(obj);
}

/* ===========================================================================
   CreditsText object (id_CreditsText = $8A)
   "SONIC TEAM PRESENTS" and credits (from _incObj/8A Credits and Sonic
   Team Presents.asm)
   =========================================================================== */
static void CreditsText_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0: /* Cred_Main: routine 0 */
            obRoutine(o) += 2; /* advance to routine 2 (Cred_Display) */

            /* Set X-position to horizontally centered ($120 = (320/2)+$80) */
            obX(o) = (int16_t)((320 / 2) + 0x80);
            /* Set Y-position to vertically centered ($F0 = (224/2)+$80) */
            obScreenY(o) = (int16_t)((224 / 2) + 0x80);

            obMap(o) = (uint32_t)(uintptr_t)Map_Cred;
            obGfx(o) = ArtTile_Credits_Font; /* default art tile offset */

            /* Load credits page index (doesn't reset between game mode changes) */
            obFrame(o) = (uint8_t)(v_creditsnum & 0xFF);

            /* Set to screen coordinates positioning mode, top priority */
            obRender(o) = sprite_cam_screen;
            obPriority(o) = 0;

            if (v_gamemode == 0x04) { /* id_Title (GM_Title = $04) */
                obGfx(o) = ArtTile_Sonic_Team_Font; /* alternate art tile for title screen */
                obFrame(o) = 0x0A;                  /* "SONIC TEAM PRESENTS" frame */

                /* Hidden Japanese credits cheat: A+B+C+Down ($72) held */
                if (f_creditscheat && (v_jpadhold1 == (btnABC | btnDn))) {
                    RAM_WORD(v_palette_fading_line_3)     = cWhite; /* 1st entry = white */
                    RAM_WORD(v_palette_fading_line_3 + 2) = 0x880; /* 2nd entry = cyan */
                    DeleteObject(obj); /* delete STP object for hidden Japanese credits */
                    return;
                }
            }
            /* fall through to Cred_Display */
            __attribute__((fallthrough));

        default: /* Cred_Display: routine 2 - just display credits sprite */
            DisplaySprite(obj);
            break;
    }
}

/* ===========================================================================
   TitleCard object (id_TitleCard = $34)
   Zone title cards. Ported from _incObj/34 Title Cards.asm.
   The root object (v_titlecard) is converted into the level "name" card;
   three more elements (ZONE, ACT, oval) are placed right after it in memory.
   =========================================================================== */

/* Card_ItemData: per-element Y-position and frame ID. All four elements
   are born in routine 2 (Card_MoveIn). */
static const int16_t Card_ItemDataY[4] = { 0xD0, 0xE4, 0xEA, 0xE0 };
static const uint8_t Card_ItemDataF[4] = { 0x00, 0x06, 0x07, 0x0A };

/* Card_ConData: four (start X, target X) pairs per zone -
   name, ZONE, ACT, oval. Element 6 is used by Final Zone. */
static const int16_t Card_ConData[7][8] = {
    { 0x000, 0x120, -0x104, 0x13C, 0x414, 0x154, 0x214, 0x154 }, /* GHZ */
    { 0x000, 0x120, -0x10C, 0x134, 0x40C, 0x14C, 0x20C, 0x14C }, /* LZ */
    { 0x000, 0x120, -0x120, 0x120, 0x3F8, 0x138, 0x1F8, 0x138 }, /* MZ */
    { 0x000, 0x120, -0x104, 0x13C, 0x414, 0x154, 0x214, 0x154 }, /* SLZ */
    { 0x000, 0x120, -0x0FC, 0x144, 0x41C, 0x15C, 0x21C, 0x15C }, /* SYZ */
    { 0x000, 0x120, -0x0FC, 0x144, 0x41C, 0x15C, 0x21C, 0x15C }, /* SBZ */
    { 0x000, 0x120, -0x11C, 0x124, 0x3EC, 0x3EC, 0x1EC, 0x12C }, /* FZ */
};

int TitleCardsSettled(void) {
    for (int i = 0; i < 4; i++) {
        uint8_t *o = &ram[v_titlecard + OBJECT_SIZE * i];
        if (obID(o) == 0) continue;
        if (obX(o) != cardMainX(o)) return 0;
    }
    return 1;
}

static void TitleCard_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t routine = obRoutine(o);

    /* Card_LoadForZone (routine 0): turn this slot into the name card and
       spawn the other three elements back-to-back after it. */
    if (routine == 0) {
        uint8_t *a1 = o;
        int d0 = v_zone;
        uint16_t zact = RAM_U16(0xFE10);

        if (zact == id_LZ_act4) {
            d0 = 5;                 /* SBZ3: use SBZ title card */
        }
        int d2 = d0;                /* name card frame ID */
        if (zact == id_FZ) {
            d0 = 6;                 /* FZ entry in Card_ConData */
            d2 = 0x0B;              /* "FINAL" mapping frame */
        }

        const int16_t *con = Card_ConData[d0];
        for (int i = 0; i < 4; i++) {
            obID(a1)       = id_TitleCard;
            obX(a1)        = (int16_t)con[i * 2];
            cardFinalX(a1) = (int16_t)con[i * 2];         /* same as start */
            cardMainX(a1)  = (int16_t)con[i * 2 + 1];
            obScreenY(a1)  = (int16_t)Card_ItemDataY[i];
            obRoutine(a1)  = 2;                           /* Card_MoveIn */
            int frame = Card_ItemDataF[i];
            if (frame == 0) {
                frame = d2;                               /* zone name frame */
            }
            if (frame == 7) {
                frame += v_act;
                if (v_act == act4) frame -= 1;            /* SBZ3/LZ4 keeps "3" */
            }
            obFrame(a1)    = (uint8_t)frame;
            obMap(a1)      = (uint32_t)(uintptr_t)Map_Card;
            obGfx(a1)      = (uint16_t)(ArtTile_Title_Card | Tile_Prio);
            obActWid(a1)   = 240 / 2;
            obRender(a1)   = sprite_cam_screen;
            obPriority(a1) = 0;
            obTimeFrame(a1)= 60;                          /* 1 second delay */
            a1 += OBJECT_SIZE;
        }
        /* ASM falls through into Card_MoveIn for this (name) element. */
    }

    /* Card_MoveIn (routine 2): slide toward cardMainX at 16 px/frame. */
    if (routine == 0 || routine == 2) {
        int16_t d1 = 0x10;
        int16_t cur = obX(o);
        int16_t target = cardMainX(o);

        if (cur != target) {
            if (target < cur) d1 = -d1;
            obX(o) = (int16_t)(cur + d1);
        }

        /* Bounds check before displaying (FixBugs variant: keep long cards
           like Spring Yard from poking in on the wrong side of the screen).
           Displays only while X is in (0x50, 0x200]. */
        int16_t x = obX(o);
        if (x <= 0x50 || x > 0x200) return;  /* off screen: don't display */
        DisplaySprite(obj);
        return;
    }

    /* Card_Wait (routine 4/6): count down, then slide back out. */
    if (routine == 4 || routine == 6) {
        if (obTimeFrame(o) != 0) {
            obTimeFrame(o)--;
            DisplaySprite(obj);
            return;
        }

        /* Card_MoveOut: 32 px/frame back toward cardFinalX (the start). */
        if (!(obRender(o) & 0x80)) {
            DeleteObject(obj);      
            return;
        }
        int16_t d1 = 0x20;
        int16_t cur = obX(o);
        int16_t target = cardFinalX(o);
        if (cur == target) {
            AddPLC(plcid_Explode); /* Card_ChangeArt */
            int d0 = (uint8_t)v_zone + plcid_GHZAnimals;
            AddPLC(d0);
            DeleteObject(obj);      
            return;
        }
        if (target < cur) d1 = -d1;
        cur = (int16_t)(cur + d1);
        obX(o) = cur;
        /* Keep moving even when off screen; only the display is gated. */
        if (cur <= 0x50 || cur > 0x200) return;
        DisplaySprite(obj);
        return;
    }

    DisplaySprite(obj);
}

/* ===========================================================================
   SonicPlayer object (id_SonicPlayer = $01)
   Ported from _incObj/01 Sonic.asm (REV01, FixBugs=0)
   =========================================================================== */

static void Sonic_Main(void *obj);
static void Sonic_Control(void *obj);
static void Sonic_Hurt(void *obj);
static void Sonic_Death(void *obj);
static void Sonic_ResetLevel(void *obj);

static void Sonic_Move(void *obj);
static void Sonic_MoveLeft(void *obj);
static void Sonic_MoveRight(void *obj);

static void Sonic_RollSpeed(void *obj);
static void Sonic_RollLeft(void *obj);
static void Sonic_RollRight(void *obj);
static void Sonic_Roll(void *obj);
static void Sonic_ChkRoll(void *obj);

static void Sonic_JumpDirection(void *obj);
static void Sonic_JumpHeight(void *obj);
static int Sonic_Jump(void *obj);

static void Sonic_LevelBound(void *obj);
void KillSonic(void *obj, void *damager);
void HurtSonic(void *obj, void *damager);
void ReactToItem(void *obj);

static void Sonic_AngledRollSpeed(void *obj);
static void Sonic_Floor(void *obj);
static void Sonic_FloorDown(void *obj);
static void Sonic_FloorLeft(void *obj);
static void Sonic_FloorUp(void *obj);
static void Sonic_FloorRight(void *obj);
static void Sonic_ResetOnFloor(void *obj);
static void Sonic_SlopeResistWalk(void *obj);
static void Sonic_SlopeResistRoll(void *obj);
static void Sonic_SlopeRepel(void *obj);
static void Sonic_JumpAngle(void *obj);

static void Sonic_Display(void *obj);
static void Sonic_RecordPosition(void *obj);
static void Sonic_Water(void *obj);
static void Sonic_Animate(void *obj);
static void Sonic_LoadGfx(void *obj);
static void Sonic_Loops(void *obj);

static void Sonic_AngleSpeed(void *obj);
static void Sonic_ResetScr(void *obj);
static void Sonic_LookUp(void *obj);
static void Sonic_Duck(void *obj);
static void Sonic_CheckDpadLetGo(void *obj);
static void Sonic_WallSpeedAdjust(void *obj);

static void Sonic_RollJumpLock(void *obj);
static void Sonic_SquashUnused(void *obj);

static void Sonic_HurtStop(void *obj);
static void Sonic_HandleDeath(void *obj);

/* ===========================================================================
   Sonic mode implementations (forward declared for Sonic_Modes array)
   =========================================================================== */
static void Sonic_MdNormal(void *obj);
static void Sonic_MdJump(void *obj);
static void Sonic_MdRoll(void *obj);
static void Sonic_MdJump2(void *obj);

static void (*const Sonic_Modes[4])(void *) = {
    Sonic_MdNormal, Sonic_MdJump, Sonic_MdRoll, Sonic_MdJump2
};

static void SonicPlayer_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    /* ASM SonicPlayer: tst.w v_debuguse; if set, jump to DebugMode */
    if (v_debuguse) {
        DebugMode_Main(o);
        return;
    }

    uint8_t routine = obRoutine(o);

    switch (routine) {
        case 0: Sonic_Main(o); break;
        case 2: Sonic_Control(o); break;
        case 4: Sonic_Hurt(o); break;
        case 6: Sonic_Death(o); break;
        case 8: Sonic_ResetLevel(o); break;
    }
}

/* ===========================================================================
   Sonic_Main — Routine 0: initialization
   =========================================================================== */
static void Sonic_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    obRoutine(o) = 2;                          /* advance to Sonic_Control */
    obHeight(o) = sonic_height;
    obWidth(o) = sonic_width;
    obMap(o) = (uint32_t)(uintptr_t)Map_Sonic;
    obGfx(o) = ArtTile_Sonic;
    obPriority(o) = 2;
    obActWid(o) = 48 / 2;
    obRender(o) = sprite_cam_field;
    v_sonspeedmax = son_maxspeed;
    v_sonspeedacc = son_acceleration;
    v_sonspeeddec = son_deceleration;
}

/* ===========================================================================
   Sonic_Control — Routine 2: main control loop
   =========================================================================== */
static void Sonic_Control(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (f_debugmode) {
        if (v_jpadpress1 & btnB) {
            v_debuguse = 1;
            f_lockctrl = 0;
            return;
        }
    }

    if (!f_lockctrl) {
        v_jpadhold2 = v_jpadhold1;
        v_jpadpress2 = v_jpadpress1;
    }

    if (f_playerctrl & 1) {
        goto ignore_modes;
    }

    uint8_t status = obStatus(o) & 0x06;       /* in-air | rolling */
    void (*mode)(void *) = Sonic_Modes[status >> 1];
    mode(o);

ignore_modes:
    Sonic_Display(o);
    Sonic_RecordPosition(o);
    Sonic_Water(o);
    angleright(o) = v_anglebuffer;
    angleleft(o) = v_anglebuffer2;

    if (f_wtunnelmode) {
        if (obAnim(o) == 0) {
            obAnim(o) = obPrevAni(o);
        }
    }

    Sonic_Animate(o);

    /* ASM: tst.b (f_playerctrl).w / bmi.s .ignoreobjcoll — bit7 clears interaction */
    if (!(f_playerctrl & 0x80)) {
        ReactToItem(o);
    }

    Sonic_Loops(o);
    Sonic_LoadGfx(o);
}

/* ===========================================================================
   Sonic_Display — display sprite + handle power-up expiration
   =========================================================================== */
static const uint8_t music_list[] = {
    bgm_GHZ, bgm_LZ, bgm_MZ, bgm_SLZ, bgm_SYZ, bgm_SBZ, bgm_FZ
};

static void Sonic_Display(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    int16_t flash = flashtime(o);
    if (flash) {
        flashtime(o) = flash - 1;
        /* ASM: lsr.w #3,d0 / bcc — el bit que va al carry es el bit 2 */
        if (!((flash >> 2) & 1)) {
            goto chk_invincible;
        }
    }

    DisplaySprite(obj);

chk_invincible:
    if (v_invinc) {
        int16_t inv = invtime(o);
        if (inv) {
            invtime(o) = inv - 1;
            if (!invtime(o)) {
                if (!f_lockscreen) {
                    if (v_air >= 12) {
                        uint8_t zone = v_zone;
                        if (RAM_U16(0xFE10) != id_LZ_act4) {
                            Sound_Queue(music_list[zone], false);
                        } else {
                            Sound_Queue(bgm_SBZ, false);
                        }
                    }
                }
                v_invinc = 0;
            }
        }
    }

    if (v_shoes) {
        int16_t shoe = shoetime(o);
        if (shoe) {
            shoetime(o) = shoe - 1;
            if (!shoetime(o)) {
                v_sonspeedmax = son_maxspeed;
                v_sonspeedacc = son_acceleration;
                v_sonspeeddec = son_deceleration;
                /* FixBugs: underwater fix already handled in Sonic_Water */
                v_shoes = 0;
                Sound_Queue(bgm_Slowdown, false);
            }
        }
    }
}

/* ===========================================================================
   Sonic_RecordPosition — record position for invincibility stars
   =========================================================================== */
static void Sonic_RecordPosition(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint16_t idx = v_trackpos;
    uint8_t *a1 = RAM_ADDR(v_tracksonic + idx);
    *(int16_t *)a1 = obX(o);
    a1 += 2;
    *(int16_t *)a1 = obY(o);
    /* addq.b #4,(v_trackbyte): on the 68k v_trackbyte is the word's low
       byte (BE), so the index wraps 4,8,...,$FC,0. RAM here is LE, so the
       +1 byte no longer overlaps the word's low byte; reproduce the wrap
       on the word value entirely. */
    v_trackbyte += 4;
    v_trackpos = (uint16_t)((idx + 4) & 0xFF);
}

/* ===========================================================================
   ResumeMusic — resume level music after countdown / underwater
   Ported from _incObj/sub ResumeMusic.asm
   =========================================================================== */
static void ResumeMusic(void) {
    if (v_air > 12) {
        uint16_t bgm = bgm_LZ;
        if (RAM_U16(0xFE10) == id_LZ_act4) {
            bgm = bgm_SBZ;
        }
        if (v_invinc) {
            bgm = bgm_Invincible;
        }
        if (f_lockscreen) {
            bgm = bgm_Boss;
        }
        Sound_Queue(bgm, false);
    }
    v_air = 30;
    RAM_BYTE(v_sonicbubbles + 0x2C) = 0;  /* bub_time offset */
}

/* ===========================================================================
   Sonic_Water — underwater handling (LZ only)
   =========================================================================== */
static void Sonic_Water(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    if (v_zone != id_LZ) return;

    int16_t water_y = v_waterpos1;
    if (obY(o) <= water_y) {
        /* below water surface - entering water */
        uint8_t was_underwater = obStatus(o) & (1 << 6);
        obStatus(o) |= (1 << 6);   /* set underwater flag */
        if (was_underwater) {
            return;  /* already underwater */
        }
        /* just entered water */
        ResumeMusic();
        {
            uint8_t *bubbles = RAM_ADDR(v_sonicbubbles);
            RAM_BYTE(v_sonicbubbles) = id_DrownCount;
            obSubtype(bubbles) = 0x81;
        }
        v_sonspeedmax = son_maxspeed / 2;
        v_sonspeedacc = son_acceleration / 2;
        v_sonspeeddec = son_deceleration / 2;
        obVelX(o) = (int16_t)(obVelX(o) >> 1);
        obVelY(o) = (int16_t)(obVelY(o) >> 2);
        if (obVelY(o) != 0) {
            /* load splash object, play sound */
        }
    } else {
        /* above water surface - exiting water */
        uint8_t was_underwater = obStatus(o) & (1 << 6);
        obStatus(o) &= ~(1 << 6);  /* clear underwater flag */
        if (!was_underwater) {
            return;  /* already above water */
        }
        /* just exited water */
        ResumeMusic();
        v_sonspeedmax = son_maxspeed;
        v_sonspeedacc = son_acceleration;
        v_sonspeeddec = son_deceleration;
        obVelY(o) = (int16_t)(obVelY(o) << 1);
        if (obVelY(o) != 0) {
            /* load splash object, play sound */
            if (obVelY(o) < -0x1000) obVelY(o) = -0x1000;
        }
    }
}

/* ===========================================================================
   HUD object (id_HUD = $21)
   Ported from _incObj/21 HUD.asm (FixBugs=0).
   "SCOR", "TIME", "RINGS" text; on screen position (0x90, 0x108).
   Flash frames: with rings, always all-yellow. Without rings, the ring
   counter flashes red on frames where v_framebyte bit 3 is clear; at 9
   minutes the time counter stays red too.
   =========================================================================== */
static void HUD_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t routine = obRoutine(o);

    if (routine == 0) {
        obRoutine(o) = 2;                              /* advance to HUD_Flash */
        obX(o)        = (int16_t)(0x80 + 0x10);        /* screen X (0x90) */
        obScreenY(o)  = (int16_t)(0x80 + 0x88);        /* screen Y (0x108) */
        obMap(o)      = (uint32_t)(uintptr_t)Map_HUD;
        obGfx(o)      = (uint16_t)ArtTile_HUD;         /* pieces carry pri/pal */
        obRender(o)   = sprite_cam_screen;
        obPriority(o) = 0;
        /* ASM falls through into HUD_Flash for the first frame */
    }

    /* HUD_Flash (routine 2) */
    if (v_rings != 0) {
        obFrame(o) = 0;                                /* all counters yellow */
        DisplaySprite(obj);
        return;
    }

    /* No rings: flash the ring counter red every 8 frames, all-red at 9:00 */
    int d0 = 0;
    if (!(v_framebyte & 0x08)) {
        d0 += 1;                                       /* ring counter red */
    }
    if (v_timemin == 9) {
        d0 += 2;                                       /* + time counter red */
    }
    obFrame(o) = (uint8_t)d0;
    DisplaySprite(obj);
}

/* ===========================================================================
   Object 25 — Ring (and Object 37 — RingLoss)
   Ported verbatim from _incObj/25, 37 Rings.asm (REV01, FixBugs=0).
   =========================================================================== */

/* Distances between rings (format: horizontal, vertical) — Ring_PosData */
static const int8_t Ring_PosData[32] = {
     0x10,    0,    0x18,    0,    0x20,    0,           /* $0-$2 right */
        0, 0x10,       0, 0x18,       0, 0x20,           /* $3-$5 down  */
     0x10, 0x10,    0x18, 0x18,    0x20, 0x20,           /* $6-$8 diag R */
    -0x10, 0x10,  -0x18, 0x18,  -0x20, 0x20,             /* $9-$B diag L */
     0x10,    8,    0x18, 0x10,                          /* $C-$D diag RR */
    -0x10,    8,   -0x18, 0x10,                          /* $E-$F diag LL */
};

static void Ring_Collect(uint8_t *o);
static void CollectRing(uint8_t *o);

/* Ring_Main (routine 0): expand a ring group from its subtype.
   The subtype's low nybble is the ring count (0 = one ring, 8 capped to 7);
   its high nybble indexes Ring_PosData for the spacing per ring.
   The group's respawn byte (at v_objstate+2+obRespawnNo) tracks, per ring,
   whether it was already collected: bit0 = first ring, bits 1..7 = the rest.
   Ring_SpawnRing semantics: the first ring is spawned directly into the
   group's own slot (no FindFreeObj); ring_respawnbit holds the ring's
   position within the group (0,1,2,...). d4 is right-shifted once per ring
   (lsr.b #1,d4) so each ring tests its own collected flag.
   (Ported verbatim from 25 Rings.asm Ring_Main / Ring_MakeRings. FixBugs=0.) */

/* Ring_SpawnRing (inline shared between the first ring and the loop) */
static void Ring_InitSlot(uint8_t *a1, uint8_t *group, int16_t x, int16_t y,
                          int index) {
    obID(a1)        = id_Rings;
    obRoutine(a1)  += 2;
    obX(a1)         = x;
    ring_origX(a1)  = obX(group);
    obY(a1)         = y;
    obMap(a1)       = (uint32_t)(uintptr_t)Map_Ring;
    obGfx(a1)       = (uint16_t)(ArtTile_Ring | Tile_Pal2);
    obRender(a1)    = sprite_cam_field;
    obPriority(a1)  = 2;
    obColType(a1)   = col_12x12 | col_item;
    obActWid(a1)    = 16 / 2;
    obRespawnNo(a1) = obRespawnNo(group);
    ring_respawnbit(a1) = (uint8_t)index;   /* move.b d1,ring_respawnbit */
}

static void Ring_Main_Expand(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    uint8_t *a2 = RAM_ADDR(v_objstate) + 2 + obRespawnNo(o);
    uint8_t d4 = *a2;                                          /* group state */

    uint8_t d1 = obSubtype(o);
    uint8_t count = d1 & 7;                                    /* andi.w #7 */
    if (count == 7) {
        count = 6;                                             /* :.not8 cap */
    }
    uint8_t orient = d1 >> 4;                                  /* lsr.b #4 */
    int idx = orient * 2;

    int16_t d5 = Ring_PosData[idx + 0];      /* X spacing (ext.w) */
    int16_t d6 = Ring_PosData[idx + 1];      /* Y spacing (ext.w) */

    int16_t d2 = obX(o);
    int16_t d3 = obY(o);
    int index = 0;

    /* First ring: test bit0 (ASM: lsr.b #1,d4 feeds carry; bcs = collected).
       The test must happen BEFORE the shift, since bcs inspects the carry
       OUT of the shift (the old bit0). Spawn directly into this slot (a1=a0,
       no FindFreeObj). Ring_NextRing then advances index/position once. */
    if (!(d4 & 1)) {                       /* bcs.s Ring_NextRing */
        *a2 &= ~0x80;                      /* bclr #7,(a2): clear respawn block */
        Ring_InitSlot(o, o, d2, d3, 0);    /* first ring = this group slot */
    }
    d4 >>= 1;                              /* lsr.b #1,d4 */
    index++;                               /* Ring_NextRing: addq.w #1,d1 */
    d2 += d5;
    d3 += d6;

    for (int i = 0; i < count; i++) {      /* dbf count */
        if (!(d4 & 1)) {                   /* bcs.s Ring_NextRing (old bit0) */
            *a2 &= ~0x80;                  /* bclr #7,(a2) */
            uint8_t *slot = (uint8_t *)FindFreeObj();
            if (!slot) {
                break;                     /* bne.s Ring_SpawningDone */
            }
            Ring_InitSlot(slot, o, d2, d3, index);
        }
        d4 >>= 1;                          /* lsr.b #1,d4 */
        index++;                           /* Ring_NextRing: addq.w #1,d1 */
        d2 += d5;
        d3 += d6;
    }

    /* Ring_SpawningDone: delete the group if the first ring was collected. */
    if (*a2 & 1) {
        DeleteObject(obj);
    }
}

/* Ring_Animate (routine 2): set frame from Sync2 and despawn when out of
   range. FixBugs=0: DisplaySprite first, then out_of_range (the branch uses
   ring_origX — the group's X — so the whole cluster despawns together). */
static void Ring_Animate(uint8_t *o) {
    obFrame(o) = v_ani1_frame;
    DisplaySprite(o);
    if (OutOfRange(o, ring_origX(o))) {
        DeleteObject(o);
    }
}

/* Ring_Collect (routine 4): started by ReactToItem when Sonic touches the
   ring (addq.b #2 advances Ring_Animate → Ring_Collect). */
static void Ring_Collect(uint8_t *o) {
    obRoutine(o) += 2;                       /* -> Ring_Sparkle */
    obColType(o)  = col_none;
    obPriority(o) = 1;
    CollectRing(o);                          /* add ring + sfx */
    uint8_t *a2 = RAM_ADDR(v_objstate) + 2 + obRespawnNo(o);
    uint8_t bit = ring_respawnbit(o);
    *a2 |= (uint8_t)(1u << bit);             /* bset d1,2(a2,d0.w) (FixBugs=0) */
}

/* Ring_Sparkle (routine 6) */
static void Ring_Sparkle(uint8_t *o) {
    AnimateSprite(o, Ani_Ring);
    DisplaySprite(o);
}

/* Ring_Delete (routine 8) */
static void Ring_Delete(uint8_t *o) {
    DeleteObject(o);
}

/* Ring dispatcher (Ring_Index) */
static void Ring_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Ring_Main_Expand(o); break;
        case 2: Ring_Animate(o); break;
        case 4: Ring_Collect(o); break;
        case 6: Ring_Sparkle(o); break;
        case 8: Ring_Delete(o); break;
    }
}

/* CollectRing — add 1 ring, update the HUD, optionally award an extra life.
   FixBugs=0: no 999 cap; ring counter just increments (addq.w). */
static void CollectRing(uint8_t *o) {
    (void)o;
    v_rings = v_rings + 1;                   /* addq.w #1,(v_rings).w */
    f_ringcount |= 1;                        /* ori.b #1,(f_ringcount).w */

    int d0 = sfx_Ring;

    if (v_rings >= 100) {                    /* cmpi.w #100, blo .playSound */
        if (!(v_lifecount & 2)) {            /* bset #1, beq .extraLife */
            v_lifecount |= 2;                /* (was 0: flag set, award) */
            goto extra_life;
        }
        if (v_rings < 200) {                 /* cmpi.w #200, blo .playSound */
            goto play_sound;
        }
        if (!(v_lifecount & 4)) {            /* bset #2, bne .playSound */
            v_lifecount |= 4;                /* (was 0: flag set, award) */
            goto extra_life;
        }
        goto play_sound;
    }
    goto play_sound;

extra_life:
    v_lives    = v_lives + 1;                /* addq.b #1,(v_lives).w */
    f_lifecount= f_lifecount + 1;            /* addq.b #1,(f_lifecount).w */
    d0 = bgm_ExtraLife;

play_sound:
    Sound_Queue(d0, false);                  /* jmp (QueueSound2).l */
}

/* ===========================================================================
   Object 37 — RingLoss (rings spill out when Sonic is hit)
   =========================================================================== */
#define rloss_spread (2 << 8) + 0x80 + 8     /* = $288 boost+angle fan */

static void RingLoss_Main(void *obj);
static void RingLoss_Bounce(uint8_t *o);
static void RingLoss_Collect(uint8_t *o);
static void RingLoss_Sparkle(uint8_t *o);
static void RingLoss_Delete(uint8_t *o);

static void RingLoss_Count(uint8_t *o) {
    uint8_t *a1 = o;
    int16_t d5 = v_rings;                    /* move.w (v_rings).w,d5 */
    if (d5 >= 32) {
        d5 = 32;                             /* cap 32 rings */
    }
    d5 -= 1;                                 /* subq.w #1 (for dbf) */

    uint16_t d4 = rloss_spread;
    /* d2/d3 carry the X/Y velocities, reused (X-flipped) on even iterations */
    int16_t d2 = 0, d3 = 0;

    for (int16_t i = 0; i <= d5; i++) {      /* dbf d5,.loop (N iters) */
        if (i != 0) {                        /* first iter: bra .makerings */
            uint8_t *slot = (uint8_t *)FindFreeObj();
            if (!slot) {
                goto reset_counter;
            }
            a1 = slot;
        }

        /* .makerings: spawn a bouncing ring */
        obID(a1)       = id_RingLoss;
        obRoutine(a1) += 2;
        obHeight(a1)   = 16 / 2;
        obWidth(a1)    = 16 / 2;
        obX(a1)        = obX(o);
        obY(a1)        = obY(o);
        obMap(a1)      = (uint32_t)(uintptr_t)Map_Ring;
        obGfx(a1)      = (uint16_t)(ArtTile_Ring | Tile_Pal2);
        obRender(a1)   = sprite_cam_field;
        obPriority(a1) = 3;
        obColType(a1)  = col_12x12 | col_item;
        obActWid(a1)   = 16 / 2;
        /* FixBugs=0: reset the bouncy ring animation timer per spilled ring */
        v_ani3_time = 255;

        /* Calculate bouncy ring angles */
        if ((int16_t)d4 < 0) {               /* tst.w d4 / bmi: reuse d2/d3 */
            goto set_speed;
        }

        {
            int16_t s0, s1;
            CalcSine(d4 & 0xFF, &s0, &s1);   /* bsr CalcSine (byte angle) */

            int d0 = (int)((uint16_t)d4 >> 8);   /* upper byte: boost */
            d2 = (int16_t)((uint16_t)s0 << d0);  /* asl.w d2,d0 */
            d3 = (int16_t)((uint16_t)s1 << d0);  /* asl.w d2,d1 */

            int low  = (d4 & 0x00FF) + 0x10;
            d4 = (uint16_t)((d4 & 0xFF00) | (low & 0xFF));  /* addi.b #$10 */
            if (low > 0xFF) {                /* bcc: only when it carried */
                uint16_t prev = d4;
                d4 = (uint16_t)(d4 - 0x80);  /* subi.w #$80 */
                if (d4 > prev) {             /* bcc: word underflow = reset */
                    d4 = rloss_spread;
                }
            }
        }
        goto set_speed;

set_speed:
        obVelX(a1) = d2;
        obVelY(a1) = d3;
        d2 = (int16_t)(-d2);                 /* neg.w d2 */
        d4 = (uint16_t)(-(int16_t)d4);       /* neg.w d4 */
    }

reset_counter:
    v_rings     = 0;                         /* move.w #0,(v_rings).w */
    f_ringcount = 0x80;                      /* move.b #$80,(f_ringcount).w */
    v_lifecount = 0;                         /* move.b #0,(v_lifecount).w */
    Sound_Queue(sfx_RingLoss, false);        /* sfx_RingLoss */
}

static void RingLoss_Bounce(uint8_t *o) {
    obFrame(o) = v_ani3_frame;

    SpeedToPos(o);
    int16_t after = (int16_t)(obVelY(o) + 0x18);
    obVelY(o) = after;
    if ((int16_t)obVelY(o) < 0) {            /* bmi .chkdel (still going up) */
        goto chkdel;
    }

    /* 1 of every 4 frames (spread across slots). In the ASM d7 is a
       descending OST counter (127..0), so the byte added here is
       (NUM_OBJECTS-1) - index, matching ExecuteObjects' dbf loop. */
    uint8_t d7 = (uint8_t)(NUM_OBJECTS - 1 - Object_GetIndex(o));
    if (((v_vblank_byte + d7) & 3) != 0) {
        goto chkdel;
    }

    {
        int16_t dist, angle;
        ObjFloorDist(o, &dist, &angle);
        /* No floor yet: stub returns dist=0 (never '< 0'), so the bounce
           never happens until a real 16x16 index is loaded. Ported logic
           below is unreachable for now but kept verbatim. */
        if (dist >= 0) {
            goto chkdel;
        }
        obY(o) = (int16_t)(obY(o) + dist);
        int16_t d0 = obVelY(o);
        d0 = (int16_t)(d0 >> 2);             /* asr.w #2 */
        obVelY(o) = (int16_t)(obVelY(o) - d0);
        obVelY(o) = (int16_t)(-obVelY(o));
    }

chkdel:
    /* FixBugs=0: global timer decides deletion */
    if (v_ani3_time == 0) {
        RingLoss_Delete(o);
        return;
    }

    int16_t d0 = (int16_t)(v_limitbtm2 + 224);
    if ((uint16_t)obY(o) > (uint16_t)d0) {  /* cmp obY,d0 / blo: below bounds */
        RingLoss_Delete(o);
        return;
    }
    DisplaySprite(o);
}

static void RingLoss_Collect(uint8_t *o) {
    obRoutine(o) += 2;
    obColType(o)  = col_none;
    obPriority(o) = 1;
    CollectRing(o);
}

static void RingLoss_Sparkle(uint8_t *o) {
    AnimateSprite(o, Ani_Ring);
    DisplaySprite(o);
}

static void RingLoss_Delete(uint8_t *o) {
    DeleteObject(o);
}

static void RingLoss_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: RingLoss_Count(o); break;
        case 2: RingLoss_Bounce(o); break;
        case 4: RingLoss_Collect(o); break;
        case 6: RingLoss_Sparkle(o); break;
        case 8: RingLoss_Delete(o); break;
    }
}

/* ===========================================================================
   Object 0D — Signpost (end-of-level goal post)
   Ported verbatim from _incObj/0D Signpost.asm (REV01, FixBugs=0).
   =========================================================================== */

/* Signpost specific fields (spintime/sparkletime are words, sparkle_id byte) */
#define sign_spintime(o)     (*(int16_t *)((uint8_t *)(o) + 0x30))  /* objoff_30 */
#define sign_sparkletime(o)  (*(int16_t *)((uint8_t *)(o) + 0x32))  /* objoff_32 */
#define sign_sparkle_id(o)   (*(uint8_t *)((uint8_t *)(o) + 0x34))  /* objoff_34 */

/* Sign_SparkPos: byte pairs (x-pos, y-pos), addressed by even byte offsets */
static const int8_t Sign_SparkPos[16] = {
    -0x18, -0x10,  /* $0  */
     0x08,  0x08,  /* $2  */
    -0x10,  0x00,  /* $4  */
     0x18, -0x08,  /* $6  */
     0x00, -0x08,  /* $8  */
     0x10,  0x00,  /* $A  */
    -0x18,  0x08,  /* $C  */
     0x18,  0x10,  /* $E  */
};

/* TimeBonuses: word table (time in 15-second increments), NoTimeBonus last */
static const uint16_t Sign_TimeBonuses[21] = {
    5000, 5000, 1000, 500, 400, 400, 300, 300, 200, 200,
    200, 200, 100, 100, 100, 100, 50, 50, 50, 50,
    0,    /* NoTimeBonus: 5:00 onwards */
};

static void GotThroughAct(void);
static void Sign_LoadEndCards(uint8_t *o);

/* Sign_Touch (routine 2): wait for Sonic to walk into the signpost */
static void Sign_Touch(uint8_t *o) {
    uint8_t *player = RAM_ADDR(v_player);
    int16_t d0 = obX(player) - obX(o);           /* move.w (v_player+obX),d0; sub.w obX(a0),d0 */
    if (d0 < 0) return;                          /* blo.s .notouch (Sonic to the left) */
    if ((uint16_t)d0 >= 32u) return;             /* cmpi.w #32 / bhs.s .notouch */

    Sound_Queue(sfx_Signpost, false);            /* jsr (QueueSound1) */
    f_timecount = 0;                             /* clr.b (f_timecount).w */
    v_limitleft2 = v_limitright2;                /* move.w (v_limitright2),(v_limitleft2): lock screen */
    obRoutine(o) += 2;                           /* addq.b #2 -> Sign_Spin */
}

/* Sign_Spin (routine 4): spin cycles, then sparkles until Sonic runs off */
static void Sign_Spin(uint8_t *o) {
    sign_spintime(o) -= 1;                       /* subq.w #1,spintime(a0) */
    if (sign_spintime(o) >= 0) {                 /* bpl.s .chksparkle */
        goto chksparkle;
    }
    sign_spintime(o) = 60;                       /* move.w #60 (1 second cycle) */
    obAnim(o) += 1;                              /* addq.b #1,obAnim(a0) */
    if (obAnim(o) != 3) {                        /* cmpi.b #3 / bne.s .chksparkle */
        goto chksparkle;
    }
    obRoutine(o) += 2;                           /* addq.b #2 -> Sign_SonicRun */

chksparkle:
    sign_sparkletime(o) -= 1;                    /* subq.w #1,sparkletime(a0) */
    if (sign_sparkletime(o) >= 0) {              /* bpl.s .return */
        return;
    }
    sign_sparkletime(o) = 12 - 1;                /* move.w #12-1 */

    {
        int d0 = sign_sparkle_id(o);             /* moveq #0,d0; move.b sparkle_id,d0 */
        sign_sparkle_id(o) = (uint8_t)((sign_sparkle_id(o) + 2) & 0x0E); /* addq/andi.b #$E */
        const int8_t *sp = &Sign_SparkPos[d0 & 0x0F]; /* lea Sign_SparkPos(pc,d0.w),a2 */

        uint8_t *a1 = (uint8_t *)FindFreeObj();
        if (!a1) return;                         /* bne.s .return (object RAM full) */

        obID(a1)       = id_Rings;               /* _move.b #id_Rings,obID(a1) (sparkle effect) */
        obRoutine(a1)  = 6;                      /* move.b #6 -> Ring_Sparkle */
        obX(a1)        = (int16_t)(obX(o) + (int16_t)sp[0]); /* X-delta + signpost base X */
        obY(a1)        = (int16_t)(obY(o) + (int16_t)sp[1]); /* Y-delta + signpost base Y */
        obMap(a1)      = (uint32_t)(uintptr_t)Map_Ring;
        obGfx(a1)      = (uint16_t)(ArtTile_Ring | Tile_Pal2);
        obRender(a1)   = sprite_cam_field;
        obPriority(a1) = 2;
        obActWid(a1)   = 8;
    }
    return;
}

/* Sign_SonicRun (routine 6): lock controls and chase Sonic to the right edge */
static void Sign_SonicRun(uint8_t *o) {
    uint8_t *player = RAM_ADDR(v_player);

    if (v_debuguse) {                            /* tst.w (v_debuguse).w / bne.w Sign_Return */
        return;
    }

    /* FixBugs=0: lock controls when not airborne, regardless of player slot */
    if (!(obStatus(player) & (1 << 1))) {        /* btst #1 / bne.s .airborne */
        f_lockctrl = 1;                          /* move.b #1 */
        v_jpadhold2 = btnR;                      /* move.w #btnR<<8: stores to the F602 byte = btnR */
    }

    if (obID(player) == 0) {                     /* tst.b (v_player+obID) / beq.s Sign_LoadEndCards */
        Sign_LoadEndCards(o);
        return;
    }

    int16_t d0 = obX(player);                    /* move.w (v_player+obX).w,d0 */
    int16_t d1 = (int16_t)v_limitright2 + (320 - 24); /* addi.w #320-24 */
    if ((uint16_t)d0 < (uint16_t)d1) {           /* cmp.w d1,d0 / blo.s Sign_Return */
        return;
    }
    Sign_LoadEndCards(o);
}

/* Sign_LoadEndCards — advance routine and queue the act-tally (once) */
static void Sign_LoadEndCards(uint8_t *o) {
    obRoutine(o) += 2;                           /* addq.b #2 -> Sign_Exit */
    GotThroughAct();
}

/* GotThroughAct — set up the score bonuses at the end of an act */
static void GotThroughAct(void) {
    if (RAM_BYTE(v_endcard)) {                   /* tst.b (v_endcard).w / bne.s Sign_Return */
        return;
    }

    v_limitleft2 = v_limitright2;                /* lock left boundary to right */
    v_invinc     = 0;                            /* clr.b (v_invinc).w */
    f_timecount  = 0;                            /* clr.b (f_timecount).w */
    RAM_BYTE(v_endcard) = id_GotThroughCard;     /* load end card object (prevents re-run) */
    NewPLC(plcid_TitleCard);                     /* jsr (NewPLC).l */
    f_endactbonus = 1;                           /* move.b #1 (pre-tally bonus HUD) */

    /* Time bonus: v_timemin*60 + v_timesec, divided by 15 seconds per entry */
    {
        int d0 = v_timemin * 60 + v_timesec;     /* move.b (v_timemin)+v_timesec, mulu.w #60 */
        d0 /= 15;                                /* divu.w #15 */
        int d1 = 20;                             /* moveq #(NoTimeBonus-TimeBonuses)/2,d1 */
        if (d0 >= d1) {                          /* cmp.w d1,d0 / blo.s .getTimeBonus */
            d0 = d1;                             /* cap to last (0 points) entry */
        }
        v_timebonus = Sign_TimeBonuses[d0];      /* move.w TimeBonuses(pc,d0.w),(v_timebonus).w */
    }

    v_ringbonus = (uint16_t)(v_rings * 10);      /* mulu.w #10 */

    Sound_Queue(bgm_GotThrough, false);          /* jsr (QueueSound2) */
}

/* Signpost dispatcher + per-frame common code */
static void Signpost_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0: /* Sign_Main */ {
            obRoutine(o) += 2;                   /* addq.b #2 -> Sign_Touch */
            obMap(o)   = (uint32_t)(uintptr_t)Map_Sign;
            obGfx(o)   = (uint16_t)ArtTile_Signpost;
            obRender(o)= sprite_cam_field;
            obActWid(o)= 48 / 2;
            obPriority(o)= 4;
            /* fall through into Sign_Touch */
        }
        /* fallthrough */
        case 2: Sign_Touch(o); break;
        case 4: Sign_Spin(o); break;
        case 6: Sign_SonicRun(o); break;
        case 8: /* Sign_Exit: nothing */ break;
    }

    if (Ani_Sign) {
        AnimateSprite(obj, Ani_Sign);            /* lea (Ani_Sign).l,a1 / bsr AnimateSprite */
    }
    DisplaySprite(obj);                          /* bsr.w DisplaySprite (FixBugs=0: before out_of_range) */
    if (OutOfRange(o, -1)) {                     /* out_of_range.w DeleteObject */
        DeleteObject(o);
    }
}

/* ===========================================================================
   Object 3A — "SONIC HAS PASSED" title card
   Ported verbatim from _incObj/3A Got Through Card.asm (REV01, FixBugs=0).
   The 7 card elements live back-to-back in RAM starting at v_endcard; slot 0
   is the controller (routine 0 sets up all seven, each element then runs its
   own routine independently).
   =========================================================================== */

/* AddPoints — _incObj/sub AddPoints.asm (REV01).
   Input: d0 = points to add / 10 (HUD score shows a trailing fake 0).
   Awards an extra life every 50000 points, Japan region only. */
void AddPoints(int32_t d0) {
    f_scorecount = 1;                            /* move.b #1,(f_scorecount).w */

    v_score += (uint32_t)d0;                     /* add.l d0,(v_score).w */
    if (v_score > 999999) {                      /* move.l #999999,d1 / cmp.l / bhi.s .belowmax */
        v_score = 999999;                        /* cap to 9999990 displayed */
    }

    if ((uint32_t)v_score < (uint32_t)v_scorelife) {
        return;                                  /* cmp.l (v_scorelife).w,d0 / blo.s .return */
    }

    v_scorelife += 5000;                         /* addi.l #5000,(v_scorelife).w */

    if (!(v_megadrive & 0x80)) {                 /* tst.b (v_megadrive).w / bmi.s .return (bit7=overseas) */
        v_lives = v_lives + 1;                   /* addq.b #1,(v_lives).w */
        f_lifecount = f_lifecount + 1;           /* addq.b #1,(f_lifecount).w */
        Sound_Queue(bgm_ExtraLife, false);       /* jmp (QueueSound1).l */
    }
}

/* Got_ItemData: per element - start X, main X (target), Y, routine, frame */
struct GotItem {
    int16_t start_x;
    int16_t main_x;
    int16_t y;
    uint8_t routine;
    uint8_t frame;
};
static const struct GotItem Got_ItemData[7] = {
    /* "SONIC HAS" */
    {  0x004,  0x124,  0xBC, 2, 0 },
    /* "PASSED"  */
    { -0x120,  0x120,  0xD0, 2, 1 },
    /* "ACT 1/2/3" (dynamic frame: + v_act) */
    {  0x40C,  0x14C,  0xD6, 2, 6 },
    /* Score tally */
    {  0x520,  0x120,  0xEC, 2, 2 },
    /* Time Bonus tally */
    {  0x540,  0x120,  0xFC, 2, 3 },
    /* Ring Bonus tally (controller element) */
    {  0x560,  0x120, 0x10C, 2, 4 },
    /* Blue oval */
    {  0x20C,  0x14C,  0xCC, 2, 5 },
};

/* LevelOrder — _inc/LevelOrder.asm (word table, indexed by
   (v_zone&7)*8 + (v_act&3)*2).  A "0" entry sends the game to the Sega
   screen (see Got_NextLevel). */
static const uint16_t LevelOrder[24] = {
    /* GHZ */      id_GHZ_act2, id_GHZ_act3, id_MZ_act1, 0,
    /* LZ */       id_LZ_act2,  id_LZ_act3,  id_SLZ_act1, id_FZ,
    /* MZ */       id_MZ_act2,  id_MZ_act3,  id_SYZ_act1, 0,
    /* SLZ */      id_SLZ_act2, id_SLZ_act3, id_SBZ_act1, 0,
    /* SYZ */      id_SYZ_act2, id_SYZ_act3, id_LZ_act1,  0,
    /* SBZ */      id_SBZ_act2, id_LZ_act4,  0,           0,
};

static void Got_MoveIn(uint8_t *o);
static void Got_Wait(uint8_t *o);
static void Got_SBZ2_MoveOut(uint8_t *o);

/* Got_ChkPLC (routine 0): wait for the PLC queue to empty, then set up all
   seven card elements in the v_endcard..v_endcardoval slots. */
static void Got_Main(uint8_t *o) {
    uint8_t *a1 = o;

    for (int i = 0; i < 7; i++) {
        const struct GotItem *it = &Got_ItemData[i];
        obID(a1)      = id_GotThroughCard;     /* load next element */
        obX(a1)       = it->start_x;
        got_finalX(a1)= it->start_x;           /* finish X (same as start) */
        got_mainX(a1) = it->main_x;
        obScreenY(a1) = it->y;
        obRoutine(a1) = it->routine;

        int d0 = it->frame;
        if (d0 == 6) {                          /* cmpi.b #6,d0 / the act element */
            d0 += v_act;                        /* add.b (v_act).w,d0 */
        }
        obFrame(a1) = (uint8_t)d0;

        obMap(a1)     = (uint32_t)(uintptr_t)Map_Got;
        obGfx(a1)     = (uint16_t)(ArtTile_Title_Card | Tile_Prio);
        obRender(a1)  = sprite_cam_screen;
        a1 += object_size;                      /* lea object_size(a1),a1 */
    }
}

static void Got_ChkPLC(uint8_t *o) {
    if (PLC_IsEmpty()) {                        /* tst.l (v_plc_buffer).w / beq.s Got_Main */
        Got_Main(o);
    }
}

/* Got_MoveIn / Got_MoveIn .checkOffScreen (routine 2):
   Slide each element toward its main X at 0x10 px/frame; suppress display
   while off-screen (X outside [0, 0x200), FixBugs=0 loses the left bound). */
static void Got_MoveIn(uint8_t *o) {
    int16_t d1 = 0x10;
    int16_t d0 = got_mainX(o);

    if (d0 == obX(o)) {
        /* .reachedXTarget */
        if (obRoutine((uint8_t *)RAM_ADDR(v_endcardring)) == 0xE) {
            /* .startSBZ2Cutscene: post-SBZ2 cutscene is in progress */
            obRoutine(o) = 0xE;                  /* move.b #$E,obRoutine(a0) */
            Got_SBZ2_MoveOut(o);                 /* bra.w Got_SBZ2_MoveOut */
            return;
        }
        if (obFrame(o) == 4) {                   /* cmpi.b #4,obFrame(a0) */
            obRoutine(o) += 2;                   /* addq.b #2 -> Got_Wait */
            got_timeframe(o) = 3 * 60;           /* 3 second delay before tally */
        }
    } else {
        if (d0 < obX(o)) {                       /* bge.s .updateXPos — negate when coming from the right */
            d1 = -d1;
        }
        obX(o) += d1;                            /* add.w d1,obX(a0) */
    }

    /* .checkOffScreen */
    d0 = obX(o);
    if (d0 < 0) return;                          /* bmi.s .return */
    if ((uint16_t)d0 >= 0x80 + 320 + 64) return; /* bhs.s .return (FixBugs=0) */
    DisplaySprite(o);                            /* bra.w DisplaySprite */
}

/* Got_Wait (routines 4, 8, $C): fixed delay then advance */
static void Got_Wait(uint8_t *o) {
    got_timeframe(o) -= 1;                       /* subq.w #1,obTimeFrame(a0) */
    if (got_timeframe(o) != 0) {                 /* bne.s .display */
        DisplaySprite(o);
        return;
    }
    obRoutine(o) += 2;                           /* addq.b #2,obRoutine(a0) */
    DisplaySprite(o);                            /* bra.w DisplaySprite */
}

/* Got_Bonus (routine 6): tick time/ring bonuses down to the score */
static void Got_Bonus(uint8_t *o) {
    DisplaySprite(o);                            /* bsr.w DisplaySprite */
    f_endactbonus = 1;                           /* move.b #1,(f_endactbonus).w keep tally HUD updating */

    int d0 = 0;
    if (v_timebonus != 0) {                      /* tst.w (v_timebonus).w / beq.s .ringBonus */
        d0 += 10;                                /* addi.w #10,d0 */
        v_timebonus -= 10;                       /* subi.w #10,(v_timebonus).w */
    }
    if (v_ringbonus != 0) {                      /* tst.w (v_ringbonus).w / beq.s .checkFinished */
        d0 += 10;
        v_ringbonus -= 10;
    }

    if (d0 != 0) {
        /* .addBonusPoints */
        AddPoints(d0);                           /* jsr (AddPoints).l */
        if ((v_vblank_byte & 3) == 0) {          /* move.b (v_vblank_byte).w,d0 / andi.b #3,d0 / bne.s .return */
            Sound_Queue(sfx_Switch, false);      /* moveq #sfx_Switch,d0 / jmp (QueueSound2).l */
        }
        return;
    }

    /* .finished */
    Sound_Queue(sfx_Cash, false);                /* move.w #sfx_Cash,d0 / jsr (QueueSound2).l */
    obRoutine(o) += 2;                           /* addq.b #2 -> Got_Wait (8) */
    if (RAM_U16(0xFE10) == id_SBZ_act2) {        /* cmpi.w #id_SBZ_act2,(v_zone_act).w / bne.s .setPostDelay */
        obRoutine(o) += 4;                       /* addq.b #4 -> Got_Wait ($C, pre-SBZ2 cutscene) */
    }
    got_timeframe(o) = 3 * 60;                   /* move.w #3*60,obTimeFrame(a0) */
}

/* Got_NextLevel (routine $A): advance to the next zone/act */
static void Got_NextLevel(uint8_t *o) {
    int d0 = (v_zone & 7) * 4 + (v_act & 3);     /* andi #7 / lsl #3 + andi #3 / add (word index) */
    uint16_t nl = LevelOrder[d0];                /* move.w LevelOrder(pc,d0.w),d0 */
    RAM_SET_U16(0xFE10, nl);                     /* move.w d0,(v_zone_act).w */

    if (nl == 0) {                               /* tst.w d0 / bne.s .validLevelNumber */
        v_gamemode = 0x00;                       /* move.b #id_Sega,(v_gamemode).w */
        DisplaySprite(o);                        /* bra.s .display */
        return;
    }

    /* .validLevelNumber */
    RAM_BYTE(v_lastlamp) = 0;                    /* clr.b (v_lastlamp).w */
    if (!f_bigring) {                            /* tst.b (f_bigring).w / beq.s .restartLevel */
        f_restart = 1;                           /* move.w #1,(f_restart).w */
    } else {
        v_gamemode = 0x10;                       /* move.b #id_Special,(v_gamemode).w */
    }
    DisplaySprite(o);                            /* bra.w DisplaySprite */
}

/* Got_SBZ2_MoveOut (routine $E): slide cards off at 0x20 px/frame, then
   trigger the SBZ2->FZ cutscene (ring bonus element controls it). */
static void Got_SBZ2_MoveOut(uint8_t *o) {
    int16_t d1 = 2 * 0x10;
    int16_t d0 = got_finalX(o);

    if (d0 == obX(o)) {
        /* Got_SBZ2_StartCutscene */
        if (obFrame(o) != 4) {                   /* cmpi.b #4,obFrame(a0) / bne.w DeleteObject */
            DeleteObject(o);
            return;
        }
        obRoutine(o) += 2;                       /* addq.b #2 -> Got_SBZ2_Boundary */
        f_lockctrl = 0;                          /* clr.b (f_lockctrl).w */
        Sound_Queue(bgm_FZ, false);              /* move.w #bgm_FZ,d0 / jmp (QueueSound1).l */
        return;
    }

    if (d0 < obX(o)) {                           /* bge.s .updateXPos */
        d1 = -d1;
    }
    obX(o) += d1;

    /* .checkOffScreen */
    d0 = obX(o);
    if (d0 < 0) return;                          /* bmi.s .return */
    if ((uint16_t)d0 >= 0x80 + 320 + 64) return; /* bhs.s .return (FixBugs=0) */
    DisplaySprite(o);
}

/* Got_SBZ2_Boundary (routine $10): push the right screen boundary forward */
static void Got_SBZ2_Boundary(uint8_t *o) {
    v_limitright2 += 2;                          /* addq.w #2,(v_limitright2).w */
    if (v_limitright2 == (uint16_t)(boss_sbz2_x + 0xB0)) { /* cmpi.w #boss_sbz2_x+$B0 / beq.w DeleteObject */
        DeleteObject(o);
    }
}

static void GotThroughCard_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00: Got_ChkPLC(o); break;
        case 0x02: Got_MoveIn(o); break;
        case 0x04:
        case 0x08:
        case 0x0C: Got_Wait(o); break;
        case 0x06: Got_Bonus(o); break;
        case 0x0A: Got_NextLevel(o); break;
        case 0x0E: Got_SBZ2_MoveOut(o); break;
        case 0x10: Got_SBZ2_Boundary(o); break;
    }
}

/* ===========================================================================
    Sonic animation IDs and frame constants
    =========================================================================== */

#define id_Walk        0x00
#define id_Run         0x01
#define id_Roll        0x02
#define id_Roll2       0x03
#define id_Push        0x04
#define id_Wait        0x05
#define id_Balance     0x06
#define id_LookUp      0x07
#define id_Duck        0x08
#define id_Warp1       0x09
#define id_Warp2       0x0A
#define id_Warp3       0x0B
#define id_Warp4       0x0C
#define id_Stop        0x0D
#define id_Float1      0x0E
#define id_Float2      0x0F
#define id_Spring      0x10
#define id_Hang        0x11
#define id_Leap1       0x12
#define id_Leap2       0x13
#define id_Surf        0x14
#define id_GetAir      0x15
#define id_Burnt       0x16
#define id_Drown       0x17
#define id_Death       0x18
#define id_Shrink      0x19
#define id_Hurt        0x1A
#define id_Slide       0x1B
#define id_Null        0x1C
#define id_Float3      0x1D
#define id_Float4      0x1E

#define fr_Null        0x00
#define fr_Stand       0x01
#define fr_Wait1       0x02
#define fr_Wait2       0x03
#define fr_Wait3       0x04
#define fr_LookUp      0x05
#define fr_Walk11      0x06
#define fr_Walk12      0x07
#define fr_Walk13      0x08
#define fr_Walk14      0x09
#define fr_Walk15      0x0A
#define fr_Walk16      0x0B
#define fr_Walk21      0x0C
#define fr_Walk22      0x0D
#define fr_Walk23      0x0E
#define fr_Walk24      0x0F
#define fr_Walk25      0x10
#define fr_Walk26      0x11
#define fr_Walk31      0x12
#define fr_Walk32      0x13
#define fr_Walk33      0x14
#define fr_Walk34      0x15
#define fr_Walk35      0x16
#define fr_Walk36      0x17
#define fr_Walk41      0x18
#define fr_Walk42      0x19
#define fr_Walk43      0x1A
#define fr_Walk44      0x1B
#define fr_Walk45      0x1C
#define fr_Walk46      0x1D
#define fr_Run11       0x1E
#define fr_Run12       0x1F
#define fr_Run13       0x20
#define fr_Run14       0x21
#define fr_Run21       0x22
#define fr_Run22       0x23
#define fr_Run23       0x24
#define fr_Run24       0x25
#define fr_Run31       0x26
#define fr_Run32       0x27
#define fr_Run33       0x28
#define fr_Run34       0x29
#define fr_Run41       0x2A
#define fr_Run42       0x2B
#define fr_Run43       0x2C
#define fr_Run44       0x2D
#define fr_Roll1       0x2E
#define fr_Roll2       0x2F
#define fr_Roll3       0x30
#define fr_Roll4       0x31
#define fr_Roll5       0x32
#define fr_Warp1       0x33
#define fr_Warp2       0x34
#define fr_Warp3       0x35
#define fr_Warp4       0x36
#define fr_Stop1       0x37
#define fr_Stop2       0x38
#define fr_Duck        0x39
#define fr_Balance1    0x3A
#define fr_Balance2    0x3B
#define fr_Float1      0x3C
#define fr_Float2      0x3D
#define fr_Float3      0x3E
#define fr_Float4      0x3F
#define fr_Spring      0x40
#define fr_Hang1       0x41
#define fr_Hang2       0x42
#define fr_Leap1       0x43
#define fr_Leap2       0x44
#define fr_Push1       0x45
#define fr_Push2       0x46
#define fr_Push3       0x47
#define fr_Push4       0x48
#define fr_Surf        0x49
#define fr_BubStand    0x4A
#define fr_Burnt       0x4B
#define fr_Drown       0x4C
#define fr_Death       0x4D
#define fr_Shrink1     0x4E
#define fr_Shrink2     0x4F
#define fr_Shrink3     0x50
#define fr_Shrink4     0x51
#define fr_Shrink5     0x52
#define fr_Float5      0x53
#define fr_Float6      0x54
#define fr_Injury      0x55
#define fr_GetAir      0x56
#define fr_Slide       0x57

#define afEnd          0xFF
#define afBack         0xFE
#define afChange       0xFD

/* ===========================================================================
     Sonic animation script stubs (loaded from data.c)
     =========================================================================== */

extern const uint8_t *Ani_Sonic;
extern const uint8_t *SonicDynPLC;
extern const uint8_t *Art_Sonic;

static void Sonic_Move(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (f_slidemode) {
        Sonic_AngleSpeed(o);
        return;
    }
    if (locktime(o)) {
        Sonic_ResetScr(o);
        return;
    }
    if (v_jpadhold2 & btnL) {
        Sonic_MoveLeft(o);
    }
    if (v_jpadhold2 & btnR) {
        Sonic_MoveRight(o);
    }
    {
        uint8_t d0 = obAngle(o);
        d0 += 0x20;
        d0 &= 0xC0;
        if (d0) {
            Sonic_ResetScr(o);
            return;
        }
        if (obInertia(o)) {
            Sonic_ResetScr(o);
            return;
        }
        obStatus(o) &= ~(1 << 5);
        obAnim(o) = id_Wait;
        if (!(obStatus(o) & (1 << 3))) {
            goto chkbalance;
        }
        {
            uint8_t d0 = standonobject(o);
            uint8_t *a1 = (uint8_t *)Object_GetSlot(d0);
            if (a1 >= ObjRAM && a1 < ObjRAM + NUM_OBJECTS * OBJECT_SIZE) {
                if (obStatus(a1) < 0x80) {
                    int16_t d1 = obActWid(a1);
                    int16_t d2 = d1 + d1 - 4;
                    int16_t d1x = obX(o) + d1 - obX(a1);
                    if (d1x < 4) {
                        goto leftbalance;
                    }
                    if (d1x >= d2) {
                        goto rightbalance;
                    }
                    Sonic_LookUp(o);
                    return;
                } else {
                    Sonic_LookUp(o);
                    return;
                }
            } else {
                Sonic_LookUp(o);
                return;
            }
        }
    }
    return;

chkbalance:
    {
        int16_t dist, angle;
        ObjFloorDist(o, &dist, &angle);
        if (dist < 12) {
            Sonic_LookUp(o);
            return;
        }
        if (angleright(o) == 3) {
            goto rightbalance;
        }
        if (angleleft(o) != 3) {
            Sonic_LookUp(o);
            return;
        }
    }

leftbalance:
    obStatus(o) |= (1 << 0);
    goto balance;

rightbalance:
    obStatus(o) &= ~(1 << 0);

balance:
    obAnim(o) = id_Balance;
    Sonic_ResetScr(o);
}

static void Sonic_MdNormal(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    if (Sonic_Jump(o)) {
        return; /* addq.l #4,sp — a successful jump skips the rest of this mode */
    }
    Sonic_SlopeResistWalk(o);
    Sonic_Move(o);
    Sonic_Roll(o);
    Sonic_LevelBound(o);
    SpeedToPos(o);
    Sonic_AnglePos(o);
    Sonic_SlopeRepel(o);
}

static void Sonic_MdJump(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    Sonic_JumpHeight(o);
    Sonic_JumpDirection(o);
    Sonic_LevelBound(o);
    ObjectFall(o);
    if (obStatus(o) & (1 << 6)) {
        obVelY(o) = (int16_t)(obVelY(o) - (gravity - 0x10));
    }
    Sonic_JumpAngle(o);
    Sonic_Floor(o);
}

static void Sonic_MdRoll(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    if (Sonic_Jump(o)) {
        return; /* addq.l #4,sp — a successful jump skips the rest of this mode */
    }
    Sonic_SlopeResistRoll(o);
    Sonic_RollSpeed(o);
    Sonic_LevelBound(o);
    SpeedToPos(o);
    Sonic_AnglePos(o);
    Sonic_SlopeRepel(o);
}

static void Sonic_MdJump2(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    Sonic_JumpHeight(o);
    Sonic_JumpDirection(o);
    Sonic_LevelBound(o);
    ObjectFall(o);
    if (obStatus(o) & (1 << 6)) {
        obVelY(o) = (int16_t)(obVelY(o) - (gravity - 0x10));
    }
    Sonic_JumpAngle(o);
    Sonic_Floor(o);
}

static void Sonic_MoveLeft(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0 = obInertia(o);
    int16_t d5 = v_sonspeedacc;
    int16_t d6 = v_sonspeedmax;

    if (d0 > 0) {
        /* .changeddirection */
        d0 = d0 - v_sonspeeddec;
        if (d0 < 0) {
            d0 = -0x80;
        }
        obInertia(o) = d0;
        {
            uint8_t d1 = obAngle(o);
            d1 += 0x20;
            d1 &= 0xC0;
            if (d1) {
                goto nostopping;
            }
            if (d0 < 0x400) {
                goto nostopping;
            }
            obAnim(o) = id_Stop;
            obStatus(o) &= ~(1 << 0);
            Sound_Queue(sfx_Skid, false);
        }
        goto nostopping;
    }

    /* .still: bset #0,obStatus → Z = valor ANTERIOR de bit 0.
     *      bne .alreadyleft  ⇔  ya estaba flipeado antes. */
    {
        uint8_t wasFlipped = obStatus(o) & (1 << 0);
        obStatus(o) |= (1 << 0);
        if (!wasFlipped) {
            obStatus(o) &= ~(1 << 5);
            obPrevAni(o) = id_Run;
        }
    }

    d0 = d0 - d5;
    {
        int16_t d1 = -d6;
        /* bgt.s .nocap: salta el cap si d0 > -max.
         *          Es decir, cap sólo cuando d0 <= -max. */
        if (d0 <= d1) {
            d0 = d1;
        }
    }
    obInertia(o) = d0;
    obAnim(o) = id_Walk;

    nostopping:
    return;
}

static void Sonic_MoveRight(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0 = obInertia(o);
    int16_t d5 = v_sonspeedacc;
    int16_t d6 = v_sonspeedmax;

    if (d0 < 0) {
        /* .changedirection */
        d0 = d0 + v_sonspeeddec;
        if (d0 >= 0) {
            d0 = 0x80;
        }
        obInertia(o) = d0;
        {
            uint8_t d1 = obAngle(o);
            d1 += 0x20;
            d1 &= 0xC0;
            if (d1) {
                goto nostopping;
            }
            if (d0 > -0x400) {
                goto nostopping;
            }
            obAnim(o) = id_Stop;
            obStatus(o) |= (1 << 0);
            Sound_Queue(sfx_Skid, false);
        }
        goto nostopping;
    }

    /* .alreadyright: bclr #0,obStatus → Z = valor ANTERIOR de bit 0.
     *      beq .alreadyright ⇔ ya estaba a 0 (ya miraba a la derecha). */
    {
        uint8_t wasFlipped = obStatus(o) & (1 << 0);
        obStatus(o) &= ~(1 << 0);
        if (wasFlipped) {
            obStatus(o) &= ~(1 << 5);
            obPrevAni(o) = id_Run;
        }
    }

    d0 = d0 + d5;
    if (d0 > d6) {
        d0 = d6;
    }
    obInertia(o) = d0;
    obAnim(o) = id_Walk;

    nostopping:
    return;
}

static void Sonic_RollSpeed(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d5 = v_sonspeedacc / 2;

    if (f_slidemode) {
        Sonic_AngledRollSpeed(o);
        return;
    }

    /* tst.w locktime; bne.s .notright → salta input L/R, NO el slowdown */
    if (!locktime(o)) {
        if (v_jpadhold2 & btnL) {
            Sonic_RollLeft(o);
        }
        if (v_jpadhold2 & btnR) {
            Sonic_RollRight(o);
        }
    }

    /* .notright */
    {
        int16_t d0 = obInertia(o);
        if (d0 == 0) {
            goto Sonic_RollSlowdownDone;
        }
        if (d0 < 0) {
            d0 = d0 + d5;
            if (d0 < 0) {
                obInertia(o) = d0;
            } else {
                obInertia(o) = 0;
            }
        } else {
            d0 = d0 - d5;
            if (d0 > 0) {
                obInertia(o) = d0;
            } else {
                obInertia(o) = 0;
            }
        }
    }

Sonic_RollSlowdownDone:
    if (obInertia(o) == 0) {
        obStatus(o) &= ~(1 << 2);
        obHeight(o) = sonic_height;
        obWidth(o) = sonic_width;
        obAnim(o) = id_Wait;
        obY(o) = (int16_t)(obY(o) - (sonic_height - sonic_roll_height));
    }
    Sonic_AngledRollSpeed(o);
}

static void Sonic_AngledRollSpeed(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0, d1;
    CalcSine(obAngle(o), &d0, &d1);
    d0 = (int16_t)(((int32_t)d0 * obInertia(o)) >> 8);
    d1 = (int16_t)(((int32_t)d1 * obInertia(o)) >> 8);
    if (d0 > 0x1000) d0 = 0x1000;
    if (d0 < -0x1000) d0 = -0x1000;
    if (d1 > 0x1000) d1 = 0x1000;
    if (d1 < -0x1000) d1 = -0x1000;
    obVelY(o) = d0;
    obVelX(o) = d1;
    Sonic_WallSpeedAdjust(o);
}

static void Sonic_RollLeft(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0 = obInertia(o);
    int16_t d4 = v_sonspeeddec / 4;   /* asr.w #2 en Sonic_RollSpeed */

    /* beq.s .still ; bpl.s .changeddirection ; fall-through a .still */
    if (d0 <= 0) {
        /* .still */
        obStatus(o) |= (1 << 0);
        obAnim(o) = id_Roll;
        return;
    }

    /* .changeddirection */
    d0 = d0 - d4;
    if (d0 < 0) {
        d0 = -0x80;
    }
    obInertia(o) = d0;
}

static void Sonic_RollRight(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0 = obInertia(o);
    int16_t d4 = v_sonspeeddec / 4;

    if (d0 < 0) {
        /* .changedirection */
        d0 = d0 + d4;
        if (d0 >= 0) {
            d0 = 0x80;
        }
        obInertia(o) = d0;
        return;
    }

    /* bclr #0,obStatus ; anim = Roll
     *      (aquí no se comprueba el bit anterior: el ASM no tiene un beq/bne) */
    obStatus(o) &= ~(1 << 0);
    obAnim(o) = id_Roll;
}

static void Sonic_Roll(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (f_slidemode) {
        return;
    }
    {
        int16_t d0 = obInertia(o);
        if (d0 < 0) {
            d0 = -d0;
        }
        if (d0 < 0x80) {
            return;
        }
        if (v_jpadhold2 & (btnL | btnR)) {
            return;
        }
        if (!(v_jpadhold2 & btnDn)) {
            return;
        }
    }
    Sonic_ChkRoll(o);
}

static void Sonic_ChkRoll(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (obStatus(o) & (1 << 2)) {
        return;
    }
    obStatus(o) |= (1 << 2);
    obHeight(o) = sonic_roll_height;
    obWidth(o) = sonic_roll_width;
    obAnim(o) = id_Roll;
    obY(o) = (int16_t)(obY(o) + (sonic_height - sonic_roll_height));
    Sound_Queue(sfx_Roll, false);
    printf("Roll: hold2=%02X btnLR=%02X btnDn=%02X\n",
           v_jpadhold2, btnL|btnR, btnDn);
    if (obInertia(o) == 0) {
        obInertia(o) = 0x200;
    }
}

static int Sonic_Jump(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (!(v_jpadpress2 & btnABC)) {
        return 0;
    }
    {
        uint8_t d0 = obAngle(o);
        d0 += 0x80;
        int16_t headroom = Sonic_CalcHeadroom(o, d0);
        if (headroom < 6) {
            return 0;
        }
    }
    {
        int16_t d2 = son_jumpspeed;
        if (obStatus(o) & (1 << 6)) {
            d2 = son_jumpspeed - 0x300;
        }
        uint8_t d0 = obAngle(o);
        d0 -= 0x40;
        {
            int16_t s0, s1;
            CalcSine(d0, &s0, &s1);
            obVelX(o) = (int16_t)(obVelX(o) + ((int16_t)(((int32_t)d2 * s1) >> 8)));
            obVelY(o) = (int16_t)(obVelY(o) + ((int16_t)(((int32_t)d2 * s0) >> 8)));
        }
    }
    obStatus(o) |= (1 << 1);
    obStatus(o) &= ~(1 << 5);
    jumping(o) = 1;
    sticktoconvex(o) = 0;
    Sound_Queue(sfx_Jump, false);
    /* FixBugs=0: Sonic's hitbox is set to standing size when roll-jumping.
       Leftover from the victory animation in prototypes. */
    obHeight(o) = sonic_height;
    obWidth(o) = sonic_width;
    if (!(obStatus(o) & (1 << 2))) {
        obHeight(o) = sonic_roll_height;
        obWidth(o) = sonic_roll_width;
        obAnim(o) = id_Roll;
        obStatus(o) |= (1 << 2);
        obY(o) = (int16_t)(obY(o) + (sonic_height - sonic_roll_height));
    } else {
        obStatus(o) |= (1 << 4);   /* roll-jump: set Roll-Jump flag */
    }
    return 1;
}

static void Sonic_JumpHeight(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (jumping(o)) {
        int16_t d1 = -0x400;
        if (obStatus(o) & (1 << 6)) {
            d1 = -0x200;
        }
        if (obVelY(o) >= d1) {
            return;
        }
        if (!(v_jpadhold2 & btnABC)) {
            obVelY(o) = d1;
        }
        return;
    }
    if (obVelY(o) < -0xFC0) {
        obVelY(o) = -0xFC0;
    }
}

static void Sonic_JumpDirection(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d6 = v_sonspeedmax;
    int16_t d5 = v_sonspeedacc * 2;

    if (obStatus(o) & (1 << 4)) {
        Sonic_RollJumpLock(o);
        return;
    }
    {
        int16_t d0 = obVelX(o);
        if (v_jpadhold2 & btnL) {
            obStatus(o) |= (1 << 0);
            d0 = d0 - d5;
            {
                int16_t d1 = -d6;
                if (d0 <= d1) {
                    d0 = d1;
                }
            }
        }
        if (v_jpadhold2 & btnR) {
            obStatus(o) &= ~(1 << 0);
            d0 = d0 + d5;
            if (d0 >= d6) {
                d0 = d6;
            }
        }
        obVelX(o) = d0;
    }
    Sonic_RollJumpLock(o);
}

static void Sonic_RollJumpLock(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (v_lookshift == 0x60) {
        goto Sonic_AirDrag;
    }
    if (v_lookshift < 0x60) {
        v_lookshift = v_lookshift + 2;
    } else {
        v_lookshift = v_lookshift - 2;
    }

Sonic_AirDrag:
    if (obVelY(o) < -0x400) {
        return;
    }
    {
        int16_t d0 = obVelX(o);
        int16_t d1 = d0;
        d1 = d1 >> 5;
        if (d1 == 0) {
            return;
        }
        if (d0 < 0) {
            d0 = d0 - d1;
            if (d0 < 0) {
                obVelX(o) = d0;
            } else {
                obVelX(o) = 0;
            }
        } else {
            d0 = d0 - d1;
            if (d0 > 0) {
                obVelX(o) = d0;
            } else {
                obVelX(o) = 0;
            }
        }
    }
}

static void Sonic_LevelBound(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int32_t d1 = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o); /* move.l obX(a0),d1 */
    int16_t d0 = obVelX(o);
    d1 += ((int32_t)d0) << 8;                      /* ext.l; asl.l #8; add.l d0,d1 */
    d1 = (int32_t)(int16_t)((uint32_t)d1 >> 16);   /* swap d1: integer X in low word */

    d0 = v_limitleft2 + 16;                        /* bhi.s .sides */
    if ((uint16_t)d0 > (uint16_t)(int16_t)d1) {
        goto sides;
    }

    d0 = v_limitright2 + (320 - 24);
    if (!f_lockscreen) {
        d0 = d0 + 64;
    }
    if ((uint16_t)(int16_t)d1 >= (uint16_t)d0) {   /* bls.s .sides */
        goto sides;
    }

chkbottom:
    d0 = v_limitbtm2 + 224;                        /* FixBugs=0: no boundary override */
    if ((int16_t)obY(o) > d0) {                    /* blt.s .bottom */
        goto bottom;
    }
    return;

bottom:
    if (RAM_U16(0xFE10) == id_SBZ_act2) {
        if ((int16_t)obX(o) >= 0x2000) {
            RAM_BYTE(v_lastlamp) = 0;
            f_restart = 1;
            RAM_SET_U16(0xFE10, id_LZ_act4);
            return;
        }
    }
    KillSonic(o, NULL);
    return;

sides:
    obX(o) = d0;                                   /* move.w d0,obX(a0) */
    obSubpixelX(o) = 0;
    obVelX(o) = 0;
    obInertia(o) = 0;
    goto chkbottom;
}

static void Sonic_SlopeResistWalk(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t d0 = obAngle(o);
    d0 += 0x60;
    if (d0 >= 0xC0) {
        return;
    }
    {
        int16_t s0, s1;
        CalcSine(obAngle(o), &s0, &s1);
        /* ASM: muls.w #$20,d0 sobre d0 = SENO (s0), no sobre el coseno. */
        int16_t d0 = (int16_t)(((int32_t)s0 * 0x20) >> 8);
        if (obInertia(o) == 0) {
            return;
        }
        if (obInertia(o) < 0) {
            obInertia(o) = obInertia(o) + d0;
        } else if (d0 != 0) {
            obInertia(o) = obInertia(o) + d0;
        }
    }
}

static void Sonic_SlopeResistRoll(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t d0 = obAngle(o);
    d0 += 0x60;
    if (d0 >= 0xC0) {
        return;
    }
    {
        int16_t s0, s1;
        CalcSine(obAngle(o), &s0, &s1);
        /* ASM: muls.w #$50,d0 sobre d0 = SENO (s0). */
        int16_t d0 = (int16_t)(((int32_t)s0 * 0x50) >> 8);
        if (obInertia(o) < 0) {
            if (d0 >= 0) {
                d0 = d0 >> 2;
            }
            obInertia(o) = obInertia(o) + d0;
        } else if (d0 >= 0) {
            obInertia(o) = obInertia(o) + d0;
        } else {
            d0 = d0 >> 2;
            obInertia(o) = obInertia(o) + d0;
        }
    }
}

static void Sonic_SlopeRepel(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (sticktoconvex(o)) {
        return;
    }
    if (locktime(o)) {
        locktime(o) = locktime(o) - 1;
        return;
    }
    {
        uint8_t d0 = obAngle(o);
        d0 += 0x20;
        d0 &= 0xC0;
        if (d0 == 0) {
            return;
        }
        {
            int16_t d0 = obInertia(o);
            if (d0 < 0) {
                d0 = -d0;
            }
            if (d0 < 0x280) {
                obInertia(o) = 0;
                obStatus(o) |= (1 << 1);
                locktime(o) = 30;
            }
        }
    }
}

static void Sonic_JumpAngle(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t d0 = obAngle(o);

    if (d0 == 0) {
        return;
    }
    if (d0 < 0x80) {
        d0 = d0 - 2;
        if (d0 >= 0x80) {
            d0 = 0;
        }
    } else {
        d0 = d0 + 2;
        if (d0 < 0x80) {
            d0 = 0;
        }
    }
    obAngle(o) = d0;
}

static void Sonic_Floor(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d1 = obVelX(o);
    int16_t d2 = obVelY(o);
    uint8_t d0 = CalcAngle(d1, d2);
    v_unused3 = d0;
    d0 = d0 - 0x20;
    v_unused4 = d0;
    d0 = d0 & 0xC0;
    v_unused5 = d0;

    if (d0 == 0x40) {
        Sonic_FloorLeft(o);
    } else if (d0 == 0x80) {
        Sonic_FloorUp(o);
    } else if (d0 == 0xC0) {
        Sonic_FloorRight(o);
    } else {
        Sonic_FloorDown(o);
    }
}

static void Sonic_FloorDown(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0, d1;
    uint8_t d3;

    /* Pared izquierda: restar, no sumar */
    d1 = Sonic_FindWallLeft_Quick(o);
    if (d1 < 0) {
        obX(o) = (int16_t)(obX(o) - d1);
        obVelX(o) = 0;
    }
    d1 = Sonic_FindWallRight_Quick(o);
    if (d1 < 0) {
        obX(o) = (int16_t)(obX(o) + d1);
        obVelX(o) = 0;
    }

    /* Ahora sí capturamos d3 (ángulo de la superficie) */
    Sonic_FindFloor(o, &d0, &d1, &d3);
    if (d1 >= 0) {
        return;
    }
    {
        uint8_t d2 = (uint8_t)obVelY(o);
        d2 = d2 + 8;
        d2 = (uint8_t)(-d2);
        if ((int8_t)d1 < (int8_t)d2) {
            if ((int8_t)d0 < (int8_t)d2) {
                return;
            }
        }
    }

    obY(o) = (int16_t)(obY(o) + d1);
    obSubpixelY(o) = 0;                       /* clr.w obSubpixelY(a0) */
    obAngle(o) = d3;                          /* ángulo real de la superficie */
    Sonic_ResetOnFloor(o);
    obAnim(o) = id_Walk;

    /* Clasificación de la superficie (FixBugs=0). Con FixBugs=1 sería
       más elaborado, pero replicamos el original. */
    {
        uint8_t tmp = d3;
        tmp = (uint8_t)(tmp + 0x20);
        if (tmp & 0x40) {
            /* Pendiente empinada */
            obVelX(o) = 0;
            if ((int16_t)obVelY(o) > 0xFC0) {
                obVelY(o) = 0xFC0;
            }
            obInertia(o) = obVelY(o);
            if ((int8_t)d3 < 0) {
                obInertia(o) = (int16_t)(-obInertia(o));
            }
            return;
        }
        tmp = d3;
        tmp = (uint8_t)(tmp + 0x10);
        if (!(tmp & 0x20)) {
            /* Superficie plana: AHORA SÍ limpia velY */
            obVelY(o) = 0;
            obInertia(o) = obVelX(o);
            return;
        }
        /* Pendiente suave: mitades */
        obVelY(o) = (int16_t)(obVelY(o) >> 1);
        obInertia(o) = obVelY(o);
        if ((int8_t)d3 < 0) {
            obInertia(o) = (int16_t)(-obInertia(o));
        }
    }
}

static void Sonic_FloorLeft(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d1;
    uint8_t d3;

    d1 = Sonic_FindWallLeft_Quick(o);
    if (d1 < 0) {
        obX(o) = (int16_t)(obX(o) - d1);
        obVelX(o) = 0;
        obInertia(o) = obVelY(o);
        return;
    }

    Sonic_FindCeiling(o, NULL, &d1, NULL);
    if (d1 < 0) {
        obY(o) = (int16_t)(obY(o) - d1);
        obSubpixelY(o) = 0;
        if (obVelY(o) < 0) {
            obVelY(o) = 0;
        }
        return;
    }

    if (obVelY(o) < 0) {                     /* tst.w obVelY(a0) / bmi.s .return: */
        return;                              /* if going up, skip the floor check */
    }
    Sonic_FindFloor(o, NULL, &d1, &d3);      /* capturar d3 */
    if (d1 >= 0) {
        return;
    }
    obY(o) = (int16_t)(obY(o) + d1);
    obSubpixelY(o) = 0;
    obAngle(o) = d3;
    Sonic_ResetOnFloor(o);
    obAnim(o) = id_Walk;
    obVelY(o) = 0;
    obInertia(o) = obVelX(o);
}

static void Sonic_FloorUp(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d1;

    /* Pared izquierda: restar, no sumar */
    d1 = Sonic_FindWallLeft_Quick(o);
    if (d1 < 0) {
        obX(o) = (int16_t)(obX(o) - d1);
        obVelX(o) = 0;
    }
    d1 = Sonic_FindWallRight_Quick(o);
    if (d1 < 0) {
        obX(o) = (int16_t)(obX(o) + d1);
        obVelX(o) = 0;
    }
    Sonic_FindCeiling(o, NULL, &d1, NULL);
    if (d1 < 0) {
        obY(o) = (int16_t)(obY(o) - d1);   /* SUB, no sum */
        if (obVelY(o) < 0) {
            obVelY(o) = 0;
        }
    }
}

static void Sonic_FloorRight(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d1;
    uint8_t d3;

    d1 = Sonic_FindWallRight_Quick(o);
    if (d1 < 0) {
        obX(o) = (int16_t)(obX(o) + d1);
        obVelX(o) = 0;
        obInertia(o) = obVelY(o);
        return;
    }

    Sonic_FindCeiling(o, NULL, &d1, NULL);
    if (d1 < 0) {
        obY(o) = (int16_t)(obY(o) - d1);
        obSubpixelY(o) = 0;
        if (obVelY(o) < 0) {
            obVelY(o) = 0;
        }
        return;
    }

    if (obVelY(o) < 0) {                     /* tst.w obVelY(a0) / bmi.s .return: */
        return;                              /* if going up, skip the floor check */
    }
    Sonic_FindFloor(o, NULL, &d1, &d3);
    if (d1 >= 0) {
        return;
    }
    obY(o) = (int16_t)(obY(o) + d1);
    obSubpixelY(o) = 0;
    obAngle(o) = d3;
    Sonic_ResetOnFloor(o);
    obAnim(o) = id_Walk;
    obVelY(o) = 0;
    obInertia(o) = obVelX(o);
}

void Sonic_ResetOnFloor(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (obStatus(o) & (1 << 4)) {
        /* roll-jump cleanup */
    }
    obStatus(o) &= ~((1 << 5) | (1 << 1) | (1 << 4));
    if (obStatus(o) & (1 << 2)) {
        obStatus(o) &= ~(1 << 2);
        obHeight(o) = sonic_height;
        obWidth(o) = sonic_width;
        obY(o) = (int16_t)(obY(o) - (sonic_height - sonic_roll_height));
    }
    obAnim(o) = id_Walk;
    jumping(o) = 0;
    v_itembonus = 0;
}

static void Sonic_Hurt(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    SpeedToPos(o);
    obVelY(o) = (int16_t)(obVelY(o) + (gravity - 8));
    if (obStatus(o) & (1 << 6)) {
        obVelY(o) = (int16_t)(obVelY(o) - (gravity - 0x18));
    }
    Sonic_HurtStop(o);
    Sonic_LevelBound(o);
    Sonic_RecordPosition(o);
    Sonic_Animate(o);
    Sonic_LoadGfx(o);
    DisplaySprite(o);
}

static void Sonic_HurtStop(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0 = v_limitbtm2 + 224;

    if ((int16_t)obY(o) >= d0) {
        KillSonic(o, NULL);
        return;
    }
    Sonic_Floor(o);
    if (obStatus(o) & (1 << 1)) {
        return;
    }
    obVelY(o) = 0;
    obVelX(o) = 0;
    obInertia(o) = 0;
    obAnim(o) = id_Walk;
    obRoutine(o) = obRoutine(o) - 2;
    flashtime(o) = 2 * 60;
}

static void Sonic_Death(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    Sonic_HandleDeath(o);
    ObjectFall(o);
    Sonic_RecordPosition(o);
    Sonic_Animate(o);
    Sonic_LoadGfx(o);
    DisplaySprite(o);
}

static void Sonic_HandleDeath(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d0 = v_limitbtm2 + 0x100;

    if ((int16_t)obY(o) < d0) {
        return;
    }
    obVelY(o) = -gravity;
    obRoutine(o) = obRoutine(o) + 2;
    f_timecount = 0;
    f_lifecount = f_lifecount + 1;
    v_lives = v_lives - 1;
    if (v_lives != 0) {
        restartime(o) = 60;
        if (f_timeover) {
            restartime(o) = 0;
            RAM_BYTE(v_gameovertext1) = id_GameOverCard;
            RAM_BYTE(v_gameovertext2) = id_GameOverCard;
            obFrame(RAM_ADDR(v_gameovertext2)) = 1;
            f_timeover = 0;
            goto playGameOverBgm;
        }
        return;
    }
    restartime(o) = 0;                             /* ASM: move.w #0,restartime(a0) */
    RAM_BYTE(v_gameovertext1) = id_GameOverCard;
    RAM_BYTE(v_gameovertext2) = id_GameOverCard;
    obFrame(RAM_ADDR(v_gameovertext2)) = 1;
    f_timeover = 0;

playGameOverBgm:
    Sound_Queue(bgm_GameOver, false);
    AddPLC(plcid_GameOver);
}

static void Sonic_ResetLevel(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (restartime(o) == 0) {
        return;
    }
    restartime(o) = restartime(o) - 1;
    if (restartime(o) != 0) {
        return;
    }
    f_restart = 1;
}

static void Sonic_AngleSpeed(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int16_t s0, s1;
    CalcSine(obAngle(o), &s0, &s1);
    obVelX(o) = (int16_t)(((int32_t)s1 * obInertia(o)) >> 8);
    obVelY(o) = (int16_t)(((int32_t)s0 * obInertia(o)) >> 8);
    Sonic_WallSpeedAdjust(o);   /* ASM cae aquí */
}

static void Sonic_ResetScr(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (v_lookshift == 0x60) {
        Sonic_CheckDpadLetGo(o);
        return;
    }
    /* ASM: el +4 y el -2 no son excluyentes; si v_lookshift < $60,
       primero suma 4 y luego resta 2 (neto +2). */
    if (v_lookshift < 0x60) {
        v_lookshift = v_lookshift + 4;
    }
    v_lookshift = v_lookshift - 2;
    Sonic_CheckDpadLetGo(o);
}

static void Sonic_LookUp(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (v_jpadhold2 & btnUp) {
        obAnim(o) = id_LookUp;
        if (v_lookshift < 0xC8) {
            v_lookshift = v_lookshift + 2;
        }
        Sonic_CheckDpadLetGo(o);
        return;
    }
    Sonic_Duck(o);
}

static void Sonic_Duck(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (v_jpadhold2 & btnDn) {
        obAnim(o) = id_Duck;
        if (v_lookshift > 8) {
            v_lookshift = v_lookshift - 2;
        }
        Sonic_CheckDpadLetGo(o);
        return;
    }
    Sonic_ResetScr(o);
}

static void Sonic_CheckDpadLetGo(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t d0 = v_jpadhold2 & (btnL | btnR);

    if (d0) {
        Sonic_AngleSpeed(o);
        return;
    }
    {
        int16_t d0 = obInertia(o);
        if (d0 == 0) {
            Sonic_AngleSpeed(o);
            return;
        }
        if (d0 < 0) {
            d0 = d0 + v_sonspeedacc;
            if (d0 < 0) {
                obInertia(o) = d0;
            } else {
                obInertia(o) = 0;
            }
        } else {
            d0 = d0 - v_sonspeedacc;
            if (d0 > 0) {
                obInertia(o) = d0;
            } else {
                obInertia(o) = 0;
            }
        }
    }
    Sonic_AngleSpeed(o);
}

static void Sonic_WallSpeedAdjust(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (obAngle(o) >= 0x40 && obAngle(o) < 0xC0) {
        return;
    }
    if (obInertia(o) == 0) {
        return;
    }
    {
        uint8_t d0 = obAngle(o);
        int16_t d1 = 0x40;
    if (obInertia(o) >= 0) {
       d1 = -0x40;
    }
        d0 = d0 + d1;
        {
            int16_t dist = Sonic_CalcRoomAhead(o, d0);
            if (dist >= 0) {
                return;
            }
            dist = dist << 8;
            d0 = d0 + 0x20;
            d0 = d0 & 0xC0;
            if (d0 == 0) {
                obVelY(o) = (int16_t)(obVelY(o) + dist);
            } else if (d0 == 0x40) {
                obVelX(o) = (int16_t)(obVelX(o) - dist);
            } else if (d0 == 0x80) {
                obVelY(o) = (int16_t)(obVelY(o) - dist);
            } else {
                obVelX(o) = (int16_t)(obVelX(o) + dist);
            }
        }
    }
}

static void Sonic_SquashUnused(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t d0 = obAngle(o);
    d0 += 0x20;
    d0 &= 0xC0;
    if (d0 != 0) {
        return;
    }
    {
        int16_t d1;
Sonic_FindCeiling(o, NULL, &d1, NULL);
        if (d1 >= 0) {
            return;
        }
        obInertia(o) = 0;
        obVelX(o) = 0;
        obVelY(o) = 0;
        obAnim(o) = id_Warp3;
    }
}

static void Sonic_Loops(void *obj) {

    uint8_t *o = (uint8_t *)obj;
    uint8_t d1;
    uint8_t d2;
    uint16_t d0;
    uint8_t *a1;
    /* cmpi.b #id_SLZ,(v_zone).w ; beq.s .isstarlight
     *      tst.b  (v_zone).w        ; bne.w .return
     *      Nota: tst.b mira sólo el byte bajo de v_zone. */
    if ((uint8_t)v_zone != id_SLZ) {
        if ((uint8_t)v_zone != 0) {
            return;
        }
    }

    /* .isstarlight: */
    /* move.w obY(a0),d0 ; lsr.w #1,d0 ; andi.w #$380,d0 */
    d0 = ((uint16_t)obY(o) >> 1) & 0x380;

    /* move.b obX(a0),d1 ; andi.w #$7F,d1 ; add.w d1,d0
     *      OJO: move.b sólo carga el BYTE BAJO de obX. */
    d1  = (uint8_t)obX(o);
    d1 &= 0x7F;
    d0  = (uint16_t)(d0 + d1);

    /* lea (v_lvllayout_fg).w,a1 ; move.b (a1,d0.w),d1 */
    a1 = RAM_ADDR(v_lvllayout_fg);
    d1 = a1[d0];

    /* cmp.b (v_256roll1).w,d1 ; beq.w Sonic_ChkRoll */
    if (d1 == (uint8_t)v_256roll1) {
      //  Sonic_ChkRoll(o);
        return;
    }
    /* cmp.b (v_256roll2).w,d1 ; beq.w Sonic_ChkRoll */
    if (d1 == (uint8_t)v_256roll2) {
      //  Sonic_ChkRoll(o);
        return;
    }
    /* cmp.b (v_256loop1).w,d1 ; beq.s .chkifleft */
    if (d1 == (uint8_t)v_256loop1) {
        goto chkifleft;
    }
    /* cmp.b (v_256loop2).w,d1 ; beq.s .chkifinair */
    if (d1 == (uint8_t)v_256loop2) {
        goto chkifinair;
    }

    /* bclr #sprite_looping_bit,obRender(a0) ; rts */
    obRender(o) &= ~sprite_looping;
    return;

    chkifinair:
    /* btst #1,obStatus(a0) ; beq.s .chkifleft */
    if (!(obStatus(o) & (1 << 1))) {
        goto chkifleft;
    }
    obRender(o) &= ~sprite_looping;
    return;

    chkifleft:
    /* move.w obX(a0),d2 ; cmpi.b #44,d2 ; bhs.s .chkifright
     *      cmpi.b compara SÓLO el byte bajo. */
    d2 = (uint8_t)obX(o);
    if (d2 >= 44) {                    /* bhs = unsigned >= */
        goto chkifright;
    }
    obRender(o) &= ~sprite_looping;
    return;

    chkifright:
    /* cmpi.b #224,d2 ; blo.s .chkangle1 */
    if (d2 < 224) {                    /* blo = unsigned < */
        goto chkangle1;
    }
    obRender(o) |= sprite_looping;
    return;

    chkangle1:
    /* btst #sprite_looping_bit,obRender(a0) ; bne.s .chkangle2 */
    if (obRender(o) & sprite_looping) {
        goto chkangle2;
    }
    /* move.b obAngle(a0),d1 ; beq.s .return */
    d1 = obAngle(o);
    if (d1 == 0) {
        return;                        /* ASM: sale SIN tocar el flag */
    }
    /* cmpi.b #$80,d1 ; bhi.s .return */
    if (d1 > 0x80) {                   /* bhi = unsigned > */
        return;                        /* ASM: sale SIN tocar el flag */
    }
    obRender(o) |= sprite_looping;
    return;

    chkangle2:
    /* move.b obAngle(a0),d1 ; cmpi.b #$80,d1 ; bls.s .return */
    d1 = obAngle(o);
    if (d1 <= 0x80) {                  /* bls = unsigned <= */
        return;
    }
    obRender(o) &= ~sprite_looping;
    return;
}

static uint8_t anim_next_frame(uint8_t *o, const uint8_t *a1) {
    uint8_t frame_idx = obAniFrame(o);
    uint8_t frame_id = a1[1 + frame_idx];

    if ((int8_t)frame_id < 0) {
        if (frame_id == 0xFF) {           /* afEnd: vuelve al primer frame */
            obAniFrame(o) = 0;
            frame_id = a1[1];
            obAniFrame(o) = 1;
        } else {
            /* afBack/afChange/... no se usan en walk/run/roll/push */
            return 0;
        }
    } else {
        obAniFrame(o) = frame_idx + 1;
    }
    return frame_id;
}

static void Sonic_Animate(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t anim_id = obAnim(o);

    if (anim_id != obPrevAni(o)) {
        obPrevAni(o) = anim_id;
        obAniFrame(o) = 0;
        obTimeFrame(o) = 0;
    }

    const uint8_t *anim_data = Ani_Sonic + ((const uint16_t *)Ani_Sonic)[anim_id];
    uint8_t frame_interval = anim_data[0];

    if ((int8_t)frame_interval >= 0) {
        /* Normal animation — inline SAnim_Do logic */
        uint8_t status = obStatus(o);
        uint8_t render = obRender(o);
        render = (render & ~(sprite_xflip | sprite_yflip)) | (status & sprite_xflip);
        obRender(o) = render;

        obTimeFrame(o)--;
        if ((int8_t)obTimeFrame(o) >= 0) {
            return;
        }
        obTimeFrame(o) = frame_interval;

        uint8_t frame_idx = obAniFrame(o);
        uint8_t frame_id = anim_data[1 + frame_idx];

        if ((int8_t)frame_id >= 0) {
            obFrame(o) = frame_id;
            obAniFrame(o) = frame_idx + 1;
        } else {
            switch (frame_id) {
                case 0xFF:
                    obAniFrame(o) = 0;
                    frame_id = anim_data[1];
                    {
                        obFrame(o) = frame_id;
                        status = obStatus(o);
                        render = obRender(o);
                        render = (render & ~(sprite_xflip | sprite_yflip)) | (status & sprite_xflip);
                        obRender(o) = render;
                        obAniFrame(o) = 1;
                    }
                    break;

                case 0xFE:
                    {
                        uint8_t back = anim_data[2 + frame_idx];
                        obAniFrame(o) -= back;
                        frame_idx = obAniFrame(o);
                        frame_id = anim_data[1 + frame_idx];
                        obFrame(o) = frame_id;
                        status = obStatus(o);
                        render = obRender(o);
                        render = (render & ~(sprite_xflip | sprite_yflip)) | (status & sprite_xflip);
                        obRender(o) = render;
                        obAniFrame(o)++;
                    }
                    break;

                case 0xFD:
                    obAnim(o) = anim_data[2 + frame_idx];
                    break;

                case 0xFC:
                    obRoutine(o) += 2;
                    break;

                case 0xFB:
                    obAniFrame(o) = 0;
                    ob2ndRout(o) = 0;
                    break;

                case 0xFA:
                    ob2ndRout(o) += 2;
                    break;
            }
        }
        return;
    }

    /* Special animation (walk/run/roll/push) */
    obTimeFrame(o)--;
    if ((int8_t)obTimeFrame(o) >= 0) {
        return;
    }

    switch (frame_interval) {
        case 0xFF: /* Walk/Run */
            {
                uint8_t angle = obAngle(o);
                uint8_t status = obStatus(o);
                uint8_t flip = status & sprite_xflip;
                if (!flip) {
                    angle = ~angle;
                }
                angle = angle + 0x10;

                /* ASM: d1 = flip flags a inyectar; eor con el flip actual */
                uint8_t d1 = (angle >= 0x80)
                             ? (sprite_xflip | sprite_yflip)
                             : 0;
                uint8_t render = obRender(o);
                render = (render & ~(sprite_xflip | sprite_yflip))
                       | (flip ^ d1);
                obRender(o) = render;

                if (status & (1 << 5)) {
                    /* TODO: push animation */
                    return;
                }

                angle = angle >> 4;
                angle = angle & 6;
                int16_t speed = obInertia(o);
                if (speed < 0) speed = -speed;

                const uint8_t *a1;
                uint8_t d3;
                if (speed >= 0x600) {
                    a1 = Ani_Sonic + ((const uint16_t *)Ani_Sonic)[id_Run];
                    d3 = angle + angle;
                } else {
                    a1 = Ani_Sonic + ((const uint16_t *)Ani_Sonic)[id_Walk];
                    d3 = angle + (angle >> 1);
                    d3 += d3;
                }

                speed = -speed + 0x800;
                if (speed < 0) speed = 0;
                speed = speed >> 8;
                obTimeFrame(o) = (uint8_t)speed;

                obFrame(o) = anim_next_frame(o, a1) + d3;
            }
            break;

        case 0xFE: /* Roll/Jump */
            {
                int16_t speed = obInertia(o);
                if (speed < 0) speed = -speed;
                const uint8_t *a1;
                if (speed >= 0x600) {
                    a1 = Ani_Sonic + ((const uint16_t *)Ani_Sonic)[id_Roll2];
                } else {
                    a1 = Ani_Sonic + ((const uint16_t *)Ani_Sonic)[id_Roll];
                }
                speed = -speed + 0x400;
                if (speed < 0) speed = 0;
                speed = speed >> 8;
                obTimeFrame(o) = (uint8_t)speed;

                uint8_t flip = obStatus(o) & sprite_xflip;
                obRender(o) = (obRender(o) & ~(sprite_xflip | sprite_yflip)) | flip;

                obFrame(o) = anim_next_frame(o, a1);
            }
            break;

        case 0xFD: /* Push */
            {
                int16_t speed = obInertia(o);
                if (speed < 0) speed = -speed;
                speed = speed + 0x800;
                if (speed < 0) speed = 0;
                speed = speed >> 6;
                obTimeFrame(o) = (uint8_t)speed;

                uint8_t flip = obStatus(o) & sprite_xflip;
                obRender(o) = (obRender(o) & ~(sprite_xflip | sprite_yflip)) | flip;

                const uint8_t *a1 = Ani_Sonic + ((const uint16_t *)Ani_Sonic)[id_Push];

                obFrame(o) = anim_next_frame(o, a1);
            }
            break;
    }
}

static void Sonic_LoadGfx(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    //fprintf(stdout, "SLG: frame=%d prev=%d\n", obFrame(o), (int)v_sonframenum); //ONLY FOR DEBUG CONSOLE
    uint8_t d0 = obFrame(o);

    if (d0 == v_sonframenum) {
        return;
    }
    v_sonframenum = d0;


    /* La tabla de offsets DPLC se almacena little-endian (ver parse_plc_asm). */
    uint16_t offset = SonicDynPLC[d0 * 2] | (SonicDynPLC[d0 * 2 + 1] << 8);
    const uint8_t *a2 = SonicDynPLC + offset;

    uint8_t d1 = *a2++; // Número de entradas DPLC
    if (d1 == 0) {
        return;
    }

    uint8_t *a3 = RAM_ADDR(v_sgfx_buffer);
    f_sonframechg = 1;

    do {
        uint8_t byte1 = *a2++;
        uint8_t byte2 = *a2++;

        uint8_t tile_count = (byte1 >> 4) + 1;
        uint16_t tile_offset = ((byte1 & 0x0F) << 8) | byte2;

        const uint8_t *a1 = Art_Sonic + (tile_offset * 32);

        for (int i = 0; i < tile_count; i++) {
            for (int b = 0; b < 32; b++) {
                *a3++ = *a1++;
            }
        }
    } while (--d1 > 0);
}

/* ===========================================================================
   GameOverCard — Port of Object 39 from _incObj/39 Game Over.asm
   "GAME OVER" / "TIME OVER" text. Two instances: frame 0 = "GAME",
   frame 1 = "OVER". They share slots with title card objects.
   =========================================================================== */
static void GameOverCard_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t routine = obRoutine(o);
    /* obTimeFrame is used as a 16-bit word in the ASM (move.w/tst.w/subq.w) */
    uint16_t *timer = (uint16_t *)(o + 0x1E);

    switch (routine) {
    case 0: {   /* Over_ChkPLC */
        if (RAM_LONG(v_plc_buffer)) {
            return;
        }
        obRoutine(o) = 2;
        __attribute__((fallthrough));
    }
    case 2: {   /* Over_MoveIn */
        int16_t center = 0x80 + (320 / 2);
        int16_t d1;

        if (obX(o) == center) {
            *timer = 12 * 60;
            obRoutine(o) = 4;
            DisplaySprite(o);
            return;
        }
        /* Initialize position on first entry */
        if (obX(o) < 0x20 || obX(o) > 0x1E0) {
            obX(o) = (int16_t)(0x80 - 48);
            if (obFrame(o) & 1) {
                obX(o) = (int16_t)(0x80 + 320 + 48);
            }
            obScreenY(o) = (int16_t)(0x80 + (224 / 2));
            obMap(o) = (uint32_t)(uintptr_t)NULL;  /* Map_Over not yet ported */
            obGfx(o) = (uint16_t)(ArtTile_Game_Over | Tile_Prio);
            obRender(o) = sprite_cam_screen;
            obPriority(o) = 0;
        }
        d1 = 0x10;
        if (obX(o) >= center) {
            d1 = -d1;
        }
        obX(o) = (int16_t)(obX(o) + d1);
        DisplaySprite(o);
        return;
    }
    case 4: {   /* Over_Wait */
        if (v_jpadpress1 & btnABC) {
            goto changeMode;
        }
        if (obFrame(o) & 1) {
            DisplaySprite(o);
            return;
        }
        if (*timer == 0) {
            goto changeMode;
        }
        *timer = *timer - 1;
        DisplaySprite(o);
        return;

changeMode:
        if (f_timeover) {
            v_lamp_time = 0;
            f_restart = 1;
        } else if (v_continues) {
            v_gamemode = 0x14;  /* id_Continue */
        } else {
            v_gamemode = 0x00;  /* id_Sega */
        }
        DisplaySprite(o);
        return;
    }
    }
}

/* ===========================================================================
   KillSonic — Port of KillSonic from _incObj/Sonic ReactToItem.asm (REV01)
   Sets up death state. Lives/game-over logic is in Sonic_HandleDeath (routine 6).
   Input: obj = Sonic object pointer (a0), damager = object killing Sonic (a2,
   NULL if unknown/undefined as in the original's non-collision callers).
   =========================================================================== */
void KillSonic(void *obj, void *damager) {
    uint8_t *o = (uint8_t *)obj;

    if (v_debuguse) {
        return;
    }
    v_invinc = 0;                        /* remove invincibility */
    obRoutine(o) = 6;                              /* set to Sonic_Death routine */
    Sonic_ResetOnFloor(o);                         /* reset airborne state */
    obStatus(o) = obStatus(o) | (1 << 1);          /* bset #1, force airborne */
    obVelY(o) = (int16_t)-0x700;                   /* launch Sonic upwards while dying */
    obVelX(o) = 0;                                 /* stop horizontal movement */
    obInertia(o) = 0;                              /* stop ground movement */
    *(int16_t *)(o + 0x38) = obY(o);               /* FixBugs=0 leftover: backup
                                                     Y-position into objoff_38 */
    obAnim(o) = id_Death;                          /* death animation */
    obGfx(o) = obGfx(o) | 0x80;                   /* bset #7, high sprite priority */
    if (damager != NULL && obID(damager) == id_Spikes) {
        Sound_Queue(sfx_HitSpikes, false);         /* killed by spikes */
    } else {
        Sound_Queue(sfx_Death, false);             /* play death sound */
    }
}

/* ===========================================================================
   HurtSonic — Port of HurtSonic from _incObj/Sonic ReactToItem.asm (REV01)
   Hurts Sonic: drops rings (RingLoss object), removes shield, bounces him away.
   Input: obj = Sonic object pointer (a0), damager = object hurting Sonic (a2)
   =========================================================================== */
void HurtSonic(void *obj, void *damager) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t *slot;

    if (v_shield) {
        goto bounceSonicAway;
    }
    if (v_rings == 0) {                            /* beq.w .hitWithoutRings */
        if (f_debugmode) {                         /* tst.w f_debugmode; bne.w .bounceSonicAway */
            goto bounceSonicAway;
        }
        KillSonic(o, damager);                     /* fall through to KillSonic */
        return;
    }
    slot = (uint8_t *)FindFreeObj();
    if (slot != NULL) {                            /* jsr FindFreeObj; bne.s .bounceSonicAway */
        obID(slot) = id_RingLoss;
        obX(slot)   = obX(o);
        obY(slot)   = obY(o);
    }

bounceSonicAway:
    v_shield = 0;                                  /* remove a potential shield */
    obRoutine(o) = 4;                              /* set to Sonic_Hurt routine */
    Sonic_ResetOnFloor(o);                         /* reset airborne state */
    obStatus(o) = obStatus(o) | (1 << 1);          /* bset #1, force airborne */

    obVelY(o) = (int16_t)-0x400;                   /* bounce Sonic vertically */
    obVelX(o) = (int16_t)-0x200;                   /* bounce Sonic horizontally */
    if (obStatus(o) & (1 << 6)) {                  /* underwater? */
        obVelY(o) = (int16_t)-0x200;               /* slower vertical bounce */
        obVelX(o) = (int16_t)-0x100;               /* slower horizontal bounce */
    }
    if ((int16_t)obX(o) >= (int16_t)obX(damager)) { /* right of object → reverse */
        obVelX(o) = -(int16_t)obVelX(o);
    }
    obInertia(o) = 0;                              /* cancel ground speed */
    obAnim(o) = id_Hurt;                           /* hurt animation */
    flashtime(o) = 2 * 60;                         /* 2 seconds of invulnerability */

    /* FixBugs=0 (buggy) sound: HitSpikes requires the damager to be BOTH
       id_Spikes and id_Harpoon simultaneously, which is impossible, so the
       generic damage sound always plays here. */
    if (obID(damager) == id_Spikes) {
        if (obID(damager) == id_Harpoon) {
            Sound_Queue(sfx_HitSpikes, false);
        } else {
            Sound_Queue(sfx_Death, false);
        }
    } else {
        Sound_Queue(sfx_Death, false);
    }
}

/* ===========================================================================
   ReactToItem — Port of _incObj/Sonic ReactToItem.asm (REV01, FixBugs=0)
   Handles Sonic's interaction with all level objects via obColType collision.
   Input: obj = Sonic object pointer (a0). Return value (d0) unused by caller.
   =========================================================================== */

/* Hitbox sizes, stored as box extents (half-width, half-height).
   Index = (obColType & $3F) - 1. Transcribed from React_Sizes (REV01). */
static const uint8_t React_Sizes[0x25 * 2] = {
    20, 20,   /* $01 col_40x40     GHZ ball */
    12, 20,   /* $02 col_24x40     (unused) */
    20, 12,   /* $03 col_40x24     (unused) */
     4, 16,   /* $04 col_8x32      GHZ spike pole, SYZ boss spike */
    12, 18,   /* $05 col_24x36     Ball Hog, Burrobot */
    16, 16,   /* $06 col_32x32     SBZ spikeball, Crabmeat, Monitor, SYZ spikeball, Prison */
     6,  6,   /* $07 col_12x12     Cannonball, Crab/Buzz missile, Ring */
    24, 12,   /* $08 col_48x24     Buzz Bomber */
    12, 16,   /* $09 col_24x32     Chopper */
    16, 12,   /* $0A col_32x24     Jaws */
     8,  8,   /* $0B col_16x16     MZ fire, Fireball, Batbrain, LZ spikeball, SLZ seesaw spike, Orbinaut, Caterkiller */
    20, 16,   /* $0C col_40x32     Newtron, Motobug, Yadrin */
    20,  8,   /* $0D col_40x16     Newtron */
    14, 14,   /* $0E col_28x28     Roller */
    24, 24,   /* $0F col_48x48     Bosses */
    40, 16,   /* $10 col_80x32     MZ vertical stomper */
    16, 24,   /* $11 col_32x48     MZ sideways stomper */
     8, 16,   /* $12 col_16x32     Giant ring */
    32,112,   /* $13 col_64x224    MZ geyser */
    64, 32,   /* $14 col_128x64    MZ lava wall, MZ lava tag */
   128, 32,   /* $15 col_256x64    MZ lava tag */
    32, 32,   /* $16 col_64x64     MZ lava tag */
     8,  8,   /* $17 col_16x16_alt SYZ bumper */
     4,  4,   /* $18 col_8x8       SYZ spike chain, Bomb shrapnel, Orbinaut spike, LZ gargoyle fire */
    32,  8,   /* $19 col_64x16     SLZ swing */
    12, 12,   /* $1A col_24x24     Bomb enemy, FZ plasma */
     8,  4,   /* $1B col_16x8      LZ harpoon */
    24,  4,   /* $1C col_48x8      LZ harpoon */
    40,  4,   /* $1D col_80x8      LZ harpoon */
     4,  8,   /* $1E col_8x16      LZ harpoon */
     4, 24,   /* $1F col_8x48      LZ harpoon */
     4, 40,   /* $20 col_8x80      LZ harpoon */
     4, 32,   /* $21 col_8x64      LZ pole */
    24, 24,   /* $22 col_48x48_alt SBZ saw */
    12, 24,   /* $23 col_24x48     SBZ flamethrower */
    72,  8,   /* $24 col_144x16    SBZ electric */
};

/* Combo points per destroyed badnik (/10); word-indexed by the bonus counter.
   The 16th and subsequent badniks are hardcoded to 10000 points. */
static const uint16_t React_PointsCombo[4] = { 10, 20, 50, 100 };

void ReactToItem(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t *a1;
    int16_t d0, d5;

    /* --- DEBUG TEMPORAL --- */
    static int dbg_init = 0;
    static int dbg_on = 0;
    if (!dbg_init) {
        dbg_init = 1;
        dbg_on = getenv("SONIC_DEBUG_REACT") != NULL;
        if (dbg_on) {
            fprintf(stderr, "ReactToItem range: start=%p end=%p count=%d\n",
                    (void*)RAM_ADDR(v_lvlobjspace), (void*)RAM_ADDR(v_lvlobjend),
                    (int)((v_lvlobjend - v_lvlobjspace) / object_size));
            fprintf(stderr, "ObjRAM=%p  v_objspace=%d  v_lvlobjspace=%d\n",
                    (void*)ObjRAM, v_objspace, v_lvlobjspace);
        }
    }
    /* --- FIN DEBUG --- */

    int16_t d2 = (int16_t)obX(o) - sonic_react_width;
    d5 = (int16_t)obHeight(o) - 3;
    int16_t d3 = (int16_t)obY(o) - d5;
    if (obFrame(o) == fr_Duck) {
        d3 += (int16_t)(((sonic_height - 3) - sonic_duck_height) * 2);
        d5 = sonic_duck_height;
    }
    int16_t d4 = sonic_react_width * 2;
    d5 += d5;

    for (a1 = (uint8_t *)RAM_ADDR(v_lvlobjspace);
         a1 < (uint8_t *)RAM_ADDR(v_lvlobjend);
    a1 += object_size) {
        if (!(obRender(a1) & 0x80)) {
            continue;
        }
        uint8_t colType = obColType(a1);
        if (colType == 0) {
            continue;
        }

        /* --- DEBUG: imprime el primer ring que encontremos por frame --- */
        if (dbg_on && colType == (col_12x12 | col_item)) {
            static int ring_dbg_count = 0;
            if (ring_dbg_count < 20) {
                fprintf(stderr, "RING id=%02X render=%02X col=%02X obj=(%d,%d) son=(%d,%d) hbX=[%d,%d] hbY=[%d,%d]\n",
                        obID(a1), obRender(a1), colType,
                        obX(a1), obY(a1), obX(o), obY(o),
                        d2, (int)(d2 + d4),
                        d3, (int)(d3 + d5));
                ring_dbg_count++;
            }
        }
        /* --- FIN DEBUG --- */

        uint8_t masked = colType & (uint8_t)~(col_item | col_hurt | col_special);
        if (masked == 0 || masked > 0x24) {
            continue;
        }
        const uint8_t *size = &React_Sizes[(masked - 1) * 2];
        int16_t hw = size[0];
        d0 = (int16_t)obX(a1) - hw - d2;
        if (d0 < 0) {
            d0 += hw * 2;
            if (d0 < 0) {
                if (dbg_on) fprintf(stderr, "  -> X test FAIL (left)\n");
                continue;
            }
        } else if (d0 > d4) {
            if (dbg_on) fprintf(stderr, "  -> X test FAIL (right)\n");
            continue;
        }

        int16_t hh = size[1];
        d0 = (int16_t)obY(a1) - hh - d3;
        if (d0 < 0) {
            d0 += hh * 2;
            if (d0 < 0) {
                if (dbg_on) fprintf(stderr, "  -> Y test FAIL (above)\n");
                continue;
            }
        } else if (d0 > d5) {
            if (dbg_on) fprintf(stderr, "  -> Y test FAIL (below)\n");
            continue;
        }

        /* ---- React_CollisionDetected ---- */
        uint8_t d1 = colType & (col_item | col_hurt | col_special);
        if (d1 == 0) {
            goto React_Enemy;
        }
        if (d1 == (col_item | col_hurt | col_special)) {
            goto React_Special;
        }
        if ((int8_t)d1 < 0) {
            goto React_ChkHurt;
        }

        /* Otherwise col_item ($40-$7F) */
        d1 = colType & (uint8_t)~(col_item | col_hurt | col_special);
        if (d1 == col_32x32) {
            goto React_Monitor;
        }
        if ((uint16_t)flashtime(o) >= 90) {
            if (dbg_on) fprintf(stderr, "  -> ring: flashtime >= 90, skip\n");
            return;
        }
        if (dbg_on) fprintf(stderr, "  -> ring: COLLECTING (routine %d -> %d)\n",
            obRoutine(a1), obRoutine(a1) + 2);
        obRoutine(a1) = obRoutine(a1) + 2;
        return;

        React_Monitor:
        if ((int16_t)obVelY(o) < 0) {
            d0 = (int16_t)obY(o) - 16;
            if (d0 >= (int16_t)obY(a1)) {
                obVelY(o) = -(int16_t)obVelY(o);
                obVelY(a1) = (int16_t)-0x180;
                if (ob2ndRout(a1) == 0) {
                    ob2ndRout(a1) = ob2ndRout(a1) + 4;
                }
            }
            return;
        }
        if (obAnim(o) == id_Roll) {
            obVelY(o) = -(int16_t)obVelY(o);
            obRoutine(a1) = obRoutine(a1) + 2;
        }
        return;

        React_Enemy:
        if (!v_invinc) {
            if (obAnim(o) != id_Roll) {
                goto React_ChkHurt;
            }
        }
        if (obBossHits(a1) == 0) {
            goto React_BadnikHit;
        }
        obVelX(o) = (int16_t)(-(int16_t)obVelX(o));
        obVelY(o) = (int16_t)(-(int16_t)obVelY(o));
        obVelX(o) = (int16_t)(obVelX(o) >> 1);
        obVelY(o) = (int16_t)(obVelY(o) >> 1);
        obColType(a1) = col_none;
        obBossHits(a1) = obBossHits(a1) - 1;
        if (obBossHits(a1) != 0) {
            return;
        }
        obStatus(a1) |= (1 << 7);
        return;

        React_BadnikHit:
        obStatus(a1) |= (1 << 7);
        uint16_t pb = (uint16_t)v_itembonus;
        v_itembonus = (uint16_t)(v_itembonus + 2);
        if (pb >= (3 * 2)) {
            pb = 3 * 2;
        }
        exitem_pointsframe(a1) = pb;
        d0 = (int16_t)React_PointsCombo[pb / 2];
        if ((uint16_t)v_itembonus >= (16 * 2)) {
            d0 = 1000;
            exitem_pointsframe(a1) = 5 * 2;
        }
        AddPoints(d0);
        obID(a1) = id_ExplosionItem;
        obRoutine(a1) = 0;
        if ((int16_t)obVelY(o) < 0) {
            obVelY(o) = (int16_t)(obVelY(o) + 0x100);
            return;
        }
        d0 = (int16_t)obY(o);
        if (d0 >= (int16_t)obY(a1)) {
            obVelY(o) = (int16_t)(obVelY(o) - 0x100);
            return;
        }
        obVelY(o) = -(int16_t)obVelY(o);
        return;

        React_ChkHurt:
        if (v_invinc) {
            return;
        }
        if (flashtime(o) != 0) {
            return;
        }
        HurtSonic(o, a1);
        return;

        React_Caterkiller:
        obStatus(a1) |= (1 << 7);
        goto React_ChkHurt;

        React_Special:
        d1 = colType & (uint8_t)~(col_item | col_hurt | col_special);
        if (d1 == col_16x16) {
            goto React_Caterkiller;
        }
        if (d1 == col_40x32) {
            goto React_Yadrin;
        }
        if (d1 == col_16x16_alt || d1 == col_8x64) {
            obColProp(a1) = obColProp(a1) + 1;
        }
        return;

        React_Yadrin:
        d5 = d5 - d0;
        if (d5 >= 8) {
            goto React_Enemy;
        }
        d0 = (int16_t)obX(a1) - 4;
        if (obStatus(a1) & 1) {
            d0 -= 16;
        }
        d0 -= d2;
        if (d0 >= 0) {
            if (d0 > d4) {
                goto React_Enemy;
            }
        } else {
            d0 += 24;
            if (d0 >= 0) {
                goto React_ChkHurt;
            }
            goto React_Enemy;
        }
        goto React_ChkHurt;
    }
}

/* ===========================================================================
   Crabmeat enemy (id_Crabmeat = $1F, GHZ/SYZ)
   Ported from _incObj/1F Badnik - Crabmeat.asm (FixBugs=0).
   crab_timedelay = objoff_30, crab_flags = objoff_32.
   =========================================================================== */

#define crab_timedelay(obj) (*(int16_t *)((uint8_t *)(obj) + 0x30)) /* objoff_30 */
#define crab_flags(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x32)) /* objoff_32 */

static void Crab_Action_WaitFire(uint8_t *o);
static void Crab_Action_Scuttle(uint8_t *o);
static void Crab_Action_Fire(uint8_t *o);

/* Crab_SetAni — set d0 to the correct animation ID based on the floor angle:
   0 = flat
   1 = sloped (regular, left leg extended)
   2 = sloped (flipped, right leg extended) */
static uint8_t Crab_SetAni(uint8_t *o) {
    uint8_t d3 = obAngle(o);                     /* moveq #0,d0 ; move.b obAngle,d3 */

    if ((int8_t)d3 < 0) {                        /* bmi Crab_SetAni_Ascending */
        /* Crab_SetAni_Ascending: ascending slope to the right */
        if ((uint8_t)d3 > (uint8_t)-6) {         /* cmpi.b #-6,d3 ; bhi.s .return */
            return 0;                            /* keep flat */
        }
        if (obStatus(o) & sprite_xflip) {        /* btst #0,obStatus ; bne.s .return */
            return 2;                            /* facing left: X-flipped sloped */
        }
        return 1;                                /* regular sloped */
    }

    /* Crab_SetAni_Descending: descending slope to the right */
    if (d3 < 6) {                                /* cmpi.b #6,d3 ; blo.s .return */
        return 0;                                /* keep flat */
    }
    if (obStatus(o) & sprite_xflip) {            /* btst #0,obStatus ; bne.s .return */
        return 1;                                /* facing left: regular sloped */
    }
    return 2;                                    /* X-flipped sloped */
}

/* Crab_Main — routine 0 */
static void Crab_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    obHeight(o)  = 32 / 2;                       /* set height */
    obWidth(o)   = 16 / 2;                       /* set width */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Crab;
    obGfx(o)     = ArtTile_Crabmeat;
    obRender(o)  = sprite_cam_field;
    obPriority(o) = 3;
    obColType(o) = (uint8_t)(col_badnik | col_32x32); /* set collision type ($06) */
    obActWid(o)  = 42 / 2;

    /* Make the Crabmeat fall until it has collided with the floor (while invisible) */
    ObjectFall(o);                               /* increase gravity and update position */
    int16_t d1;
    int16_t d3;
    ObjFloorDist(obj, &d1, &d3);                 /* get distance between Crabmeat and floor */
    if (d1 >= 0) {                               /* tst.w d1 ; bpl.s .hide: not hit floor */
        return;                                  /* .hide: rts, do NOT display sprite yet */
    }
    obY(o)     += d1;                            /* add.w d1,obY: match position with floor */
    obAngle(o) = (uint8_t)d3;                    /* update angle to floor */
    obVelY(o)  = 0;                              /* clear falling speed */
    obRoutine(o) += 2;                           /* advance to Crab_Action */
    /* FixBugs=1-only "delete below $7FF" guard is omitted (FixBugs=0). */
}

/* Crab_Action — routine 2 */
static void Crab_Action(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (ob2ndRout(o)) {                      /* Crab_ActIndex: 0 = WaitFire, 2 = Scuttle */
        case 0:  Crab_Action_WaitFire(o); break;
        case 2:  Crab_Action_Scuttle(o); break;
    }

    if (Ani_Crab) {                              /* lea (Ani_Crab).l,a1 */
        AnimateSprite(obj, Ani_Crab);            /* bsr.w AnimateSprite */
    }
    RememberState(obj);                          /* bra.w RememberState */
}

/* Crab_Action_WaitFire */
static void Crab_Action_WaitFire(uint8_t *o) {
    crab_timedelay(o)--;                         /* subq.w #1,crab_timedelay */
    if ((int16_t)crab_timedelay(o) >= 0) {       /* bpl.s .return */
        return;
    }

    if ((int8_t)obRender(o) < 0) {               /* tst.b obRender ; bpl.s .startMoving */
        /* on screen: toggle the firing flag */
        crab_flags(o) ^= (1 << 1);               /* bchg #1,crab_flags */
        if (!(crab_flags(o) & (1 << 1))) {       /* bne.s Crab_Action_Fire: it was already set */
            Crab_Action_Fire(o);
            return;
        }
    }

    /* .startMoving */
    ob2ndRout(o) += 2;                           /* advance to Crab_Action_Scuttle */
    crab_timedelay(o) = 128 - 1;                 /* set time delay to approx 2 seconds */
    obVelX(o) = 0x80;                            /* move Crabmeat to the right */
    obAnim(o) = (uint8_t)(Crab_SetAni(o) + 3);   /* advance to walking set of animations */
    obStatus(o) ^= sprite_xflip;                 /* bchg #0,obStatus: X-flip Crabmeat */
    if (obStatus(o) & sprite_xflip) {            /* bne.s .return: now facing RIGHT? */
        obVelX(o) = -obVelX(o);                  /* negate direction when moving left */
    }
    /* .return */
}

/* Crab_Action_Fire */
static void Crab_Action_Fire(uint8_t *o) {
    crab_timedelay(o) = 60 - 1;                  /* set time to stay on post-firing animation */
    obAnim(o) = 6;                               /* use firing animation */

    /* .loadLeftFireball */
    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (a1) {                                    /* bne.s .loadRightFireball: RAM full */
        obID(a1) = id_Crabmeat;                  /* _move.b #id_Crabmeat,obID */
        obRoutine(a1) = 6;                       /* set to Crab_BallMain */
        obX(a1) = obX(o);                        /* copy X-position */
        obX(a1) -= 0x10;                         /* align with left claw */
        obY(a1) = obY(o);                        /* copy Y-position */
        obVelX(a1) = -0x100;                     /* launch ball leftward */
    }

    /* .loadRightFireball */
    a1 = (uint8_t *)FindFreeObj();
    if (!a1) {                                   /* if RAM is full, branch */
        return;
    }
    obID(a1) = id_Crabmeat;
    obRoutine(a1) = 6;                           /* set to Crab_BallMain */
    obX(a1) = obX(o);
    obX(a1) += 0x10;                             /* align with right claw */
    obY(a1) = obY(o);
    obVelX(a1) = 0x100;                          /* launch ball rightward */
}

/* Crab_Action_Scuttle */
static void Crab_Action_Scuttle(uint8_t *o) {
    crab_timedelay(o)--;                         /* decrement timer until firing */
    if ((int16_t)crab_timedelay(o) < 0) {        /* bmi.s .initFire */
        goto initFire;
    }

    SpeedToPos(o);                               /* update Crabmeat position */
    crab_flags(o) ^= (1 << 0);                   /* bchg #0,crab_flags: alternate wall check/align */
    if (!(crab_flags(o) & (1 << 0))) {           /* bne.s .alignAndAnimate: it was already set */
        goto alignAndAnimate;
    }

    /* .checkLedge: look 16px ahead in the facing direction */
    int16_t d3 = obX(o);                         /* move.w obX,d3 */
    d3 += 16;                                    /* addi.w #16 */
    if (obStatus(o) & sprite_xflip) {            /* btst #0,obStatus ; beq.s .checkLedge */
        d3 -= 16 * 2;                            /* subi.w #16*2 */
    }
    int16_t d1;
    ObjFloorDist2(o, d3, &d1, NULL);             /* jsr (ObjFloorDist2).l */
    if (d1 < -8 || d1 >= 0x0C) {                 /* cmpi.w #-8 blt / cmpi.w #$C bge */
        goto initFire;                           /* steep slope or drop ahead */
    }
    return;

alignAndAnimate:
    {
        int16_t d1b;
        int16_t d3b;
        ObjFloorDist(o, &d1b, &d3b);             /* jsr (ObjFloorDist).l */
        obY(o)    += d1b;                        /* align to floor */
        obAngle(o) = (uint8_t)d3b;               /* update angle to floor */
        obAnim(o)  = (uint8_t)(Crab_SetAni(o) + 3); /* advance to walking set */
    }
    return;

initFire:
    ob2ndRout(o) -= 2;                           /* go back to Crab_Action_WaitFire */
    crab_timedelay(o) = 60 - 1;                  /* set pre-firing delay to 1 second */
    obVelX(o) = 0;                               /* stop Crabmeat from moving */
    obAnim(o) = Crab_SetAni(o);                  /* standing animation for current angle */
}

/* Crab_Delete — routine 4 (unreachable, deletion is handled elsewhere) */
static void Crab_Delete(void *obj) {
    DeleteObject(obj);                           /* delete object */
}

/* Crab_BallMain — routine 6 (missile thrown by the Crabmeat) */
static void Crab_BallMain(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    obRoutine(o) += 2;                           /* advance to Crab_BallMove */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Crab;
    obGfx(o)     = ArtTile_Crabmeat;
    obRender(o)  = sprite_cam_field;
    obPriority(o) = 3;
    obColType(o) = (uint8_t)(col_12x12 | col_hurt); /* damaging 12x12 hitbox */
    obActWid(o)  = 16 / 2;
    obVelY(o)    = -0x400;                       /* launch balls upwards */
    obAnim(o)    = 7;                            /* use ball animation */
}

/* Crab_BallMove — routine 8 */
static void Crab_BallMove(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (Ani_Crab) {                              /* lea (Ani_Crab).l,a1 */
        AnimateSprite(obj, Ani_Crab);            /* bsr.w AnimateSprite: animate balls */
    }
    ObjectFall(o);                               /* make balls fall (apply gravity) */

    /* FixBugs=0: another bug where an object is queued for display and then
       deleted, causing a null-pointer dereference in the real game. */
    DisplaySprite(obj);                          /* bsr.w DisplaySprite */
    int16_t d0 = v_limitbtm2;                    /* move.w (v_limitbtm2).w,d0 */
    d0 += 224;                                   /* addi.w #224 */
    if ((uint16_t)d0 < (uint16_t)obY(o)) {       /* cmp.w obY(a0),d0 ; blo.s .delete */
        DeleteObject(obj);                       /* delete balls */
    }
}

/* Crabmeat_Main — object entry: dispatch by obRoutine (Crab_Index) */
static void Crabmeat_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                      /* Crab_Index: 0/2/4/6/8 */
        case 0: Crab_Main(obj);     break;
        case 2: Crab_Action(obj);   break;
        case 4: Crab_Delete(obj);   break;
        case 6: Crab_BallMain(obj); break;
        case 8: Crab_BallMove(obj); break;
    }
}

/* ===========================================================================
   Moto Bug enemy (id_MotoBug = $40, GHZ)
   Ported from _incObj/40 Badnik - Moto Bug.asm (FixBugs=0).
   moto_ledgewait = objoff_30, moto_smokewait = objoff_33.
   =========================================================================== */

#define moto_ledgewait(obj) (*(int16_t *)((uint8_t *)(obj) + 0x30)) /* objoff_30 */
#define moto_smokewait(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x33)) /* objoff_33 */

static void Moto_Main(void *obj);
static void Moto_Action(void *obj);
static void Moto_Smoke_Animate(void *obj);
static void Moto_Smoke_Delete(void *obj);
static void Moto_Action_Ledge(uint8_t *o);
static void Moto_Action_Drive(uint8_t *o);

/* Moto_Main — routine 0: initialization */
static void Moto_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    obMap(o)     = (uint32_t)(uintptr_t)Map_Moto;
    obGfx(o)     = ArtTile_Moto_Bug;
    obRender(o)  = sprite_cam_field;
    obPriority(o) = 4;
    obActWid(o)  = 40 / 2;

    if (obAnim(o) != 0) {                        /* tst.b obAnim ; bne.s .smoke: smoke particle? */
        obRoutine(o) += 4;                       /* set to Moto_Smoke_Animate */
        Moto_Smoke_Animate(obj);                 /* bra.w Moto_Smoke_Animate */
        return;
    }

    obHeight(o)  = 28 / 2;
    obWidth(o)   = 16 / 2;
    obColType(o) = (uint8_t)(col_40x32 | col_badnik);

    /* Make the Motobug fall until it has collided with the floor (while invisible) */
    ObjectFall(o);                               /* increase gravity and update position */
    int16_t d1;
    int16_t d3;
    ObjFloorDist(obj, &d1, &d3);                 /* get distance between Motobug and floor */
    if (d1 >= 0) {                               /* tst.w d1 ; bpl.s .hide: not hit floor */
        return;                                  /* .hide: rts, do NOT display sprite yet */
    }
    obY(o)     += d1;                            /* match object's position with the floor */
    obVelY(o)  = 0;                              /* clear falling speed */
    obRoutine(o) += 2;                           /* advance to Moto_Action */
    obStatus(o) ^= sprite_xflip;                 /* make Motobug face to the left on spawn */
    /* FixBugs=1-only "delete below $7FF" guard is omitted (FixBugs=0). */
}

/* Moto_Action — routine 2 */
static void Moto_Action(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (ob2ndRout(o)) {                      /* Moto_ActIndex: 0 = Ledge, 2 = Drive */
        case 0:  Moto_Action_Ledge(o); break;
        case 2:  Moto_Action_Drive(o); break;
    }

    if (Ani_Moto) {                              /* lea (Ani_Moto).l,a1 */
        AnimateSprite(obj, Ani_Moto);            /* bsr.w AnimateSprite */
    }
    RememberState(obj);                          /* RememberState is inlined here in the ASM */
}

/* Moto_Action_Ledge — pause when reaching a ledge, then drive the other way */
static void Moto_Action_Ledge(uint8_t *o) {
    moto_ledgewait(o)--;                         /* subq.w #1,moto_ledgewait */
    if ((int16_t)moto_ledgewait(o) >= 0) {       /* bpl.s .wait */
        return;
    }

    ob2ndRout(o) += 2;                           /* advance to Moto_Action_Drive */
    obVelX(o) = -0x100;                          /* move Motobug to the left */
    obAnim(o) = 1;                               /* use "drive" animation */
    obStatus(o) ^= sprite_xflip;                 /* invert X-flip flag */
    if (obStatus(o) & sprite_xflip) {            /* bne.s .wait (not taken): change direction */
        obVelX(o) = -obVelX(o);                  /* make Motobug move to the right */
    }
    /* .wait */
}

/* Moto_Action_Drive — drive forward, aligning to the floor and pumping smoke */
static void Moto_Action_Drive(uint8_t *o) {
    SpeedToPos(o);                               /* update position based on velocities */

    int16_t d1;
    int16_t d3;
    ObjFloorDist(o, &d1, &d3);                   /* find Motobug's distance to floor */
    if (d1 < -8 || d1 >= 0x0C) {                 /* cmpi.w #-8 blt / cmpi.w #$C bge */
        goto ledgeHit;                           /* steep slope or drop ahead */
    }
    obY(o) += d1;                                /* match position with the floor */

    moto_smokewait(o)--;                         /* subq.b #1,moto_smokewait */
    if ((int8_t)moto_smokewait(o) >= 0) {        /* bpl.s .return */
        return;
    }
    moto_smokewait(o) = 16 - 1;                  /* reset smoke delay timer */

    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (!a1) {                                   /* bne.s .return: RAM full */
        return;
    }
    obID(a1) = id_MotoBug;                       /* exhaust smoke particle (obAnim != 0) */
    obX(a1) = obX(o);                            /* copy X-position */
    obY(a1) = obY(o);                            /* copy Y-position */
    obStatus(a1) = obStatus(o);                  /* copy flipped status */
    obAnim(a1) = 2;                              /* set to smoke animation */
    /* .return */
    return;

ledgeHit:
    ob2ndRout(o) -= 2;                           /* go back to Moto_Action_Ledge */
    moto_ledgewait(o) = 60 - 1;                  /* set time to wait at ledge to 1 second */
    obVelX(o) = 0;                               /* stop the Motobug moving */
    obAnim(o) = 0;                               /* set to "wait" animation */
}

/* Moto_Smoke_Animate — routine 4 */
static void Moto_Smoke_Animate(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    if (Ani_Moto) {                              /* lea (Ani_Moto).l,a1 */
        AnimateSprite(obj, Ani_Moto);            /* bsr.w AnimateSprite (afRoutine -> routine 6) */
    }
    DisplaySprite(obj);                          /* display smoke sprite */
}

/* Moto_Smoke_Delete — routine 6 */
static void Moto_Smoke_Delete(void *obj) {
    DeleteObject(obj);                           /* delete smoke object */
}

/* MotoBug_Main — object entry: dispatch by obRoutine (Moto_Index) */
static void MotoBug_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                      /* Moto_Index: 0/2/4/6 */
        case 0: Moto_Main(obj);           break;
        case 2: Moto_Action(obj);         break;
        case 4: Moto_Smoke_Animate(obj);  break;
        case 6: Moto_Smoke_Delete(obj);   break;
    }
}

/* ===========================================================================
   Buzz Bomber enemy (id_BuzzBomber = $22) and its missile (id_Missile = $23),
   Ported from _incObj/22, 23 Badnik - Buzz Bomber and Missile.asm (FixBugs=0).
   buzz_timedelay = objoff_32, buzz_buzzstate = objoff_34;
   missile msl_timedelay = objoff_32, msl_parent = objoff_3C.
   =========================================================================== */

#define buzz_timedelay(obj) (*(int16_t *)((uint8_t *)(obj) + 0x32)) /* objoff_32 */
#define buzz_buzzstate(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x34)) /* objoff_34 */
#define msl_timedelay(obj)  (*(int16_t *)((uint8_t *)(obj) + 0x32)) /* objoff_32 */
/* objoff_3C is a 4-byte field (move.l a0,msl_parent(a1) in the disasm), but
   x86-64 object pointers live above 4 GB, so we store the parent's SLOT INDEX
   (word value, < 128) here and rebuild the pointer later. Same 32-bit width
   and same semantics as the ASM long. */
#define msl_parent(obj)     (*(uint32_t *)((uint8_t *)(obj) + 0x3C)) /* objoff_3C */

static void Buzz_Main(uint8_t *o);
static void Buzz_Action(uint8_t *o);
static void Buzz_Action_Wait(uint8_t *o);
static void Buzz_Action_Fire(uint8_t *o);
static void Buzz_Action_Move(uint8_t *o);
static void Buzz_Delete(uint8_t *o);
static void Msl_Main(uint8_t *o);
static void Msl_Animate(uint8_t *o);
static void Msl_FromBuzz(uint8_t *o);
static void Msl_FromNewt(uint8_t *o);
static void Msl_FromNewt_Animate(uint8_t *o);
static void Msl_ChkCancel(uint8_t *o);
static void Msl_Delete(uint8_t *o);

/* Buzz_Main — routine 0: initialization */
static void Buzz_Main(uint8_t *o) {
    obRoutine(o) += 2;                           /* advance to Buzz_Action */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Buzz; /* set mappings */
    obGfx(o)     = ArtTile_Buzz_Bomber;          /* set art tile */
    obRender(o)  = sprite_cam_field;             /* set to playfield-positioned mode */
    obPriority(o) = 3;                           /* set sprite priority */
    obColType(o) = (uint8_t)(col_48x24 | col_badnik); /* ReactToItem entry 8 (badnik, 48x24) */
    obActWid(o)  = 48 / 2;                       /* set sprite display width */
}

/* Buzz_Action — routine 2 */
static void Buzz_Action(uint8_t *o) {
    switch (ob2ndRout(o)) {                      /* Buzz_ActIndex: 0 = Wait, 2 = Move */
        case 0:  Buzz_Action_Wait(o); break;
        case 2:  Buzz_Action_Move(o); break;
    }

    if (Ani_Buzz) {                              /* lea (Ani_Buzz).l,a1 */
        AnimateSprite(o, Ani_Buzz);              /* bsr.w AnimateSprite */
    }
    RememberState(o);                            /* display sprite, or delete object if offscreen */
}

/* .move */
static void Buzz_Action_Wait(uint8_t *o) {
    buzz_timedelay(o)--;                         /* subq.w #1,buzz_timedelay */
    if ((int16_t)buzz_timedelay(o) >= 0) {       /* bpl.s .return */
        return;
    }
    if (buzz_buzzstate(o) & (1 << 1)) {          /* btst #1,buzz_buzzstate / bne.s Buzz_Action_Fire */
        Buzz_Action_Fire(o);                     /* Buzz Bomber is near Sonic: fire missile */
        return;
    }

    ob2ndRout(o) += 2;                           /* set to Buzz_Action_Move */
    buzz_timedelay(o) = 128 - 1;                 /* set flight time to just over 2 seconds */
    obVelX(o) = 0x400;                           /* move Buzz Bomber to the right */
    obAnim(o) = 1;                               /* use "flying" animation */
    if (obStatus(o) & sprite_xflip) {            /* btst #0,obStatus / bne.s .return */
        return;                                  /* facing right, keep moving right */
    }
    obVelX(o) = -obVelX(o);                      /* neg.w obVelX: move to the left instead */
}

/* .fire */
static void Buzz_Action_Fire(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (!a1) {                                   /* bne.s .return: object RAM is full */
        return;
    }
    obID(a1) = id_Missile;                       /* _move.b #id_Missile,obID(a1) */
    obX(a1) = obX(o);                            /* copy Buzz Bomber's X-position */
    obY(a1) = obY(o);                            /* copy Buzz Bomber's Y-position */
    obY(a1) += 0x1C;                             /* addi.w #$1C: align missile vertically */
    obVelY(a1) = 0x200;                          /* move missile downwards */
    obVelX(a1) = 0x200;                          /* move missile to the right */

    int16_t d0 = 0x18;                           /* FixBugs=0: misaligned horizontal offset */
    if (obStatus(o) & sprite_xflip) {            /* btst #0,obStatus / bne.s .alignX */
        /* facing right, keep offsets and velocities */
    } else {
        d0 = -d0;                                /* neg.w d0 */
        obVelX(a1) = -obVelX(a1);                /* neg.w obVelX(a1): missile to the left */
    }
    /* .alignX */
    obX(a1) += d0;                               /* add.w d0: align missile horizontally */

    obStatus(a1) = obStatus(o);                  /* copy X-flip flag to missile */
    msl_timedelay(a1) = 15 - 1;                  /* 15 frames delay before missile becomes active */
    msl_parent(a1) = (uint32_t)Object_GetIndex(o);  /* missile remembers the parent object */
    buzz_buzzstate(o) = 1;                       /* "already fired" to prevent refiring */
    buzz_timedelay(o) = 60 - 1;                  /* stay on firing animation for 1 second */
    obAnim(o) = 2;                               /* use "firing" animation */
}

/* .chknearsonic */
static void Buzz_Action_Move(uint8_t *o) {
    buzz_timedelay(o)--;                         /* subq.w #1,buzz_timedelay */
    if ((int16_t)buzz_timedelay(o) < 0) {        /* bmi.s .changeDirection */
        goto changeDirection;
    }

    SpeedToPos(o);                               /* update Buzz Bomber's position */

    if (buzz_buzzstate(o) != 0) {                /* tst.b / bne.s .return: just fired */
        return;                                  /* prevent firing again until it changed direction */
    }

    int16_t d0 = obX(RAM_ADDR(v_player));        /* move.w (v_player+obX).w,d0 */
    d0 -= obX(o);                                /* sub.w obX(a0): difference to Buzz Bomber */
    if (d0 < 0) {                                /* bpl.s .checkDistance */
        d0 = -d0;                                /* neg.w d0: make difference positive */
    }
    /* .checkDistance */
    if ((uint16_t)d0 >= 96) {                    /* cmpi.w #96,d0 / bhs.s .return: not near */
        return;
    }
    if (!(obRender(o) & sprite_rendered)) {      /* tst.b obRender / bpl.s .return: offscreen */
        return;
    }

    buzz_buzzstate(o) = 2;                       /* set Buzz Bomber to "near Sonic" */
    buzz_timedelay(o) = 30 - 1;                  /* set time delay before firing to half a second */
    goto stopMoving;                             /* bra.s .stopMoving */

changeDirection:
    buzz_buzzstate(o) = 0;                       /* set state to "normal" (no firing) */
    obStatus(o) ^= sprite_xflip;                 /* reverse direction */
    buzz_timedelay(o) = 60 - 1;                  /* set delay before moving again to 1 second */

stopMoving:
    ob2ndRout(o) -= 2;                           /* go back to Buzz_Action_Wait */
    obVelX(o) = 0;                               /* stop Buzz Bomber moving */
    obAnim(o) = 0;                               /* use "hovering" animation */
}

/* Buzz_Delete — routine 4 (unreachable, deletion is handled elsewhere) */
static void Buzz_Delete(uint8_t *o) {
    DeleteObject(o);                             /* bsr.w DeleteObject */
}

/* Msl_Main — missile routine 0 */
static void Msl_Main(uint8_t *o) {
    msl_timedelay(o)--;                          /* subq.w #1,msl_timedelay */
    if ((int16_t)msl_timedelay(o) >= 0) {        /* bpl.s Msl_ChkCancel */
        Msl_ChkCancel(o);                        /* time remains: check if parent was destroyed */
        return;                                  /* (branch, no rts to Msl_Main) */
    }

    obRoutine(o) += 2;                           /* advance to Msl_Animate */
    obMap(o)    = (uint32_t)(uintptr_t)Map_Missile; /* set mappings */
    obGfx(o)    = (uint16_t)(ArtTile_Buzz_Bomber | Tile_Pal2); /* art tile and palette line */
    obRender(o) = sprite_cam_field;              /* set to playfield-positioned mode */
    obPriority(o) = 3;                           /* set sprite priority */
    obActWid(o) = 16 / 2;                        /* set sprite display width */
    obStatus(o) &= 3;                            /* andi.b #3: clear flags except X/Y-flip */

    if (obSubtype(o) != 0) {                     /* tst.b obSubtype / beq.s Msl_Animate */
        obRoutine(o) = 8;                        /* set to Msl_FromNewt */
        obColType(o) = (uint8_t)(col_12x12 | col_hurt); /* damaging 12x12 hitbox */
        obAnim(o) = 1;                           /* set animation directly to ".missile" */
        Msl_FromNewt_Animate(o);                 /* bra.s Msl_FromNewt_Animate */
        return;
    }
    /* Msl_Animate */
    Msl_ChkCancel(o);                            /* check if parent Buzz Bomber was destroyed */
    if (Ani_Missile) {                           /* lea (Ani_Missile).l,a1 */
        AnimateSprite(o, Ani_Missile);           /* bsr.w AnimateSprite */
    }
    DisplaySprite(o);                            /* display missile sprite */
}

/* Msl_Animate — missile routine 2 */
static void Msl_Animate(uint8_t *o) {
    Msl_ChkCancel(o);                            /* delete missile if parent Buzz Bomber died */
    /* FixBugs=0: no return check after Msl_ChkCancel (may display a freed slot) */
    if (Ani_Missile) {                           /* lea (Ani_Missile).l,a1 */
        AnimateSprite(o, Ani_Missile);           /* bsr.w AnimateSprite (.flare advances routine) */
    }
    DisplaySprite(o);                            /* display missile sprite */
}

/* Msl_ChkCancel — delete missile if the Buzz Bomber which fired it was destroyed */
static void Msl_ChkCancel(uint8_t *o) {
    uint8_t *parent = Object_GetSlot((int)msl_parent(o)); /* movea.l msl_parent(a0),a1 */
    if (obID(parent) == id_ExplosionItem) {      /* cmpi.b #id_ExplosionItem,obID(a1) / beq.s Msl_Delete */
        Msl_Delete(o);                           /* parent destroyed: delete missile */
    }
}

/* Msl_FromBuzz — missile routine 4 */
static void Msl_FromBuzz(uint8_t *o) {
    /* Bit 7 of status is never set, so this branch is unreachable (see ASM notes). */
    if (obStatus(o) & (1 << 7)) {                /* btst #7,obStatus / bne.s .explode */
        /* .explode: change missile into the (broken gfx) small explosion */
        obID(o) = id_UnusedExplosion;            /* _move.b #id_UnusedExplosion,obID(a0) */
        obRoutine(o) = 0;                        /* reset routine counter */
        /* ASM branches to the unported UnusedExplosion object ($24); it resolves
           to the unmapped-ID slot (NullObject) in the PC port. */
        return;
    }

    obColType(o) = (uint8_t)(col_12x12 | col_hurt); /* damaging 12x12 hitbox */
    obAnim(o) = 1;                               /* set to ".missile" animation */
    SpeedToPos(o);                               /* update missile position */

    /* FixBugs=0: animate and display before the bottom-boundary check */
    if (Ani_Missile) {                           /* lea (Ani_Missile).l,a1 */
        AnimateSprite(o, Ani_Missile);           /* bsr.w AnimateSprite */
    }
    DisplaySprite(o);                            /* display missile sprite */

    int16_t d0 = v_limitbtm2;                    /* move.w (v_limitbtm2).w,d0 */
    d0 += 224;                                   /* addi.w #224: add screen height */
    if (d0 < obY(o)) {                           /* cmp.w obY(a0) / blo.s Msl_Delete */
        Msl_Delete(o);                           /* below the bottom level boundary */
    }
}

/* Msl_Delete — missile routine 6 */
static void Msl_Delete(uint8_t *o) {
    DeleteObject(o);                             /* bsr.w DeleteObject */
}

/* Msl_FromNewt — missile routine 8 (spawned by wall Newtron badniks) */
static void Msl_FromNewt(uint8_t *o) {
    if (!(obRender(o) & sprite_rendered)) {      /* tst.b obRender / bpl.s Msl_Delete */
        Msl_Delete(o);                           /* missile is offscreen */
        return;
    }
    SpeedToPos(o);                               /* update missile's position */
    Msl_FromNewt_Animate(o);
}

/* Msl_FromNewt_Animate */
static void Msl_FromNewt_Animate(uint8_t *o) {
    if (Ani_Missile) {                           /* lea (Ani_Missile).l,a1 */
        AnimateSprite(o, Ani_Missile);           /* bsr.w AnimateSprite */
    }
    DisplaySprite(o);                            /* display missile sprite */
}

/* BuzzBomber_Main — object entry: dispatch by obRoutine (Buzz_Index) */
static void BuzzBomber_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                      /* Buzz_Index: 0/2/4 */
        case 0: Buzz_Main(obj);        break;
        case 2: Buzz_Action(obj);      break;
        case 4: Buzz_Delete(obj);      break;
    }
}

/* Missile_Main — object entry: dispatch by obRoutine (Msl_Index) */
static void Missile_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                      /* Msl_Index: 0/2/4/6/8 */
        case 0: Msl_Main(obj);        break;
        case 2: Msl_Animate(obj);     break;
        case 4: Msl_FromBuzz(obj);    break;
        case 6: Msl_Delete(obj);      break;
        case 8: Msl_FromNewt(obj);    break;
    }
}

/* ===========================================================================
   GHZ bridge (id_Bridge = $11) and the shared platform solidity routines it
   is built on. Ported from _incObj/11 GHZ Bridge.asm (FixBugs=0); that file
   sandwiches in _incObj/sub PlatformObject & SlopeObject.asm and
   _incObj/sub ExitPlatform.asm, so those subroutines live here too.

   bridge_children      = obSubtype ($28): number of logs after construction
   bridge_children_ram  = $29-$39: object-slot index of every log (incl. parent)
   bridge_origY         = objoff_3C (word): initial Y each log remembers
   bridge_nudge         = objoff_3E: 0-$40, how far the bridge has bent
   bridge_currentlog    = objoff_3F: 0-based log Sonic is standing on
   =========================================================================== */

#define bri_children(obj)     (*(uint8_t *)((uint8_t *)(obj) + 0x28))  /* bridge_children = obSubtype */
#define bri_children_ram(obj) ((uint8_t *)(obj) + 0x29)                /* bridge_children_ram */
#define bri_origY(obj)        (*(int16_t *)((uint8_t *)(obj) + 0x3C))  /* objoff_3C */
#define bri_nudge(obj)        (*(uint8_t *)((uint8_t *)(obj) + 0x3E))  /* objoff_3E */
#define bri_curlog(obj)       (*(uint8_t *)((uint8_t *)(obj) + 0x3F))  /* objoff_3F */

/* GHZ bridge-bending data (Bri_Data_Y_Max: max Y a log dips when stood on,
   indexed by log count*16 + current log; only 12 logs are used in-game).
   Bri_Data_Align: per-standing-log bend fractions for each log left/right,
   $FF = full bend. Ported byte-for-byte; the `_` placeholder is 0. */
static const uint8_t Bri_Data_Y_Max[17 * 16] = {
    /* 0 logs  */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 1 log   */ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 2 logs  */ 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 3 logs  */ 2, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 4 logs  */ 2, 4, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 5 logs  */ 2, 4, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 6 logs  */ 2, 4, 6, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 7 logs  */ 2, 4, 6, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 8 logs  */ 2, 4, 6, 8, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 9 logs  */ 2, 4, 6, 8,10, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0,
    /* 10 logs */ 2, 4, 6, 8,10,10, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0,
    /* 11 logs */ 2, 4, 6, 8,10,12,10, 8, 6, 4, 2, 0, 0, 0, 0, 0,
    /* 12 logs */ 2, 4, 6, 8,10,12,12,10, 8, 6, 4, 2, 0, 0, 0, 0,
    /* 13 logs */ 2, 4, 6, 8,10,12,14,12,10, 8, 6, 4, 2, 0, 0, 0,
    /* 14 logs */ 2, 4, 6, 8,10,12,14,14,12,10, 8, 6, 4, 2, 0, 0,
    /* 15 logs */ 2, 4, 6, 8,10,12,14,16,14,12,10, 8, 6, 4, 2, 0,
    /* 16 logs */ 2, 4, 6, 8,10,12,14,16,16,14,12,10, 8, 6, 4, 2,
};

static const uint8_t Bri_Data_Align[16 * 16] = {
    /* log 0  */ 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 1  */ 0xB5, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 2  */ 0x7E, 0xDB, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 3  */ 0x61, 0xB5, 0xEC, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 4  */ 0x4A, 0x93, 0xCD, 0xF3, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 5  */ 0x3E, 0x7E, 0xB0, 0xDB, 0xF6, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 6  */ 0x38, 0x6D, 0x9D, 0xC5, 0xE4, 0xF8, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 7  */ 0x31, 0x61, 0x8E, 0xB5, 0xD4, 0xEC, 0xFB, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0,
    /* log 8  */ 0x2B, 0x56, 0x7E, 0xA2, 0xC1, 0xDB, 0xEE, 0xFB, 0xFF, 0, 0, 0, 0, 0, 0, 0,
    /* log 9  */ 0x25, 0x4A, 0x73, 0x93, 0xB0, 0xCD, 0xE1, 0xF3, 0xFC, 0xFF, 0, 0, 0, 0, 0, 0,
    /* log 10 */ 0x1F, 0x44, 0x67, 0x88, 0xA7, 0xBD, 0xD4, 0xE7, 0xF4, 0xFD, 0xFF, 0, 0, 0, 0, 0,
    /* log 11 */ 0x1F, 0x3E, 0x5C, 0x7E, 0x98, 0xB0, 0xC9, 0xDB, 0xEA, 0xF6, 0xFD, 0xFF, 0, 0, 0, 0,
    /* log 12 */ 0x19, 0x38, 0x56, 0x73, 0x8E, 0xA7, 0xBD, 0xD1, 0xE1, 0xEE, 0xF8, 0xFE, 0xFF, 0, 0, 0,
    /* log 13 */ 0x19, 0x38, 0x50, 0x6D, 0x83, 0x9D, 0xB0, 0xC5, 0xD8, 0xE4, 0xF1, 0xF8, 0xFE, 0xFF, 0, 0,
    /* log 14 */ 0x19, 0x31, 0x4A, 0x67, 0x7E, 0x93, 0xA7, 0xBD, 0xCD, 0xDB, 0xE7, 0xF3, 0xF9, 0xFE, 0xFF, 0,
    /* log 15 */ 0x19, 0x31, 0x4A, 0x61, 0x78, 0x8E, 0xA2, 0xB5, 0xC5, 0xD4, 0xE1, 0xEC, 0xF4, 0xFB, 0xFE, 0xFF,
};

/* --- _incObj/sub PlatformObject & SlopeObject.asm --------------------------
   Shared "stand on top of" solidity. PlatformObject does the x-range check
   then falls into the y check; Plat_NoXCheck skips the x check and uses
   obY-8 as the platform top; Plat_NoXCheck_AltY picks a caller-supplied top.
   The y check makes Sonic land, then falls into Plat_NoCheck which clears
   the previous platform's stood-on flag and records the new one.
   Returns 1 if Sonic landed, 0 if he walked into Plat_Exit. */

static void Plat_NoCheck(uint8_t *a1, uint8_t *o) {
    if (obStatus(a1) & (1 << 3)) {               /* btst #3,obStatus(a1) / beq.s .no */
        uint8_t *a2 = (uint8_t *)Object_GetSlot((int)standonobject(a1));
        obStatus(a2) &= ~(1 << 3);               /* bclr #3,obStatus(a2) */
        ob2ndRout(a2) = 0;                       /* clr.b ob2ndRout(a2) */
        if (obRoutine(a2) == 4) {                /* cmpi.b #4,obRoutine(a2) / bne.s .no */
            obRoutine(a2) -= 2;                  /* subq.b #2,obRoutine(a2) */
        }
    }

    /* .no */
    standonobject(a1) = (uint8_t)Object_GetIndex(o); /* convert address to index */
    obAngle(a1) = 0;                             /* move.b #0,obAngle(a1) */
    obVelY(a1) = 0;                              /* move.w #0,obVelY(a1) */
    obInertia(a1) = obVelX(a1);                  /* move.w obVelX(a1),obInertia(a1) */
    if (obStatus(a1) & (1 << 1)) {               /* btst #1,obStatus(a1) / beq.s .notinair */
        Sonic_ResetOnFloor(a1);                  /* was airborne: make Sonic land */
    }
    /* .notinair */
    obStatus(a1) |= (1 << 3);                    /* bset #3,obStatus(a1) */
    obStatus(o)  |= (1 << 3);                    /* bset #3,obStatus(a0) */
}

/* Plat_NoXCheck_AltY onward: y-range check using d0 = platform top Y. */
static int Plat_DoYCheck(uint8_t *o, int16_t d0) {
    uint8_t *a1 = RAM_ADDR(v_player);
    int16_t d2 = obY(a1);                        /* move.w obY(a1),d2 */
    int16_t d1 = (int16_t)(int8_t)obHeight(a1);  /* move.b obHeight(a1),d1 / ext.w d1 */
    d1 = (int16_t)(d1 + d2 + 4);                 /* add.w / addq.w #4: bottom edge + 4 */
    d0 = (int16_t)(d0 - d1);                     /* sub.w d1,d0: top vs bottom edge */
    if (d0 > 0) return 0;                        /* bhi.w Plat_Exit: Sonic above platform */
    if (d0 < -16) return 0;                      /* cmpi.w #-16,d0 / blo.w Plat_Exit */
    if ((int8_t)f_playerctrl < 0) return 0;      /* tst.b (f_playerctrl) / bmi.w Plat_Exit */
    if ((uint8_t)obRoutine(a1) >= 6) return 0;   /* cmpi.b #6,obRoutine(a1) / bhs.w Plat_Exit */
    d2 = (int16_t)(d2 + d0 + 3);                 /* add.w d0,d2 / addq.w #3,d2 */
    obY(a1) = d2;                                /* move.w d2,obY(a1) */
    obRoutine(o) += 2;                           /* addq.b #2,obRoutine(a0) */
    Plat_NoCheck(a1, o);                         /* fall through to Plat_NoCheck */
    return 1;
}

/* PlatformObject (full x + y check). d1 = platform half-width. */
static int PlatformObject(uint8_t *o, int16_t d1) __attribute__((unused));
static int PlatformObject(uint8_t *o, int16_t d1) {
    uint8_t *a1 = RAM_ADDR(v_player);
    if (obVelY(a1) < 0) return 0;                /* tst.w obVelY(a1) / bmi.w Plat_Exit */
    int16_t d0 = (int16_t)(obX(a1) - obX(o) + d1);
    if (d0 < 0) return 0;                        /* bmi.w Plat_Exit */
    d1 = (int16_t)(d1 + d1);                     /* add.w d1,d1 */
    if (d0 >= d1) return 0;                      /* cmp.w d1,d0 / bhs.w Plat_Exit */
    return Plat_DoYCheck(o, (int16_t)(obY(o) - 8)); /* Plat_NoXCheck: assume 8px tall */
}

/* Plat_NoXCheck: skip the x check, assume 8px tall platform. */
static int Plat_NoXCheck(uint8_t *o) {
    return Plat_DoYCheck(o, (int16_t)(obY(o) - 8));
}

/* SlopeObject: like PlatformObject but the platform top follows a heightmap
   (a2) under Sonic's x position; used by GHZ ledges and SLZ seesaws. */
static int SlopeObject(uint8_t *o, int16_t d1, const uint8_t *a2) __attribute__((unused));
static int SlopeObject(uint8_t *o, int16_t d1, const uint8_t *a2) {
    uint8_t *a1 = RAM_ADDR(v_player);
    if (obVelY(a1) < 0) return 0;                /* bmi.w Plat_Exit */
    int16_t d0 = (int16_t)(obX(a1) - obX(o) + d1);
    if (d0 < 0) return 0;                        /* bmi.s Plat_Exit */
    d1 = (int16_t)(d1 + d1);                     /* add.w d1,d1 */
    if (d0 >= d1) return 0;                      /* bhs.s Plat_Exit */
    if (obRender(o) & sprite_xflip) {            /* btst #sprite_xflip_bit,obRender / beq.s .noflip */
        d0 = (int16_t)(d1 + (~(uint16_t)d0));    /* not.w d0 / add.w d1,d0 */
    }
    /* .noflip */
    d0 >>= 1;                                    /* lsr.w #1,d0 */
    int d3 = a2[(uint16_t)d0];                   /* move.b (a2,d0.w),d3 */
    d0 = (int16_t)(obY(o) - d3);                 /* move.w obY(a0),d0 / sub.w d3,d0 */
    return Plat_DoYCheck(o, d0);                 /* bra.w Plat_NoXCheck_AltY */
}

/* PlatformObject_CustomHeight: like PlatformObject but with a custom solidity
   height d3 instead of the assumed 8px (used by swinging platforms). */
static int PlatformObject_CustomHeight(uint8_t *o, int16_t d1, int16_t d3) __attribute__((unused));
static int PlatformObject_CustomHeight(uint8_t *o, int16_t d1, int16_t d3) {
    uint8_t *a1 = RAM_ADDR(v_player);
    if (obVelY(a1) < 0) return 0;                /* bmi.w Plat_Exit */
    int16_t d0 = (int16_t)(obX(a1) - obX(o) + d1);
    if (d0 < 0) return 0;                        /* bmi.w Plat_Exit */
    d1 = (int16_t)(d1 + d1);                     /* add.w d1,d1 */
    if (d0 >= d1) return 0;                      /* bhs.w Plat_Exit */
    return Plat_DoYCheck(o, (int16_t)(obY(o) - d3)); /* use custom height in d3 */
}

/* --- _incObj/sub ExitPlatform.asm ------------------------------------------
   Allow Sonic to walk/jump off a platform. d1 = platform width/2 (d2 already
   set when entering at ExitPlatform2). Returns 1 ("carry set" in the ASM)
   while Sonic remains on the platform, 0 once he left it (the ASM's carry
   from the `blo` branch). Sonic's x-offset from the platform's left edge is
   written to *out_d0 for the caller (used to find the log index). */
static int ExitPlatform2(uint8_t *o, int16_t d1, int16_t d2, int16_t *out_d0) {
    uint8_t *a1 = RAM_ADDR(v_player);
    d2 = (int16_t)(d2 + d2);                     /* add.w d2,d2: double input width */
    if (obStatus(a1) & (1 << 1)) {               /* btst #1,obStatus(a1) / bne.s .exitedPlatform */
        goto exitedPlatform;                     /* airborne: exit platform */
    }
    int16_t d0 = (int16_t)(obX(a1) - obX(o) + d1);
    if (d0 < 0) {                                /* bmi.s .exitedPlatform: left of platform */
        goto exitedPlatform;
    }
    if ((uint16_t)d0 < (uint16_t)d2) {           /* cmp.w d2,d0 / blo.s .return */
        if (out_d0) *out_d0 = d0;                /* still on platform */
        return 1;                                /* carry set */
    }
exitedPlatform:
    obStatus(a1) &= ~(1 << 3);                   /* bclr #3,obStatus(a1) */
    obRoutine(o) = 2;                            /* move.b #2,obRoutine(a0) */
    obStatus(o)  &= ~(1 << 3);                   /* bclr #3,obStatus(a0) */
    return 0;                                    /* carry clear */
}

/* ExitPlatform entry: width is passed in d1 only. */
static int ExitPlatform(uint8_t *o, int16_t d1, int16_t *out_d0) __attribute__((unused));
static int ExitPlatform(uint8_t *o, int16_t d1, int16_t *out_d0) {
    return ExitPlatform2(o, d1, d1, out_d0);     /* move.w d1,d2 */
}

/* --- Bridge object routines ------------------------------------------------ */

static void Bri_Bend(uint8_t *o);
static void Bri_Action(uint8_t *o);
static void Bri_StoodOn(uint8_t *o);
static void Bri_CheckOnBridge(uint8_t *o);
static void Bri_WalkOff(uint8_t *o);
static void Bri_MoveSonic(uint8_t *o);
static void Bri_ChkDel(uint8_t *o);

/* Bri_ChildLog — routine $A: child logs are updated and deleted through the
   parent object; they just display themselves every frame. */
static void Bri_ChildLog(uint8_t *o) {
    DisplaySprite(o);                            /* bsr.w DisplaySprite */
}

/* Bri_Delete — routine 6/8 (unused?) */
static void Bri_Delete(uint8_t *o) {
    DeleteObject(o);                             /* bsr.w DeleteObject */
}

/* Bri_Main — routine 0: spawn all the child logs. Falls through into
   Bri_Action at the end, exactly like the ASM. */
static void Bri_Main(uint8_t *o) {
    obRoutine(o) += 2;                           /* addq.b #2,obRoutine(a0) */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Bri; /* set mappings */
    obGfx(o)     = ArtTile_GHZ_Bridge | Tile_Pal3;
    obRender(o)  = sprite_cam_field;             /* playfield-positioned mode */
    obPriority(o) = 3;                           /* set sprite priority */
    /* FixBugs=0: the display width is 256/2, way too large; it was kept so the
       bridge could screen-wrap when Sonic is standing on it (see the ASM). */
    obActWid(o)  = 256 / 2;

    int16_t d2 = obY(o);                         /* copy Y-position from parent */
    int16_t d3 = obX(o);                         /* center X-position of bridge */
    uint8_t d4 = obID(o);                        /* copy parent object ID to children */
    uint8_t *a2 = &bri_children(o);              /* load child index array (= obSubtype) */
    uint8_t sub = *a2;                           /* get subtype for bridge */
    *a2++ = 0;                                   /* clear subtype, array now starts at $29 */
    d3 = (int16_t)(d3 - (((sub >> 1) << 4) & 0xFF)); /* lsr#1 * 16: X of leftmost log */

    if (sub < 2) {                               /* subq.b #2 / bcs.s Bri_Action: 1 log only */
        Bri_Action(o);
        return;
    }
    uint8_t d1 = (uint8_t)(sub - 2);             /* -1 for dbf, -1 for parent log */

    for (;;) {                                   /* .loopBuildBridge */
        uint8_t *a1 = (uint8_t *)FindFreeObj();  /* bsr.w FindFreeObj */
        if (!a1) {                               /* bne.s Bri_Action: object RAM full */
            Bri_Action(o);
            return;
        }
        bri_children(o)++;                       /* addq.b #1,bridge_children(a0) */

        if (d3 == obX(o)) {                      /* cmp.w obX(a0),d3 / bne.s .setupChild */
            d3 = (int16_t)(d3 + 16);             /* skip parent position */
            obY(o) = d2;                         /* move.w d2,obY(a0) (redundant) */
            bri_origY(o) = d2;                   /* remember initial Y-position */
            *a2++ = (uint8_t)Object_GetIndex(o); /* store parent as first entry */
            bri_children(o)++;                   /* account for parent log */
        }

        /* .setupChild */
        *a2++ = (uint8_t)Object_GetIndex(a1);    /* store child index at array end */
        obRoutine(a1) = 0x0A;                    /* Bri_ChildLog (display only) */
        obID(a1) = d4;                           /* copy object ID from parent */
        obY(a1) = d2;                            /* copy Y-position from parent */
        bri_origY(a1) = d2;                      /* remember initial Y-position */
        obX(a1) = d3;                            /* write current X-position */
        obMap(a1)     = (uint32_t)(uintptr_t)Map_Bri;
        obGfx(a1)     = ArtTile_GHZ_Bridge | Tile_Pal3;
        obRender(a1)  = sprite_cam_field;
        obPriority(a1) = 3;
        obActWid(a1)  = 16 / 2;                  /* individual log width */
        d3 = (int16_t)(d3 + 16);                 /* position next log 16px right */

        if (--d1 == 0xFF) break;                 /* dbf d1 */
    }

    Bri_Action(o);                               /* fall through to Bri_Action */
}

/* Bri_Action — routine 2 */
static void Bri_Action(uint8_t *o) {
    Bri_CheckOnBridge(o);                        /* allow stepping on bridge */

    if (bri_nudge(o) == 0) {                     /* tst.b bridge_nudge / beq.s .display */
        goto bri_display;
    }
    bri_nudge(o) = (uint8_t)(bri_nudge(o) - 4);  /* subq.b #4: reduce nudging */
    Bri_Bend(o);                                 /* bsr.w Bri_Bend */

bri_display:
    DisplaySprite(o);                            /* FixBugs=0: display main bridge */
    Bri_ChkDel(o);                               /* bra.w Bri_ChkDel */
}

/* Bri_CheckOnBridge — check if Sonic is over the bridge and let him land. */
static void Bri_CheckOnBridge(uint8_t *o) {
    uint16_t d1 = (uint16_t)(bri_children(o) << 3); /* moveq #0,d1; move.b: count*8 */
    uint16_t d2 = d1;                            /* copy for right-side check */
    d1 = (uint16_t)(d1 + 8);                     /* d1 = left edge of bridge */
    d2 = (uint16_t)(d2 + d2);                    /* d2 = right edge of bridge */
    uint8_t *a1 = RAM_ADDR(v_player);
    if (obVelY(a1) < 0) {                        /* tst.w obVelY(a1) / bmi.w Plat_Exit */
        return;
    }
    int16_t d0 = (int16_t)(obX(a1) - obX(o) + (int16_t)d1);
    if (d0 < 0) {                                /* bmi.w Plat_Exit: left of the bridge */
        return;
    }
    if ((uint16_t)d0 >= (uint16_t)d2) {          /* cmp.w d2,d0 / bhs.w Plat_Exit */
        return;
    }
    Plat_NoXCheck(o);                            /* bra.s Plat_NoXCheck: assume 8px */
}

/* Bri_StoodOn — routine 4 */
static void Bri_StoodOn(uint8_t *o) {
    Bri_WalkOff(o);                              /* allow exiting bridge */
    DisplaySprite(o);                            /* FixBugs=0: display main bridge */
    Bri_ChkDel(o);                               /* bra.w Bri_ChkDel */
}

/* Bri_WalkOff — bend the bridge while Sonic stands on it. */
static void Bri_WalkOff(uint8_t *o) {
    uint16_t d1w = (uint16_t)(bri_children(o) << 3); /* count*8 */
    uint16_t d2w = d1w;                          /* d2 = half-width for right check */
    d1w = (uint16_t)(d1w + 8);                   /* d1 = half-width for left check */
    int16_t d0 = 0;
    /* bsr.s ExitPlatform2 ; bcc.s .return: only bend while Sonic is still on */
    if (!ExitPlatform2(o, (int16_t)d1w, (int16_t)d2w, &d0)) {
        return;                                  /* bcc.s .return: Sonic exited, cleanup done */
    }
    /* .return: still on the bridge */
    bri_curlog(o) = (uint8_t)((uint16_t)d0 >> 4); /* lsr.w #4,d0: log Sonic is on */
    if (bri_nudge(o) != 0x40) {                  /* cmpi.b #$40,d0 / beq.s .bridgeBehavior */
        bri_nudge(o) = (uint8_t)(bri_nudge(o) + 4); /* addq.b #4: depress the bridge */
    }
    /* .bridgeBehavior */
    Bri_Bend(o);                                 /* bsr.w Bri_Bend */
    Bri_MoveSonic(o);                            /* bsr.w Bri_MoveSonic */
}

/* Bri_MoveSonic — vertically align Sonic with the log he's standing on. */
static void Bri_MoveSonic(uint8_t *o) {
    uint8_t *a2 = (uint8_t *)Object_GetSlot((int)bri_children_ram(o)[bri_curlog(o)]);
    uint8_t *a1 = RAM_ADDR(v_player);
    int16_t d0 = (int16_t)(obY(a2) - 8);         /* subq.w #8: align 8px upwards */
    d0 = (int16_t)(d0 - (int16_t)(int8_t)obHeight(a1)); /* sub.w obHeight: adjust by collision height */
    obY(a1) = d0;                                /* move.w d0,obY(a1) */
}

/* Bri_Bend — bend the bridge by aligning the logs left/right of the one
   Sonic stands on, using a sine of the nudge value (0-$40). */
static void Bri_Bend(uint8_t *o) {
    int16_t d0s, d1s;
    CalcSine(bri_nudge(o), &d0s, &d1s);          /* bsr.w CalcSine */
    int16_t d4 = d0s;                            /* move.w d0,d4: backup sine */

    const uint8_t *a4 = Bri_Data_Align;
    uint8_t count  = bri_children(o);            /* move.b bridge_children,d0 */
    uint8_t curlog = bri_curlog(o);              /* move.b bridge_currentlog,d3 */
    uint8_t d5 = Bri_Data_Y_Max[(uint16_t)(count * 16) + curlog]; /* max Y-bend distance */
    int d2 = curlog;                             /* number of logs left of Sonic */
    const uint8_t *a3 = a4 + (uint16_t)(curlog & 0x0F) * 16; /* align row for current log */
    uint8_t *a2 = bri_children_ram(o);           /* RAM indices to log objects */

    for (;;) {                                   /* .loopLeftLogs */
        uint8_t *a1 = (uint8_t *)Object_GetSlot(*a2++);
        uint16_t bend = (uint16_t)(*a3++ + 1);   /* move.b (a3)+,d0 / addq.w #1,d0 */
        uint16_t prod = (uint16_t)(bend * d5);   /* mulu.w d5,d0 (low word) */
        uint32_t total = (uint32_t)prod * (uint16_t)d4; /* mulu.w d4,d0 */
        obY(a1) = (int16_t)((uint16_t)(total >> 16) + bri_origY(a1)); /* swap + add origY */
        if (--d2 == -1) break;                   /* dbf d2 */
    }

    /* right side: reflected through the (count - curlog - 1) row of Align */
    int d3b = curlog + 1 - count;                /* addq #1,d3 / sub.b d0,d3 */
    d3b = -d3b;                                  /* neg.b d3 */
    if (d3b < 0) return;                         /* bmi.s .return */
    int d2b = d3b;                               /* move.w d3,d2 */
    const uint8_t *a3b = a4 + (d3b << 4);        /* lsl.w #4,d3 / lea (a4,d3.w),a3 */
    a3b += d2b;                                  /* adda.w d2,a3: first right-side log */
    d2b -= 1;                                    /* subq.w #1,d2: undo +1 for dbf */
    if (d2b < 0) return;                         /* bcs.s .return: rightmost log */

    for (;;) {                                   /* .loopRightLogs */
        uint8_t *a1 = (uint8_t *)Object_GetSlot(*a2++);
        uint16_t bend = (uint16_t)(*(a3b - 1) + 1); a3b--; /* move.b -(a3),d0 / addq.w #1 */
        uint16_t prod = (uint16_t)(bend * d5);   /* mulu.w d5,d0 */
        uint32_t total = (uint32_t)prod * (uint16_t)d4; /* mulu.w d4,d0 */
        obY(a1) = (int16_t)((uint16_t)(total >> 16) + bri_origY(a1)); /* swap + add origY */
        if (--d2b == -1) break;                  /* dbf d2 */
    }
}

/* Bri_ChkDel — delete the main bridge object and all child logs if offscreen. */
static void Bri_ChkDel(uint8_t *o) {
    if (!OutOfRange(o, -1)) {                    /* out_of_range.w .deleteBridge */
        return;                                  /* FixBugs=0: rts (no DisplaySprite here) */
    }

    /* .deleteBridge */
    uint8_t *a2 = bri_children_ram(o);
    int parent_idx = Object_GetIndex(o);
    uint8_t count = bri_children(o);             /* number of logs incl. parent */
    for (int i = (int)count - 1; i >= 0; i--) { /* subq.b #1 for dbf / .loopDeleteLogs */
        uint8_t *a1 = (uint8_t *)Object_GetSlot(*a2++);
        if (Object_GetIndex(a1) != parent_idx) { /* cmp.w a0,d0 / beq.s .next */
            DeleteObject(a1);                    /* bsr.w DeleteChild */
        }
    }

    /* .deleteParentLog */
    DeleteObject(o);                             /* bsr.w DeleteObject */
}

/* Bridge_Main — object entry: dispatch by obRoutine (Bri_Index) */
static void Bridge_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                      /* Bri_Index: 0/2/4/6/8/$A */
        case 0:     Bri_Main(o);     break;
        case 2:     Bri_Action(o);   break;
        case 4:     Bri_StoodOn(o);  break;
        case 6:     Bri_Delete(o);   break;      /* unused */
        case 8:     Bri_Delete(o);   break;      /* unused */
        case 0x0A:  Bri_ChildLog(o); break;
    }
}

/* --- _incObj/3B GHZ Purple Rock.asm ----------------------------------------
   Solid green-hill rock. Uses SolidObject (sub SolidObject.asm) for solidity;
   FixBugs=0 (obActWid too small; DisplaySprite then out-of-range delete). */

static void MoveWithPlatform(uint8_t *o, int16_t d0, int16_t d2);
static void MvSonicOnPtfm(uint8_t *o, int16_t d2, int16_t d3);
static void Solid_NotPushing(uint8_t *a1, uint8_t *o);
static void Solid_ResetFloor(uint8_t *o);
static int SolidObject(uint8_t *o, int16_t d1, int16_t d2, int16_t d3,
                       int16_t d4, int16_t *d3out, int16_t *d5out);

static void Rock_Main(uint8_t *o) {
    obRoutine(o) += 2;                          /* advance to Rock_Solid */
    obMap(o) = (uint32_t)(uintptr_t)Map_PRock;  /* set mappings */
    obGfx(o) = (uint16_t)(ArtTile_GHZ_Purple_Rock | Tile_Pal4);
    obRender(o) = sprite_cam_field;             /* playfield-positioned mode */
    obActWid(o) = 38 / 2;                       /* FixBugs=0: too small */
    obPriority(o) = 4;
}

static void Rock_Solid(uint8_t *o) {
    int16_t d1 = (int16_t)(32 / 2 + sonic_solid_width); /* SolidObject: width */
    int16_t d2 = 32 / 2;                                /* SolidObject: height (initial) */
    int16_t d3 = 32 / 2;                                /* SolidObject: height (stood-on) */
    int16_t d4 = obX(o);                                /* SolidObject: X (stood-on) */
    int16_t out_d3 = 0, out_d5 = 0;
    SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);   /* make rock solid */

    /* FixBugs=0: DisplaySprite then out_of_range DeleteObject */
    DisplaySprite(o);
    if (OutOfRange(o, -1)) {                    /* out_of_range.w DeleteObject */
        DeleteObject(o);
    }
}

static void PurpleRock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                     /* Rock_Index: 0/2 */
        case 0:  Rock_Main(o);   break;
        case 2:  Rock_Solid(o);  break;
    }
}

/* --- _incObj/sub MvSonicOnPtfm.asm ------------------------------------------
   Update Sonic's position when standing on a platform. d2 = platform X
   position of previous frame (for the X delta). MvSonicOnPtfm takes the
   platform height in d3; MvSonicOnPtfm2 assumes a fixed 9px height. */

static void MoveWithPlatform(uint8_t *o, int16_t d0, int16_t d2) {
    uint8_t *a1 = RAM_ADDR(v_player);           /* lea (v_player).w,a1 */
    if ((int8_t)f_playerctrl < 0) return;       /* tst.b (f_playerctrl).w / bmi.s .return */
    if ((uint8_t)obRoutine(a1) >= 6) return;    /* cmpi.b #6,(v_player+obRoutine).w / bhs.s .return */
    if (v_debuguse) return;                     /* tst.w (v_debuguse).w / bne.s .return */

    int16_t d1 = (int16_t)(int8_t)obHeight(a1); /* moveq #0,d1 / move.b obHeight(a1),d1 */
    d0 = (int16_t)(d0 - d1);                    /* sub.w d1,d0: Y for feet on platform */
    obY(a1) = d0;                               /* move.w d0,obY(a1) */

    d2 = (int16_t)(d2 - (int16_t)obX(o));       /* sub.w obX(a0),d2: X-delta since last frame */
    obX(a1) = (int16_t)((int16_t)obX(a1) - d2); /* sub.w d2,obX(a1) */
}

static void MvSonicOnPtfm(uint8_t *o, int16_t d2, int16_t d3) {
    int16_t d0 = (int16_t)((int16_t)obY(o) - d3); /* move.w obY(a0),d0 / sub.w d3,d0 */
    MoveWithPlatform(o, d0, d2);                /* bra.s MoveWithPlatform */
}

static void MvSonicOnPtfm2(uint8_t *o, int16_t d2) __attribute__((unused));
static void MvSonicOnPtfm2(uint8_t *o, int16_t d2) {
    int16_t d0 = (int16_t)((int16_t)obY(o) - 9); /* subi.w #9,d0 */
    MoveWithPlatform(o, d0, d2);                /* bra.s MoveWithPlatform */
}

/* --- _incObj/sub SolidObject.asm (FixBugs=0) --------------------------------
   General solid-object collision for Sonic (spikes, blocks, rocks...).
   Inputs: d1 = half width; d2 = half height (initial); d3 = half height
   (stood-on); d4 = object X position (stood-on).
   Output: returns d4 collision type (0=none, 1=side, -1=top/bottom);
   *d3out = y distance from nearest top/bottom edge (-ve if on bottom);
   *d5out = x distance from nearest left/right edge. */

static void Solid_NotPushing(uint8_t *a1, uint8_t *o) {
    obStatus(o)  &= ~(1 << 5);                  /* bclr #5,obStatus(a0) */
    obStatus(a1) &= ~(1 << 5);                  /* bclr #5,obStatus(a1) */
}

static void Solid_ResetFloor(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_player);

    if (obStatus(a1) & (1 << 3)) {              /* btst #3,obStatus(a1) / beq.s .notonobj */
        uint8_t *a2 = (uint8_t *)Object_GetSlot((int)standonobject(a1));
        obStatus(a2) &= ~(1 << 3);              /* bclr #3,obStatus(a2) */
        obSolid(a2) = 0;                        /* clr.b obSolid(a2) */
    }
    /* .notonobj */
    standonobject(a1) = (uint8_t)Object_GetIndex(o); /* convert OST address to index */
    obAngle(a1) = 0;                            /* move.b #0,obAngle(a1) */
    obVelY(a1) = 0;                             /* move.w #0,obVelY(a1) */
    obInertia(a1) = obVelX(a1);                 /* move.w obVelX(a1),obInertia(a1) */
    if (obStatus(a1) & (1 << 1)) {              /* btst #1,obStatus(a1) / beq.s .notinair */
        Sonic_ResetOnFloor(a1);                 /* reset Sonic as if on floor */
    }
    /* .notinair */
    obStatus(a1) |= (1 << 3);                   /* bset #3,obStatus(a1) */
    obStatus(o)  |= (1 << 3);                   /* bset #3,obStatus(a0) */
}

static int SolidObject(uint8_t *o, int16_t d1, int16_t d2, int16_t d3,
                       int16_t d4, int16_t *d3out, int16_t *d5out) {
    uint8_t *a1;
    int16_t d0 = 0, d5 = 0;

    if (obSolid(o) == 0) goto Solid_ChkCollision; /* tst.b obSolid(a0) / beq.w Solid_ChkCollision */

    /* Sonic is standing on the object: keep him riding, or let him walk off. */
    d2 = (int16_t)(d1 + d1);                    /* move.w d1,d2 / add.w d2,d2: full width */
    a1 = RAM_ADDR(v_player);
    if (obStatus(a1) & (1 << 1)) goto solid_leave; /* btst #1,obStatus(a1) / bne.s .leave (in air) */
    d0 = (int16_t)((int16_t)obX(a1) - (int16_t)obX(o) + d1); /* x pos of Sonic on object */
    if (d0 < 0) goto solid_leave;               /* bmi.s .leave */
    if (d0 >= d2) goto solid_leave;             /* FixBugs=0: blo.s .stand (1px too soon) */
    d2 = d4;                                    /* move.w d4,d2: platform X in previous frame */
    MvSonicOnPtfm(o, d2, d3);                   /* bsr.w MvSonicOnPtfm */
    goto solid_noreq;                           /* moveq #0,d4 / rts */

solid_leave:
    obStatus(a1) &= ~(1 << 3);                  /* bclr #3,obStatus(a1) */
    obStatus(o)  &= ~(1 << 3);                  /* bclr #3,obStatus(a0) */
    obSolid(o) = 0;                             /* clr.b obSolid(a0) */
    goto solid_noreq;                           /* moveq #0,d4 / rts */

Solid_ChkCollision:
    if (!(obRender(o) & 0x80)) goto Solid_NoCollision; /* tst.b obRender(a0) / bpl.w Solid_NoCollision */

    /* Solid_SkipRenderChk */
    a1 = RAM_ADDR(v_player);
    d0 = (int16_t)((int16_t)obX(a1) - (int16_t)obX(o) + d1); /* x pos of Sonic on object */
    if (d0 < 0) goto Solid_NoCollision;         /* bmi.w Solid_NoCollision */
    d3 = (int16_t)(d1 + d1);                    /* move.w d1,d3 / add.w d3,d3: full width */
    if (d0 > d3) goto Solid_NoCollision;        /* cmp.w d3,d0 / bhi.w Solid_NoCollision */
    d3 = (int16_t)(int8_t)obHeight(a1);         /* move.b obHeight(a1),d3 / ext.w d3 */
    d2 = (int16_t)(d2 + d3);                    /* add.w d3,d2: combined half height */
    d3 = (int16_t)((int16_t)obY(a1) - (int16_t)obY(o)); /* move.w obY(a1),d3 / sub.w obY(a0),d3 */
    d3 = (int16_t)(d3 + 4);                     /* addq.w #4,d3 */
    d3 = (int16_t)(d3 + d2);                    /* add.w d2,d3: feet y on object (0 = top) */
    if (d3 < 0) goto Solid_NoCollision;         /* bmi.w Solid_NoCollision */
    d4 = (int16_t)(d2 + d2);                    /* move.w d2,d4 / add.w d4,d4: full height */
    if (d3 >= d4) goto Solid_NoCollision;       /* cmp.w d4,d3 / bhs.w Solid_NoCollision */

    /* Solid_Collision */
    if ((int8_t)f_playerctrl < 0) goto Solid_NoCollision; /* tst.b / bmi.w */
    if ((uint8_t)obRoutine(a1) >= 6) goto Solid_Debug;    /* cmpi.b #6 / bhs.w Solid_Debug */
    if (v_debuguse) goto Solid_Debug;           /* tst.w (v_debuguse).w / bne.w */
    d5 = d0;                                    /* move.w d0,d5 */
    if (d0 <= d1) goto solid_left;              /* cmp.w d0,d1 / bhs.s .sonic_left */
    d1 = (int16_t)(d1 + d1);                    /* add.w d1,d1 */
    d0 = (int16_t)(d0 - d1);                    /* sub.w d1,d0 */
    d5 = (int16_t)(-d0);                        /* move.w d0,d5 / neg.w d5 */
solid_left:
    d1 = d3;                                    /* move.w d3,d1 */
    if (d3 <= d2) goto solid_top;               /* cmp.w d3,d2 / bhs.s .sonic_top */
    d3 = (int16_t)(d3 - 4);                     /* subq.w #4,d3 */
    d3 = (int16_t)(d3 - d4);                    /* sub.w d4,d3 */
    d1 = (int16_t)(-d3);                        /* move.w d3,d1 / neg.w d1 */
solid_top:
    if (d5 > d1) goto Solid_TopBottom;          /* cmp.w d1,d5 / bhi.w */
    if (d1 <= 4) goto Solid_SideAir;            /* cmpi.w #4,d1 / bls.s */
    if (d0 == 0) goto Solid_AlignToSide;        /* tst.w d0 / beq.s */
    if (d0 < 0) goto Solid_OnRight;             /* bmi.s */
    if (obVelX(a1) < 0) goto Solid_AlignToSide; /* tst.w obVelX(a1) / bmi.s */
    goto Solid_StopX;                           /* bra.s Solid_StopX */

    /* Solid_OnRight (Sonic nearer right edge) */
Solid_OnRight:
    if (obVelX(a1) >= 0) goto Solid_AlignToSide; /* tst.w obVelX(a1) / bpl.s */
    /* Solid_StopX */
Solid_StopX:
    obInertia(a1) = 0;                          /* move.w #0,obInertia(a1) */
    obVelX(a1) = 0;                             /* move.w #0,obVelX(a1) */

    /* Solid_AlignToSide */
Solid_AlignToSide:
    obX(a1) = (int16_t)((int16_t)obX(a1) - d0); /* sub.w d0,obX(a1) */
    if (obStatus(a1) & (1 << 1)) goto Solid_SideAir; /* btst #1,obStatus(a1) / bne.s */
    obStatus(a1) |= (1 << 5);                   /* bset #5,obStatus(a1): push object */
    obStatus(o)  |= (1 << 5);                   /* bset #5,obStatus(a0): be pushed */
    goto solid_side_ret;                        /* moveq #1,d4 / rts */

    /* Solid_SideAir */
Solid_SideAir:
    Solid_NotPushing(a1, o);                    /* bsr.s Solid_NotPushing */
solid_side_ret:
    if (d3out) *d3out = d3;
    if (d5out) *d5out = d5;
    return 1;                                   /* moveq #1,d4 / rts */

    /* Solid_NoCollision */
Solid_NoCollision:
    if (obStatus(o) & (1 << 5)) {               /* btst #5,obStatus(a0) / beq.s Solid_Debug */
        obAnim(a1) = id_Run;                    /* FixBugs=0 "walk-jump bug" */
        Solid_NotPushing(a1, o);                /* fall through to Solid_NotPushing */
    }
    /* Solid_Debug */
Solid_Debug:
    goto solid_noreq;                           /* moveq #0,d4 / rts */

    /* Solid_TopBottom */
Solid_TopBottom:
    if (d3 < 0) goto Solid_Below;               /* tst.w d3 / bmi.s */
    if (d3 < 16) goto Solid_Landed;             /* cmpi.w #$10,d3 / blo.s */
    goto Solid_NoCollision;                     /* bra.s Solid_NoCollision */

Solid_Below:
    if (obVelY(a1) == 0) goto Solid_Squash;     /* tst.w obVelY(a1) / beq.s */
    if (obVelY(a1) > 0) goto Solid_TopBtmAir;   /* bpl.s: moving downwards */
    if (d3 >= 0) goto Solid_TopBtmAir;          /* tst.w d3 / bpl.s */
    obY(a1) = (int16_t)((int16_t)obY(a1) - d3); /* FixBugs=0: sub.w d3,obY(a1) (wrong place) */
    obVelY(a1) = 0;                             /* move.w #0,obVelY(a1) */

Solid_TopBtmAir:
    goto solid_top_ret;                         /* moveq #-1,d4 / rts */

Solid_Squash:
    if (obStatus(a1) & (1 << 1)) goto Solid_TopBtmAir; /* btst #1,obStatus(a1) / bne.s */
    KillSonic(a1, NULL);                        /* save a0 / movea.l a1,a0 / KillSonic */
solid_top_ret:
    if (d3out) *d3out = d3;
    if (d5out) *d5out = d5;
    return -1;                                  /* moveq #-1,d4 / rts */

Solid_Landed:
    d3 = (int16_t)(d3 - 4);                     /* subq.w #4,d3 */
    {
        int16_t d1l = (int16_t)(int8_t)obActWid(o); /* moveq #0,d1 / move.b obActWid(a0),d1 */
        int16_t d1x = (int16_t)((int16_t)obX(a1) + d1l - (int16_t)obX(o)); /* x pos on object */
        if (d1x < 0) goto Solid_Miss;           /* bmi.s Solid_Miss */
        if (d1x >= d1l * 2) goto Solid_Miss;    /* add.w d2,d2 / cmp.w d2,d1 / bhs.s Solid_Miss */
        if (obVelY(a1) < 0) goto Solid_Miss;    /* tst.w obVelY(a1) / bmi.s Solid_Miss */
        obY(a1) = (int16_t)((int16_t)obY(a1) - d3); /* sub.w d3,obY(a1) */
        obY(a1) = (int16_t)((int16_t)obY(a1) - 1);  /* subq.w #1,obY(a1) */
        Solid_ResetFloor(o);                    /* bsr.s Solid_ResetFloor */
        obSolid(o) = 2;                         /* move.b #2,obSolid(a0) */
        obStatus(o) |= (1 << 3);                /* bset #3,obStatus(a0) */
        goto solid_top_ret;                     /* moveq #-1,d4 / rts */
    }
Solid_Miss:
    goto solid_noreq;                           /* moveq #0,d4 / rts */

solid_noreq:
    if (d3out) *d3out = d3;
    if (d5out) *d5out = d5;
    return 0;                                   /* moveq #0,d4 / rts */
}

/* --- _incObj/44 GHZ Edge Walls.asm ------------------------------------------
   Decorative GHZ edge walls. Solid when obSubtype bit 4 ($10) is clear,
   cosmetic-only when set. Solid via EdgeWall_SolidWall (sub SolidWall.asm). */

static void Edge_Display(uint8_t *o);
static void Edge_Solid(uint8_t *o);
static void EdgeWall_SolidWall(uint8_t *o, int16_t d1, int16_t d2);
static int EdgeWall_ChkCollision(uint8_t *o, int16_t d1, int16_t d2,
                                 int16_t *d0out, int16_t *d3out);

static void Edge_Main(uint8_t *o) {
    obRoutine(o) += 2;                          /* advance to Edge_Solid */
    obMap(o) = (uint32_t)(uintptr_t)Map_Edge;   /* load mappings */
    obGfx(o) = (uint16_t)(ArtTile_GHZ_Edge_Wall | Tile_Pal3);
    obRender(o) |= sprite_cam_field;            /* playfield-positioned mode */
    obActWid(o) = 16 / 2;                       /* sprite display width */
    obPriority(o) = 6;                          /* very low priority */

    obFrame(o) = obSubtype(o);                  /* copy type to frame number */
    if (obFrame(o) & 0x10) {                    /* bclr #4,obFrame / (Z=0) */
        obFrame(o) &= ~0x10;                    /* bclr #4,obFrame(a0) */
        obRoutine(o) += 2;                      /* advance to Edge_Display */
        Edge_Display(o);                        /* bra.s Edge_Display */
        return;
    }
    obFrame(o) &= ~0x10;                        /* bclr #4,obFrame(a0) (Z set) */
    Edge_Solid(o);                              /* beq.s Edge_Solid */
}

static void Edge_Solid(uint8_t *o) {
    EdgeWall_SolidWall(o, 38 / 2, 80 / 2);      /* collision detection width/height */
    Edge_Display(o);                            /* fall through to Edge_Display */
}

static void Edge_Display(uint8_t *o) {
    DisplaySprite(o);                           /* bsr.w DisplaySprite */
    if (OutOfRange(o, -1)) {                    /* out_of_range.w DeleteObject */
        DeleteObject(o);
    }
}

static void EdgeWalls_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                     /* Edge_Index: 0/2/4 */
        case 0:  Edge_Main(o);    break;
        case 2:  Edge_Solid(o);   break;
        case 4:  Edge_Display(o); break;
    }
}

/* --- _incObj/sub SolidWall.asm (FixBugs=0) ---------------------------------
   Stripped-down SolidObject: side push and top/bottom bump, no landing.
   Input: d1 = width, d2 = height/2. Returns d4 collision type to caller:
   0 = none, 1 = side collision, -1 = top/bottom collision. */

static int EdgeWall_ChkCollision(uint8_t *o, int16_t d1, int16_t d2,
                                 int16_t *d0out, int16_t *d3out) {
    uint8_t *a1 = RAM_ADDR(v_player);          /* lea (v_player).w,a1 */
    int16_t d0 = (int16_t)((int16_t)obX(a1) - (int16_t)obX(o) + d1); /* x rel + width */
    if (d0 < 0) return 0;                      /* bmi.s Edge_Ignore */
    int16_t d3 = (int16_t)(d1 + d1);           /* full width */
    if (d0 > d3) return 0;                     /* bhi.s Edge_Ignore */
    d3 = (int16_t)(int8_t)obHeight(a1);        /* move.b obHeight(a1),d3 / ext.w d3 */
    d2 = (int16_t)(d2 + d3);                   /* add obHeight to stated height */
    d3 = (int16_t)((int16_t)obY(a1) - (int16_t)obY(o)); /* y rel (+ve below) */
    d3 = (int16_t)(d3 + d2);                   /* add total height */
    if (d3 < 0) return 0;                      /* bmi.s Edge_Ignore */
    int16_t d4 = (int16_t)(d2 + d2);           /* full height */
    if (d3 >= d4) return 0;                    /* bhs.s Edge_Ignore */
    if ((int8_t)f_playerctrl < 0) return 0;    /* tst.b / bmi.s Edge_Ignore */
    if ((uint8_t)obRoutine(a1) >= 6) return 0; /* cmpi.b #6 / bhs.s Edge_Ignore */
    if (v_debuguse) return 0;                  /* tst.w (v_debuguse).w / bne.s */
    int16_t d5 = d0;                           /* move.w d0,d5 */
    if (d0 <= d1) goto isright;                /* cmp.w d0,d1 / bhs.s .isright */
    d1 = (int16_t)(d1 + d1);                   /* add.w d1,d1 */
    d0 = (int16_t)(d0 - d1);                   /* sub.w d1,d0 */
    d5 = (int16_t)(-d0);                       /* move.w d0,d5 / neg.w d5 */
isright:
    d1 = d3;                                   /* move.w d3,d1 */
    if (d3 <= d2) goto isbelow;                /* cmp.w d3,d2 / bhs.s .isbelow */
    d3 = (int16_t)(d3 - d4);                   /* sub.w d4,d3 */
    d1 = (int16_t)(-d3);                       /* move.w d3,d1 / neg.w d1 */
isbelow:
    if (d5 > d1) {                             /* cmp.w d1,d5 / bhi.s Edge_TopBottom */
        *d0out = d0;
        *d3out = d3;
        return -1;                             /* moveq #-1,d4 / rts */
    }
    *d0out = d0;
    *d3out = d3;
    return 1;                                  /* moveq #1,d4 / rts */
}

/* EdgeWall_SolidWall — act on the collision returned by ChkCollision. */
static void EdgeWall_SolidWall(uint8_t *o, int16_t d1, int16_t d2) {
    int16_t d0 = 0, d3 = 0;
    uint8_t *a1;
    int type;

    a1 = RAM_ADDR(v_player); /* ChkCollision leaves a1 = Sonic OST */
    type = EdgeWall_ChkCollision(o, d1, d2, &d0, &d3);
    if (type == 0) {                           /* beq.s .no_collision */
        if (obStatus(o) & (1 << 5)) {          /* btst #5,obStatus(a0) / beq.s .exit */
            obAnim(a1) = id_Run;               /* FixBugs=0 "walk-jump bug" */
        }
        /* .air */
        obStatus(o)  &= ~(1 << 5);             /* bclr #5,obStatus(a0) */
        obStatus(a1) &= ~(1 << 5);             /* bclr #5,obStatus(a1) */
        /* .exit */
        return;
    }
    if (type < 0) {                            /* bmi.w .topbottom */
        if (obVelY(a1) >= 0) return;           /* tst.w obVelY(a1) / bpl.s .exit2 */
        if (d3 >= 0) return;                   /* tst.w d3 / bpl.s .exit2 (above object) */
        obY(a1) = (int16_t)((int16_t)obY(a1) - d3); /* sub.w d3,obY(a1) */
        obVelY(a1) = 0;                        /* move.w #0,obVelY(a1) */
        /* .exit2 */
        return;
    }

    /* side collision: stop Sonic against the wall */
    if (d0 == 0) goto wall_centre;             /* tst.w d0 / beq.w .centre */
    if (d0 < 0) goto wall_right;               /* bmi.s .right */
    if (obVelX(a1) < 0) goto wall_centre;      /* tst.w obVelX(a1) / bmi.s .centre */
    goto wall_left;                            /* bra.s .left */
wall_right:
    if (obVelX(a1) >= 0) goto wall_centre;     /* tst.w obVelX(a1) / bpl.s .centre */
wall_left:
    obX(a1) = (int16_t)((int16_t)obX(a1) - d0);/* sub.w d0,obX(a1) */
    obInertia(a1) = 0;                         /* move.w #0,obInertia(a1) */
    obVelX(a1) = 0;                            /* move.w #0,obVelX(a1) */
wall_centre:
    if (obStatus(a1) & (1 << 1)) goto wall_air;/* btst #1,obStatus(a1) / bne.s .air */
    obStatus(a1) |= (1 << 5);                  /* bset #5,obStatus(a1): push object */
    obStatus(o)  |= (1 << 5);                  /* bset #5,obStatus(a0): be pushed */
    return;
wall_air:
    obStatus(o)  &= ~(1 << 5);                 /* bclr #5,obStatus(a0) */
    obStatus(a1) &= ~(1 << 5);                 /* bclr #5,obStatus(a1) */
}

/* ===========================================================================
   AnimateSprite - Port of _incObj/sub AnimateSprite.asm
   Input: obj = object pointer, anim_script = animation script pointer (a1)
   =========================================================================== */
void AnimateSprite(void *obj, const uint8_t *anim_script) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t anim_id = obAnim(o);

    if (anim_id != obPrevAni(o)) {
        obPrevAni(o) = anim_id;
        obAniFrame(o) = 0;
        obTimeFrame(o) = 0;
    }

    obTimeFrame(o)--;
    if ((int8_t)obTimeFrame(o) >= 0) {
        return; /* Anim_Wait */
    }

    /* Anim_LoadNextFrame */
    uint16_t offset = ((const uint16_t *)anim_script)[anim_id];
    const uint8_t *anim_data = anim_script + offset;

    obTimeFrame(o) = anim_data[0];
    uint8_t frame_idx = obAniFrame(o);
    uint8_t frame_id = anim_data[1 + frame_idx];

    if ((int8_t)frame_id >= 0) {
        /* Anim_SetFrameAndFlipFlags */
        obFrame(o) = frame_id & 0x1F;

        uint8_t status = obStatus(o);
        uint8_t render = obRender(o);
        uint8_t flip_bits = (frame_id >> 5) & (sprite_xflip | sprite_yflip);
        render = (render & ~(sprite_xflip | sprite_yflip)) | ((status ^ flip_bits) & (sprite_xflip | sprite_yflip));
        obRender(o) = render;

        obAniFrame(o)++;
    } else {
        /* Special animation flags */
        switch (frame_id) {
            case afEnd: /* $FF - loop to beginning */
                obAniFrame(o) = 0;
                frame_id = anim_data[1];
                {
                    obFrame(o) = frame_id & 0x1F;
                    uint8_t status = obStatus(o);
                    uint8_t render = obRender(o);
                    uint8_t flip_bits = (frame_id >> 5) & (sprite_xflip | sprite_yflip);
                    render = (render & ~(sprite_xflip | sprite_yflip)) | ((status ^ flip_bits) & (sprite_xflip | sprite_yflip));
                    obRender(o) = render;
                    obAniFrame(o) = 1;
                }
                break;

            case afBack: /* $FE - go back N frames */
                {
                    uint8_t back = anim_data[2 + frame_idx];
                    obAniFrame(o) -= back;
                    frame_idx = obAniFrame(o);
                    frame_id = anim_data[1 + frame_idx];
obFrame(o) = frame_id & 0x1F;
                        uint8_t status = obStatus(o);
                        uint8_t render = obRender(o);
                        uint8_t flip_bits = (frame_id >> 5) & (sprite_xflip | sprite_yflip);
                        render = (render & ~(sprite_xflip | sprite_yflip)) | ((status ^ flip_bits) & (sprite_xflip | sprite_yflip));
                        obRender(o) = render;
                        obAniFrame(o)++;
                }
                break;

            case afChange: /* $FD - change to different animation */
                obAnim(o) = anim_data[2 + frame_idx];
                break;

            case afRoutine: /* $FC - increment routine counter */
                obRoutine(o) += 2;
                break;

            case afReset: /* $FB - reset animation and 2nd routine */
                obAniFrame(o) = 0;
                ob2ndRout(o) = 0;
                break;

            case af2ndRoutine: /* $FA - increment 2nd routine counter */
                ob2ndRout(o) += 2;
                break;
        }
    }
}

/* ===========================================================================
   ExplosionItem (id_ExplosionItem = $27) - gray explosion from a destroyed
   enemy or monitor, plus Explosion (id_Explosion = $3F) - fiery explosion
   from destroyed boss, Walking Bomb, or Ball Hog cannonball.
   Ported from _incObj/27, 3F Explosions.asm (FixBugs=0).
   =========================================================================== */

/* Forward declarations: routines shared later in this file. */
static void ExItem_Main(uint8_t *o);
static void ExItem_Animate(uint8_t *o);

/* ExItem_Animal — Routine 0: spawn the animal that pops out of exploded
   badniks, then fall through to ExItem_Main. */
static void ExItem_Animal(uint8_t *o) {
    obRoutine(o) += 2;                            /* addq.b #2,obRoutine(a0) */

    uint8_t *a1 = (uint8_t *)FindFreeObj();       /* bsr.w FindFreeObj */
    if (a1 == NULL) {                             /* bne.s ExItem_Main: RAM full */
        ExItem_Main(o);                           /* idem: explosion still fires */
        return;
    }
    /* _move.b #id_Animals,obID(a1) */
    obID(a1) = id_Animals;
    obX(a1) = obX(o);                             /* move.w obX(a0),obX(a1) */
    obY(a1) = obY(o);                             /* move.w obY(a0),obY(a1) */
    /* move.w exitem_pointsframe(a0),animal_pointsframe(a1) */
    animal_pointsframe(a1) = exitem_pointsframe(o);
}

/* ExItem_Main — Routine 2 (also set directly for non-Badnik objects such as
   monitors), then falls through into ExItem_Animate. */
static void ExItem_Main(uint8_t *o) {
    obRoutine(o) += 2;                            /* addq.b #2,obRoutine(a0) */
    obMap(o)    = (uint32_t)(uintptr_t)Map_ExplodeItem; /* move.l #Map_ExplodeItem,obMap(a0) */
    obGfx(o)    = ArtTile_Explosion;              /* move.w #ArtTile_Explosion,obGfx(a0) */
    obRender(o) = sprite_cam_field;               /* move.b #sprite_cam_field,obRender(a0) */
    obPriority(o) = 1;                            /* move.b #1,obPriority(a0) */
    obColType(o) = col_none;                      /* move.b #col_none,obColType(a0) */
    obActWid(o)  = 24 / 2;                        /* move.b #24/2,obActWid(a0) */
    obTimeFrame(o) = 8 - 1;                       /* move.b #8-1,obTimeFrame(a0) */
    obFrame(o)   = 0;                             /* move.b #0,obFrame(a0) */
    Sound_Queue(sfx_BreakItem, false);            /* move.w #sfx_BreakItem,d0 / jsr (QueueSound2).l */
    ExItem_Animate(o);                            /* fall through to ExItem_Animate */
}

/* ExItem_Animate — Routine 4 (2 for Explosion): frame timer, delete after
   the final frame (05) is displayed. Holds the sprite. */
static void ExItem_Animate(uint8_t *o) {
    obTimeFrame(o) -= 1;                          /* subq.b #1,obTimeFrame(a0) */
    if ((int8_t)obTimeFrame(o) >= 0) {            /* bpl.s .display */
        DisplaySprite(o);                         /* bra.w DisplaySprite */
        return;
    }
    obTimeFrame(o) = 8 - 1;                       /* move.b #8-1,obTimeFrame(a0) */
    obFrame(o) += 1;                              /* addq.b #1,obFrame(a0) */
    if (obFrame(o) == 5) {                        /* cmpi.b #5,obFrame(a0) / beq.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                             /* .display: bra.w DisplaySprite */
}

/* ExplosionItem dispatcher — ExItem_Index: 0=Animal, 2=Main, 4=Animate.
   ExItem_Animal falls through into ExItem_Main (ASM: jmp into the routine). */
static void ExplosionItem_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                       /* ExItem_Index */
        case 0:
            ExItem_Animal(o);                    /* falls through to ExItem_Main */
            /* fall through */
        case 2: ExItem_Main(o);   break;
        case 4: ExItem_Animate(o); break;
    }
}

/* Expl_Main — Routine 0 for Explosion (3F). */
static void Expl_Main(uint8_t *o) {
    obRoutine(o) += 2;                            /* addq.b #2,obRoutine(a0) */
    obMap(o)    = (uint32_t)(uintptr_t)Map_ExplodeBomb; /* move.l #Map_ExplodeBomb,obMap(a0) */
    obGfx(o)    = ArtTile_Explosion;              /* move.w #ArtTile_Explosion,obGfx(a0) */
    obRender(o) = sprite_cam_field;               /* move.b #sprite_cam_field,obRender(a0) */
    obPriority(o) = 1;                            /* move.b #1,obPriority(a0) */
    obColType(o) = col_none;                      /* move.b #col_none,obColType(a0) */
    obActWid(o)  = 24 / 2;                        /* move.b #24/2,obActWid(a0) */
    obTimeFrame(o) = 8 - 1;                       /* move.b #8-1,obTimeFrame(a0) */
    obFrame(o)   = 0;                             /* move.b #0,obFrame(a0) */
    Sound_Queue(sfx_Bomb, false);                 /* move.w #sfx_Bomb,d0 / jmp (QueueSound2).l */
}

/* Explosion dispatcher — Expl_Index: 0=Main, 2=ExItem_Animate (27) */
static void Explosion_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                       /* Expl_Index */
        case 0: Expl_Main(o);     break;
        case 2: ExItem_Animate(o); break;         /* <-- branches to object 27 above */
    }
}

/* ===========================================================================
   Animals (id_Animals = $28) - animals from destroyed badniks, prison
   capsules, and the ending sequence.
   Ported from _incObj/28, 29 Animals and Points.asm.
   ===========================================================================
   Anml_VarIndex: two animal IDs per zone, must be "even/odd".               */

static const uint8_t Anml_VarIndex[12] = {   /* dc.b 0,5 / 2,3 / ... (6 zones x 2) */
    0, 5,                                   /* Green Hill Zone */
    2, 3,                                   /* Labyrinth Zone */
    6, 3,                                   /* Marble Zone */
    4, 5,                                   /* Star Light Zone */
    4, 1,                                   /* Spring Yard Zone */
    0, 1,                                   /* Scrap Brain Zone */
};

/* Anml_Variables: horizontal speed, vertical speed, mappings (1/2/3 maps
   resolved at runtime through Anml_MapFor, like Debug_MapForId). */
typedef struct {
    int16_t speedX;
    int16_t speedY;
    uint8_t mapsel;                          /* 1 = Map_Animal1, 2 = Map_Animal2, 3 = Map_Animal3 */
} Anml_Variables_t;

static const uint8_t *Anml_MapFor(uint8_t sel) {
    switch (sel) {
        case 2:  return Map_Animal2;
        case 3:  return Map_Animal3;
        default: return Map_Animal1;
    }
}

static const Anml_Variables_t Anml_Variables[7] = {
    { -0x200, -0x400, 1 },                  /* type 0: Pocky/bunny (GHZ/SBZ) */
    { -0x200, -0x300, 2 },                  /* type 1: Cucky/chicken (SYZ/SBZ) */
    { -0x180, -0x300, 1 },                  /* type 2: Pecky/penguin (LZ) */
    { -0x140, -0x180, 2 },                  /* type 3: Ricky/squirrel (MZ/LZ) */
    { -0x1C0, -0x300, 3 },                  /* type 4: Picky/pig (SYZ/SLZ) */
    { -0x300, -0x400, 2 },                  /* type 5: Flicky/bird (GHZ/SLZ) */
    { -0x280, -0x380, 3 },                  /* type 6: Rocky/seal (MZ) */
};

/* Ending sequence config; each entry one ending animal, subtype $A-$14 used
   as index (Anml_EndSpeed / Anml_EndMap / Anml_EndVram). */
static const int16_t Anml_EndSpeed[11][2] = {
    { -0x440, -0x400 },                     /* 0A - Flicky/bird (type A) */
    { -0x440, -0x400 },                     /* 0B - Flicky/bird (type B, unused) */
    { -0x440, -0x400 },                     /* 0C - Flicky/bird (type C) */
    { -0x300, -0x400 },                     /* 0D - Pocky/bunny (type A) */
    { -0x300, -0x400 },                     /* 0E - Pocky/bunny (type B) */
    { -0x180, -0x300 },                     /* 0F - Pecky/penguin (type A) */
    { -0x180, -0x300 },                     /* 10 - Pecky/penguin (type B) */
    { -0x140, -0x180 },                     /* 11 - Rocky/seal */
    { -0x1C0, -0x300 },                     /* 12 - Picky/pig */
    { -0x200, -0x300 },                     /* 13 - Cucky/chicken */
    { -0x280, -0x380 },                     /* 14 - Ricky/squirrel */
};

static const uint8_t Anml_EndMap[11] = {
    2,                                      /* 0A - Flicky/bird (type A) */
    2,                                      /* 0B - Flicky/bird (type B, unused) */
    2,                                      /* 0C - Flicky/bird (type C) */
    1,                                      /* 0D - Pocky/bunny (type A) */
    1,                                      /* 0E - Pocky/bunny (type B) */
    1,                                      /* 0F - Pecky/penguin (type A) */
    1,                                      /* 10 - Pecky/penguin (type B) */
    2,                                      /* 11 - Rocky/seal */
    3,                                      /* 12 - Picky/pig */
    2,                                      /* 13 - Cucky/chicken */
    3,                                      /* 14 - Ricky/squirrel */
};

static const uint16_t Anml_EndVram[11] = {
    ArtTile_Ending_Flicky,                  /* 0A - Flicky/bird (type A) */
    ArtTile_Ending_Flicky,                  /* 0B - Flicky/bird (type B, unused) */
    ArtTile_Ending_Flicky,                  /* 0C - Flicky/bird (type C) */
    ArtTile_Ending_Rabbit,                  /* 0D - Pocky/bunny (type A) */
    ArtTile_Ending_Rabbit,                  /* 0E - Pocky/bunny (type B) */
    ArtTile_Ending_Penguin,                 /* 0F - Pecky/penguin (type A) */
    ArtTile_Ending_Penguin,                 /* 10 - Pecky/penguin (type B) */
    ArtTile_Ending_Seal,                    /* 11 - Rocky/seal */
    ArtTile_Ending_Pig,                     /* 12 - Picky/pig */
    ArtTile_Ending_Chicken,                 /* 13 - Cucky/chicken */
    ArtTile_Ending_Squirrel,                /* 14 - Ricky/squirrel */
};

/* Anml_Main — Routine 0: pick the animal to spawn. */
static void Anml_FromEnemy(uint8_t *o);
static void Anml_End_ChkDel(uint8_t *o);
static void Anml_CheckCloseToSonic(uint8_t *o, int *bhs, int *bpl);
static void Anml_NormalGravity(uint8_t *o);
static void Anml_SlowGravity(uint8_t *o);
static void Anml_End_Bounce(uint8_t *o);
static void Anml_End_FaceSonic(uint8_t *o);

/* Anml_Main — Routine 0 */
static void Anml_Main(uint8_t *o) {
    if (obSubtype(o) == 0) {                /* tst.b obSubtype / beq.w Anml_FromEnemy */
        Anml_FromEnemy(o);
        return;
    }

    /* Ending sequence animal with custom subtype ($A-$14) */
    int S = obSubtype(o);
    obRoutine(o) = (uint8_t)(S * 2);        /* add.w d0,d0 ; move.b d0,obRoutine(a0) */
    int idx = S - 0x0A;                     /* subi.w #$14,d0 ; /2 */
    obGfx(o) = Anml_EndVram[idx];           /* move.w Anml_EndVram(pc,d0.w),obGfx(a0) */
    obMap(o) = (uint32_t)(uintptr_t)Anml_MapFor(Anml_EndMap[idx]); /* move.l ... obMap */
    animal_speedX(o) = Anml_EndSpeed[idx][0];   /* move.w (a1,d0.w),animal_speedX(a0) */
    obVelX(o)        = Anml_EndSpeed[idx][0];
    animal_speedY(o) = Anml_EndSpeed[idx][1];   /* move.w 2(a1,d0.w),animal_speedY(a0) */
    obVelY(o)        = Anml_EndSpeed[idx][1];

    obHeight(o)     = 24 / 2;               /* move.b #24/2,obHeight(a0) */
    obRender(o)     = sprite_cam_field;     /* move.b #sprite_cam_field,obRender(a0) */
    obRender(o)    |= (1 << sprite_xflip_bit); /* bset #sprite_xflip_bit,obRender(a0) */
    obPriority(o)   = 6;                    /* move.b #6,obPriority(a0) */
    obActWid(o)     = 16 / 2;               /* move.b #16/2,obActWid(a0) */
    obTimeFrame(o)  = 8 - 1;                /* move.b #8-1,obTimeFrame(a0) */
    DisplaySprite(o);                       /* bra.w DisplaySprite */
}

/* Anml_FromEnemy — animal from a destroyed badnik. */
static void Anml_FromEnemy(uint8_t *o) {
    obRoutine(o) += 2;                      /* addq.b #2,obRoutine(a0) -> Anml_ChkFloor */

    uint16_t rand = RandomNumber() & 1;     /* bsr.w RandomNumber ; andi.w #1,d0 */
    uint8_t zone  = v_zone;                 /* move.b (v_zone).w,d1 */
    uint8_t animal = Anml_VarIndex[zone * 2 + rand]; /* add.w d1,d1 ; add.w d0,d1 ; move.b (a1,d1.w),d0 */
    animal_id(o) = animal;                  /* move.b d0,animal_id(a0) */

    const Anml_Variables_t *v = &Anml_Variables[animal];   /* lsl.w #3,d0 */
    animal_speedX(o) = v->speedX;           /* move.w (a1)+,animal_speedX(a0) */
    animal_speedY(o) = v->speedY;           /* move.w (a1)+,animal_speedY(a0) */
    obMap(o) = (uint32_t)(uintptr_t)Anml_MapFor(v->mapsel); /* move.l (a1)+,obMap(a0) */

    obGfx(o) = ArtTile_Animal_1;            /* move.w #ArtTile_Animal_1,obGfx(a0) */
    if (animal_id(o) & 1) {                 /* btst #0,animal_id(a0) / beq.s .setupAnimal */
        obGfx(o) = ArtTile_Animal_2;        /* move.w #ArtTile_Animal_2,obGfx(a0) */
    }
    /* .setupAnimal */
    obHeight(o)    = 24 / 2;                /* move.b #24/2,obHeight(a0) */
    obRender(o)    = sprite_cam_field;      /* move.b #sprite_cam_field,obRender(a0) */
    obRender(o)   |= (1 << sprite_xflip_bit); /* bset #sprite_xflip_bit,obRender(a0) */
    obPriority(o)  = 6;                     /* move.b #6,obPriority(a0) */
    obActWid(o)    = 16 / 2;                /* move.b #16/2,obActWid(a0) */
    obTimeFrame(o) = 8 - 1;                 /* move.b #8-1,obTimeFrame(a0) */
    obFrame(o)     = 2;                     /* move.b #2,obFrame(a0) */
    obVelY(o)      = (int16_t)-0x400;       /* move.w #-$400,obVelY(a0) */

    if (v_bossstatus != 0) {                /* tst.b (v_bossstatus).w / bne.s .fromPrison */
        /* .fromPrison */
        obRoutine(o) = 0x12;                /* move.b #$12,obRoutine(a0) */
        obVelX(o)    = 0;                   /* clr.w obVelX(a0) */
        DisplaySprite(o);
        return;
    }

    /* spawn the points object */
    uint8_t *a1 = (uint8_t *)FindFreeObj(); /* bsr.w FindFreeObj */
    if (a1 == NULL) {                       /* bne.s .display */
        DisplaySprite(o);
        return;
    }
    obID(a1) = id_Points;                   /* _move.b #id_Points,obID(a1) */
    obX(a1)  = obX(o);                      /* move.w obX(a0),obX(a1) */
    obY(a1)  = obY(o);                      /* move.w obY(a0),obY(a1) */
    /* move.w animal_pointsframe(a0),d0 ; lsr.w #1,d0 ; move.b d0,obFrame(a1) */
    obFrame(a1) = (uint8_t)(animal_pointsframe(o) >> 1);
    /* .display */
    DisplaySprite(o);
}

/* Anml_CheckCloseToSonic — d0 = playerX - animalX - 184. Sets both flags
   the callers branch on: bhs = no borrow (Sonic > 184px right), bpl = N clear. */
static void Anml_CheckCloseToSonic(uint8_t *o, int *bhs, int *bpl) {
    uint8_t *player = (uint8_t *)RAM_ADDR(v_player);
    uint16_t pre = (uint16_t)((uint16_t)obX(player) - (uint16_t)obX(o)); /* move.w ; sub.w */
    int16_t d0f  = (int16_t)(pre - 184);    /* subi.w #(320/2)+24,d0 */
    *bhs = pre >= 184;                      /* CC clear after subi */
    *bpl = d0f >= 0;                        /* N clear after subi */
}

/* Anml_ChkFloor — Routine 2: wait for first floor hit after initial spawn. */
static void Anml_ChkFloor(uint8_t *o) {
    if (!(obRender(o) & sprite_rendered)) { /* tst.b obRender / bpl.w DeleteObject */
        DeleteObject(o);
        return;
    }

    ObjectFall(o);                          /* bsr.w ObjectFall */
    if ((int16_t)obVelY(o) < 0) {           /* tst.w obVelY / bmi.s .display */
        DisplaySprite(o);
        return;
    }

    int16_t d1, angle;
    ObjFloorDist(o, &d1, &angle);           /* jsr (ObjFloorDist).l */
    if (d1 >= 0) {                          /* tst.w d1 / bpl.s .display */
        DisplaySprite(o);
        return;
    }
    obY(o) = (int16_t)(obY(o) + d1);        /* add.w d1,obY(a0) */
    obVelX(o) = animal_speedX(o);           /* move.w animal_speedX(a0),obVelX(a0) */
    obVelY(o) = animal_speedY(o);           /* move.w animal_speedY(a0),obVelY(a0) */
    obFrame(o) = 1;                         /* move.b #1,obFrame(a0) */

    obRoutine(o) = (uint8_t)(animal_id(o) * 2 + 4); /* move.b animal_id; add.b d0,d0; addq.b #4 */

    if (v_bossstatus != 0) {                /* tst.b (v_bossstatus).w / beq.s .display */
        if (v_vblank_byte & (1 << 4)) {     /* btst #4,(v_vblank_byte).w / beq.s .display */
            obVelX(o) = (int16_t)-obVelX(o);    /* neg.w obVelX(a0) */
            obRender(o) ^= (1 << sprite_xflip_bit); /* bchg #sprite_xflip_bit,obRender(a0) */
        }
    }
    /* .display */
    DisplaySprite(o);
}

/* Anml_End_ChkDel — ending animals offscreen delete helper. */
static void Anml_End_ChkDel(uint8_t *o) {
    uint8_t *player = (uint8_t *)RAM_ADDR(v_player);
    /* move.w obX(a0),d0 ; sub.w (v_player+obX).w,d0 */
    uint16_t pre = (uint16_t)((uint16_t)obX(o) - (uint16_t)obX(player));
    if ((uint16_t)obX(o) < (uint16_t)obX(player)) {  /* blo.s .display (borrow) */
        DisplaySprite(o);
        return;
    }
    int16_t d0f = (int16_t)(pre - 384);     /* subi.w #320+64,d0 */
    if (d0f >= 0) {                         /* bpl.s .display */
        DisplaySprite(o);
        return;
    }
    if (!(obRender(o) & sprite_rendered)) { /* tst.b obRender(a0) / bpl.w DeleteObject */
        DeleteObject(o);
        return;
    }
    /* .display */
    DisplaySprite(o);
}

/* Anml_NormalGravity — Routine 4/8/A/C/10: normal gravity, animate on floor hit. */
static void Anml_NormalGravity(uint8_t *o) {
    ObjectFall(o);                          /* bsr.w ObjectFall */
    obFrame(o) = 1;                         /* move.b #1,obFrame(a0) */
    if ((int16_t)obVelY(o) >= 0) {          /* tst.w obVelY / bmi.s .chkDel */
        obFrame(o) = 0;                     /* move.b #0,obFrame(a0) */
        int16_t d1, angle;
        ObjFloorDist(o, &d1, &angle);       /* jsr (ObjFloorDist).l */
        if (d1 >= 0) goto chkDel;           /* tst.w d1 / bpl.s .chkDel */
        obY(o) = (int16_t)(obY(o) + d1);    /* add.w d1,obY(a0) */
        obVelY(o) = animal_speedY(o);       /* move.w animal_speedY(a0),obVelY(a0) */
    }
chkDel:
    if (obSubtype(o) != 0) {                /* tst.b obSubtype(a0) / bne.s Anml_End_ChkDel */
        Anml_End_ChkDel(o);
        return;
    }
    if (!(obRender(o) & sprite_rendered)) { /* tst.b obRender(a0) / bpl.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                       /* bra.w DisplaySprite */
}

/* Anml_SlowGravity — Routine 6/E: reduced gravity, animate every other frame. */
static void Anml_SlowGravity(uint8_t *o) {
    SpeedToPos(o);                          /* bsr.w SpeedToPos */
    obVelY(o) = (int16_t)(obVelY(o) + 0x18); /* addi.w #$18,obVelY(a0) */
    if ((int16_t)obVelY(o) >= 0) {          /* tst.w obVelY / bmi.s .animate */
        int16_t d1, angle;
        ObjFloorDist(o, &d1, &angle);       /* jsr (ObjFloorDist).l */
        if (d1 >= 0) goto animate;          /* tst.w d1 / bpl.s .animate */
        obY(o) = (int16_t)(obY(o) + d1);    /* add.w d1,obY(a0) */
        obVelY(o) = animal_speedY(o);       /* move.w animal_speedY(a0),obVelY(a0) */
        if (obSubtype(o) != 0 && obSubtype(o) != 0x0A) { /* tst.b ; cmpi.b #$A / beq.s */
            obVelX(o) = (int16_t)-obVelX(o);   /* neg.w obVelX(a0) */
            obRender(o) ^= (1 << sprite_xflip_bit); /* bchg #sprite_xflip_bit */
        }
    }
animate:
    obTimeFrame(o)--;                       /* subq.b #1,obTimeFrame(a0) */
    if ((int8_t)obTimeFrame(o) >= 0) goto chkDel; /* bpl.s .chkDel */
    obTimeFrame(o) = 2 - 1;                 /* move.b #2-1,obTimeFrame(a0) */
    obFrame(o) = (uint8_t)((obFrame(o) + 1) & 1); /* addq.b #1 ; andi.b #1 */
chkDel:
    if (obSubtype(o) != 0) {                /* tst.b obSubtype(a0) / bne.s Anml_End_ChkDel */
        Anml_End_ChkDel(o);
        return;
    }
    if (!(obRender(o) & sprite_rendered)) { /* tst.b obRender(a0) / bpl.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                       /* bra.w DisplaySprite */
}

/* Anml_FromPrison — Routine $12: delay hopping out of the prison capsule. */
static void Anml_FromPrison(uint8_t *o) {
    if (!(obRender(o) & sprite_rendered)) { /* tst.b obRender / bpl.w DeleteObject */
        DeleteObject(o);
        return;
    }
    animal_prisondelay(o)--;                /* subq.w #1,animal_prisondelay(a0) */
    if (animal_prisondelay(o) != 0) {       /* bne.w .display */
        DisplaySprite(o);
        return;
    }
    obRoutine(o) = 2;                       /* move.b #2,obRoutine(a0) -> Anml_ChkFloor */
    obPriority(o) = 3;                      /* move.b #3,obPriority(a0) */
    /* .display */
    DisplaySprite(o);
}

/* Anml_End_FlyLeft — Routine $14/$16 */
static void Anml_End_FlyLeft(uint8_t *o) {
    int bhs, bpl;
    Anml_CheckCloseToSonic(o, &bhs, &bpl);  /* bsr.w Anml_CheckCloseToSonic */
    if (bhs) {                              /* bhs.s .chkDel */
        Anml_End_ChkDel(o);
        return;
    }
    obVelX(o) = animal_speedX(o);           /* move.w animal_speedX(a0),obVelX(a0) */
    obVelY(o) = animal_speedY(o);           /* move.w animal_speedY(a0),obVelY(a0) */
    obRoutine(o) = 0x0E;                    /* move.b #$E,obRoutine(a0) -> Anml_SlowGravity */
    Anml_SlowGravity(o);                    /* bra.w Anml_SlowGravity */
}

/* Anml_End_StayFace_Slow — Routine $18 */
static void Anml_End_StayFace_Slow(uint8_t *o) {
    int bhs, bpl;
    Anml_CheckCloseToSonic(o, &bhs, &bpl);  /* bsr.w Anml_CheckCloseToSonic */
    if (bpl) goto chkDel;                   /* bpl.s .chkDel */

    obVelX(o) = 0;                          /* clr.w obVelX(a0) */
    animal_speedX(o) = 0;                   /* clr.w animal_speedX(a0) */
    SpeedToPos(o);                          /* bsr.w SpeedToPos */
    obVelY(o) = (int16_t)(obVelY(o) + 0x18); /* addi.w #$18,obVelY(a0) */
    Anml_End_Bounce(o);                     /* bsr.w Anml_End_Bounce */
    Anml_End_FaceSonic(o);                  /* bsr.w Anml_End_FaceSonic */

    obTimeFrame(o)--;                       /* subq.b #1,obTimeFrame(a0) */
    if ((int8_t)obTimeFrame(o) < 0) {       /* bpl.s .chkDel */
        obTimeFrame(o) = 2 - 1;             /* move.b #2-1,obTimeFrame(a0) */
        obFrame(o) = (uint8_t)((obFrame(o) + 1) & 1); /* addq.b #1 ; andi.b #1 */
    }
chkDel:
    Anml_End_ChkDel(o);                     /* bra.w Anml_End_ChkDel */
}

/* Anml_End_HopLeft — Routine $1A */
static void Anml_End_HopLeft(uint8_t *o) {
    int bhs, bpl;
    Anml_CheckCloseToSonic(o, &bhs, &bpl);  /* bsr.w Anml_CheckCloseToSonic */
    if (bpl) {                              /* bpl.s Anml_End_DoubleHop.chkDel */
        Anml_End_ChkDel(o);
        return;
    }
    obVelX(o) = animal_speedX(o);           /* move.w animal_speedX(a0),obVelX(a0) */
    obVelY(o) = animal_speedY(o);           /* move.w animal_speedY(a0),obVelY(a0) */
    obRoutine(o) = 4;                       /* move.b #4,obRoutine(a0) -> Anml_NormalGravity */
    Anml_NormalGravity(o);                  /* bra.w Anml_NormalGravity */
}

/* Anml_End_StayFace_Fast — Routine $1C/$20/$24 */
static void Anml_End_StayFace_Fast(uint8_t *o) {
    int bhs, bpl;
    Anml_CheckCloseToSonic(o, &bhs, &bpl);  /* bsr.w Anml_CheckCloseToSonic */
    if (bpl) goto chkDel;                   /* bpl.s .chkDel */

    obVelX(o) = 0;                          /* clr.w obVelX(a0) */
    animal_speedX(o) = 0;                   /* clr.w animal_speedX(a0) */
    ObjectFall(o);                          /* bsr.w ObjectFall */
    Anml_End_Bounce(o);                     /* bsr.w Anml_End_Bounce */
    Anml_End_FaceSonic(o);                  /* bsr.w Anml_End_FaceSonic */
chkDel:
    Anml_End_ChkDel(o);                     /* bra.w Anml_End_ChkDel */
}

/* Anml_End_HopAround — Routine $1E/$22 */
static void Anml_End_HopAround(uint8_t *o) {
    int bhs, bpl;
    Anml_CheckCloseToSonic(o, &bhs, &bpl);  /* bsr.w Anml_CheckCloseToSonic */
    if (bpl) goto chkDel;                   /* bpl.s .chkDel */

    ObjectFall(o);                          /* bsr.w ObjectFall */
    obFrame(o) = 1;                         /* move.b #1,obFrame(a0) */
    if ((int16_t)obVelY(o) >= 0) {          /* tst.w obVelY / bmi.s .chkDel */
        obFrame(o) = 0;                     /* move.b #0,obFrame(a0) */
        int16_t d1, angle;
        ObjFloorDist(o, &d1, &angle);       /* jsr (ObjFloorDist).l */
        if (d1 >= 0) goto chkDel;           /* tst.w d1 / bpl.s .chkDel */
        obVelX(o) = (int16_t)-obVelX(o);    /* neg.w obVelX(a0) */
        obRender(o) ^= (1 << sprite_xflip_bit); /* bchg #sprite_xflip_bit */
        obY(o) = (int16_t)(obY(o) + d1);    /* add.w d1,obY(a0) */
        obVelY(o) = animal_speedY(o);       /* move.w animal_speedY(a0),obVelY(a0) */
    }
chkDel:
    Anml_End_ChkDel(o);                     /* bra.w Anml_End_ChkDel */
}

/* Anml_End_DoubleFly — Routine $26 */
static void Anml_End_DoubleFly(uint8_t *o) {
    int bhs, bpl;
    Anml_CheckCloseToSonic(o, &bhs, &bpl);  /* bsr.w Anml_CheckCloseToSonic */
    if (bpl) goto chkDel;                   /* bpl.s .chkDel */

    SpeedToPos(o);                          /* bsr.w SpeedToPos */
    obVelY(o) = (int16_t)(obVelY(o) + 0x18); /* addi.w #$18,obVelY(a0) */
    if ((int16_t)obVelY(o) >= 0) {          /* tst.w obVelY / bmi.s .animate */
        int16_t d1, angle;
        ObjFloorDist(o, &d1, &angle);       /* jsr (ObjFloorDist).l */
        if (d1 >= 0) goto animate;          /* tst.w d1 / bpl.s .animate */
        animal_doublehop(o) = (uint8_t)~(animal_doublehop(o));  /* not.b */
        if (animal_doublehop(o) == 0) {     /* bne.s .bounce */
            obVelX(o) = (int16_t)-obVelX(o);    /* neg.w obVelX(a0) */
            obRender(o) ^= (1 << sprite_xflip_bit); /* bchg #sprite_xflip_bit */
        }
        /* .bounce */
        obY(o) = (int16_t)(obY(o) + d1);    /* add.w d1,obY(a0) */
        obVelY(o) = animal_speedY(o);       /* move.w animal_speedY(a0),obVelY(a0) */
    }
animate:
    obTimeFrame(o)--;                       /* subq.b #1,obTimeFrame(a0) */
    if ((int8_t)obTimeFrame(o) >= 0) goto chkDel; /* bpl.s .chkDel */
    obTimeFrame(o) = 2 - 1;                 /* move.b #2-1,obTimeFrame(a0) */
    obFrame(o) = (uint8_t)((obFrame(o) + 1) & 1); /* addq.b #1 ; andi.b #1 */
chkDel:
    Anml_End_ChkDel(o);                     /* bra.w Anml_End_ChkDel */
}

/* Anml_End_DoubleHop — Routine $28 */
static void Anml_End_DoubleHop(uint8_t *o) {
    ObjectFall(o);                          /* bsr.w ObjectFall */
    obFrame(o) = 1;                         /* move.b #1,obFrame(a0) */
    if ((int16_t)obVelY(o) >= 0) {          /* tst.w obVelY / bmi.s .chkDel */
        obFrame(o) = 0;                     /* move.b #0,obFrame(a0) */
        int16_t d1, angle;
        ObjFloorDist(o, &d1, &angle);       /* jsr (ObjFloorDist).l */
        if (d1 >= 0) goto chkDel;           /* tst.w d1 / bpl.s .chkDel */
        animal_doublehop(o) = (uint8_t)~(animal_doublehop(o));  /* not.b */
        if (animal_doublehop(o) == 0) {     /* bne.s .bounce */
            obVelX(o) = (int16_t)-obVelX(o);    /* neg.w obVelX(a0) */
            obRender(o) ^= (1 << sprite_xflip_bit); /* bchg #sprite_xflip_bit */
        }
        /* .bounce */
        obY(o) = (int16_t)(obY(o) + d1);    /* add.w d1,obY(a0) */
        obVelY(o) = animal_speedY(o);       /* move.w animal_speedY(a0),obVelY(a0) */
    }
chkDel:
    Anml_End_ChkDel(o);                     /* bra.w Anml_End_ChkDel */
}

/* Anml_End_Bounce — bounce and animate helper (returns, not dispatch). */
static void Anml_End_Bounce(uint8_t *o) {
    obFrame(o) = 1;                         /* move.b #1,obFrame(a0) */
    if ((int16_t)obVelY(o) < 0) return;     /* tst.w obVelY / bmi.s .return */
    obFrame(o) = 0;                         /* move.b #0,obFrame(a0) */
    int16_t d1, angle;
    ObjFloorDist(o, &d1, &angle);           /* jsr (ObjFloorDist).l */
    if (d1 >= 0) return;                    /* tst.w d1 / bpl.s .return */
    obY(o) = (int16_t)(obY(o) + d1);        /* add.w d1,obY(a0) */
    obVelY(o) = animal_speedY(o);           /* move.w animal_speedY(a0),obVelY(a0) */
    /* .return: rts */
}

/* Anml_End_FaceSonic — face Sonic through the X-flip flag. */
static void Anml_End_FaceSonic(uint8_t *o) {
    uint8_t *player = (uint8_t *)RAM_ADDR(v_player);
    obRender(o) |= (1 << sprite_xflip_bit); /* bset #sprite_xflip_bit,obRender(a0) -> face left */
    /* move.w obX(a0),d0 ; sub.w (v_player+obX).w,d0 ; bhs.s .return */
    if ((uint16_t)obX(o) >= (uint16_t)obX(player)) return;
    obRender(o) &= (uint8_t)~(1 << sprite_xflip_bit); /* bclr -> face right */
}

/* Animals dispatcher — Anml_Index */
static void Animals_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                 /* Anml_Index */
        case 0x00: Anml_Main(o);               break;   /* init */
        case 0x02: Anml_ChkFloor(o);           break;   /* wait for first floor hit */
        case 0x04: case 0x08: case 0x0A: case 0x0C: case 0x10:
            Anml_NormalGravity(o);           break;   /* types 0/2/3/4/6 */
        case 0x06: case 0x0E:
            Anml_SlowGravity(o);             break;   /* types 1/5 */
        case 0x12: Anml_FromPrison(o);       break;   /* prison capsule */
        case 0x14: case 0x16:
            Anml_End_FlyLeft(o);             break;   /* ending Flicky A/B */
        case 0x18: Anml_End_StayFace_Slow(o);break;   /* ending Flicky C */
        case 0x1A: Anml_End_HopLeft(o);      break;   /* ending Pocky A */
        case 0x1C: case 0x20: case 0x24:
            Anml_End_StayFace_Fast(o);       break;   /* ending Pocky B / Penguin B / Pig */
        case 0x1E: case 0x22:
            Anml_End_HopAround(o);           break;   /* ending Penguin A / Seal */
        case 0x26: Anml_End_DoubleFly(o);    break;   /* ending Cucky/chicken */
        case 0x28: Anml_End_DoubleHop(o);    break;   /* ending Ricky/squirrel */
    }
}

/* ===========================================================================
   Points (id_Points = $29) - points that appear from destroyed badniks.
   Uses the FixBugs=0 path: the routine is jsr'd and Points_Main always
   calls DisplaySprite afterwards (even for a just-deleted slot; harmless,
   as BuildSprites skips obID==0 objects).
   =========================================================================== */

/* Forward declaration: Poi_Slower is defined below Poi_Main but called from it. */
static void Poi_Slower(uint8_t *o);

/* Poi_Main — Routine 0, falls through into Poi_Slower. */
static void Poi_Main(uint8_t *o) {
    obRoutine(o) += 2;                      /* addq.b #2,obRoutine(a0) -> Poi_Slower */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Points;   /* move.l #Map_Points,obMap(a0) */
    obGfx(o)     = (uint16_t)(ArtTile_Points | Tile_Pal2); /* move.w #ArtTile_Points|Tile_Pal2 */
    obRender(o)  = sprite_cam_field;        /* move.b #sprite_cam_field,obRender(a0) */
    obPriority(o) = 1;                      /* move.b #1,obPriority(a0) */
    obActWid(o)  = 16 / 2;                  /* move.b #16/2,obActWid(a0) */
    obVelY(o)    = (int16_t)-0x300;         /* move.w #-$300,obVelY(a0) */
    Poi_Slower(o);                          /* (falls through in ASM) */
}

/* Poi_Slower — Routine 2 */
static void Poi_Slower(uint8_t *o) {
    if ((int16_t)obVelY(o) >= 0) {          /* tst.w obVelY / bpl.w DeleteObject */
        DeleteObject(o);
        return;
    }
    SpeedToPos(o);                          /* bsr.w SpeedToPos */
    obVelY(o) = (int16_t)(obVelY(o) + 0x18); /* addi.w #$18,obVelY(a0) */
    /* FixBugs=0: rts — return to Points_Main for DisplaySprite */
}

/* Points dispatcher — Poi_Index: 0=Main, 2=Slower */
static void Points_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                 /* Poi_Index */
        case 0: Poi_Main(o);   break;
        case 2: Poi_Slower(o); break;
    }
    DisplaySprite(o);                       /* FixBugs=0: bra.w DisplaySprite after jsr */
}

/* ===========================================================================
   Object 26 — Monitors (id_Monitor = $26)
   Object 2E — Monitor contents / Power-ups (id_PowerUp = $2E)
   Ported from _incObj/26, 2E Monitors and Power-Ups.asm
   (REV01, FixBugs=0).
   =========================================================================== */

/* Mon_SolidSides — make the sides of a monitor solid.
   Input:  d1 = width/2, d2 = height/2
   Output: *d0out = distance from side of monitor
           *d3out = distance from top of monitor
   Returns collision type: 0 = none, 1 = side, -1 = top/bottom. */
static int16_t Mon_SolidSides(uint8_t *o, int16_t d1, int16_t d2,
                              int16_t *d0out, int16_t *d3out) {
    uint8_t *a1 = RAM_ADDR(v_player);
    int16_t d0 = (int16_t)((int16_t)obX(a1) - (int16_t)obX(o) + d1);
    int16_t d3;

    if (d0 < 0) goto no_collision;               /* bmi.s .no_collision */

    d3 = (int16_t)(d1 + d1);                     /* move.w d1,d3 / add.w d3,d3 */
    if ((uint16_t)d0 > (uint16_t)d3) goto no_collision; /* bhi.s */

    d3 = (int16_t)(int8_t)obHeight(a1);          /* move.b obHeight(a1),d3 / ext.w */
    d2 = (int16_t)(d2 + d3);                     /* add.w d3,d2 */
    d3 = (int16_t)((int16_t)obY(a1) - (int16_t)obY(o) + d2); /* sub + add */
    if (d3 < 0) goto no_collision;               /* bmi.s */
    d2 = (int16_t)(d2 + d2);                     /* add.w d2,d2 */
    if ((uint16_t)d3 >= (uint16_t)d2) goto no_collision; /* bcc.s */

    if ((int8_t)f_playerctrl < 0) goto no_collision; /* tst.b / bmi.s */
    if ((uint8_t)obRoutine(a1) >= 6) goto no_collision; /* cmpi.b #6 / bhs.s */
    if (v_debuguse) goto no_collision;           /* tst.w / bne.s */

    if ((uint16_t)d0 < (uint16_t)d1) {
        /* .left_hit: Sonic between left side and middle */
    } else {
        /* .right_hit */
        d1 = (int16_t)(d1 + d1);                 /* add.w d1,d1 */
        d0 = (int16_t)(d0 - d1);                 /* sub.w d1,d0 */
    }
    /* .left_hit */
    if ((uint16_t)d3 < 0x10) {
        /* .top_hit */
        int16_t d1b = (int16_t)((int8_t)obActWid(o) + 4); /* moveq #0,d1 / move.b / addq #4 */
        int16_t d2b = (int16_t)(d1b + d1b);      /* move.w d1,d2 / add.w d2,d2 */
        d1b = (int16_t)(d1b + (int16_t)obX(a1) - (int16_t)obX(o)); /* add obX(a1) / sub obX(a0) */
        if (d1b < 0) goto side_hit;              /* bmi.s .side_hit */
        if ((uint16_t)d1b >= (uint16_t)d2b) goto side_hit; /* cmp.w d2,d1 / bhs.s */
        if (d0out) *d0out = d0;
        if (d3out) *d3out = d3;
        return -1;                               /* moveq #-1,d1 */
    }
side_hit:
    if (d0out) *d0out = d0;
    if (d3out) *d3out = d3;
    return 1;                                    /* moveq #1,d1 */

no_collision:
    if (d0out) *d0out = d0;
    if (d3out) *d3out = 0;
    return 0;                                    /* moveq #0,d1 */
}

/* Mon_Main — Routine 0 */
static void Mon_Main(uint8_t *o) {
    /* FixBugs=0: no conversion of invalid subtypes to invisibarriers. */

    obRoutine(o) += 2;                           /* addq.b #2 */
    obHeight(o)  = 28 / 2;                       /* move.b #28/2,obHeight */
    obWidth(o)   = 28 / 2;                       /* move.b #28/2,obWidth */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Monitor; /* move.l #Map_Monitor */
    obGfx(o)     = ArtTile_Monitor;              /* move.w #ArtTile_Monitor */
    obRender(o)  = sprite_cam_field;             /* move.b #sprite_cam_field */
    obPriority(o)= 3;                            /* move.b #3 */
    obActWid(o)  = 30 / 2;                       /* move.b #30/2 */

    {
        uint8_t *a2 = RAM_ADDR(v_objstate);
        uint8_t d0 = obRespawnNo(o);             /* moveq #0,d0 / move.b obRespawnNo */
        /* FixBugs=0: bclr #7 relocated to RememberState; skipped here. */
        if (a2[2 + d0] & 1) {                    /* btst #0,2(a2,d0.w) / beq.s .notbroken */
            obRoutine(o) = 8;                    /* move.b #8,obRoutine */
            obFrame(o)   = 0x0B;                 /* move.b #$B,obFrame */
            return;                              /* rts */
        }
    }

    obColType(o) = (uint8_t)(col_32x32 | col_item); /* move.b #col_32x32|col_item */
    obAnim(o)    = obSubtype(o);                 /* move.b obSubtype,obAnim */
    /* fall through into Mon_Solid */
    /* (ASM: falls through to Mon_Solid) */
    /* We call it here explicitly. */
    /* Note: Mon_Solid is declared below; forward-declared above. */
    {
        /* Inline tail-call: Mon_Solid(o) */
        /* --- Mon_Solid body begins here --- */
        uint8_t *a1 = RAM_ADDR(v_player);
        uint8_t d0 = ob2ndRout(o);
        int16_t d0_out = 0, d3_out = 0;
        int16_t coltype;
        int16_t d1, d2;

        if (d0 != 0) {
            uint8_t d0b = (uint8_t)(d0 - 2);     /* subq.b #2 */
            if (d0b != 0) {
                /* .fall: 2nd Routine 4 */
                ObjectFall(o);                   /* bsr.w ObjectFall */
                {
                    int16_t dist, angle;
                    ObjFloorDist(o, &dist, &angle); /* jsr ObjFloorDist */
                    if (dist >= 0) {             /* tst.w d1 / bpl.w Mon_Animate */
                        goto mon_animate;
                    }
                    obY(o) = (int16_t)(obY(o) + dist); /* add.w d1,obY */
                }
                obVelY(o) = 0;                   /* clr.w obVelY */
                ob2ndRout(o) = 0;                /* clr.b ob2ndRout */
                goto mon_animate;                /* bra.w Mon_Animate */
            }
            /* 2nd Routine 2: .ontop */
            d1 = (int16_t)((int16_t)obActWid(o) + sonic_solid_width); /* moveq #0,d1 / move.b / addi.w */
            {
                int16_t dummy;
                ExitPlatform(o, d1, &dummy);     /* bsr.w ExitPlatform */
            }
            if (obStatus(a1) & (1 << 3)) {       /* btst #3,obStatus(a1) / bne.w .ontop */
                int16_t d3 = 32 / 2;             /* move.w #32/2,d3 */
                int16_t d2x = obX(o);            /* move.w obX(a0),d2 */
                MvSonicOnPtfm(o, d2x, d3);       /* bsr.w MvSonicOnPtfm */
                goto mon_animate;
            }
            ob2ndRout(o) = 0;                    /* clr.b ob2ndRout */
            goto mon_animate;                    /* bra.w Mon_Animate */
        }

        /* .normal: 2nd Routine 0 */
        d1 = (int16_t)(30 / 2 + sonic_solid_width); /* move.w #30/2+sonic_solid_width,d1 */
        d2 = 30 / 2;                             /* move.w #30/2,d2 */
        coltype = Mon_SolidSides(o, d1, d2, &d0_out, &d3_out); /* bsr.w Mon_SolidSides */
        if (coltype == 0) goto checkpush;        /* beq.w .checkpush */

        if ((int16_t)obVelY(a1) < 0) goto dontbreak; /* tst.w obVelY / bmi.s .dontbreak */
        if (obAnim(a1) == id_Roll) goto checkpush;   /* cmpi.b #id_Roll / beq.s .checkpush */

dontbreak:
        if (coltype >= 0) goto sidetouch;        /* tst.w d1 / bpl.s .sidetouch */
        /* Top/bottom collision */
        obY(a1) = (int16_t)(obY(a1) - d3_out);   /* sub.w d3,obY(a1) */
        Plat_NoCheck(a1, o);                     /* bsr.w Plat_NoCheck */
        ob2ndRout(o) = 2;                        /* move.b #2,ob2ndRout */
        goto mon_animate;                        /* bra.w Mon_Animate */

sidetouch:
        if (d0_out == 0) goto push;              /* tst.w d0 / beq.w .push */
        if (d0_out < 0) goto sonicleft;          /* bmi.s .sonicleft */

sonicright:
        if ((int16_t)obVelX(a1) < 0) goto push;  /* tst.w obVelX / bmi.s .push */
        goto stopsonic;                          /* bra.s .stopsonic */

sonicleft:
        if ((int16_t)obVelX(a1) >= 0) goto push; /* tst.w obVelX / bpl.s .push */

stopsonic:
        obX(a1) = (int16_t)(obX(a1) - d0_out);   /* sub.w d0,obX(a1) */
        obInertia(a1) = 0;                       /* move.w #0,obInertia */
        obVelX(a1) = 0;                          /* move.w #0,obVelX */

push:
        if (obStatus(a1) & (1 << 1)) goto stoppushing; /* btst #1,obStatus / bne.s */
        obStatus(a1) |= (1 << 5);                /* bset #5,obStatus(a1) */
        obStatus(o)  |= (1 << 5);                /* bset #5,obStatus(a0) */
        goto mon_animate;                        /* bra.s Mon_Animate */

checkpush:
        if (!(obStatus(o) & (1 << 5))) {         /* btst #5,obStatus(a0) / beq.s Mon_Animate */
            goto mon_animate;
        }
        /* FixBugs=0: walk-jump bug */
        obAnim(a1) = id_Run;                     /* move.w #id_Run,obAnim(a1) */

stoppushing:
        obStatus(o)  &= ~(1 << 5);               /* bclr #5,obStatus(a0) */
        obStatus(a1) &= ~(1 << 5);               /* bclr #5,obStatus(a1) */

mon_animate:
        if (Ani_Monitor) {
            AnimateSprite(o, Ani_Monitor);       /* lea Ani_Monitor / bsr.w AnimateSprite */
        }
        /* Mon_Display (falls through) */
        DisplaySprite(o);                        /* bsr.w DisplaySprite */
        if (OutOfRange(o, -1)) {                 /* out_of_range.w DeleteObject */
            DeleteObject(o);
        }
        /* rts */
    }
}

/* Mon_BreakOpen — Routine 4 (set from ReactToItem) */
static void Mon_BreakOpen(uint8_t *o) {
    obRoutine(o) += 2;                           /* addq.b #2,obRoutine -> Mon_Animate */
    obColType(o) = col_none;                     /* move.b #col_none,obColType */

    {
        uint8_t *a1 = (uint8_t *)FindFreeObj();  /* bsr.w FindFreeObj */
        if (a1) {                                /* bne.s Mon_Explode (in C, success) */
            obID(a1) = id_PowerUp;               /* _move.b #id_PowerUp,obID(a1) */
            obX(a1)  = obX(o);                   /* move.w obX(a0),obX(a1) */
            obY(a1)  = obY(o);                   /* move.w obY(a0),obY(a1) */
            obAnim(a1) = obAnim(o);              /* move.b obAnim(a0),obAnim(a1) */
        }
    }
    /* Mon_Explode: */
    {
        uint8_t *a1 = (uint8_t *)FindFreeObj();  /* bsr.w FindFreeObj */
        if (a1) {                                /* bne.s Mon_RememberBroken */
            obID(a1) = id_ExplosionItem;         /* _move.b #id_ExplosionItem,obID(a1) */
            obRoutine(a1) += 2;                  /* addq.b #2,obRoutine(a1) */
            obX(a1)  = obX(o);                   /* move.w obX(a0),obX(a1) */
            obY(a1)  = obY(o);                   /* move.w obY(a0),obY(a1) */
        }
    }
    /* Mon_RememberBroken: */
    {
        uint8_t *a2 = RAM_ADDR(v_objstate);
        uint8_t d0 = obRespawnNo(o);             /* moveq #0,d0 / move.b obRespawnNo */
        a2[2 + d0] |= 1;                         /* bset #0,2(a2,d0.w) */
    }

    obAnim(o) = 9;                               /* move.b #9,obAnim */
    DisplaySprite(o);                            /* bra.w DisplaySprite */
}

/* Monitor dispatcher — Mon_Index: 0/2/4/6/8 */
static void Monitor_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0: Mon_Main(o);      break;
        case 2:
            /* Mon_Solid — Routine 2. We inline the call here for clarity;
               Mon_Main already contains the full Mon_Solid body. For
               routine 2 entry we just run it directly. */
            {
                uint8_t *a1 = RAM_ADDR(v_player);
                uint8_t d0 = ob2ndRout(o);
                int16_t d0_out = 0, d3_out = 0;
                int16_t coltype;
                int16_t d1, d2;

                if (d0 != 0) {
                    uint8_t d0b = (uint8_t)(d0 - 2);
                    if (d0b != 0) {
                        /* .fall */
                        ObjectFall(o);
                        {
                            int16_t dist, angle;
                            ObjFloorDist(o, &dist, &angle);
                            if (dist >= 0) goto mon2_animate;
                            obY(o) = (int16_t)(obY(o) + dist);
                        }
                        obVelY(o) = 0;
                        ob2ndRout(o) = 0;
                        goto mon2_animate;
                    }
                    /* 2nd Routine 2: .ontop */
                    d1 = (int16_t)((int16_t)obActWid(o) + sonic_solid_width);
                    {
                        int16_t dummy;
                        ExitPlatform(o, d1, &dummy);
                    }
                    if (obStatus(a1) & (1 << 3)) {
                        int16_t d3 = 32 / 2;
                        int16_t d2x = obX(o);
                        MvSonicOnPtfm(o, d2x, d3);
                        goto mon2_animate;
                    }
                    ob2ndRout(o) = 0;
                    goto mon2_animate;
                }

                /* .normal */
                d1 = (int16_t)(30 / 2 + sonic_solid_width);
                d2 = 30 / 2;
                coltype = Mon_SolidSides(o, d1, d2, &d0_out, &d3_out);
                if (coltype == 0) goto mon2_checkpush;
                if ((int16_t)obVelY(a1) < 0) goto mon2_dontbreak;
                if (obAnim(a1) == id_Roll) goto mon2_checkpush;

            mon2_dontbreak:
                if (coltype >= 0) goto mon2_sidetouch;
                obY(a1) = (int16_t)(obY(a1) - d3_out);
                Plat_NoCheck(a1, o);
                ob2ndRout(o) = 2;
                goto mon2_animate;

            mon2_sidetouch:
                if (d0_out == 0) goto mon2_push;
                if (d0_out < 0) goto mon2_sonicleft;

            mon2_sonicright:
                if ((int16_t)obVelX(a1) < 0) goto mon2_push;
                goto mon2_stopsonic;

            mon2_sonicleft:
                if ((int16_t)obVelX(a1) >= 0) goto mon2_push;

            mon2_stopsonic:
                obX(a1) = (int16_t)(obX(a1) - d0_out);
                obInertia(a1) = 0;
                obVelX(a1) = 0;

            mon2_push:
                if (obStatus(a1) & (1 << 1)) goto mon2_stoppushing;
                obStatus(a1) |= (1 << 5);
                obStatus(o)  |= (1 << 5);
                goto mon2_animate;

            mon2_checkpush:
                if (!(obStatus(o) & (1 << 5))) {
                    goto mon2_animate;
                }
                obAnim(a1) = id_Run;

            mon2_stoppushing:
                obStatus(o)  &= ~(1 << 5);
                obStatus(a1) &= ~(1 << 5);

            mon2_animate:
                if (Ani_Monitor) {
                    AnimateSprite(o, Ani_Monitor);
                }
                DisplaySprite(o);
                if (OutOfRange(o, -1)) {
                    DeleteObject(o);
                }
            }
            break;
        case 4: Mon_BreakOpen(o); break;
        case 6:
            /* Mon_Animate — Routine 6 */
            if (Ani_Monitor) {
                AnimateSprite(o, Ani_Monitor);
            }
            /* falls through to Mon_Display */
            /* fall through */
        case 8:
            /* Mon_Display — Routine 8 */
            DisplaySprite(o);
            if (OutOfRange(o, -1)) {
                DeleteObject(o);
            }
            break;
    }
}

/* ===========================================================================
   Object 2E — PowerUp (monitor contents)
   =========================================================================== */

/* PowerUp dispatcher — Pow_Index: 0/2/4 */
static void PowerUp_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0: {
            /* Pow_Main — Routine 0 */
            obRoutine(o) += 2;                   /* addq.b #2 */
            obGfx(o)     = ArtTile_Monitor;      /* move.w #ArtTile_Monitor */
            obRender(o)  = sprite_rawmappings | sprite_cam_field; /* move.b #sprite_rawmappings|sprite_cam_field */
            obPriority(o)= 3;                    /* move.b #3 */
            obActWid(o)  = 16 / 2;               /* move.b #16/2 */
            obVelY(o)    = -0x300;               /* move.w #-$300 */

            {
                uint8_t d0 = (uint8_t)(obAnim(o) + 2); /* moveq #0,d0 / move.b obAnim / addq.b #2 */
                obFrame(o) = d0;                 /* move.b d0,obFrame (redundant) */
                const uint8_t *a1 = (const uint8_t *)Map_Monitor;
                uint16_t offset = ((const uint16_t *)a1)[d0]; /* adda.w (a1,d0.w) */
                a1 += offset;
                a1 += 2;                         /* addq.w #1,a1 */
                obMap(o) = (uint32_t)(uintptr_t)a1; /* move.l a1,obMap */
            }
            /* falls through to Pow_Move */
            /* fall through */
        }
        case 2: {
            /* Pow_Move — Routine 2 */
            if ((int16_t)obVelY(o) >= 0) {       /* tst.w obVelY / bpl.w Pow_Checks */
                goto pow_checks;
            }
            SpeedToPos(o);                       /* bsr.w SpeedToPos */
            obVelY(o) = (int16_t)(obVelY(o) + 0x18); /* addi.w #$18 */
            break;                               /* rts */
        }
        case 4: {
            /* Pow_Delete — Routine 4 */
            {
                uint16_t *timer = (uint16_t *)((uint8_t *)o + 0x1E); /* obTimeFrame word */
                *timer = (uint16_t)(*timer - 1); /* subq.w #1 */
                if ((int16_t)*timer < 0) {       /* bmi.w DeleteObject */
                    DeleteObject(o);
                    return;
                }
            }
            break;                               /* .return: rts */
        }
    }

    DisplaySprite(o);                            /* bra.w DisplaySprite */
    return;

pow_checks:
    /* Pow_Checks */
    obRoutine(o) += 2;                           /* addq.b #2 */
    {
        uint16_t *timer = (uint16_t *)((uint8_t *)o + 0x1E);
        *timer = 30 - 1;                         /* move.w #30-1,obTimeFrame */
    }
    {
        uint8_t d0 = obAnim(o);                  /* move.b obAnim,d0 */

        /* Pow_ChkEggman */
        if (d0 == 1) {                           /* cmpi.b #1 / bne.s Pow_ChkSonic */
            /* FixBugs=0: Eggman monitor does nothing */
            goto pow_display;
        }

        /* Pow_ChkSonic */
        if (d0 == 2) {                           /* cmpi.b #2 / bne.s Pow_ChkShoes */
            /* ExtraLife */
            v_lives     = v_lives + 1;           /* addq.b #1,(v_lives).w */
            f_lifecount = f_lifecount + 1;       /* addq.b #1,(f_lifecount).w */
            Sound_Queue(bgm_ExtraLife, false);   /* jmp QueueSound1 */
            goto pow_display;
        }

        /* Pow_ChkShoes */
        if (d0 == 3) {                           /* cmpi.b #3 / bne.s Pow_ChkShield */
            v_shoes = 1;                         /* move.b #1,(v_shoes).w */
            RAM_WORD(v_player + 0x2A) = 20 * 60; /* move.w #20*60,(v_player+shoetime).w */
            v_sonspeedmax = son_maxspeed * 2;    /* move.w #son_maxspeed*2 */
            v_sonspeedacc = son_acceleration * 2;/* move.w #son_acceleration*2 */
            v_sonspeeddec = son_deceleration;    /* move.w #son_deceleration */
            /* FixBugs=0: no underwater fix */
            Sound_Queue(bgm_Speedup, false);     /* jmp QueueSound1 */
            goto pow_display;
        }

        /* Pow_ChkShield */
        if (d0 == 4) {                           /* cmpi.b #4 / bne.s Pow_ChkInvinc */
            v_shield = 1;                        /* move.b #1,(v_shield).w */
            RAM_BYTE(v_shieldobj) = id_ShieldItem; /* move.b #id_ShieldItem,(v_shieldobj).w */
            Sound_Queue(sfx_Shield, false);      /* jmp QueueSound1 */
            goto pow_display;
        }

        /* Pow_ChkInvinc */
        if (d0 == 5) {                           /* cmpi.b #5 / bne.s Pow_ChkRings */
            v_invinc = 1;                        /* move.b #1,(v_invinc).w */
            RAM_WORD(v_player + 0x30) = 20 * 60; /* move.w #20*60,(v_player+invtime).w */
            RAM_BYTE(v_starsobj1) = id_ShieldItem; /* move.b #id_ShieldItem,(v_starsobj1).w */
            RAM_BYTE(v_starsobj1 + 0x1A) = 1;    /* move.b #1,(v_starsobj1+obAnim).w */
            RAM_BYTE(v_starsobj2) = id_ShieldItem;
            RAM_BYTE(v_starsobj2 + 0x1A) = 2;
            RAM_BYTE(v_starsobj3) = id_ShieldItem;
            RAM_BYTE(v_starsobj3 + 0x1A) = 3;
            RAM_BYTE(v_starsobj4) = id_ShieldItem;
            RAM_BYTE(v_starsobj4 + 0x1A) = 4;

            if (f_lockscreen) {                  /* tst.b (f_lockscreen).w / bne.s Pow_NoMusic */
                goto pow_display;
            }
            /* Revision<>0 (REV01): check drowning */
            if (v_air <= 12) {                   /* cmpi.w #12,(v_air).w / bls.s Pow_NoMusic */
                goto pow_display;
            }
            Sound_Queue(bgm_Invincible, true);  /* jmp QueueSound1 */
            goto pow_display;
        }

        /* Pow_ChkRings */
        if (d0 == 6) {                           /* cmpi.b #6 / bne.s Pow_ChkS */
            v_rings = v_rings + 10;              /* addi.w #10,(v_rings).w */
            /* FixBugs=0: no 999 cap */
            f_ringcount |= 1;                    /* ori.b #1,(f_ringcount).w */
            if (v_rings >= 100) {                /* cmpi.w #100 / blo.s Pow_RingSound */
                if (!(v_lifecount & 2)) {        /* bset #1 / beq.w ExtraLife */
                    v_lifecount |= 2;
                    v_lives     = v_lives + 1;
                    f_lifecount = f_lifecount + 1;
                    Sound_Queue(bgm_ExtraLife, false);
                    goto pow_display;
                }
                if (v_rings >= 200) {            /* cmpi.w #200 / blo.s Pow_RingSound */
                    if (!(v_lifecount & 4)) {    /* bset #2 / beq.w ExtraLife */
                        v_lifecount |= 4;
                        v_lives     = v_lives + 1;
                        f_lifecount = f_lifecount + 1;
                        Sound_Queue(bgm_ExtraLife, false);
                        goto pow_display;
                    }
                }
            }
            /* Pow_RingSound */
            Sound_Queue(sfx_Ring, false);        /* jmp QueueSound1 */
            goto pow_display;
        }

        /* Pow_ChkS */
        if (d0 == 7) {                           /* cmpi.b #7 / bne.s Pow_ChkGoggles */
            /* 'S' does nothing */
            goto pow_display;
        }

        /* Pow_ChkGoggles */
        /* FixBugs=0: goggles monitor disabled (commented out in ASM) */

        /* Pow_ChkEnd */
        /* subtype isn't any valid monitor ID: rts (no display) */
        /* In C, we fall through to DisplaySprite, matching Pow_Delete's
           rts which returns to PowerUp's dispatcher and then DisplaySprite. */
    }

pow_display:
    DisplaySprite(o);                            /* bra.w DisplaySprite */
}


/* ===========================================================================
 *  Object 36 — Spikes (id_Spikes = $36)
 *  Ported verbatim from _incObj/36 Spikes.asm (REV01, FixBugs=0).
 *
 *  spikes_origX          = objoff_30 (word): initial X (for out_of_range)
 *  spikes_origY          = objoff_32 (word): initial Y
 *  spikes_move_pos       = objoff_34 (word): 16.8 fixed-point delta,
 *                                            pixel offset is the HIGH byte
 *  spikes_move_direction = objoff_36 (word): 0 = retracting, 1 = moving in
 *  spikes_move_delay     = objoff_38 (word): frames until next move
 *  =========================================================================== */

#define spikes_origX(obj)          (*(int16_t  *)((uint8_t *)(obj) + 0x30))
#define spikes_origY(obj)          (*(int16_t  *)((uint8_t *)(obj) + 0x32))
#define spikes_move_pos(obj)       (*(uint16_t *)((uint8_t *)(obj) + 0x34))
#define spikes_move_direction(obj) (*(uint16_t *)((uint8_t *)(obj) + 0x36))
#define spikes_move_delay(obj)     (*(uint16_t *)((uint8_t *)(obj) + 0x38))

/* Spikes_Config: { frame, display & collision width/2 }, indexed by the
 *  subtype's UPPER nybble ($0x..$5x). */
static const uint8_t Spikes_Config[6][2] = {
    { 0, 40  / 2 },   /* $0x: 3 spikes, upright           */
    { 1, 32  / 2 },   /* $1x: 3 spikes, sideways          */
    { 2,  8  / 2 },   /* $2x: 1 spike,  upright           */
    { 3, 56  / 2 },   /* $3x: 3 spikes, upright (wide)    */
    { 4, 128 / 2 },   /* $4x: 6 spikes, upright (wide)    */
    { 5, 32  / 2 },   /* $5x: 1 spike,  sideways          */
};

static void Spikes_Main(uint8_t *o);
static void Spikes_Solid(uint8_t *o);
static void Spikes_Move(uint8_t *o);

/* -------------------------------------------------------------------------
 *  Spikes_WaitAndMove — delay spikes movement, or update the position delta
 *  once the delay expires. Ported verbatim from Spikes_WaitAndMove.
 *  ------------------------------------------------------------------------- */
static void Spikes_WaitAndMove(uint8_t *o) {
    if (spikes_move_delay(o) != 0) {                     /* tst.w / beq.s */
        spikes_move_delay(o) = (uint16_t)(spikes_move_delay(o) - 1); /* subq.w #1 */
        if (spikes_move_delay(o) != 0) return;           /* bne.s .return */

            /* delay just expired: play the moving sound if on screen */
            if ((int8_t)obRender(o) < 0) {                   /* tst.b / bpl.s */
                Sound_Queue(sfx_SpikesMove, false);          /* jsr (QueueSound2) */
            }
            return;
    }

    /* .doSpikesMove */
    if (spikes_move_direction(o) == 0) {                 /* tst.w / beq.s .retractSpikes */
        /* .retractSpikes: push spikes out by 8px, up to 32px total */
        spikes_move_pos(o) = (uint16_t)(spikes_move_pos(o) + 8 * 0x100); /* addi.w #8*$100 */
        if ((uint16_t)spikes_move_pos(o) < (uint16_t)(32 * 0x100)) {     /* cmpi.w #32*$100 / blo */
            return;
        }
        spikes_move_pos(o) = 32 * 0x100;                 /* clamp */
        spikes_move_direction(o) = 1;                    /* next: move back in */
        spikes_move_delay(o) = 60;                       /* 1 second delay */
        return;
    }

    /* Direction = 1: move spikes back in by 8px, down to 0 */
    if (spikes_move_pos(o) >= 8 * 0x100) {               /* subi.w #8*$100 / bhs.s */
        spikes_move_pos(o) = (uint16_t)(spikes_move_pos(o) - 8 * 0x100);
        return;
    }
    spikes_move_pos(o) = 0;                              /* clamp */
    spikes_move_direction(o) = 0;                        /* next: retract */
    spikes_move_delay(o) = 60;
}

/* -------------------------------------------------------------------------
 *  Spikes_Move — dispatch on the lower nybble of obSubtype.
 *  $x0 = static, $x1 = up/down, $x2 = left/right.
 *  ------------------------------------------------------------------------- */
static void Spikes_Move(uint8_t *o) {
    switch (obSubtype(o)) {                              /* move.b obSubtype,d0 / add / jmp */
        case 0:                                          /* Spikes_Type0: static */
            break;

        case 1:                                          /* Spikes_Type1: up/down */
            Spikes_WaitAndMove(o);
            /* move.b spikes_move_pos(a0),d0 (reads HIGH byte) ; add.w origY */
            obY(o) = (int16_t)(spikes_origY(o) + (uint8_t)(spikes_move_pos(o) >> 8));
            break;

        case 2:                                          /* Spikes_Type2: left/right */
            Spikes_WaitAndMove(o);
            obX(o) = (int16_t)(spikes_origX(o) + (uint8_t)(spikes_move_pos(o) >> 8));
            break;
    }
}

/* -------------------------------------------------------------------------
 *  Spikes_Main — routine 0: init.
 *  Reads the config from the upper nybble of obSubtype, then clears that
 *  nybble so obSubtype ends up holding only the movement type (lower nybble).
 *  ------------------------------------------------------------------------- */
static void Spikes_Main(uint8_t *o) {
    obRoutine(o) += 2;                                   /* addq.b #2 -> Spikes_Solid */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Spike;       /* move.l #Map_Spike */
    obGfx(o)     = (uint16_t)ArtTile_Spikes;             /* move.w #ArtTile_Spikes */
    obRender(o) |= sprite_cam_field;                     /* ori.b #sprite_cam_field */
    obPriority(o)= 4;                                    /* move.b #4 */

    uint8_t subtype   = obSubtype(o);                    /* move.b obSubtype(a0),d0 */
    uint8_t config_i  = (uint8_t)(subtype >> 4);         /* andi.w #$F0 / lsr.w #3 */
    obSubtype(o)      = (uint8_t)(subtype & 0x0F);       /* andi.b #$F,obSubtype */

    obFrame(o)  = Spikes_Config[config_i][0];            /* move.b (a1)+,obFrame */
    obActWid(o) = Spikes_Config[config_i][1];            /* move.b (a1)+,obActWid */

    spikes_origX(o) = obX(o);                            /* move.w obX,spikes_origX */
    spikes_origY(o) = obY(o);                            /* move.w obY,spikes_origY */
    /* ASM falls through into Spikes_Solid */
    Spikes_Solid(o);
}

/* -------------------------------------------------------------------------
 *  Spikes_Solid — routine 2: main mode. Calls SolidObject, then applies
 *  the FixBugs=0 damage rules (standing on top / side collision).
 *  ------------------------------------------------------------------------- */
static void Spikes_Solid(uint8_t *o) {
    int16_t d2;
    int16_t solid_ret;
    int16_t out_d3 = 0, out_d5 = 0;

    Spikes_Move(o);                                      /* bsr.w Spikes_Move */

    d2 = 8 / 2;                                          /* move.w #8/2,d2 */
    if (obFrame(o) == 5) goto Spikes_SideWays;           /* cmpi.b #5 / beq.s */
        if (obFrame(o) != 1) goto Spikes_Upright;            /* cmpi.b #1 / bne.s */
            d2 = 40 / 2;                                         /* move.w #40/2,d2 */

            Spikes_SideWays:
            {
                int16_t d1 = (int16_t)(32 / 2 + sonic_solid_width); /* move.w #32/2+sonic_solid_width,d1 */
                int16_t d3 = (int16_t)(d2 + 1);                     /* move.w d2,d3 / addq.w #1 */
                int16_t d4 = obX(o);                                /* move.w obX(a0),d4 */
                solid_ret = SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);

                /* FixBugs=0: Sonic standing on top -> solid platform, no damage.
                 *          SolidObject return 1 (side collision) -> damage. Otherwise display. */
                if (obStatus(o) & (1 << 3)) goto Spikes_Display;    /* btst #3 / bne.s */
                    if (solid_ret == 1) goto Spikes_Hurt;               /* cmpi.w #1 / beq.s */
                        goto Spikes_Display;
            }

            Spikes_Upright:
            {
                int16_t d1 = (int16_t)(int8_t)obActWid(o);          /* moveq #0,d1 / move.b obActWid,d1 */
                d1 = (int16_t)(d1 + sonic_solid_width);             /* addi.w #sonic_solid_width,d1 */
                int16_t d2u = 32 / 2;                               /* move.w #32/2,d2 */
                int16_t d3  = 32 / 2 + 1;                           /* move.w #(32/2)+1,d3 */
                int16_t d4  = obX(o);                               /* move.w obX(a0),d4 */
                solid_ret = SolidObject(o, d1, d2u, d3, d4, &out_d3, &out_d5);

                /* FixBugs=0: standing on top -> damage. SolidObject return >= 0
                 *          (none or side) -> display. Return -1 (top/bottom) -> damage. */
                if (obStatus(o) & (1 << 3)) goto Spikes_Hurt;       /* btst #3 / bne.s */
                    if (solid_ret >= 0) goto Spikes_Display;            /* tst.w d4 / bpl.s */
                        /* fall through to Spikes_Hurt */
            }

            Spikes_Hurt:
            if (v_invinc) goto Spikes_Display;                       /* tst.b (v_invinc) / bne.s */
            {
                uint8_t *player = RAM_ADDR(v_player);
                /* FixBugs=0: no flashtime early-out here (only FixBugs path adds it) */
                if ((uint8_t)obRoutine(player) >= 4) goto Spikes_Display; /* cmpi.b #4 / bhs.s */

                    /* REV01 (FixBugs=0): push Sonic up by his own vertical velocity
                     *          before triggering the hurt. Reads the 32-bit Y (pixel+subpixel),
                     *          subtracts velY<<8, writes it back. */
                    {
                        int32_t y = ((uint32_t)obY(player) << 16) | (uint16_t)obSubpixelY(player);
                        y -= ((int32_t)obVelY(player)) << 8;
                        obY(player)        = (int16_t)((uint32_t)y >> 16);
                        obSubpixelY(player)= (int16_t)(y & 0xFFFF);
                    }
                    HurtSonic(player, o);
            }

            Spikes_Display:
            /* FixBugs=0: DisplaySprite first, then out_of_range DeleteObject using
             *      spikes_origX (the spike's spawn X), so moving spikes don't despawn
             *      when they slide out of the camera range. */
            DisplaySprite(o);                                        /* bsr.w DisplaySprite */
            if (OutOfRange(o, spikes_origX(o))) {                    /* out_of_range.w DeleteObject,spikes_origX */
                DeleteObject(o);
            }
}

/* Spikes dispatcher — Spikes_Index: 0 = Main, 2 = Solid */
static void Spikes_ObjectMain(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Spikes_Main(o);  break;
        case 2: Spikes_Solid(o); break;
    }
}

/* ===========================================================================
 *  Object 41 — Springs (id_Springs = $41)
 *  Ported verbatim from _incObj/41 Springs.asm (REV01, FixBugs=0).
 *
 *  spring_pow = objoff_30 (word): bounce velocity (negative = up/left)
 *
 *  Subtype bits:
 *    bit 4 = sideways spring (LR)
 *    bit 5 = downwards spring
 *    bit 1 = yellow (palette line 2, weaker power)
 *    bits 0-3 = power index into Spring_Powers[]
 *
 *  Spring_Powers (ASM):
 *      dc.w -$1000     ; red
 *      dc.w -$0A00     ; yellow
 *
 *  El 68k direcciona con `move.w Spring_Powers(pc,d0.w), ...` — d0 es
 *  un OFFSET DE BYTE, no un índice de word. La tabla se representa como
 *  bytes big-endian (como estaría en ROM) para replicar esa semántica:
 *
 *      offset 0: -$1000 = 0xF000 = bytes F0 00
 *      offset 2: -$0A00 = 0xF600 = bytes F6 00
 * =========================================================================== */

#define spring_pow(obj) (*(int16_t *)((uint8_t *)(obj) + 0x30))

static const uint8_t Spring_Powers[4] = {
    0xF0, 0x00,   /* offset 0: -$1000 (red)    */
    0xF6, 0x00,   /* offset 2: -$0A00 (yellow) */
};

static void Spring_Main(uint8_t *o);
static void Spring_Up(uint8_t *o);
static void Spring_AniUp(uint8_t *o);
static void Spring_ResetUp(uint8_t *o);
static void Spring_LR(uint8_t *o);
static void Spring_AniLR(uint8_t *o);
static void Spring_ResetLR(uint8_t *o);
static void Spring_Down(uint8_t *o);
static void Spring_AniDown(uint8_t *o);
static void Spring_ResetDown(uint8_t *o);

/* -------------------------------------------------------------------------
 *  Spring_Main — routine 0
 *  ------------------------------------------------------------------------- */
static void Spring_Main(uint8_t *o) {
    obRoutine(o) += 2;                                    /* addq.b #2 -> Spring_Up */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Spring;       /* move.l #Map_Spring,obMap */
    obGfx(o)     = (uint16_t)ArtTile_Spring_Horizontal;   /* move.w #ArtTile_Spring_Horizontal */
    obRender(o) |= sprite_cam_field;                      /* ori.b #sprite_cam_field */
    obActWid(o)  = 32 / 2;                                /* move.b #32/2 */
    obPriority(o)= 4;                                     /* move.b #4 */

    uint8_t d0 = obSubtype(o);                            /* move.b obSubtype,d0 */

    /* .checkSideways */
    if (d0 & (1 << 4)) {                                  /* btst #4 / beq.s */
        obRoutine(o) = 8;                                 /* move.b #8 -> Spring_LR */
        obAnim(o)    = 1;                                 /* move.b #1,obAnim */
        obFrame(o)   = 3;                                 /* move.b #3,obFrame */
        obGfx(o)     = (uint16_t)ArtTile_Spring_Vertical; /* move.w #ArtTile_Spring_Vertical */
        obActWid(o)  = 16 / 2;                            /* move.b #16/2 */
    }

    /* .checkDownwards */
    if (d0 & (1 << 5)) {                                  /* btst #5 / beq.s */
        obRoutine(o) = 0x0E;                              /* move.b #$E -> Spring_Down */
        obStatus(o) |= (1 << 1);                          /* bset #1: Y-flip */
    }

    /* .checkYellow */
    if (d0 & (1 << 1)) {                                  /* btst #1 / beq.s */
        obGfx(o) = (uint16_t)(ArtTile_Spring_Horizontal | Tile_Pal2);                             /* bset #5,obGfx (palette line 2) */
        fprintf(stderr, "SPRING gfx=%04X (yellow bit set)\n", obGfx(o));
    }

    /* .getPower
     *
     * move.w Spring_Powers(pc,d0.w), spring_pow(a0)
     *   (pc,d0.w) indexa por BYTE: d0 es un byte-displacement.
     *   La tabla se lee byte a byte en orden big-endian (como en ROM). */
    d0 &= 0x0F;                                           /* andi.w #$F,d0 */
    {
        const uint8_t *sp = (const uint8_t *)Spring_Powers;
        spring_pow(o) = (int16_t)(((uint16_t)sp[d0] << 8) | (uint16_t)sp[d0 + 1]);
    }
    fprintf(stderr, "SPRING final: gfx=%04X pow=%d sub=%02X\n",
            obGfx(o), spring_pow(o), obSubtype(o));
    /* returns to the outer dispatcher (bra.s -> DisplaySprite) */
}

/* -------------------------------------------------------------------------
 *  Spring_Up — routine 2: upright spring, bounces Sonic up.
 *  ------------------------------------------------------------------------- */
static void Spring_Up(uint8_t *o) {
    int16_t d1 = (int16_t)(32 / 2 + sonic_solid_width);
    int16_t d2 = 16 / 2;
    int16_t d3 = 32 / 2;
    int16_t d4 = obX(o);
    int16_t out_d3 = 0, out_d5 = 0;

    SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);     /* bsr.w SolidObject */

    if (obSolid(o) == 0) return;                          /* tst.b obSolid(a0) / bne.s .bounceUp */

        /* .bounceUp */
        {
            uint8_t *a1 = RAM_ADDR(v_player);
            obRoutine(o) += 2;                                /* addq.b #2 -> Spring_AniUp */
            obY(a1) = (int16_t)(obY(a1) + 8);                 /* addq.w #8,obY(a1) */
            obVelY(a1) = spring_pow(o);                       /* move.w spring_pow(a0),obVelY(a1) */
            obStatus(a1) |= (1 << 1);                         /* bset #1: airborne */
            obStatus(a1) &= ~(1 << 3);                        /* bclr #3: not on platform */
            obAnim(a1)   = id_Spring;                         /* move.b #id_Spring,obAnim(a1) */
            obRoutine(a1)= 2;                                 /* move.b #2,obRoutine(a1) -> Sonic_Control */
            obStatus(o)  &= ~(1 << 3);                        /* bclr #3,obStatus(a0) */
            obSolid(o)   = 0;                                 /* clr.b obSolid(a0) */
            Sound_Queue(sfx_Spring, false);                   /* jsr (QueueSound2) */
        }
}

/* -------------------------------------------------------------------------
 *  Spring_AniUp — routine 4: animate; the script advances routine to 6.
 *  ------------------------------------------------------------------------- */
static void Spring_AniUp(uint8_t *o) {
    if (Ani_Spring) AnimateSprite(o, Ani_Spring);         /* lea Ani_Spring / bra AnimateSprite */
}

/* Spring_ResetUp — routine 6 */
static void Spring_ResetUp(uint8_t *o) {
    obPrevAni(o) = 1;                                     /* move.b #1,obPrevAni */
    obRoutine(o) -= 4;                                    /* subq.b #4 -> Spring_Up (2) */
}

/* -------------------------------------------------------------------------
 *  Spring_LR — routine 8: sideways spring, bounces Sonic left/right.
 *  ------------------------------------------------------------------------- */
static void Spring_LR(uint8_t *o) {
    int16_t d1 = (int16_t)(16 / 2 + sonic_solid_width);
    int16_t d2 = 28 / 2;
    int16_t d3 = 30 / 2;
    int16_t d4 = obX(o);
    int16_t out_d3 = 0, out_d5 = 0;

    SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);     /* bsr.w SolidObject */

    if (obRoutine(o) == 2) {                              /* cmpi.b #2 / bne.s .checkPushing */
        obRoutine(o) = 8;                                 /* move.b #8: force back to Spring_LR */
    }

    /* .checkPushing */
    if (!(obStatus(o) & (1 << 5))) return;                /* btst #5 / bne.s .bounceSideways */

        /* .bounceSideways */
        {
            uint8_t *a1 = RAM_ADDR(v_player);
            obRoutine(o) += 2;                                /* addq.b #2 -> Spring_AniLR */
            obVelX(a1) = spring_pow(o);                       /* move.w spring_pow(a0),obVelX(a1) */
            obX(a1) = (int16_t)(obX(a1) + 8);                 /* addq.w #8,obX(a1) */

            if (!(obStatus(o) & (1 << 0))) {                  /* btst #0 / bne.s .doBounce */
                /* Facing right (default): push left into spring, then bounce right */
                obX(a1)    = (int16_t)(obX(a1) - (8 + 8));    /* subi.w #8+8,obX(a1) */
                obVelX(a1) = (int16_t)(-obVelX(a1));          /* neg.w obVelX(a1) */
            }

            /* .doBounce */
            locktime(a1) = 15;                                /* move.w #15,locktime(a1) */
            obInertia(a1) = obVelX(a1);                       /* move.w obVelX(a1),obInertia(a1) */
            obStatus(a1) ^= (1 << 0);                         /* bchg #0: flip X-orientation */

            if (!(obStatus(a1) & (1 << 2))) {                 /* btst #2 / bne.s .clearPush (rolling?) */
                obAnim(a1) = id_Walk;                         /* move.b #id_Walk,obAnim */
            }

            /* .clearPush */
            obStatus(o)  &= ~(1 << 5);                        /* bclr #5,obStatus(a0) */
            obStatus(a1) &= ~(1 << 5);                        /* bclr #5,obStatus(a1) */
            Sound_Queue(sfx_Spring, false);                   /* jsr (QueueSound2) */
        }
}

/* Spring_AniLR — routine $A */
static void Spring_AniLR(uint8_t *o) {
    if (Ani_Spring) AnimateSprite(o, Ani_Spring);
}

/* Spring_ResetLR — routine $C */
static void Spring_ResetLR(uint8_t *o) {
    obPrevAni(o) = 2;                                     /* move.b #2,obPrevAni */
    obRoutine(o) -= 4;                                    /* subq.b #4 -> Spring_LR (8) */
}

/* -------------------------------------------------------------------------
 *  Spring_Down — routine $E: ceiling-mounted spring, bounces Sonic down.
 *  ------------------------------------------------------------------------- */
static void Spring_Down(uint8_t *o) {
    int16_t d1 = (int16_t)(32 / 2 + sonic_solid_width);
    int16_t d2 = 16 / 2;
    int16_t d3 = 32 / 2;
    int16_t d4 = obX(o);
    int16_t out_d3 = 0, out_d5 = 0;

    int16_t ret = SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5); /* bsr.w SolidObject */

    if (obRoutine(o) == 2) {                              /* cmpi.b #2 / bne.s .checkTouch */
        obRoutine(o) = 0x0E;                              /* move.b #$E: force back to Spring_Down */
    }

    /* .checkTouch */
    if (obSolid(o) != 0) return;                          /* tst.b obSolid / bne.s .return */
        if (ret >= 0) return;                                 /* tst.w d4 / bmi.s .bounceDown */

            /* .bounceDown */
            {
                uint8_t *a1 = RAM_ADDR(v_player);
                obRoutine(o) += 2;                                /* addq.b #2 -> Spring_AniDown */
                obY(a1) = (int16_t)(obY(a1) - 8);                 /* subq.w #8,obY(a1) */
                obVelY(a1) = spring_pow(o);                       /* move.w spring_pow(a0),obVelY(a1) */
                obVelY(a1) = (int16_t)(-obVelY(a1));              /* neg.w obVelY(a1): move down */
                obStatus(a1) |= (1 << 1);                         /* bset #1: airborne */
                obStatus(a1) &= ~(1 << 3);                        /* bclr #3: not on platform */
                obRoutine(a1)= 2;                                 /* move.b #2 -> Sonic_Control */
                obStatus(o)  &= ~(1 << 3);                        /* bclr #3,obStatus(a0) */
                obSolid(o)   = 0;                                 /* clr.b obSolid(a0) */
                Sound_Queue(sfx_Spring, false);                   /* jsr (QueueSound2) */
            }
}

/* Spring_AniDown — routine $10 */
static void Spring_AniDown(uint8_t *o) {
    if (Ani_Spring) AnimateSprite(o, Ani_Spring);
}

/* Spring_ResetDown — routine $12 */
static void Spring_ResetDown(uint8_t *o) {
    obPrevAni(o) = 1;                                     /* move.b #1,obPrevAni */
    obRoutine(o) -= 4;                                    /* subq.b #4 -> Spring_Down ($E) */
}

/* -------------------------------------------------------------------------
 *  Springs dispatcher — Spring_Index: 0/2/4/6/8/A/C/E/$10/$12
 *  FixBugs=0: DisplaySprite first, then out_of_range DeleteObject.
 *  ------------------------------------------------------------------------- */
static void Springs_ObjectMain(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00: Spring_Main(o);       break;
        case 0x02: Spring_Up(o);         break;
        case 0x04: Spring_AniUp(o);      break;
        case 0x06: Spring_ResetUp(o);    break;
        case 0x08: Spring_LR(o);         break;
        case 0x0A: Spring_AniLR(o);      break;
        case 0x0C: Spring_ResetLR(o);    break;
        case 0x0E: Spring_Down(o);       break;
        case 0x10: Spring_AniDown(o);    break;
        case 0x12: Spring_ResetDown(o);  break;
    }

    /* Outer display + range check (FixBugs=0 order) */
    DisplaySprite(o);                                     /* bsr.w DisplaySprite */
    if (OutOfRange(o, -1)) {                              /* out_of_range.w DeleteObject */
        DeleteObject(o);
    }
}
