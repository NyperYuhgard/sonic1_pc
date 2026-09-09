#ifndef SONIC1_COLLISION_H
#define SONIC1_COLLISION_H

#include "types.h"
#include "ram.h"

/* ===========================================================================
   Collision core — ported from _incObj/sub FindNearestTile & FindFloor & FindWall.asm
   and _incObj/Sonic Collision.asm + _incObj/Sonic AnglePos.asm
   =========================================================================== */

/* Global collision index pointer (set by ColIndexLoad in level.c) */
extern const uint8_t *col_index_ptr;

/* Get the current zone's collision index table (AngleMap) */
static inline const uint8_t *GetColIndex(void) {
    return col_index_ptr;
}

/* BE word read from the 256x256 layout (v_lvllayout_fg) */
static inline uint16_t layout_be16(const uint8_t *p);

/* BE word read from the 16x16 block mappings (v_16x16) */
static inline uint16_t block_be16(const uint8_t *p);

/* ===========================================================================
   FindNearestTile
   Inputs:
     d2 = y position (pixel)
     d3 = x position (pixel)
   Outputs:
     a1 = address in 256x256 layout
     d1 = 16x16 block word (x/yflip + solidness + block ID)
   =========================================================================== */
void FindNearestTile(int16_t y, int16_t x, uint8_t **out_a1, uint16_t *out_d1);

/* ===========================================================================
   FindFloor2 (helper for blank-tile continuation)
   Inputs: a1 = layout address, d2 = y, d3 = x
   Output: d1 = distance to floor
   =========================================================================== */
void FindFloor2(int16_t y, int16_t x, uint8_t *a1, int16_t *out_d1);

/* ===========================================================================
   FindFloor
   Inputs:
     d2 = y position of object's bottom edge
     d3 = x position of object
     d5 = bit to test for solidness ($D = top solid; $E = left/right/bottom solid)
     d6 = eor bitmask for 16x16 block
     a3 = height of 16x16 blocks ($10 or -$10 if inverted)
     a4 = RAM address to write angle byte
   Outputs:
     d1 = distance to floor
     a1 = address within 256x256 mappings
     (a1).w = 16x16 block word
     (a4).b = floor angle
   =========================================================================== */
void FindFloor(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3, uint8_t *a4, int16_t *out_d1);

/* ===========================================================================
   FindWall
   Inputs (same as FindFloor, but checks vertical solidness):
     d2 = y position
     d3 = x position
     d5 = bit to test ($E = left/right/bottom solid)
     d6 = eor bitmask
     a3 = tile width ($10 or -$10 if xflip)
     a4 = RAM address to write angle byte
   Outputs:
     d1 = distance to wall
     a1 = address within 256x256 mappings
     (a4).b = wall angle
   =========================================================================== */
void FindWall(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3, uint8_t *a4, int16_t *out_d1);

/* ===========================================================================
   FindWall2 (recursive helper for max-height / neg-floor cases)
   =========================================================================== */
void FindWall2(int16_t y, int16_t x, uint8_t d5, int16_t d6, int16_t a3, uint8_t *a4, int16_t *out_d1);

/* ===========================================================================
   CalcAngle — arctangent of (d1, d2)
   Input: d1 = x distance, d2 = y distance
   Output: d0 = angle (0..255)
   =========================================================================== */
uint8_t CalcAngle(int16_t dx, int16_t dy);

/* ===========================================================================
   Sonic_CalcRoomAhead — calculate distance to wall ahead
   Input: d0 = Sonic's floor angle rotated 90 degrees
   Output: d1 = distance to wall
   =========================================================================== */
int16_t Sonic_CalcRoomAhead(uint8_t angle_ahead);

/* ===========================================================================
   Sonic_CalcHeadroom — calculate distance to ceiling
   Input: d0 = Sonic's floor angle inverted
   Output: d1 = distance to ceiling
   =========================================================================== */
int16_t Sonic_CalcHeadroom(uint8_t angle_inverted);

/* ===========================================================================
   Sonic_FindFloor — find distance to floor (with width/height checks)
   Outputs: d0 = distance (larger if slope), d1 = distance (smaller if slope),
            d3 = floor angle, a1 = layout address, (a4) = angle
   =========================================================================== */
void Sonic_FindFloor(void *obj, int16_t *out_d0, int16_t *out_d1, uint8_t *out_d3);

/* ===========================================================================
   Sonic_FindFloor_Quick — quick floor check (no width/height)
   Output: d1 = distance to floor
   =========================================================================== */
int16_t Sonic_FindFloor_Quick(void *obj);

/* ===========================================================================
   Sonic_FindCeiling — find distance to ceiling
   =========================================================================== */
void Sonic_FindCeiling(void *obj, int16_t *out_d0, int16_t *out_d1);

/* ===========================================================================
   Sonic_FindCeiling_Quick
   =========================================================================== */
int16_t Sonic_FindCeiling_Quick(void *obj);

/* ===========================================================================
   Sonic_FindWallRight — find distance to right wall
   =========================================================================== */
void Sonic_FindWallRight(void *obj, int16_t *out_d0, int16_t *out_d1);

/* ===========================================================================
   Sonic_FindWallRight_Quick
   =========================================================================== */
int16_t Sonic_FindWallRight_Quick(void *obj);

/* ===========================================================================
   Sonic_FindWallLeft — find distance to left wall
   =========================================================================== */
void Sonic_FindWallLeft(void *obj, int16_t *out_d0, int16_t *out_d1);

/* ===========================================================================
   Sonic_FindWallLeft_Quick
   =========================================================================== */
int16_t Sonic_FindWallLeft_Quick(void *obj);

/* ===========================================================================
   Sonic_FindSmaller — make d1 the smaller of d0/d1, pick angle from buffer
   =========================================================================== */
void Sonic_FindSmaller(int16_t d0, int16_t d1, int16_t *out_d1, uint8_t *out_d3);

/* ===========================================================================
   Sonic_SnapAngle — snap angle if bit 0 is set
   =========================================================================== */
void Sonic_SnapAngle(uint8_t *out_d3);

/* ===========================================================================
   Sonic_Angle — update angle based on left/right floor distances
   Inputs: d0 = right distance, d1 = left distance
   Outputs: d1 = smaller distance, d2 = angle
   =========================================================================== */
void Sonic_Angle(int16_t d0, int16_t d1, void *obj, int16_t *out_d1, uint8_t *out_d2);

/* ===========================================================================
   Sonic_AnglePos — update Sonic's angle as he walks along floor
   =========================================================================== */
void Sonic_AnglePos(void *obj);

/* ===========================================================================
   Sonic_WalkVertR — walk up vertical wall to right
   =========================================================================== */
void Sonic_WalkVertR(void *obj);

/* ===========================================================================
   Sonic_WalkCeiling — walk upside-down on ceiling
   =========================================================================== */
void Sonic_WalkCeiling(void *obj);

/* ===========================================================================
   Sonic_WalkVertL — walk up vertical wall to left
   =========================================================================== */
void Sonic_WalkVertL(void *obj);

/* ===========================================================================
    ObjectFall — apply gravity and update position
    =========================================================================== */
void ObjectFall(void *obj);

#endif /* SONIC1_COLLISION_H */