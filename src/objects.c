#include "objects.h"
#include "ram.h"
#include "constants.h"
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

/* Forward declarations for title screen objects */
static void TitleSonic_Main(void *obj);

void Objects_Init(void) {
    memset(obj_dispatch, 0, sizeof(obj_dispatch));

    /* Register title screen objects */
    obj_dispatch[id_TitleSonic] = TitleSonic_Main;

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
   TitleSonic object (id_TitleSonic = $0E)
   Minimal port: move right and display sprite
   =========================================================================== */
static void TitleSonic_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    int16_t x = (int16_t)((o[0x08] << 8) | o[0x09]);
    x += 2;
    o[0x08] = (uint8_t)(x >> 8);
    o[0x09] = (uint8_t)(x & 0xFF);

    DisplaySprite(obj);
}
