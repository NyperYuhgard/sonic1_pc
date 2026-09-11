#include "collision.h"
#include "ram.h"
#include "constants.h"
#include "objects.h"
#include "data.h"
#include <stdint.h>

/* BE word read from the 256x256 layout (v_lvllayout_fg) */
static inline uint16_t layout_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* BE word read from the 16x16 block mappings (v_16x16) */
static inline uint16_t block_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* ===========================================================================
   FindNearestTile
   Inputs:
     d2 = y position (pixel)
     d3 = x position (pixel)
     a0 = object (for the sprite_looping loop-chunk override)
   Outputs:
     a1 = address in 256x256 layout (returned via out_a1)
     d1 = 16x16 block word (returned via out_d1)
   =========================================================================== */
void FindNearestTile(int16_t y, int16_t x, const void *obj, uint8_t **out_a1, uint16_t *out_d1) {
    /* d0 = (Y >> 1) & $380 + (X >> 8) & $7F: position within the layout */
    int16_t off = (int16_t)(((uint16_t)y >> 1) & 0x380)
                + (int16_t)(((uint16_t)x >> 8) & 0x7F);

    uint8_t chunk = RAM_ADDR(v_lvllayout_fg)[off];

    if (chunk == 0) {
        /* Blank chunk. In the 68k, a1 ends up at a RAM word that is always
           0 (v_chunk0collision); return an equivalent zero word. */
        static const uint8_t chunk0[2] = { 0, 0 };
        *out_a1 = (uint8_t *)chunk0;
        *out_d1 = 0;
        return;
    }

    if (chunk & 0x80) {
        /* Special chunk: if the object is "behind a loop", replace
           chunk number $A8 with $51 (the loop-back chunk). */
        chunk &= 0x7F;
        if (obRender(obj) & sprite_looping) {
            chunk += 1;
            if (chunk == 0x29)
                chunk = 0x51;
        }
    }

    chunk -= 1;                             /* chunks start at 1 */

    /* Cell address: (chunk << 9) + (Y*2 & $1E0) + (X>>3 & $1E) within v_256x256 */
    uint16_t cell = (uint16_t)(((uint16_t)chunk << 9)
                             | (((uint16_t)y << 1) & 0x1E0)
                             | (((uint16_t)x >> 3) & 0x1E));

    uint8_t *a1 = RAM_ADDR(v_256x256) + cell;
    *out_a1 = a1;
    *out_d1 = block_be16(a1);
}

/* ===========================================================================
   FindFloor2 (helper for blank-tile continuation)
   Inputs: y, x, a1 = layout address
   Output: d1 = distance to floor (returned via out_d1)
   =========================================================================== */
void FindFloor2(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3,
                uint8_t *a4, const void *obj, int16_t *out_d1) {
    (void)a3;
    uint8_t *a1;
    uint16_t word;
    FindNearestTile(y, x, obj, &a1, &word);

    uint16_t d4 = word;
    uint16_t d0 = word & 0x7FF;

    if (d0 == 0 || !(d4 & (1u << d5)))
        goto isblank;
    if (col_index_ptr == NULL || Col_AngleMap == NULL || Col_CollArray1 == NULL)
        goto isblank;
    d0 = col_index_ptr[d0];                 /* heightmap id */
    if (d0 == 0)
        goto isblank;

    uint8_t angle = Col_AngleMap[d0];
    int16_t d1 = x;
    if (d4 & 0x800) {                       /* xflip */
        d1 = (int16_t)~x;
        angle = (uint8_t)(0 - angle);
    }
    if (d4 & 0x1000) {                      /* yflip */
        angle = (uint8_t)(angle + 0x40);
        angle = (uint8_t)(0 - angle);
        angle = (uint8_t)(angle - 0x40);
    }
    *a4 = angle;

    d1 = (d1 & 0xF) + (int16_t)(d0 << 4);
    int16_t height = (int8_t)Col_CollArray1[d1];    /* ext.w */
    d4 ^= (uint16_t)d6;
    if (d4 & 0x1000)                        /* yflip on collision data */
        height = -height;

    if (height == 0)
        goto isblank;
    if (height < 0) {
        int16_t tmp = (int16_t)((y & 0xF) + height);
        if (tmp >= 0)
            goto isblank;
        *out_d1 = (int16_t)(uint16_t)(~tmp);        /* not.w */
        return;
    }
    *out_d1 = (int16_t)(0xF - ((y & 0xF) + height));
    return;

isblank:
    /* no solid block: distance to bottom of the 16x16 block */
    *out_d1 = (int16_t)(0xF - (y & 0xF));
}

/* ===========================================================================
   FindFloor
   Inputs:
     y = y position of object's bottom edge
     x = x position of object
     d5 = bit to test for solidness ($D = top solid; $E = left/right/bottom solid)
     d6 = eor bitmask for 16x16 block
     a3 = height of 16x16 blocks ($10 or -$10 if inverted)
     a4 = RAM address to write angle byte
   Outputs:
     d1 = distance to floor (returned via out_d1)
     a1 = address within 256x256 mappings
     (a1).w = 16x16 block word
     (a4).b = floor angle
   =========================================================================== */
void FindFloor(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3,
               uint8_t *a4, const void *obj, int16_t *out_d1) {
    uint8_t *a1;
    uint16_t word;
    FindNearestTile(y, x, obj, &a1, &word);

    uint16_t d4 = word;
    uint16_t d0 = word & 0x7FF;

    if (d0 == 0 || !(d4 & (1u << d5)))
        goto isblank;
    if (col_index_ptr == NULL || Col_AngleMap == NULL || Col_CollArray1 == NULL)
        goto isblank;
    d0 = col_index_ptr[d0];                 /* heightmap id */
    if (d0 == 0)
        goto isblank;

    uint8_t angle = Col_AngleMap[d0];
    int16_t d1 = x;
    if (d4 & 0x800) {                       /* xflip */
        d1 = (int16_t)~x;
        angle = (uint8_t)(0 - angle);
    }
    if (d4 & 0x1000) {                      /* yflip */
        angle = (uint8_t)(angle + 0x40);
        angle = (uint8_t)(0 - angle);
        angle = (uint8_t)(angle - 0x40);
    }
    *a4 = angle;

    d1 = (d1 & 0xF) + (int16_t)(d0 << 4);
    int16_t height = (int8_t)Col_CollArray1[d1];    /* ext.w */
    d4 ^= (uint16_t)d6;
    if (d4 & 0x1000)                        /* yflip on collision data */
        height = -height;

    if (height == 0)
        goto isblank;
    if (height < 0) {
        if ((int16_t)((y & 0xF) + height) >= 0)
            goto isblank;
    } else if (height != 0x10) {
        /* normal floor distance */
        *out_d1 = (int16_t)(0xF - ((y & 0xF) + height));
        return;
    }

    /* maxfloor / negative floor: try the block above the nearest */
    FindFloor2(y - a3, x, d5, d6, a3, a4, obj, out_d1);
    *out_d1 -= 0x10;
    return;

isblank:
    /* not solid: try the block below the nearest */
    FindFloor2(y + a3, x, d5, d6, a3, a4, obj, out_d1);
    *out_d1 += 0x10;
}

/* ===========================================================================
   FindWall
   Inputs (same as FindFloor, but checks vertical solidness):
     y = y position
     x = x position
     d5 = bit to test ($E = left/right/bottom solid)
     d6 = eor bitmask
     a3 = tile width ($10 or -$10 if xflip)
     a4 = RAM address to write angle byte
   Outputs:
     d1 = distance to wall (returned via out_d1)
     a1 = address within 256x256 mappings
     (a4).b = wall angle
   =========================================================================== */
void FindWall(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3,
              uint8_t *a4, const void *obj, int16_t *out_d1) {
    uint8_t *a1;
    uint16_t word;
    FindNearestTile(y, x, obj, &a1, &word);

    uint16_t d4 = word;
    uint16_t d0 = word & 0x7FF;

    if (d0 == 0 || !(d4 & (1u << d5)))
        goto isblank;
    if (col_index_ptr == NULL || Col_AngleMap == NULL || Col_CollArray2 == NULL)
        goto isblank;
    d0 = col_index_ptr[d0];                 /* heightmap id */
    if (d0 == 0)
        goto isblank;

    uint8_t angle = Col_AngleMap[d0];
    int16_t d1 = y;
    if (d4 & 0x1000) {                      /* yflip */
        d1 = (int16_t)~y;
        angle = (uint8_t)(angle + 0x40);
        angle = (uint8_t)(0 - angle);
        angle = (uint8_t)(angle - 0x40);
    }
    if (d4 & 0x800) {                       /* xflip */
        angle = (uint8_t)(0 - angle);
    }
    *a4 = angle;

    d1 = (d1 & 0xF) + (int16_t)(d0 << 4);
    int16_t height = (int8_t)Col_CollArray2[d1];    /* ext.w */
    d4 ^= (uint16_t)d6;
    if (d4 & 0x800)                         /* xflip on collision data */
        height = -height;

    if (height == 0)
        goto isblank;
    if (height < 0) {
        if ((int16_t)((x & 0xF) + height) >= 0)
            goto isblank;
    } else if (height != 0x10) {
        /* normal wall distance */
        *out_d1 = (int16_t)(0xF - ((x & 0xF) + height));
        return;
    }

    /* maxfloor / negative floor: try the block to the left */
    FindWall2(y, x - a3, d5, d6, a3, a4, obj, out_d1);
    *out_d1 -= 0x10;
    return;

isblank:
    /* not solid: try the block to the right */
    FindWall2(y, x + a3, d5, d6, a3, a4, obj, out_d1);
    *out_d1 += 0x10;
}

/* ===========================================================================
   FindWall2 (recursive helper for max-height / neg-floor cases)
   =========================================================================== */
void FindWall2(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3,
               uint8_t *a4, const void *obj, int16_t *out_d1) {
    (void)a3;
    uint8_t *a1;
    uint16_t word;
    FindNearestTile(y, x, obj, &a1, &word);

    uint16_t d4 = word;
    uint16_t d0 = word & 0x7FF;

    if (d0 == 0 || !(d4 & (1u << d5)))
        goto isblank;
    if (col_index_ptr == NULL || Col_AngleMap == NULL || Col_CollArray2 == NULL)
        goto isblank;
    d0 = col_index_ptr[d0];                 /* heightmap id */
    if (d0 == 0)
        goto isblank;

    uint8_t angle = Col_AngleMap[d0];
    int16_t d1 = y;
    if (d4 & 0x1000) {                      /* yflip */
        d1 = (int16_t)~y;
        angle = (uint8_t)(angle + 0x40);
        angle = (uint8_t)(0 - angle);
        angle = (uint8_t)(angle - 0x40);
    }
    if (d4 & 0x800) {                       /* xflip */
        angle = (uint8_t)(0 - angle);
    }
    *a4 = angle;

    d1 = (d1 & 0xF) + (int16_t)(d0 << 4);
    int16_t height = (int8_t)Col_CollArray2[d1];    /* ext.w */
    d4 ^= (uint16_t)d6;
    if (d4 & 0x800)                         /* xflip on collision data */
        height = -height;

    if (height == 0)
        goto isblank;
    if (height < 0) {
        int16_t tmp = (int16_t)((x & 0xF) + height);
        if (tmp >= 0)
            goto isblank;
        *out_d1 = (int16_t)(uint16_t)(~tmp);        /* not.w */
        return;
    }
    *out_d1 = (int16_t)(0xF - ((x & 0xF) + height));
    return;

isblank:
    /* no solid block: distance to right edge of the 16x16 block */
    *out_d1 = (int16_t)(0xF - (x & 0xF));
}

/* ===========================================================================
   Angle_Data — one byte per 45° section of a circle. The other quadrants
   are read by adding multiples of $40. The extra 257th byte handles the
   X == Y case (ratio exactly $100). (CalcAngle.asm:76-94)
   =========================================================================== */
static const uint8_t Angle_Data[257] = {
    0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,
    3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,
    8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  0xA, 0xA, 0xA,
    0xA, 0xA, 0xA, 0xA, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xC, 0xC,
    0xC, 0xC, 0xD, 0xD, 0xD, 0xD, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xE, 0xE, 0xE, 0xE,
    0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13,
    0x13, 0x13, 0x13, 0x13, 0x13, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17,
    0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
    0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C,
    0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D,
    0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1F, 0x1F,
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20
};

/* ===========================================================================
   CalcAngle — arctangent of (dx, dy)
   Input: dx = x distance, dy = y distance
   Output: angle (0..255)
   =========================================================================== */
uint8_t CalcAngle(int16_t dx, int16_t dy) {
    int16_t d3 = dx;
    int16_t d4 = dy;
    if ((d3 | d4) == 0)
        return 0x40;                        /* CA_NullAngle */

    if (d3 < 0) d3 = -d3;                   /* positive X distance */
    if (d4 < 0) d4 = -d4;                   /* positive Y distance */

    uint8_t angle;
    if (d4 >= d3) {
        /* Y has the larger distance: degrees 45 to 90 */
        uint16_t ratio = (uint16_t)(((uint32_t)d3 << 8) / (uint16_t)d4);
        angle = (uint8_t)(0x40 - Angle_Data[ratio]);
    } else {
        /* X has the larger distance: degrees 0 to 45 */
        uint16_t ratio = (uint16_t)(((uint32_t)d4 << 8) / (uint16_t)d3);
        angle = Angle_Data[ratio];
    }

    if (dx < 0)                             /* mirror angle */
        angle = (uint8_t)(0x80 - angle);
    if (dy < 0)                             /* flip angle */
        angle = (uint8_t)(0 - angle);

    return angle;
}

/* ===========================================================================
   Sonic collision helpers (Sonic Collision.asm). The Quick routines double
   as the tail of Sonic_CalcRoomAhead, which preloads d2/d3 with Sonic's
   predicted next-frame position.
   =========================================================================== */

/* Sonic_SnapAngle (Sonic Collision.asm:206-214): d1 passes through; d3 is
   the angle, snapped to d2 if bit 0 of the angle is set. */
static void sonic_snap_angle(int16_t d2, int16_t d1, int16_t *out_d1, uint8_t *out_d3) {
    uint8_t d3 = v_anglebuffer;
    if (d3 & 0x01)
        d3 = (uint8_t)d2;
    if (out_d1) *out_d1 = d1;
    if (out_d3) *out_d3 = d3;
}

/* Sonic_FindFloor_Quick (Sonic Collision.asm:196-215), with d2/d3 preset. */
static int16_t find_floor_quick(const void *obj, int16_t y, int16_t x) {
    int16_t d1;
    uint8_t d3;
    FindFloor(y + sonic_quick_size, x, 0x0E, 0, 0x10, &v_anglebuffer, obj, &d1);
    sonic_snap_angle(0, d1, &d1, &d3);
    return d1;
}

/* Sonic_FindCeiling_Quick (Sonic Collision.asm:424-434), with d2/d3 preset. */
static int16_t find_ceiling_quick(const void *obj, int16_t y, int16_t x) {
    int16_t d1;
    uint8_t d3;
    y = (int16_t)((y - sonic_quick_size) ^ 0xF);
    FindFloor(y, x, 0x0E, 0x1000, -0x10, &v_anglebuffer, obj, &d1);
    sonic_snap_angle(0x80, d1, &d1, &d3);
    return d1;
}

/* Sonic_FindWallRight_Quick (Sonic Collision.asm:298-307), with d2/d3 preset. */
static int16_t find_wall_right_quick(const void *obj, int16_t y, int16_t x) {
    int16_t d1;
    uint8_t d3;
    FindWall(y, x + sonic_quick_size, 0x0E, 0, 0x10, &v_anglebuffer, obj, &d1);
    sonic_snap_angle(0xC0, d1, &d1, &d3);
    return d1;
}

/* Sonic_FindWallLeft_Quick (Sonic Collision.asm:553-563), with d2/d3 preset. */
static int16_t find_wall_left_quick(const void *obj, int16_t y, int16_t x) {
    int16_t d1;
    uint8_t d3;
    x = (int16_t)((x - sonic_quick_size) ^ 0xF);
    FindWall(y, x, 0x0E, 0x800, -0x10, &v_anglebuffer, obj, &d1);
    sonic_snap_angle(0x40, d1, &d1, &d3);
    return d1;
}

/* ===========================================================================
   Sonic_CalcRoomAhead — calculate distance from Sonic to the wall in front
   Input: angle_ahead = Sonic's floor angle rotated 90 degrees
   Output: distance to wall (d1)
   =========================================================================== */
int16_t Sonic_CalcRoomAhead(void *obj, uint8_t angle_ahead) {
    uint8_t *o = (uint8_t *)obj;

    /* d3 = predicted x pos. at next frame: obX + (velX << 8) */
    int32_t d3 = ((int32_t)(uint16_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
    d3 += (int32_t)obVelX(o) << 8;
    /* d2 = predicted y pos. at next frame: obY + (velY << 8) */
    int32_t d2 = ((int32_t)(uint16_t)obY(o) << 16) | (uint16_t)obSubpixelY(o);
    d2 += (int32_t)obVelY(o) << 8;

    int16_t y = (int16_t)(d2 >> 16);        /* swap d2 */
    int16_t x = (int16_t)(d3 >> 16);        /* swap d3 */

    v_anglebuffer = angle_ahead;
    v_anglebuffer2 = angle_ahead;

    uint8_t d0 = (uint8_t)(angle_ahead + 0x20);
    if (d0 & 0x80) {                        /* addi.b didn't go bpl (negative) */
        d0 = angle_ahead;
        if (d0 & 0x80)                      /* bmi along the way */
            d0--;
        d0 = (uint8_t)(d0 + 0x20);
    } else {
        d0 = angle_ahead;
        if (d0 & 0x80)                      /* addq.b #1 */
            d0++;
        d0 = (uint8_t)(d0 + 0x1F);
    }

    d0 &= 0xC0;
    if (d0 == 0x00)
        return find_floor_quick(obj, y, x);
    if (d0 == 0x80)
        return find_ceiling_quick(obj, y, x);

    if ((angle_ahead & 0x38) == 0)          /* andi.b #$38,d1 */
        y += 8;                             /* addq.w #8,d2 */

    if (d0 == 0x40)
        return find_wall_left_quick(obj, y, x);
    return find_wall_right_quick(obj, y, x);
}

/* ===========================================================================
   Sonic_CalcHeadroom — calculate distance from Sonic's head to the ceiling
   Input: angle_inverted = Sonic's floor angle inverted
   Output: distance to ceiling (d1)
   =========================================================================== */
int16_t Sonic_CalcHeadroom(void *obj, uint8_t angle_inverted) {
    v_anglebuffer = angle_inverted;
    v_anglebuffer2 = angle_inverted;

    uint8_t d0 = (uint8_t)(angle_inverted + 0x20) & 0xC0;
    if (d0 == 0x40) {
        int16_t d1;
        Sonic_FindWallLeft(obj, NULL, &d1, NULL);
        return d1;
    }
    if (d0 == 0x80) {
        int16_t d1;
        Sonic_FindCeiling(obj, NULL, &d1, NULL);
        return d1;
    }
    if (d0 == 0xC0) {
        int16_t d1;
        Sonic_FindWallRight(obj, NULL, &d1, NULL);
        return d1;
    }

    /* El ASM cae aquí sin tocar d1 (queda undefined). En la práctica
       significa "sin techo detectado". Devolvemos un valor grande para
       que `if (headroom < 6)` no bloquee el salto. */
    return 0x7FFF;
}

/* ===========================================================================
   Sonic_FindFloor — find distance to floor (with width/height checks)
   Output: d0 = larger distance, d1 = smaller distance, d3 = floor angle
   =========================================================================== */
void Sonic_FindFloor(void *obj, int16_t *out_d0, int16_t *out_d1, uint8_t *out_d3) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d2 = (int16_t)(obY(o) + obHeight(o));
    int16_t d3 = (int16_t)(obX(o) + obWidth(o));
    int16_t d1;
    FindFloor(d2, d3, 0x0D, 0, 0x10, &v_anglebuffer, obj, &d1);        /* right side */
    int16_t d0 = d1;                        /* saved to stack */
    d2 = (int16_t)(obY(o) + obHeight(o));
    d3 = (int16_t)(obX(o) - obWidth(o));
    FindFloor(d2, d3, 0x0D, 0, 0x10, &v_anglebuffer2, obj, &d1);       /* left side */
    Sonic_FindSmaller(d0, d1, 0, out_d0, out_d1, out_d3);
}

/* ===========================================================================
   Sonic_FindFloor_Quick — quick floor check (no width/height)
   Output: distance to floor
   =========================================================================== */
int16_t Sonic_FindFloor_Quick(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    return find_floor_quick(obj, obY(o), obX(o));
}

/* ===========================================================================
   Sonic_FindCeiling — find distance to ceiling (with width/height checks)
   =========================================================================== */
void Sonic_FindCeiling(void *obj, int16_t *out_d0, int16_t *out_d1, uint8_t *out_d3) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d2 = (int16_t)((obY(o) - obHeight(o)) ^ 0xF);
    int16_t d3 = (int16_t)(obX(o) + obWidth(o));
    int16_t d1;
    FindFloor(d2, d3, 0x0E, 0x1000, -0x10, &v_anglebuffer, obj, &d1);   /* right side */
    int16_t d0 = d1;                        /* saved to stack */
    d2 = (int16_t)((obY(o) - obHeight(o)) ^ 0xF);
    d3 = (int16_t)(obX(o) - obWidth(o));
    FindFloor(d2, d3, 0x0E, 0x1000, -0x10, &v_anglebuffer2, obj, &d1);  /* left side */
    Sonic_FindSmaller(d0, d1, 0x80, out_d0, out_d1, out_d3);
}

/* ===========================================================================
   Sonic_FindCeiling_Quick
   =========================================================================== */
int16_t Sonic_FindCeiling_Quick(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    return find_ceiling_quick(obj, obY(o), obX(o));
}

/* ===========================================================================
   Sonic_FindWallRight — find distance to right wall (with width/height)
   =========================================================================== */
void Sonic_FindWallRight(void *obj, int16_t *out_d0, int16_t *out_d1, uint8_t *out_d3) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d2 = (int16_t)(obY(o) - obWidth(o));
    int16_t d3 = (int16_t)(obX(o) + obHeight(o));
    int16_t d1;
    FindWall(d2, d3, 0x0E, 0, 0x10, &v_anglebuffer, obj, &d1);          /* upper edge */
    int16_t d0 = d1;                        /* saved to stack */
    d2 = (int16_t)(obY(o) + obWidth(o));
    d3 = (int16_t)(obX(o) + obHeight(o));
    FindWall(d2, d3, 0x0E, 0, 0x10, &v_anglebuffer2, obj, &d1);         /* lower edge */
    Sonic_FindSmaller(d0, d1, 0xC0, out_d0, out_d1, out_d3);
}

/* ===========================================================================
   Sonic_FindWallRight_Quick
   =========================================================================== */
int16_t Sonic_FindWallRight_Quick(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    return find_wall_right_quick(obj, obY(o), obX(o));
}

/* ===========================================================================
   Sonic_FindWallLeft — find distance to the left wall (with width/height)
   =========================================================================== */
void Sonic_FindWallLeft(void *obj, int16_t *out_d0, int16_t *out_d1, uint8_t *out_d3) {
    uint8_t *o = (uint8_t *)obj;
    int16_t d2 = (int16_t)(obY(o) - obWidth(o));
    int16_t d3 = (int16_t)((obX(o) - obHeight(o)) ^ 0xF);
    int16_t d1;
    FindWall(d2, d3, 0x0E, 0x800, -0x10, &v_anglebuffer, obj, &d1);     /* upper edge */
    int16_t d0 = d1;                        /* saved to stack */
    d2 = (int16_t)(obY(o) + obWidth(o));
    d3 = (int16_t)((obX(o) - obHeight(o)) ^ 0xF);
    FindWall(d2, d3, 0x0E, 0x800, -0x10, &v_anglebuffer2, obj, &d1);    /* lower edge */
    Sonic_FindSmaller(d0, d1, 0x40, out_d0, out_d1, out_d3);
}

/* ===========================================================================
   Sonic_FindWallLeft_Quick
   =========================================================================== */
int16_t Sonic_FindWallLeft_Quick(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    return find_wall_left_quick(obj, obY(o), obX(o));
}

/* ===========================================================================
   Sonic_FindSmaller — make d1 the smaller of d0/d1, pick angle from buffer
   Inputs: d0 = first distance, d1 = second distance, d2 = angle snap value
   Outputs: d0 = larger distance, d1 = smaller distance, d3 = angle
   =========================================================================== */
void Sonic_FindSmaller(int16_t d0, int16_t d1, int16_t d2,
                       int16_t *out_d0, int16_t *out_d1, uint8_t *out_d3) {
    uint8_t d3 = v_anglebuffer2;
    if (d1 > d0) {                          /* ble.s .no_swap — swap when d1 > d0 */
        d3 = v_anglebuffer;
        int16_t t = d0; d0 = d1; d1 = t;
    }
    if (d3 & 0x01)                          /* snap angle if bit 0 is set */
        d3 = (uint8_t)d2;
    if (out_d0) *out_d0 = d0;
    if (out_d1) *out_d1 = d1;
    if (out_d3) *out_d3 = d3;
}

/* ===========================================================================
   Sonic_SnapAngle — snap angle if bit 0 is set (d1 passes through)
   =========================================================================== */
void Sonic_SnapAngle(int16_t d2, int16_t d1, int16_t *out_d1, uint8_t *out_d3) {
    sonic_snap_angle(d2, d1, out_d1, out_d3);
}

/* ===========================================================================
   Sonic_Angle — update angle based on left/right floor distances
   Inputs: d0 = right distance, d1 = left distance
   Outputs: d1 = smaller distance, d2 = angle (obAngle updated in place)
   (Sonic AnglePos.asm:186-208)
   =========================================================================== */
void Sonic_Angle(int16_t d0, int16_t d1, void *obj, int16_t *out_d1, uint8_t *out_d2) {
    uint8_t *o = (uint8_t *)obj;
    uint8_t d2 = v_anglebuffer2;            /* use left side angle */
    if (d1 > d0) {                          /* cmp.w d0,d1 / ble.s */
        d2 = v_anglebuffer;                 /* use right side angle */
        d1 = d0;                            /* use right side distance */
    }
    if (!(d2 & 0x01)) {
        obAngle(o) = d2;                    /* update angle */
    } else {
        obAngle(o) = (uint8_t)((obAngle(o) + 0x20) & 0xC0);  /* snap */
    }
    if (out_d1) *out_d1 = d1;
    if (out_d2) *out_d2 = d2;
}

/* Shared tail for the four Walk routines: align Sonic to the surface, or
   flag him as in the air when he is more than 14px away. `sign` is +1 for
   the floor-style routines (obY += d1) and -1 for the ceiling-style ones. */
static void sonic_align_or_air(void *obj, int16_t d1, int axis, int sign) {
    uint8_t *o = (uint8_t *)obj;
    if (d1 > 0) {
        if (d1 > 0xE && !sticktoconvex(o)) {
            obStatus(o) |= 0x02;            /* bset #1 */
            obStatus(o) &= (uint8_t)~0x20;  /* bclr #5 */
            obPrevAni(o) = 0x01;            /* id_Run */
            return;
        }
    } else if (d1 < -0xE) {
        return;                             /* Sonic_BelowFloor / InsideWall */
    } else if (d1 == 0) {
        return;                             /* .on_floor / .on_wall / .on_ceiling */
    }
    if (axis == 0)
        obX(o) = (int16_t)(obX(o) + sign * d1);
    else
        obY(o) = (int16_t)(obY(o) + sign * d1);
}

/* ===========================================================================
   Sonic_AnglePos — update Sonic's angle as he walks along floor
   (Sonic AnglePos.asm:6-171)
   =========================================================================== */
void Sonic_AnglePos(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (obStatus(o) & 0x08) {               /* btst #3: on a platform? */
        v_anglebuffer = 0;                  /* clear angle hotspots */
        v_anglebuffer2 = 0;
        return;
    }

    v_anglebuffer = 3;
    v_anglebuffer2 = 3;

    uint8_t angle = obAngle(o);
    uint8_t d0;
    if ((int8_t)(angle + 0x20) >= 0) {      /* bpl .floor_or_left */
        d0 = angle;
        if ((int8_t)d0 < 0)
            d0++;                            /* addq.b #1 */
        d0 = (uint8_t)(d0 + 0x1F);
    } else {
        d0 = angle;
        if ((int8_t)d0 >= 0) {
            d0 = (uint8_t)(d0 + 0x20);
        } else {
            d0--;                            /* subq.b #1 */
            d0 = (uint8_t)(d0 + 0x20);
        }
    }

    d0 &= 0xC0;
    if (d0 == 0x40) { Sonic_WalkVertL(obj); return; }
    if (d0 == 0x80) { Sonic_WalkCeiling(obj); return; }
    if (d0 == 0xC0) { Sonic_WalkVertR(obj); return; }

    int16_t d1, d0w;
    int16_t d2 = (int16_t)(obY(o) + (int8_t)obHeight(o));
    int16_t d3 = (int16_t)(obX(o) + (int8_t)obWidth(o));
    FindFloor(d2, d3, 0x0D, 0, 0x10, &v_anglebuffer, obj, &d1);
    d0w = d1;                               /* distance to floor (right side) */

    d2 = (int16_t)(obY(o) + (int8_t)obHeight(o));
    d3 = (int16_t)(obX(o) - (int8_t)obWidth(o));
    FindFloor(d2, d3, 0x0D, 0, 0x10, &v_anglebuffer2, obj, &d1);

    Sonic_Angle(d0w, d1, obj, &d1, NULL);
    sonic_align_or_air(obj, d1, 1, +1);
}

/* ===========================================================================
   Sonic_WalkVertR — walk up vertical wall to right
   (Sonic AnglePos.asm:215-280)
   =========================================================================== */
void Sonic_WalkVertR(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    int16_t d1, d0w;
    int16_t d2 = (int16_t)(obY(o) - (int8_t)obWidth(o));
    int16_t d3 = (int16_t)(obX(o) + (int8_t)obHeight(o));
    FindWall(d2, d3, 0x0D, 0, 0x10, &v_anglebuffer, obj, &d1);
    d0w = d1;                               /* distance to wall (upper side) */

    d2 = (int16_t)(obY(o) + (int8_t)obWidth(o));
    d3 = (int16_t)(obX(o) + (int8_t)obHeight(o));
    FindWall(d2, d3, 0x0D, 0, 0x10, &v_anglebuffer2, obj, &d1);

    Sonic_Angle(d0w, d1, obj, &d1, NULL);
    sonic_align_or_air(obj, d1, 0, +1);
}

/* ===========================================================================
   Sonic_WalkCeiling — walk upside-down on ceiling
   (Sonic AnglePos.asm:287-353)
   =========================================================================== */
void Sonic_WalkCeiling(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    int16_t d1, d0w;
    int16_t d2 = (int16_t)((obY(o) - (int8_t)obHeight(o)) ^ 0xF);
    int16_t d3 = (int16_t)(obX(o) + (int8_t)obWidth(o));
    FindFloor(d2, d3, 0x0D, 0x1000, -0x10, &v_anglebuffer, obj, &d1);
    d0w = d1;                               /* distance to ceiling (right side) */

    d2 = (int16_t)((obY(o) - (int8_t)obHeight(o)) ^ 0xF);
    d3 = (int16_t)(obX(o) - (int8_t)obWidth(o));
    FindFloor(d2, d3, 0x0D, 0x1000, -0x10, &v_anglebuffer2, obj, &d1);

    Sonic_Angle(d0w, d1, obj, &d1, NULL);
    sonic_align_or_air(obj, d1, 1, -1);
}

/* ===========================================================================
   Sonic_WalkVertL — walk up vertical wall to left
   (Sonic AnglePos.asm:360-426)
   =========================================================================== */
void Sonic_WalkVertL(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    int16_t d1, d0w;
    int16_t d2 = (int16_t)(obY(o) - (int8_t)obWidth(o));
    int16_t d3 = (int16_t)((obX(o) - (int8_t)obHeight(o)) ^ 0xF);
    FindWall(d2, d3, 0x0D, 0x800, -0x10, &v_anglebuffer, obj, &d1);
    d0w = d1;                               /* distance to wall (upper side) */

    d2 = (int16_t)(obY(o) + (int8_t)obWidth(o));
    d3 = (int16_t)((obX(o) - (int8_t)obHeight(o)) ^ 0xF);
    FindWall(d2, d3, 0x0D, 0x800, -0x10, &v_anglebuffer2, obj, &d1);

    Sonic_Angle(d0w, d1, obj, &d1, NULL);
    sonic_align_or_air(obj, d1, 0, -1);
}

/* ===========================================================================
   ObjectFall — apply gravity and update position
   (_incObj/sub ObjectFall & SpeedToPos.asm:8-24)
   =========================================================================== */
void ObjectFall(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    int32_t x = ((int32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
    int32_t y = ((int32_t)obY(o) << 16) | (uint16_t)obSubpixelY(o);

    int32_t vel_x = (int32_t)obVelX(o) << 8;
    int32_t vel_y = (int32_t)obVelY(o) << 8;   /* velocidad VIEJA */

    obVelY(o) = (int16_t)(obVelY(o) + gravity); /* gravedad se aplica DESPUÉS */

    x += vel_x;
    y += vel_y;

    obX(o) = (int16_t)(x >> 16);
    obSubpixelX(o) = (int16_t)(x & 0xFFFF);
    obY(o) = (int16_t)(y >> 16);
    obSubpixelY(o) = (int16_t)(y & 0xFFFF);
}