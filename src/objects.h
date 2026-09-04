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

/* Get a pointer to the object at the given index */
static inline void *Object_GetSlot(int index) {
    return &ram[v_objspace + OBJECT_SIZE * index];
}

/* Get the index of an object pointer */
static inline int Object_GetIndex(void *obj) {
    return (int)((uint8_t *)obj - ObjRAM) / OBJECT_SIZE;
}

#endif /* SONIC1_OBJECTS_H */
