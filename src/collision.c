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
   Outputs:
     a1 = address in 256x256 layout (returned via out_a1)
     d1 = 16x16 block word (returned via out_d1)
   =========================================================================== */
void FindNearestTile(int16_t y, int16_t x, uint8_t **out_a1, uint16_t *out_d1) {
    (void)y; (void)x; (void)out_a1; (void)out_d1;
    /* TODO: full port from _incObj/sub FindNearestTile & FindFloor & FindWall.asm lines 1-88 */
}

/* ===========================================================================
   FindFloor2 (helper for blank-tile continuation)
   Inputs: y, x, a1 = layout address
   Output: d1 = distance to floor (returned via out_d1)
   =========================================================================== */
void FindFloor2(int16_t y, int16_t x, uint8_t *a1, int16_t *out_d1) {
    (void)y; (void)x; (void)a1; (void)out_d1;
    /* TODO: full port from _incObj/sub FindNearestTile & FindFloor & FindWall.asm lines 193-260 */
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
void FindFloor(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3, uint8_t *a4, int16_t *out_d1) {
    (void)y; (void)x; (void)d5; (void)d6; (void)a3; (void)a4; (void)out_d1;
    /* TODO: full port from _incObj/sub FindNearestTile & FindFloor & FindWall.asm lines 111-189 */
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
void FindWall(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3, uint8_t *a4, int16_t *out_d1) {
    (void)y; (void)x; (void)d5; (void)d6; (void)a3; (void)a4; (void)out_d1;
    /* TODO: full port from _incObj/sub FindNearestTile & FindFloor & FindWall.asm lines 277-456 */
}

/* ===========================================================================
   FindWall2 (recursive helper for max-height / neg-floor cases)
   =========================================================================== */
void FindWall2(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3, uint8_t *a4, int16_t *out_d1) {
    (void)y; (void)x; (void)d5; (void)d6; (void)a3; (void)a4; (void)out_d1;
    /* TODO: full port from _incObj/sub FindNearestTile & FindFloor & FindWall.asm lines 377-456 */
}

/* ===========================================================================
   CalcAngle — arctangent of (dx, dy)
   Input: dx = x distance, dy = y distance
   Output: angle (0..255)
   =========================================================================== */
uint8_t CalcAngle(int16_t dx, int16_t dy) {
    (void)dx; (void)dy;
    /* TODO: full port from _incObj/sub CalcAngle.asm */
    return 0;
}

/* ===========================================================================
   Sonic_CalcRoomAhead — calculate distance to wall ahead
   Input: angle_ahead = Sonic's floor angle rotated 90 degrees
   Output: distance to wall
   =========================================================================== */
int16_t Sonic_CalcRoomAhead(uint8_t angle_ahead) {
    (void)angle_ahead;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 13-72 */
    return 0;
}

/* ===========================================================================
   Sonic_CalcHeadroom — calculate distance to ceiling
   Input: angle_inverted = Sonic's floor angle inverted
   Output: distance to ceiling
   =========================================================================== */
int16_t Sonic_CalcHeadroom(uint8_t angle_inverted) {
    (void)angle_inverted;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 86-97 */
    return 0;
}

/* ===========================================================================
   Sonic_FindFloor — find distance to floor (with width/height checks)
   =========================================================================== */
void Sonic_FindFloor(void *obj, int16_t *out_d0, int16_t *out_d1, uint8_t *out_d3) {
    (void)obj; (void)out_d0; (void)out_d1; (void)out_d3;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 113-173 */
}

/* ===========================================================================
   Sonic_FindFloor_Quick — quick floor check (no width/height)
   Output: distance to floor
   =========================================================================== */
int16_t Sonic_FindFloor_Quick(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 196-215 */
    return 0;
}

/* ===========================================================================
   Sonic_FindCeiling — find distance to ceiling
   =========================================================================== */
void Sonic_FindCeiling(void *obj, int16_t *out_d0, int16_t *out_d1) {
    (void)obj; (void)out_d0; (void)out_d1;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 359-402 */
}

/* ===========================================================================
   Sonic_FindCeiling_Quick
   =========================================================================== */
int16_t Sonic_FindCeiling_Quick(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 419-434 */
    return 0;
}

/* ===========================================================================
   Sonic_FindWallRight — find distance to right wall
   =========================================================================== */
void Sonic_FindWallRight(void *obj, int16_t *out_d0, int16_t *out_d1) {
    (void)obj; (void)out_d0; (void)out_d1;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 233-274 */
}

/* ===========================================================================
   Sonic_FindWallRight_Quick
   =========================================================================== */
int16_t Sonic_FindWallRight_Quick(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 298-307 */
    return 0;
}

/* ===========================================================================
   Sonic_FindWallLeft — find distance to left wall
   =========================================================================== */
void Sonic_FindWallLeft(void *obj, int16_t *out_d0, int16_t *out_d1) {
    (void)obj; (void)out_d0; (void)out_d1;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 486-529 */
}

/* ===========================================================================
   Sonic_FindWallLeft_Quick
   =========================================================================== */
int16_t Sonic_FindWallLeft_Quick(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 553-563 */
    return 0;
}

/* ===========================================================================
   Sonic_FindSmaller — make d1 the smaller of d0/d1, pick angle from buffer
   =========================================================================== */
void Sonic_FindSmaller(int16_t d0, int16_t d1, int16_t *out_d1, uint8_t *out_d3) {
    (void)d0; (void)d1; (void)out_d1; (void)out_d3;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 154-172 */
}

/* ===========================================================================
   Sonic_SnapAngle — snap angle if bit 0 is set
   =========================================================================== */
void Sonic_SnapAngle(uint8_t *out_d3) {
    (void)out_d3;
    /* TODO: full port from _incObj/Sonic Collision.asm lines 206-214 */
}

/* ===========================================================================
   Sonic_Angle — update angle based on left/right floor distances
   Inputs: d0 = right distance, d1 = left distance
   Outputs: d1 = smaller distance, d2 = angle
   =========================================================================== */
void Sonic_Angle(int16_t d0, int16_t d1, void *obj, int16_t *out_d1, uint8_t *out_d2) {
    (void)d0; (void)d1; (void)obj; (void)out_d1; (void)out_d2;
    /* TODO: full port from _incObj/Sonic AnglePos.asm lines 186-208 */
}

/* ===========================================================================
   Sonic_AnglePos — update Sonic's angle as he walks along floor
   =========================================================================== */
void Sonic_AnglePos(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic AnglePos.asm lines 1-171 */
}

/* ===========================================================================
   Sonic_WalkVertR — walk up vertical wall to right
   =========================================================================== */
void Sonic_WalkVertR(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic AnglePos.asm lines 215-280 */
}

/* ===========================================================================
   Sonic_WalkCeiling — walk upside-down on ceiling
   =========================================================================== */
void Sonic_WalkCeiling(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic AnglePos.asm lines 287-353 */
}

/* ===========================================================================
   Sonic_WalkVertL — walk up vertical wall to left
   =========================================================================== */
void Sonic_WalkVertL(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/Sonic AnglePos.asm lines 360-426 */
}

/* ===========================================================================
   ObjectFall — apply gravity and update position
   =========================================================================== */
void ObjectFall(void *obj) {
    (void)obj;
    /* TODO: full port from _incObj/sub ObjectFall & SpeedToPos.asm */
}