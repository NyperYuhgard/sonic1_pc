#include "objects.h"
#include "ram.h"
#include "constants.h"
#include <string.h>

/* Object dispatch table - will be populated as objects are ported.
   Index 0 = no object, indices 1-N = object IDs.
   For now, all entries are NULL stubs. */
static ObjFunc obj_dispatch[256];

void Objects_Init(void) {
    memset(obj_dispatch, 0, sizeof(obj_dispatch));
    /* Clear all object RAM */
    memset(ObjRAM, 0, NUM_OBJECTS * OBJECT_SIZE);
}

void ExecuteObjects(void) {
    uint8_t *obj = ObjRAM;

    /* Check if Sonic is dying (routine >= 6) */
    uint8_t sonic_routine = RAM_BYTE(v_player + obRoutine(NULL));
    /* We use the raw offset since obRoutine is a macro that needs a pointer */
    sonic_routine = *(uint8_t *)(&ram[v_player + 0x24]);

    if (sonic_routine >= 6) {
        /* When Sonic is dead: run first 32 objects normally, display-only for rest */
        for (int i = 0; i < 32; i++) {
            uint8_t id = obj[i * OBJECT_SIZE];
            if (id != 0 && obj_dispatch[id]) {
                obj_dispatch[id](&obj[i * OBJECT_SIZE]);
            }
        }
        for (int i = 32; i < NUM_OBJECTS; i++) {
            uint8_t id = obj[i * OBJECT_SIZE];
            if (id != 0) {
                uint8_t render = obj[i * OBJECT_SIZE + 1]; /* obRender */
                if (render & 0x80) { /* sprite_rendered_bit */
                    DisplaySprite(&obj[i * OBJECT_SIZE]);
                }
            }
        }
    } else {
        /* Normal: execute all objects */
        for (int i = 0; i < NUM_OBJECTS; i++) {
            uint8_t id = obj[i * OBJECT_SIZE];
            if (id != 0 && obj_dispatch[id]) {
                obj_dispatch[id](&obj[i * OBJECT_SIZE]);
            }
        }
    }
}

void DisplaySprite(void *obj) {
    /* TODO: Add sprite to priority queue for BuildSprites */
    (void)obj;
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
