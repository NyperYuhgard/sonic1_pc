#ifndef SONIC1_OBJECTS_H
#define SONIC1_OBJECTS_H

#include "types.h"
#include "ram.h"

/* Number of objects and slot size (from constants.h) */
#define NUM_OBJECTS      128
#define OBJECT_SIZE      64

/* Pointer to the start of object RAM in ram[] */
#define ObjRAM (&ram[v_objspace])

/* Object dispatch function type */
typedef void (*ObjFunc)(void *obj);

/* Initialize the object system */
void Objects_Init(void);

/* Execute all active objects (translated from ExecuteObjects.asm) */
void ExecuteObjects(void);

/* Display a sprite in the sprite queue (translated from DisplaySprite.asm) */
void DisplaySprite(void *obj);

/* Find a free object slot (translated from FindFreeObj.asm) */
void *FindFreeObj(void);

/* Delete an object (translated from DeleteObject.asm) */
void DeleteObject(void *obj);

/* Has every title card element reached its resting X-position? */
int TitleCardsSettled(void);

/* Get a pointer to the object at the given index */
static inline void *Object_GetSlot(int index) {
    return &ram[v_objspace + OBJECT_SIZE * index];
}

/* Get the index of an object pointer */
static inline int Object_GetIndex(void *obj) {
    return (int)((uint8_t *)obj - ObjRAM) / OBJECT_SIZE;
}

/* Calculate sine (d0) and cosine (d1) of an angle
   (ported from _incObj/sub CalcSine.asm, REV01, FixBugs=0).
   angle = value in [0,255]; outputs are Sine_Data values (-0x100..0x100).
   The output regs map to salsa columns: d0 = *s0, d1 = *s1 (both words). */
void CalcSine(int angle, int16_t *s0, int16_t *s1);

/* Advance the synchronised animation timers (rings, bouncing rings). Only
   Sync2 (rings) and Sync4 (bouncing rings) are needed by Ring/RingLoss;
   Sync1/Sync3 are unported (see Important Details). Ported from the
   SynchroAnimate subroutine in sonic.asm (REV01, FixBugs=0). */
void SynchroAnimate(void);

/* Translate an object's obVelX/obVelY into a position change (pixel+subpixel,
   16.16 fixed point). Ported from _incObj/sub ObjectFall & SpeedToPos.asm
   (no gravity is applied). obX/obY carry the low word; obSubpixelX/Y the high. */
void SpeedToPos(void *obj);

/* Distance from the object's feet (obY + obHeight) to the floor. Ported
   from _incObj/sub ObjFloorDist.asm. Without a real 16x16 collision index,
   the distance is 0 (flat floor at Sonic's feet) and d3 snaps to angle 0.
   outputs: *dist = d1, *angle = d3. */
void ObjFloorDist(void *obj, int16_t *dist, int16_t *angle);

/* Is the object's spawn marker (default obX, else the passed field) outside
   the range of ± 128+320+192 px around the screen? Mirrors the out_of_range
   macro in Macros.asm. Returns nonzero when out of range. */
int OutOfRange(void *obj, int16_t ring_origX);

#endif /* SONIC1_OBJECTS_H */
