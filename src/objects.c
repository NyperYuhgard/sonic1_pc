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
        uint16_t zact = (uint16_t)v_zone_act;

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
            DeleteObject(obj);      /* Card_ChangeArt extra loads skipped in PC */
            return;
        }
        int16_t d1 = 0x20;
        int16_t cur = obX(o);
        int16_t target = cardFinalX(o);
        if (cur == target) {
            DeleteObject(obj);      /* Card_ChangeArt extra loads skipped in PC */
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
                        if (v_zone_act != id_LZ_act4) {
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
        if (v_zone_act == id_LZ_act4) {
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
    if (v_zone_act == id_SBZ_act2) {             /* cmpi.w #id_SBZ_act2,(v_zone_act).w / bne.s .setPostDelay */
        obRoutine(o) += 4;                       /* addq.b #4 -> Got_Wait ($C, pre-SBZ2 cutscene) */
    }
    got_timeframe(o) = 3 * 60;                   /* move.w #3*60,obTimeFrame(a0) */
}

/* Got_NextLevel (routine $A): advance to the next zone/act */
static void Got_NextLevel(uint8_t *o) {
    int d0 = (v_zone & 7) * 4 + (v_act & 3);     /* andi #7 / lsl #3 + andi #3 / add (word index) */
    uint16_t nl = LevelOrder[d0];                /* move.w LevelOrder(pc,d0.w),d0 */
    v_zone_act = nl;                             /* move.w d0,(v_zone_act).w */

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
    if (v_zone_act == id_SBZ_act2) {
        if ((int16_t)obX(o) >= 0x2000) {
            RAM_BYTE(v_lastlamp) = 0;
            f_restart = 1;
            v_zone_act = id_LZ_act4;
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

    if (obVelY(o) >= 0) {
        return;
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

    if (obVelY(o) >= 0) {
        return;
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
        obFrame(o) = frame_id;

        uint8_t status = obStatus(o);
        uint8_t render = obRender(o);
        uint8_t flip_bits = (frame_id << 3) & (sprite_xflip | sprite_yflip);
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
                    obFrame(o) = frame_id;
                    uint8_t status = obStatus(o);
                    uint8_t render = obRender(o);
                    uint8_t flip_bits = (frame_id << 3) & (sprite_xflip | sprite_yflip);
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
                    obFrame(o) = frame_id;
                    uint8_t status = obStatus(o);
                    uint8_t render = obRender(o);
                    uint8_t flip_bits = (frame_id << 3) & (sprite_xflip | sprite_yflip);
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
