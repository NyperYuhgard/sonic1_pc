#include "objects.h"
#include "ram.h"
#include "constants.h"
#include "data.h"
#include "sound.h"
#include <string.h>
#include <stdio.h>

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
static void Ring_Main(void *obj);
static void RingLoss_Main(void *obj);
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
    obj_dispatch[id_Rings]        = Ring_Main;
    obj_dispatch[id_RingLoss]     = RingLoss_Main;

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
    uint8_t *base = ObjRAM;
    for (int i = 0; i < NUM_OBJECTS; i++) {
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

    int32_t x = ((uint32_t)obSubpixelX(o) << 16) | (uint16_t)obX(o);
    int32_t y = ((uint32_t)obSubpixelY(o) << 16) | (uint16_t)obY(o);

    x += (int32_t)obVelX(o) << 8;
    y += (int32_t)obVelY(o) << 8;

    obX(o)         = (int16_t)(x & 0xFFFF);
    obSubpixelX(o) = (int16_t)((uint32_t)x >> 16);
    obY(o)         = (int16_t)(y & 0xFFFF);
    obSubpixelY(o) = (int16_t)((uint32_t)y >> 16);
}

/* ===========================================================================
   ObjFloorDist — _incObj/sub ObjFloorDist.asm.
   y = obY + obHeight; angle snaps to 0. Without a real 16x16 collision
   index (v_collindex is not yet populated), d1 (distance) is 0 — a flat
   floor at the object's feet.
   =========================================================================== */
void ObjFloorDist(void *obj, int16_t *dist, int16_t *angle) {
    (void)obj;
    if (dist)  *dist  = 0;
    if (angle) *angle = 0;
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
    uint16_t d1 = ((uint16_t)RAM_WORD(v_screenposx) - 128) & 0xFF80; /* subi+andi */
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
   STUB — will be ported from _incObj/01 Sonic - Main.asm in a later pass.
   =========================================================================== */
static void SonicPlayer_Main(void *obj) {
    /* TODO: full player physics + collision */
    (void)obj;
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

/* Ring_Collect (routine 4): started by Sonic's collision in the original
   (ReactToItem). Ring collection is not yet reachable (Sonic collision not
   ported), but the routine is kept verbatim so the write path exists. */
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
        uint8_t frame = frame_id & 0x1F;
        obFrame(o) = frame;

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
                    uint8_t frame = frame_id & 0x1F;
                    obFrame(o) = frame;
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
                    uint8_t frame = frame_id & 0x1F;
                    obFrame(o) = frame;
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
