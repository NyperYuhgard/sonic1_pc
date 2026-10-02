/* ===========================================================================
   Sonic 1 object system
   ---------------------------------------------------------------------------
   Every object lives in ObjRAM as a flat OBJECT_SIZE-byte record whose first
   byte is the object ID and whose code is selected by obRoutine. The ASM keeps
   each object in disasm/_incObj/XX *.asm; this file is the 1:1 C translation
   of those files, and the banner above every section names the .asm it came
   from so a diff against the disassembly stays mechanical.

   LAYOUT
     1. Sprite queue and forward declarations
     2. obj_map[] — the ID -> routine dispatch table
     3. Objects_Init / ExecuteObjects / DisplaySprite / FindFreeObj / DeleteObject
     4. Part 1  Shared object helpers (disasm/_incObj/sub / .asm)
     5. Part 2  Object implementations, $01-$2F
     6. Part 3  Object implementations, $30-$5F
     7. Part 4  Object implementations, $60-$8F

   Objects are laid out in ascending object-ID order, the same order as the
   obj_map[] table below, so each routine sits right next to its own banner and
   the file reads like the table. IDs with no implementation yet appear in
   neither: they resolve to NullObject_Main, which deletes the object on the
   spot, matching the ASM NullObject. Objects-List.md (repo root) lists every
   ID from $01 to $8C with its disasm source and its ported status.
   =========================================================================== */
#include "objects.h"
#include "ram.h"
#include "constants.h"
#include "data.h"
#include "config.h"
#include "sound.h"
#include "collision.h"
#include "plc.h"
#include "debugmode.h"
#include "special.h"
#include "vdp.h"
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
static void SSResult_Main(void *obj);
static void SSRChaos_Main(void *obj);
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
static void CollapseLedge_Main(void *obj);
static void CollapseFloor_Main(void *obj);
static void Scenery_Main(void *obj);
static void Platform_Main(void *obj);
static void ShieldItem_Main(void *obj);
static void Chopper_Main(void *obj);
static void SmashWall_Main(void *obj);
static void Smash_Main(uint8_t *o);
static void Smash_Solid(uint8_t *o);
static void Smash_Fragment(uint8_t *o);
static void Helix_Main(void *obj);
static void BossGreenHill_Main(void *obj);
static void BossBall_Main(void *obj);
static void SwingingPlatform_Main(void *obj);
static void Prison_Main(void *obj);
static void Newtron_Main(void *obj);
static void SonicSpecial_Main(void *obj);
static void GiantRing_Main(void *obj);
static void RingFlash_Main(void *obj);
static void GlassBlock_Main(void *obj);
static void ChainStomp_Main(void *obj);
static void Button_Main(void *obj);
static void PushBlock_Main(void *obj);
static void MovingBlock_Main(void *obj);
static void LavaTag_Main(void *obj);
static void MarbleBrick_Main(void *obj);
static void Basaran_Main(void *obj);
static void LargeGrass_Main(void *obj);
static void GrassFire_Main(void *obj);
static void Caterkiller_Main(void *obj);
static void LavaMaker_Main(void *obj);
static void LavaBall_Main(void *obj);
static void GeyserMaker_Main(void *obj);
static void LavaGeyser_Main(void *obj);
static void LavaWall_Main(void *obj);
static void SmashBlock_Main(void *obj);
static void BossMarble_Main(void *obj);
static void BossFire_Main(void *obj);
static void SpinningLight_Main(void *obj);
static void FloatingBlock_Main(void *obj);
static void Yadrin_Main(void *obj);
static void Bumper_Main(void *obj);
static void BigSpikeBall_Main(void *obj);
static void SpikeBall_Main(void *obj);
static void Roller_Main(void *obj);
static void BossSpringYard_Main(void *obj);
static void BossBlock_Main(void *obj);
static void WaterSplash_Main(void *obj);
/* Labyrinth Zone objects */
static void DrownCount_Main(void *obj);
static void Pole_ObjectMain(void *obj);
static void FlapDoor_Main(void *obj);
static void Harpoon_Main(void *obj);
static void WaterSurface_Main(void *obj);
static void Jaws_ObjectMain(void *obj);
static void Burrobot_Main(void *obj);
static void Orbinaut_Main(void *obj);
static void LabyrinthBlock_Main(void *obj);
static void Gargoyle_Main(void *obj);
static void LabyrinthConvey_Main(void *obj);
static void Bubble_Main(void *obj);
static void Waterfall_Main(void *obj);
static void BossLabyrinth_ObjectMain(void *obj);

void AnimateSprite(void *obj, const uint8_t *anim_script);

/* Forward declarations for helpers whose body lives in a later section (the
   file is ordered by object ID, but these are shared between objects):
     CollectRing          -> $25/$37 Rings            (called by $09 SonicSS)
     PushB_ObjHitWall*    -> $32 Button               (called by $14 LavaBall) */
static void CollectRing(uint8_t *o);
static int16_t PushB_ObjHitWallRight(uint8_t *o, int16_t d3);
static int16_t PushB_ObjHitWallLeft(uint8_t *o, int16_t d3);

/* Stub: objects not yet ported do nothing (matches NullObject -> DeleteObject) */
static void NullObject_Main(void *obj) {
    DeleteObject(obj);
}

/* ===========================================================================
   Object dispatch table
   ---------------------------------------------------------------------------
   obj_dispatch[] maps the object ID (the obType byte the ASM object loader
   writes, from disasm/_inc/Object Pointers.asm) to the routine dispatcher that
   switches on obRoutine. Every ID missing from obj_map[] falls to
   NullObject_Main, which deletes the object on the spot -- that is what the
   ASM NullObject does, so unimplemented IDs stay invisible instead of
   crashing.

   The bodies are further down this file, in ascending object-ID order, so the
   table below and the file layout share the same order. Objects-List.md lists
   all IDs ($01-$8C) with their disasm source and ported status.
   =========================================================================== */
typedef struct {
    uint8_t id;    /* id_* token from src/constants.h                       */
    ObjFunc main;  /* obRoutine dispatcher, defined in the object's section */
} ObjEntry;

static const ObjEntry obj_map[] = {
    /* $01-$0F Sonic, title screen and credits ------------------------------------- */
    { id_SonicPlayer,      SonicPlayer_Main,      },  /* $01  01 Sonic.asm */
    { id_Splash,           WaterSplash_Main,      },  /* $08  08 LZ Water Splash.asm */
    { id_SonicSpecial,     SonicSpecial_Main,     },  /* $09  09 Sonic in Special Stage.asm */
    { id_DrownCount,       DrownCount_Main,       },  /* $0A  0A LZ Drowning Countdown.asm */
    { id_Pole,             Pole_ObjectMain,       },  /* $0B  0B LZ Pole that Breaks.asm */
    { id_FlapDoor,         FlapDoor_Main,         },  /* $0C  0C LZ Flapping Door.asm */
    { id_Signpost,         Signpost_Main,         },  /* $0D  0D Signpost.asm */
    { id_TitleSonic,       TitleSonic_Main,       },  /* $0E  0E,0F Title Screen - Sonic, Press Start, TM.asm */
    { id_PSBTM,            PSBTM_Main,            },  /* $0F  0E,0F Title Screen - Sonic, Press Start, TM.asm */
    /* $11-$1E furniture shared by several zones ----------------------------------- */
    { id_Bridge,           Bridge_Main,           },  /* $11  11 GHZ Bridge.asm */
    { id_SpinningLight,    SpinningLight_Main,    },  /* $12  12 SYZ Search Light.asm */
    { id_LavaMaker,        LavaMaker_Main,        },  /* $13  13,14 MZ, SLZ Fire Balls and Maker.asm */
    { id_LavaBall,         LavaBall_Main,         },  /* $14  13,14 MZ, SLZ Fire Balls and Maker.asm */
    { id_SwingingPlatform, SwingingPlatform_Main, },  /* $15  15 Swinging Platforms.asm */
    { id_Harpoon,          Harpoon_Main,          },  /* $16  16 LZ Harpoon.asm */
    { id_Helix,            Helix_Main,            },  /* $17  17 GHZ Spiked Pole Helix.asm */
    { id_BasicPlatform,    Platform_Main,         },  /* $18  18 Platforms.asm */
    { id_CollapseLedge,    CollapseLedge_Main,    },  /* $1A  1A,53 Collapsing Ledges and Floors.asm */
    { id_WaterSurface,     WaterSurface_Main,     },  /* $1B  1B LZ Water Surface.asm */
    { id_Scenery,          Scenery_Main,          },  /* $1C  1C GHZ, SYZ Scenery.asm */
    /* $1F-$2F badniks, rings, monitors, power-ups and FX -------------------------- */
    { id_Crabmeat,         Crabmeat_Main,         },  /* $1F  1F Badnik - Crabmeat.asm */
    { id_HUD,              HUD_Main,              },  /* $21  21 HUD.asm */
    { id_BuzzBomber,       BuzzBomber_Main,       },  /* $22  22,23 Badnik - Buzz Bomber and Missile.asm */
    { id_Missile,          Missile_Main,          },  /* $23  22,23 Badnik - Buzz Bomber and Missile.asm */
    { id_Rings,            Ring_Main,             },  /* $25  25,37 Rings.asm */
    { id_Monitor,          Monitor_Main,          },  /* $26  26,2E Monitors and Power-Ups.asm */
    { id_ExplosionItem,    ExplosionItem_Main,    },  /* $27  27,3F Explosions.asm */
    { id_Animals,          Animals_Main,          },  /* $28  28,29 Animals and Points.asm */
    { id_Points,           Points_Main,           },  /* $29  28,29 Animals and Points.asm */
    { id_Chopper,          Chopper_Main,          },  /* $2B  2B Badnik - Chopper.asm */
    { id_Jaws,             Jaws_ObjectMain,       },  /* $2C  2C Badnik - Jaws.asm */
    { id_Burrobot,         Burrobot_Main,         },  /* $2D  2D Badnik - Burrobot.asm */
    { id_PowerUp,          PowerUp_Main,          },  /* $2E  26,2E Monitors and Power-Ups.asm */
    { id_LargeGrass,       LargeGrass_Main,       },  /* $2F  2F,35 MZ Large Grassy Platforms and Burning Grass.asm */
    /* $30-$4F platforms, spikes, cards and Green Hill objects --------------------- */
    { id_GlassBlock,       GlassBlock_Main,       },  /* $30  30 MZ Large Green Glass Blocks.asm */
    { id_ChainStomp,       ChainStomp_Main,       },  /* $31  31 MZ Chained Stompers.asm */
    { id_Button,           Button_Main,           },  /* $32  32 Button.asm */
    { id_PushBlock,        PushBlock_Main,        },  /* $33  33 MZ, LZ Pushable Blocks.asm */
    { id_TitleCard,        TitleCard_Main,        },  /* $34  34 Title Cards.asm */
    { id_GrassFire,        GrassFire_Main,        },  /* $35  2F,35 MZ Large Grassy Platforms and Burning Grass.asm */
    { id_Spikes,           Spikes_ObjectMain,     },  /* $36  36 Spikes.asm */
    { id_RingLoss,         RingLoss_Main,         },  /* $37  25,37 Rings.asm */
    { id_ShieldItem,       ShieldItem_Main,       },  /* $38  38 Shield and Invincibility.asm */
    { id_GameOverCard,     GameOverCard_Main,     },  /* $39  39 Game Over.asm */
    { id_GotThroughCard,   GotThroughCard_Main,   },  /* $3A  3A Got Through Card.asm */
    { id_PurpleRock,       PurpleRock_Main,       },  /* $3B  3B GHZ Purple Rock.asm */
    { id_SmashWall,        SmashWall_Main,        },  /* $3C  3C GHZ, SLZ Smashable Wall.asm */
    { id_BossGreenHill,    BossGreenHill_Main,    },  /* $3D  3D,48 Boss - GHZ Main and Wrecking Ball.asm */
    { id_Prison,           Prison_Main,           },  /* $3E  3E Prison Capsule.asm */
    { id_Explosion,        Explosion_Main,        },  /* $3F  27,3F Explosions.asm */
    { id_MotoBug,          MotoBug_Main,          },  /* $40  40 Badnik - Moto Bug.asm */
    { id_Springs,          Springs_ObjectMain,    },  /* $41  41 Springs.asm */
    { id_Newtron,          Newtron_Main,          },  /* $42  42 Badnik - Newtron.asm */
    { id_Roller,           Roller_Main,           },  /* $43  43 Badnik - Roller.asm */
    { id_EdgeWalls,        EdgeWalls_Main,        },  /* $44  44 GHZ Edge Walls.asm */
    { id_MarbleBrick,      MarbleBrick_Main,      },  /* $46  46 MZ Bricks.asm */
    { id_Bumper,           Bumper_Main,           },  /* $47  47 SYZ Bumper.asm */
    { id_BossBall,         BossBall_Main,         },  /* $48  3D,48 Boss - GHZ Main and Wrecking Ball.asm */
    { id_GiantRing,        GiantRing_Main,        },  /* $4B  4B,7C Giant Ring and Flash.asm */
    { id_GeyserMaker,      GeyserMaker_Main,      },  /* $4C  4C,4D MZ Lava Geyser and Maker.asm */
    { id_LavaGeyser,       LavaGeyser_Main,       },  /* $4D  4C,4D MZ Lava Geyser and Maker.asm */
    { id_LavaWall,         LavaWall_Main,         },  /* $4E  4E MZ Wall of Lava.asm */
    /* $50-$5F Star Light and Marble objects --------------------------------------- */
    { id_Yadrin,           Yadrin_Main,           },  /* $50  50 Badnik - Yadrin.asm */
    { id_SmashBlock,       SmashBlock_Main,       },  /* $51  51 MZ Smashable Green Block.asm */
    { id_MovingBlock,      MovingBlock_Main,      },  /* $52  52 Moving Blocks.asm */
    { id_CollapseFloor,    CollapseFloor_Main,    },  /* $53  1A,53 Collapsing Ledges and Floors.asm */
    { id_LavaTag,          LavaTag_Main,          },  /* $54  54 MZ Invisible Lava Tag.asm */
    { id_Basaran,          Basaran_Main,          },  /* $55  55 Badnik - Basaran.asm */
    { id_FloatingBlock,    FloatingBlock_Main,    },  /* $56  56 SYZ, SLZ Floating Blocks and LZ Doors.asm */
    { id_SpikeBall,        SpikeBall_Main,        },  /* $57  57 SYZ, LZ Spiked Ball and Chain.asm */
    { id_BigSpikeBall,     BigSpikeBall_Main,     },  /* $58  58 SYZ Big Spiked Ball.asm */
    /* $60-$8C bosses, Special Stage and end screens ------------------------------- */
    { id_Orbinaut,         Orbinaut_Main,         },  /* $60  60 Badnik - Orbinaut.asm */
    { id_LabyrinthBlock,   LabyrinthBlock_Main,   },  /* $61  61 LZ Blocks.asm */
    { id_Gargoyle,         Gargoyle_Main,         },  /* $62  62 LZ Gargoyle.asm */
    { id_LabyrinthConvey,  LabyrinthConvey_Main,  },  /* $63  63 LZ Conveyor.asm */
    { id_Bubble,           Bubble_Main,           },  /* $64  64 LZ Air Bubbles.asm */
    { id_Waterfall,        Waterfall_Main,        },  /* $65  65 LZ Waterfalls.asm */
    { id_BossMarble,       BossMarble_Main,       },  /* $73  73,74 Boss - MZ Main and Fire.asm */
    { id_BossFire,         BossFire_Main,         },  /* $74  73,74 Boss - MZ Main and Fire.asm */
    { id_BossSpringYard,   BossSpringYard_Main,   },  /* $75  75,76 Boss - SYZ Main and Blocks.asm */
    { id_BossBlock,        BossBlock_Main,        },  /* $76  75,76 Boss - SYZ Main and Blocks.asm */
    { id_BossLabyrinth,    BossLabyrinth_ObjectMain,},  /* $77  77 Boss - LZ Main.asm */
    { id_Caterkiller,      Caterkiller_Main,      },  /* $78  78 Badnik - Caterkiller.asm */
    { id_RingFlash,        RingFlash_Main,        },  /* $7C  4B,7C Giant Ring and Flash.asm */
    { id_SSResult,         SSResult_Main,         },  /* $7E  7E,7F Special Stage Results and Chaos Emeralds.asm */
    { id_SSRChaos,         SSRChaos_Main,         },  /* $7F  7E,7F Special Stage Results and Chaos Emeralds.asm */
    { id_CreditsText,      CreditsText_Main,      },  /* $8A  8A Credits and Sonic Team Presents.asm */
};

#define OBJ_MAP_COUNT ((int)(sizeof(obj_map) / sizeof(obj_map[0])))

void Objects_Init(void) {
    /* Default: every unmapped ID self-deletes (matches ASM NullObject) */
    for (int i = 1; i < 256; i++) {
        obj_dispatch[i] = NullObject_Main;
    }

    for (int i = 0; i < OBJ_MAP_COUNT; i++) {
        obj_dispatch[obj_map[i].id] = obj_map[i].main;
    }

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
   Part 1  Shared object helpers (disasm/_incObj/sub / .asm)
   =========================================================================== */

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
   SynchroAnimate sonic.asm 3136-3179, FixBugs=0.
   =========================================================================== */
void SynchroAnimate(void) {
    /* Sync1: GHZ spiked pole helix (Object 17). */
    if ((int8_t)(--v_ani0_time) < 0) {           /* subq.b / bpl.s Sync2 */
        v_ani0_time  = 12 - 1;                   /* move.b #12-1 */
        v_ani0_frame = (uint8_t)((v_ani0_frame - 1) & 7); /* subq.b / andi.b #7 */
    }

    /* Sync2: Rings */
    if ((int8_t)(--v_ani1_time) < 0) {           /* subq.b / bpl.s Sync3 */
        v_ani1_time  = 8 - 1;                    /* move.b #8-1 */
        v_ani1_frame = (uint8_t)((v_ani1_frame + 1) & 3); /* addq.b / andi.b #3 */
    }

    /* Sync3: Unused */
    if ((int8_t)(--v_ani2_time) < 0) {           /* subq.b / bpl.s Sync4 */
        v_ani2_time  = 8 - 1;                    /* move.b #8-1 */
        v_ani2_frame = (uint8_t)(v_ani2_frame + 1);    /* addq.b #1 */
        if (v_ani2_frame >= 6) {                 /* cmpi.b #6 / blo.s Sync4 */
            v_ani2_frame = 0;                    /* move.b #0 */
        }
    }

    /* Sync4: RingLoss */
    if (v_ani3_time == 0) {                      /* tst.b / beq.s SyncEnd */
        return;
    }
    int d0 = v_ani3_time;                        /* moveq #0,d0 / move.b */
    d0 += v_ani3_buf;                            /* add.w (v_ani3_buf).w,d0 */
    v_ani3_buf = (uint16_t)d0;                   /* move.w d0,(v_ani3_buf).w */
    d0 = (int)(uint16_t)((d0 << 7) | ((uint16_t)d0 >> 9));  /* rol.w #7 */
    d0 &= 3;                                     /* andi.w #3 */
    v_ani3_frame = (uint8_t)d0;                  /* move.b d0,(v_ani3_frame).w */
    v_ani3_time  = (uint8_t)(v_ani3_time - 1);   /* subq.b #1 */
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
    int16_t x = obX(o);          /* bclr #0,d3 (FixBugs-style) */
    FindFloor(y, x, 0x0D, 0, 0x10, &v_anglebuffer, obj, &d1);   /* ← esta línea */
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

    if (!OutOfRange(obj, -1, 0)) {               /* out_of_range.w .offscreen (bne) */
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

   bmicheck selects the optional `bmi.w exit` (Macros.asm 289-292). Only three
   call sites in the whole disasm pass it: 54 MZ Invisible Lava Tag:41,
   5E SLZ Seesaw:14 and 7A/7B Boss SLZ:517. The macro itself calls it
   redundant, and it is (the bmi range is a subset of the bhi range), but the
   flag is kept so the C mirrors the macro instead of hardcoding dead
   behaviour on the ~80 call sites that do not emit it.
   =========================================================================== */
int OutOfRange(void *obj, int16_t pos, int bmicheck) {
    uint8_t *o = (uint8_t *)obj;
    if (pos == -1) {
        pos = obX(o);
    }
    uint16_t d0 = (uint16_t)pos & 0xFF80;                      /* andi.w #$FF80 */
    uint16_t d1 = ((uint16_t)RAM_WORD(0xF700) - 128) & 0xFF80; /* v_screenposx */
    int16_t diff = (int16_t)(d0 - d1);                         /* sub.w d1,d0 */

    if (bmicheck && diff < 0) {                                /* bmi.w exit */
        return 1;
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
   .scramble: d1 = d1*41 (via asl/add); d0 = low(d1) + high(d1)
   move.w d0,d1 / swap d1 / move.l d1,(v_random).w

   The stored seed is NOT plain "d0 << 16". `move.w d0,d1` rewrites only the
   low word of d1, so its upper word survives the following swap and ends up as
   the seed's low half. Zeroing it instead collapses the sequence to
   R' = 41*R mod 65536 (period 8192) and desyncs every routine that seeds
   itself from v_random.
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

    d0 = (uint32_t)(uint16_t)d1;                     /* move.w d1,d0 -> L */
    d0 = (d0 + (uint16_t)(d1 >> 16)) & 0xFFFF;       /* swap d1 / add.w d1,d0 -> (L+H) */

    /* move.w d0,d1 rewrites only the LOW word of d1, so its upper word (L)
       survives; the following swap.d1 then yields (d0 << 16) | L. The seed is
       therefore NOT d0 << 16 — the next call folds L back in via 41*seed. */
    v_random = ((uint32_t)(uint16_t)d0 << 16) | (uint16_t)d1; /* move.l d1,(v_random).w */

    return (uint16_t)d0;                             /* d0 contains pseudo-random number */
}

/* ===========================================================================
 *  Shared helpers: FindNextFreeObj, BossMove, BossDefeated
 *  Ported verbatim from _incObj/sub FindFreeObj.asm,
 *  _incObj/sub BossDefeated & BossMove.asm (REV01, FixBugs=0).
 * =========================================================================== */

/* Find the next free object slot AFTER the given object. Used by all boss
   init routines (matches FindNextFreeObj in FindFreeObj.asm). */
static void *FindNextFreeObj(void *after) {
    uint8_t *base = RAM_ADDR(v_lvlobjspace);
    uint8_t *end  = RAM_ADDR(v_lvlobjend);
    uint8_t *p    = (uint8_t *)after + OBJECT_SIZE;

    if (p < base) p = base;               /* clamp por si `after` está antes */
    for (; p + OBJECT_SIZE <= end; p += OBJECT_SIZE) {
        if (p[0] == 0) return p;          /* obID == 0 = libre */
    }
    return NULL;
}

/* BossMove — modified SpeedToPos operating on the 16.16 fixed-point
   obBossX/obBossY fields (BossDefeated & BossMove.asm). */
static void BossMove(uint8_t *o) {
    int32_t d2 = obBossX(o);
    int32_t d3 = obBossY(o);
    d2 += ((int32_t)obVelX(o)) << 8;
    d3 += ((int32_t)obVelY(o)) << 8;
    obBossX(o) = d2;
    obBossY(o) = d3;
}

/* BossDefeated — spawn a gray explosion every 8 frames with randomized
   X/Y offsets, until the boss's obBossHits countdown expires (the caller
   keeps decrementing it), at which point the slot self-deletes. */
static void BossDefeated(uint8_t *o) {
    /* move.b (v_vblank_byte).w,d0 ; andi.b #7,d0 ; bne.s .noExplosion */
    if ((v_vblank_byte & 7) != 0) return;

    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (!a1) return;

    obID(a1) = id_Explosion;
    obX(a1)  = obX(o);
    obY(a1)  = obY(o);

    /* jsr (RandomNumber).l ; move.w d0,d1 ; moveq #0,d1 ; move.b d0,d1
       lsr.b #2,d1 ; subi.w #$20,d1 ; add.w d1,obX(a1) */
    uint16_t r = RandomNumber();
    int8_t dx = (int8_t)((uint8_t)r >> 2);
    int16_t d1 = (int16_t)dx - 0x20;
    obX(a1) = (int16_t)(obX(a1) + d1);

    /* lsr.w #8,d0 ; lsr.b #3,d0 ; add.w d0,obY(a1) */
    uint8_t dy = (uint8_t)(r >> 8);
    dy >>= 3;
    obY(a1) = (int16_t)(obY(a1) + (int16_t)dy);

    /* The ASM omits a return here so the caller's flow continues. */
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
   Part 2  Object implementations, $01-$2F
   =========================================================================== */

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
                            Sound_Queue(music_list[zone], true);
                        } else {
                            Sound_Queue(bgm_SBZ, true);
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
        Sound_Queue(bgm, true);
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
    if (obY(o) > water_y) {
        /* --- below water surface: entering water --- */
        uint8_t was_underwater = obStatus(o) & (1 << 6);
        obStatus(o) |= (1 << 6);              /* bset #6,obStatus(a0) */
        if (was_underwater) {
            return;                            /* bne.s .return: ya estaba dentro */
        }

        /* just entered water */
        ResumeMusic();                         /* bsr.w ResumeMusic */
        {
            uint8_t *bubbles = RAM_ADDR(v_sonicbubbles);
            RAM_BYTE(v_sonicbubbles) = id_DrownCount;   /* move.b #id_DrownCount,(v_sonicbubbles) */
            obSubtype(bubbles)       = 0x81;            /* move.b #$81,(v_sonicbubbles+obSubtype) */
        }
        v_sonspeedmax = son_maxspeed / 2;      /* move.w #son_maxspeed/2 */
        v_sonspeedacc = son_acceleration / 2;  /* move.w #son_acceleration/2 */
        v_sonspeeddec = son_deceleration / 2;  /* move.w #son_deceleration/2 */

        /* asr.w obVelX: ÷2 ; asr.w obVelY ×2: ÷4 */
        obVelX(o) = (int16_t)(obVelX(o) >> 1);
        obVelY(o) = (int16_t)(obVelY(o) >> 2);
        if (obVelY(o) == 0) {
            return;                            /* beq.s .return: sin splash si velY == 0 */
        }
        RAM_BYTE(v_splash) = id_Splash;   /* move.b #id_Splash,(v_splash).w */
        Sound_Queue(sfx_Splash, false);        /* move.w #sfx_Splash,d0 ; jmp (QueueSound2).l */
    } else {
        /* --- above water surface: exiting water --- */
        uint8_t was_underwater = obStatus(o) & (1 << 6);
        obStatus(o) &= ~(1 << 6);              /* bclr #6,obStatus(a0) */
        if (!was_underwater) {
            return;                            /* beq.s .return: ya estaba fuera */
        }

        /* just exited water */
        ResumeMusic();                         /* bsr.w ResumeMusic */
        v_sonspeedmax = son_maxspeed;          /* move.w #son_maxspeed */
        v_sonspeedacc = son_acceleration;      /* move.w #son_acceleration */
        v_sonspeeddec = son_deceleration;      /* move.w #son_deceleration */

        obVelY(o) = (int16_t)(obVelY(o) << 1); /* asl.w obVelY(a0): ×2 */
        if (obVelY(o) == 0) {
            return;                            /* beq.w .return: sin splash si velY == 0 */
        }
        RAM_BYTE(v_splash) = id_Splash;   /* move.b #id_Splash,(v_splash).w */

        /* cmpi.w #-$1000,obVelY(a0) ; bgt.s .belowmaxspeed
           → clamp SOLO si velY <= -$1000 (saltamos si velY > -$1000) */
        if (obVelY(o) <= -0x1000) {
            obVelY(o) = -0x1000;               /* move.w #-$1000,obVelY(a0) */
        }
        /* .belowmaxspeed: */
        Sound_Queue(sfx_Splash, false);        /* move.w #sfx_Splash,d0 ; jmp (QueueSound2).l */
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
        uint8_t d2 = (uint8_t)((uint16_t)obVelY(o) >> 8);  // byte ALTO = pixel delta
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
    uint8_t d3;                              /* <-- nuevo: capturamos el ángulo */

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

    Sonic_FindCeiling(o, NULL, &d1, &d3);    /* <-- ya no descartamos d3 */
    if (d1 >= 0) {
        return;
    }

    obY(o) = (int16_t)(obY(o) - d1);

    /* ¿Techo inclinado? (ASM: addi #$20 / andi #$40 / bne) */
    {
        uint8_t d0 = (uint8_t)(d3 + 0x20);
        if (d0 & 0x40) {
            /* Tratarlo como suelo: adopta el ángulo, resetea estado,
               y convierte velY en inertia conservando el impulso. */
            obAngle(o) = d3;
            Sonic_ResetOnFloor(o);
            obInertia(o) = obVelY(o);
            if ((int8_t)d3 < 0) {
                obInertia(o) = (int16_t)(-obInertia(o));
            }
            return;
        }
    }

    /* Techo plano: parar Y como antes. */
    obVelY(o) = 0;
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
    Sonic_WallSpeedAdjust(o);

    /* Debug: log de velocidad, una línea cada 4 frames */
    static int dbg = -1;
    static uint16_t last_frame = 0xFFFF;
    if (dbg < 0) dbg = getenv("SONIC_LOG_SPEED") ? 1 : 0;
    if (dbg && v_framecount != last_frame) {
        last_frame = v_framecount;
        if ((v_framecount & 3) == 0) {
            fprintf(stderr, "f=%u inertia=%d angle=%d velX=%d velY=%d\n",
                    (unsigned)v_framecount,
                    obInertia(o), obAngle(o), obVelX(o), obVelY(o));
        }
    }
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

static void __attribute__((unused)) Sonic_SquashUnused(void *obj) {
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
    d1 = (uint8_t)((obX(o) >> 8) & 0x7F);
    d0  = (uint16_t)(d0 + d1);

    /* lea (v_lvllayout_fg).w,a1 ; move.b (a1,d0.w),d1 */
    a1 = RAM_ADDR(v_lvllayout_fg);
    d1 = a1[d0];
    static int first = 0;
    if (!first) {
        first = 1;
        fprintf(stderr, "[Loops] d0=%03X d1=%02X loop1=%02X roll1=%02X\n",
                d0, d1, v_256loop1, v_256roll1);
    }

    /* cmp.b (v_256roll1).w,d1 ; beq.w Sonic_ChkRoll */
    if (d1 == (uint8_t)v_256roll1)  { Sonic_ChkRoll(o); return; }
    if (d1 == (uint8_t)v_256roll2)  { Sonic_ChkRoll(o); return; }
    if (d1 == (uint8_t)v_256loop1)  goto chkifleft;
    if (d1 == (uint8_t)v_256loop2)  goto chkifinair;

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
    d2 = (uint8_t)obX(o);
    if (d2 >= 44) {
        goto chkifright;
    }
    obRender(o) &= ~sprite_looping;
    return;

    chkifright:
    if (d2 < 224) {
        goto chkangle1;
    }
    obRender(o) |= sprite_looping;
    return;

    chkangle1:
    if (obRender(o) & sprite_looping) {
        goto chkangle2;
    }
    d1 = obAngle(o);
    if (d1 == 0) {
        return;
    }
    if (d1 > 0x80) {
        return;
    }
    obRender(o) |= sprite_looping;
    return;

    chkangle2:
    d1 = obAngle(o);
    if (d1 <= 0x80) {
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
                    /* ASM: bne.w .push — el mismo handler que case 0xFD (Push).
                     *      Sobreescribe los flip flags calculados arriba: el push sólo usa
                     *      obStatus & sprite_xflip, no el angle. */
                    int16_t d2 = obInertia(o);
                    if (d2 >= 0) d2 = (int16_t)-d2;      /* bmi.s .negspeed / neg.w d2 */
                        d2 = (int16_t)(d2 + 0x800);          /* addi.w #$800,d2 */
                        if (d2 < 0) d2 = 0;                  /* bpl.s .belowmax3 / moveq #0,d2 */
                            d2 = (int16_t)(d2 >> 6);             /* lsr.w #6,d2 */
                            obTimeFrame(o) = (uint8_t)d2;

                        const uint8_t *a1 = Ani_Sonic + ((const uint16_t *)Ani_Sonic)[id_Push];
                    uint8_t flip = obStatus(o) & sprite_xflip;
                    obRender(o) = (obRender(o) & ~(sprite_xflip | sprite_yflip)) | flip;

                    obFrame(o) = anim_next_frame(o, a1);
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
 *  Object 08 — LZ Water Splash
 *  Ported verbatim from _incObj/08 LZ Water Splash.asm (REV01, FixBugs=0).
 *
 *  Splash que aparece en la superficie del agua (LZ) cuando Sonic entra o
 *  sale. Se spawnea desde Sonic_Water y se autodestruye cuando termina la
 *  animación (Ani_Splash usa afRoutine $FC para avanzar obRoutine → 4).
 *
 *  Notas de traducción:
 *    - Spla_Main NO tiene rts: cae directamente en Spla_Display en el mismo
 *      frame. Se replica llamando a Spla_Display(o) al final.
 *    - Spla_Display copia obY desde v_waterpos1 (no desde Sonic), de modo
 *      que el splash sigue la altura del agua aunque Sonic salte.
 * =========================================================================== */

static void Spla_Main(uint8_t *o);
static void Spla_Display(uint8_t *o);
static void Spla_Delete(uint8_t *o);

/* --- Spla_Main — Routine 0 ------------------------------------------------
 * move.l #Map_Splash,obMap / ori.b #sprite_cam_field,obRender /
 * move.b #1,obPriority / move.b #32/2,obActWid /
 * move.w #ArtTile_LZ_Splash|Tile_Pal3,obGfx /
 * move.w (v_player+obX).w,obX(a0) / fall-through a Spla_Display
 * ------------------------------------------------------------------------ */
static void Spla_Main(uint8_t *o) {
    obRoutine(o) += 2;                              /* addq.b #2 → Spla_Display */

    obMap(o)      = (uint32_t)(uintptr_t)Map_Splash;
    obRender(o)  |= sprite_cam_field;               /* ori.b #sprite_cam_field */
    obPriority(o) = 1;                              /* move.b #1: encima de Sonic */
    obActWid(o)   = 32 / 2;                         /* move.b #32/2 */
    obGfx(o)      = (uint16_t)(ArtTile_LZ_Splash | Tile_Pal3);

    obX(o) = obX(RAM_ADDR(v_player));               /* copia X desde Sonic */

    /* ASM falls through into Spla_Display en el mismo frame. */
    Spla_Display(o);
}

/* --- Spla_Display — Routine 2 --------------------------------------------
 * move.w (v_waterpos1).w,obY(a0) /
 * lea (Ani_Splash).l,a1 / jsr (AnimateSprite).l / jmp (DisplaySprite).l
 * ------------------------------------------------------------------------ */
static void Spla_Display(uint8_t *o) {
    obY(o) = (int16_t)v_waterpos1;                  /* copia Y desde la altura del agua */

    if (Ani_Splash) {                               /* lea (Ani_Splash).l,a1 */
        AnimateSprite(o, Ani_Splash);               /* jsr (AnimateSprite).l */
    }
    DisplaySprite(o);                               /* jmp (DisplaySprite).l */
}

/* --- Spla_Delete — Routine 4 ---------------------------------------------
 * jmp (DeleteObject).l
 * ------------------------------------------------------------------------ */
static void Spla_Delete(uint8_t *o) {
    DeleteObject(o);
}

/* --- Dispatcher Object 08 — Spla_Index: 0=Main, 2=Display, 4=Delete ----- */
static void WaterSplash_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Spla_Main(o);    break;
        case 2: Spla_Display(o); break;
        case 4: Spla_Delete(o);  break;
    }
}
/* ===========================================================================
 *  Object 09 — Sonic in Special Stage
 *  Ported from _incObj/09 Sonic in Special Stage.asm (REV01, FixBugs=0).
 *
 *  Física propia del SS:
 *    - Sin slope resist, sin air drag, sin cap de jump height
 *    - Movimiento D-pad es relativo a la rotación del stage
 *    - La gravedad tira en la dirección de la rotación del stage
 *    - La colisión con el layout usa block IDs, no FindFloor
 *    - Los ítems se recogen leyendo el bloque sobre el que está Sonic
 * =========================================================================== */

#define sonss_maxspeed      0x800
#define sonss_acceleration  0x0C
#define sonss_deceleration  0x40
#define sonss_jumpspeed     0x680
#define sonss_gravity       (gravity - 0x0E)   /* $38 - $0E = $2A */

/* Campos específicos del objeto 09 */
#define sonss_touchedblock_id(o)  (*(uint8_t  *)((uint8_t *)(o) + 0x30))
#define sonss_touchedblock_ram(o) (*(uint32_t *)((uint8_t *)(o) + 0x32))
#define sonss_timeout_updown(o)   (*(uint8_t  *)((uint8_t *)(o) + 0x36))
#define sonss_timeout_r(o)        (*(uint8_t  *)((uint8_t *)(o) + 0x37))
#define sonss_exittimer(o)        (*(uint16_t *)((uint8_t *)(o) + 0x38))
#define sonss_ghoststate(o)       (*(uint8_t  *)((uint8_t *)(o) + 0x3A))

/* El offset 0x3C–0x3F queda libre; el bloque empieza en 0x30 (16 bytes). */

static void SonicSpecial_Main(void *obj);

static void SonicSS_Main(uint8_t *o);
static void SonicSS_Control(uint8_t *o);
static void SonicSS_ExitStage(uint8_t *o);
static void SonicSS_ExitStage_Unused(uint8_t *o);
static void SonicSS_OnWall(uint8_t *o);
static void SonicSS_InAir(uint8_t *o);
static void SonicSS_Display(uint8_t *o);
static void SonicSS_Move(uint8_t *o);
static void SonicSS_MoveLeft(uint8_t *o);
static void SonicSS_MoveRight(uint8_t *o);
static void SonicSS_CheckDpadLetGo(uint8_t *o);
static void SonicSS_AngleSpeed(uint8_t *o);
static void SonicSS_Jump(uint8_t *o);
static void SonicSS_Fall(uint8_t *o);
static int  SonicSS_FindWall(uint8_t *o, int32_t y_fp, int32_t x_fp);
static void SonicSS_FindWall_CheckType(uint8_t *o, uint8_t block_id,
                                       uint8_t *block_addr, uint8_t *flag);
static void SonicSS_ChkItems_NonSolidActionBlock(uint8_t *o);
static void SonicSS_ChkItems_SolidActionBlock(uint8_t *o);
static void SonicSS_MakeGhostSolid(uint8_t *o);
static void SS_FixCamera(uint8_t *o);

/* --- Dispatcher principal (Object 09 entry). --- */
static void SonicSpecial_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (v_debuguse) {
        SS_FixCamera(o);             /* keep camera centered while in debug mode */
        DebugMode_Main(o);           /* run debug mode instead of Sonic */
        return;
    }

    switch (obRoutine(o)) {
        case 0: SonicSS_Main(o);              break;
        case 2: SonicSS_Control(o);           break;
        case 4: SonicSS_ExitStage(o);         break;
        case 6: SonicSS_ExitStage_Unused(o);  break;
    }
}

/* --- Routine 0: initialization. --- */
static void SonicSS_Main(uint8_t *o) {
    obRoutine(o) += 2;                              /* → SonicSS_Control */
    obHeight(o)  = sonic_roll_height;
    obWidth(o)   = sonic_roll_width;
    obMap(o)     = (uint32_t)(uintptr_t)Map_Sonic;
    obGfx(o)     = ArtTile_Sonic;
    obRender(o)  = sprite_cam_field;
    obPriority(o)= 0;

    obAnim(o)    = id_Roll;
    obStatus(o) |= (1 << 2);                        /* rolling flag */
    obStatus(o) |= (1 << 1);                        /* in-air flag */
}

/* --- Routine 2: main control loop. --- */
static void SonicSS_Control(uint8_t *o) {
    /* Debug mode toggle: B while f_debugmode set. FixBugs=0 lacks the rts
       after the flag is set (comment in ASM says the original jumps when
       entering debug — we replicate it by just setting the flag). */
    if (f_debugmode) {
        if (v_jpadpress1 & btnB) {
            v_debuguse = 1;
            /* no return — matches FixBugs=0 */
        }
    }

    sonss_touchedblock_id(o) = 0;                   /* no block touched yet */

    uint8_t status = obStatus(o) & 0x02;            /* in-air flag only */
    if (status == 0) SonicSS_OnWall(o);
    else             SonicSS_InAir(o);

    Sonic_LoadGfx(o);
    DisplaySprite(o);
}

/* --- Routine 4: spin stage while exiting. --- */
static void SonicSS_ExitStage(uint8_t *o) {
    v_ssrotate += ss_rotatespeed;
    if ((uint16_t)v_ssrotate == (uint16_t)(0x60 * ss_rotatespeed)) {
        v_gamemode = GM_Level;                      /* signal main loop to exit */
    }
    /* The second cmp never fires in practice (the game mode change above
       short-circuits before v_ssrotate reaches $3000). Kept verbatim. */
    if ((int16_t)v_ssrotate >= (int16_t)(2 * 0x60 * ss_rotatespeed)) {
        v_ssrotate = 0;
        v_ssangle  = 0x4000;
        obRoutine(o) += 2;
        sonss_exittimer(o) = 60;
    }

    v_ssangle += v_ssrotate;
    Sonic_Animate(o);
    Sonic_LoadGfx(o);
    SS_FixCamera(o);
    DisplaySprite(o);
}

/* --- Routine 6: secondary exit (unreachable in-game). --- */
static void SonicSS_ExitStage_Unused(uint8_t *o) {
    sonss_exittimer(o) -= 1;
    if (sonss_exittimer(o) == 0) {
        v_gamemode = GM_Level;
    }
    Sonic_Animate(o);
    Sonic_LoadGfx(o);
    SS_FixCamera(o);
    DisplaySprite(o);
}

/* --- Mode 0: touching a solid block. --- */
static void SonicSS_OnWall(uint8_t *o) {
    SonicSS_Jump(o);
    SonicSS_Move(o);
    SonicSS_Fall(o);
    SonicSS_Display(o);
}

/* --- Mode 2: airborne from jumping or falling. --- */
static void SonicSS_InAir(uint8_t *o) {
    /* SonicSS_JumpHeight_Unused is a bare rts in REV01. */
    SonicSS_Move(o);
    SonicSS_Fall(o);
    /* fall through to display */
    SonicSS_Display(o);
}

/* --- Common display / movement / item-pickup tail. --- */
static void SonicSS_Display(uint8_t *o) {
    SonicSS_ChkItems_NonSolidActionBlock(o);
    SonicSS_ChkItems_SolidActionBlock(o);

    SpeedToPos(o);
    SS_FixCamera(o);

    v_ssangle += v_ssrotate;

    Sonic_Animate(o);
}

/* --- D-pad → inertia. --- */
static void SonicSS_Move(uint8_t *o) {
    if (v_jpadhold2 & btnL) SonicSS_MoveLeft(o);
    if (v_jpadhold2 & btnR) SonicSS_MoveRight(o);
    SonicSS_CheckDpadLetGo(o);
}

static void SonicSS_MoveLeft(uint8_t *o) {
    obStatus(o) |= (1 << 0);                        /* face left */

    int16_t d0 = obInertia(o);
    if (d0 > 0) {
        /* .changeddirection */
        d0 -= sonss_deceleration;
        obInertia(o) = d0;
        return;
    }
    /* .accelerate */
    d0 -= sonss_acceleration;
    if (d0 <= -sonss_maxspeed) d0 = -sonss_maxspeed;
    obInertia(o) = d0;
}

static void SonicSS_MoveRight(uint8_t *o) {
    obStatus(o) &= ~(1 << 0);                       /* face right */

    int16_t d0 = obInertia(o);
    if (d0 < 0) {
        /* .changedirection */
        d0 += sonss_deceleration;
        obInertia(o) = d0;
        return;
    }
    /* .accelerate */
    d0 += sonss_acceleration;
    if (d0 >= sonss_maxspeed) d0 = sonss_maxspeed;
    obInertia(o) = d0;
}

static void SonicSS_CheckDpadLetGo(uint8_t *o) {
    if (v_jpadhold2 & (btnL | btnR)) {
        SonicSS_AngleSpeed(o);
        return;
    }
    int16_t d0 = obInertia(o);
    if (d0 == 0) {
        SonicSS_AngleSpeed(o);
        return;
    }
    if (d0 < 0) {
        d0 += sonss_acceleration;
        if (d0 < 0) obInertia(o) = d0;
        else        obInertia(o) = 0;
    } else {
        d0 -= sonss_acceleration;
        if (d0 > 0) obInertia(o) = d0;
        else        obInertia(o) = 0;
    }
    SonicSS_AngleSpeed(o);
}

/* --- Apply inertia in the rotated frame. --- */
static void SonicSS_AngleSpeed(uint8_t *o) {
    uint8_t angle = (uint8_t)((v_ssangle >> 8) + 0x20);
    angle &= 0xC0;
    angle = (uint8_t)(-angle);                      /* neg.b */

    int16_t s0, s1;
    CalcSine(angle, &s0, &s1);

    int32_t dx = (int32_t)s1 * obInertia(o);        /* cos * inertia */
    int32_t dy = (int32_t)s0 * obInertia(o);        /* sin * inertia */

    /* ASM: add.l d1,obX(a0) / add.l d0,obY(a0) — the .l read covers pixel+subpixel */
    int32_t x_fp = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
    int32_t y_fp = ((uint32_t)obY(o) << 16) | (uint16_t)obSubpixelY(o);
    x_fp += dx;
    y_fp += dy;

    /* Check the *target* position for wall collision before committing. */
    if (SonicSS_FindWall(o, y_fp, x_fp)) {
        /* Hit a wall: undo, zero inertia. */
        obInertia(o) = 0;
        return;
    }

    obX(o)         = (int16_t)((uint32_t)x_fp >> 16);
    obSubpixelX(o) = (int16_t)(x_fp & 0xFFFF);
    obY(o)         = (int16_t)((uint32_t)y_fp >> 16);
    obSubpixelY(o) = (int16_t)(y_fp & 0xFFFF);
}

/* --- Jump from a wall. --- */
static void SonicSS_Jump(uint8_t *o) {
    if (!(v_jpadpress2 & btnABC)) return;
    uint8_t angle;
    if (g_settings.ss_smooth) {
        angle = (uint8_t)((v_ssangle >> 8));
    } else {
        angle = (uint8_t)((v_ssangle >> 8) & 0xFC);
    }
    angle = (uint8_t)(-angle);
    angle -= 0x40;

    int16_t s0, s1;
    CalcSine(angle, &s0, &s1);

    int32_t vx = ((int32_t)s1 * sonss_jumpspeed) >> 8;
    int32_t vy = ((int32_t)s0 * sonss_jumpspeed) >> 8;
    obVelX(o) = (int16_t)vx;
    obVelY(o) = (int16_t)vy;
    obStatus(o) |= (1 << 1);                        /* in-air */

    Sound_Queue(sfx_Jump, false);
}

/* --- Gravity in the rotated frame. --- */
static void SonicSS_Fall(uint8_t *o) {
    uint8_t angle;
    if (g_settings.ss_smooth) {
        angle = (uint8_t)((v_ssangle >> 8));
    } else {
        angle = (uint8_t)((v_ssangle >> 8) & 0xFC);
    }
    int16_t s0, s1;
    CalcSine(angle, &s0, &s1);

    int32_t d0 = (int32_t)s0 * sonss_gravity;
    int32_t d1 = (int32_t)s1 * sonss_gravity;
    d0 += (int32_t)obVelX(o) << 8;
    d1 += (int32_t)obVelY(o) << 8;

    int32_t x_fp = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
    int32_t y_fp = ((uint32_t)obY(o) << 16) | (uint16_t)obSubpixelY(o);

    /* ---- Try X first. ---- */
    x_fp += d0;
    if (SonicSS_FindWall(o, y_fp, x_fp)) {
        /* X collision: d0 = 0 (matches ASM `moveq #0,d0`) */
        x_fp -= d0;
        obVelX(o) = 0;
        obStatus(o) &= ~(1 << 1);

        /* Try Y with the original X. */
        y_fp += d1;
        if (SonicSS_FindWall(o, y_fp, x_fp)) {
            /* Both collisions: early rts in ASM.
             * obVelX stays 0, obVelY stays 0. */
            y_fp -= d1;
            obVelY(o) = 0;
            return;
        }
        /* Only X collision: fall-through to .nofloor in ASM.
         * obVelX = d0>>8 = 0, obVelY = d1>>8 (still moving vertically). */
        obVelY(o) = (int16_t)(d1 >> 8);
        return;
    }

    /* ---- X move is fine: try Y. ---- */
    y_fp += d1;
    if (SonicSS_FindWall(o, y_fp, x_fp)) {
        /* Only Y collision: obVelX = d0>>8, obVelY = 0.
         * IMPORTANT: do NOT overwrite obVelY with d1>>8 after zeroing it. */
        y_fp -= d1;
        obVelX(o) = (int16_t)(d0 >> 8);
        obVelY(o) = 0;
        obStatus(o) &= ~(1 << 1);
        return;
    }

    /* ---- Both moves fit: commit and stay airborne. ---- */
    obVelX(o) = (int16_t)(d0 >> 8);
    obVelY(o) = (int16_t)(d1 >> 8);
    obStatus(o) |= (1 << 1);
}

/* --- Collision with SS layout: check the 4 blocks around (x_fp, y_fp). --- */
static int SonicSS_FindWall(uint8_t *o, int32_t y_fp, int32_t x_fp) {
    int dbg = 0;  // cambialo a 0 después de debuggear
    if (dbg) {
       fprintf(stderr, "[FW] y=%d x=%d (pix)\n",
               (int)(int16_t)(y_fp >> 16), (int)(int16_t)(x_fp >> 16));
    }
    uint8_t *a1 = RAM_ADDR(v_sslayout_base);

    uint16_t d4 = (uint16_t)(int16_t)(y_fp >> 16);
    d4 += 20 + (ss_blocksize * 2);
    d4  = (uint16_t)(d4 / ss_blocksize);
    d4  = (uint16_t)(d4 * ss_layout_rowlength);
    a1 += d4;

    d4  = (uint16_t)(int16_t)(x_fp >> 16);
    d4 += 20;
    d4  = (uint16_t)(d4 / ss_blocksize);
    a1 += d4;

    uint8_t flag = 0;
    uint8_t block;

    block = *a1++;  SonicSS_FindWall_CheckType(o, block, a1 - 1, &flag);
    block = *a1++;  SonicSS_FindWall_CheckType(o, block, a1 - 1, &flag);
    a1 += ss_layout_rowlength - 2;
    block = *a1++;  SonicSS_FindWall_CheckType(o, block, a1 - 1, &flag);
    block = *a1++;  SonicSS_FindWall_CheckType(o, block, a1 - 1, &flag);

    if (dbg) {
        fprintf(stderr, "[FW] offset=%04X (row=%d col=%d) flags=%02X first_blk=%02X\n",
                (unsigned)(a1 - RAM_ADDR(v_sslayout_base)),
                (int)((int16_t)(y_fp >> 16) + 68) / 24,
                (int)((int16_t)(x_fp >> 16) + 20) / 24,
                flag,
                RAM_ADDR(v_sslayout_base)[(int)((int16_t)(y_fp >> 16) + 68) / 24 * 0x80
                                        + (int)((int16_t)(x_fp >> 16) + 20) / 24]);
    }

    return flag != 0;
}

static void SonicSS_FindWall_CheckType(uint8_t *o, uint8_t block_id,
                                       uint8_t *block_addr, uint8_t *flag) {
    if (block_id == 0) return;                      /* blank */
    if (block_id == id_SS_1Up) return;              /* 1-Up is not solid */
    if (block_id < id_SS_Ring) goto solid;          /* $01-$39 are solid */
    if (block_id >= id_SS_Glass_Ani1) goto solid;   /* $4B-$4E (broken glass) solid */
    return;

solid:
    sonss_touchedblock_id(o)  = block_id;
    sonss_touchedblock_ram(o) = (uint32_t)(block_addr + 1 - ram);
    *flag = 0xFF;
}

/* --- Non-solid items (rings, emeralds, 1-Ups, ghost tags). --- */
static void SonicSS_ChkItems_NonSolidActionBlock(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_sslayout_base);

    uint16_t d4 = (uint16_t)obY(o);
    d4 += 80;
    d4  = (uint16_t)(d4 / ss_blocksize);
    d4  = (uint16_t)(d4 * ss_layout_rowlength);
    a1 += d4;

    d4  = (uint16_t)obX(o);
    d4 += 32;
    d4  = (uint16_t)(d4 / ss_blocksize);
    a1 += d4;

    uint8_t block = *a1;
    if (block == 0) {
        /* No item here. If ghost state was armed (==2), make the ghost
           blocks solid. */
        if (sonss_ghoststate(o) != 0) {
            SonicSS_MakeGhostSolid(o);
        }
        return;
    }

    /* Ring? ($3A) */
    if (block == id_SS_Ring) {
        uint8_t *a2 = SS_FindFreeAnimationSlot();
        ss_ani_id(a2) = SS_ANI_ID_RINGSPARKS;
        ss_ani_block(a2) = (uint32_t)(a1 - ram);

        CollectRing(o);
        if (v_rings >= ss_continue_rings) {
            if (!(v_lifecount & 1)) {
                v_lifecount |= 1;
                v_continues++;
                Sound_Queue(sfx_Continue, false);
            }
        }
        return;
    }

    /* 1-Up? ($28) */
    if (block == id_SS_1Up) {
        uint8_t *a2 = SS_FindFreeAnimationSlot();
        ss_ani_id(a2) = SS_ANI_ID_1UP;
        ss_ani_block(a2) = (uint32_t)(a1 - ram);

        v_lives++;
        f_lifecount++;
        Sound_Queue(bgm_ExtraLife, false);
        return;
    }

    /* Emerald? ($3B-$40) */
    if (block >= id_SS_Emerald1_Blue && block <= id_SS_Emerald6_Grey) {
        uint8_t *a2 = SS_FindFreeAnimationSlot();
        ss_ani_id(a2) = SS_ANI_ID_EMERALDSPARKS;
        ss_ani_block(a2) = (uint32_t)(a1 - ram);

        if (v_emeralds != ss_emeralds_num) {
            uint8_t d4 = (uint8_t)(block - id_SS_Emerald1_Blue);
            RAM_ADDR(v_emldlist)[v_emeralds] = d4;
            v_emeralds++;
        }
        if (g_settings.ss_alt_anim) {                        /* → SonicSS_ExitStage */
            obAnim(o) = id_Leap1;
            obAnim(o) = id_Leap2;
        }
        Sound_Queue(bgm_Emerald, false);
        return;
    }

    /* Ghost block? ($41) */
    if (block == id_SS_Ghost) {
        sonss_ghoststate(o) = 1;
        return;
    }

    /* Ghost trigger? ($4A) */
    if (block == id_SS_InvGhostTrigger) {
        if (sonss_ghoststate(o) == 1) {
            sonss_ghoststate(o) = 2;
        }
    }
    /* Anything else: nothing to do. */
}

/* --- Make every ghost block solid (replace $41 with $2C). --- */
static void SonicSS_MakeGhostSolid(uint8_t *o) {
    if (sonss_ghoststate(o) == 2) {
        /* Convertir ghost blocks en solid (el bucle actual) */
        uint8_t *p = RAM_ADDR(v_sslayout_actual);
        int rows = (v_sslayout_end - v_sslayout_actual) / ss_layout_rowlength;
        for (int r = 0; r < rows; r++) {
            for (int i = 0; i < ss_layout_rowlength / 2; i++) {
                if (p[i] == id_SS_Ghost) {
                    p[i] = id_SS_RedWhite;
                }
            }
            p += ss_layout_rowlength;
        }
    }
    /* .GhostNotSolid: se ejecuta SIEMPRE */
    sonss_ghoststate(o) = 0;
}

/* --- Solid action blocks (bumper, GOAL, UP/DOWN, R, glass). --- */
static void SonicSS_ChkItems_SolidActionBlock(uint8_t *o) {
    uint8_t id = sonss_touchedblock_id(o);

    if (id == 0) {
        /* Decrement timeouts. */
        if (sonss_timeout_updown(o) != 0) {
            sonss_timeout_updown(o)--;
            if ((int8_t)sonss_timeout_updown(o) < 0) {
                sonss_timeout_updown(o) = 0;
            }
        }
        if (sonss_timeout_r(o) != 0) {
            sonss_timeout_r(o)--;
            if ((int8_t)sonss_timeout_r(o) < 0) {
                sonss_timeout_r(o) = 0;
            }
        }
        return;
    }

    /* Bumper? */
    if (id == id_SS_Bumper) {
        uint32_t off = sonss_touchedblock_ram(o) - 1;
        uint16_t col = off & (ss_layout_rowlength - 1);
        int16_t bumper_x = (int16_t)(col * ss_blocksize) - 20;
        uint16_t row = off >> 7;
        int16_t bumper_y = (int16_t)(row * ss_blocksize) - (20 + ss_blocksize * 2);

        int16_t dx = (int16_t)(bumper_x - obX(o));
        int16_t dy = (int16_t)(bumper_y - obY(o));
        uint8_t angle = CalcAngle(dx, dy);

        int16_t s0, s1;
        CalcSine(angle, &s0, &s1);
        int16_t vx = (int16_t)(((int32_t)s1 * -0x700) >> 8);
        int16_t vy = (int16_t)(((int32_t)s0 * -0x700) >> 8);
        obVelX(o) = vx;
        obVelY(o) = vy;
        obStatus(o) |= (1 << 1);

        uint8_t *a2 = SS_FindFreeAnimationSlot();
        ss_ani_id(a2) = SS_ANI_ID_BUMPER;
        ss_ani_block(a2) = sonss_touchedblock_ram(o) - 1;
        Sound_Queue(sfx_Bumper, false);
        return;
    }

    /* GOAL? */
    if (id == id_SS_GOAL) {
        obRoutine(o) += 2;
        if (g_settings.ss_alt_anim) {                        /* → SonicSS_ExitStage */
            obAnim(o) = id_Shrink;
        }
        Sound_Queue(sfx_SSGoal, false);
        return;
    }

    /* UP block? */
    if (id == id_SS_UP) {
        if (sonss_timeout_updown(o) != 0) return;
        sonss_timeout_updown(o) = ss_timeout;

        /* ASM: btst #6,(v_ssrotate+1).w ; beq.s SonicSS_UPsnd
         * Traducido: shift SOLO si bit 6 = 1 (base $40). */
        if (v_ssrotate & 0x0040) {
            v_ssrotate <<= 1;
            uint8_t *p = RAM_ADDR(sonss_touchedblock_ram(o) - 1);
        *p = id_SS_DOWN;
        }
        Sound_Queue(sfx_SSItem, false);
        return;
    }

    /* DOWN block? */
    if (id == id_SS_DOWN) {
        if (sonss_timeout_updown(o) != 0) return;
       sonss_timeout_updown(o) = ss_timeout;

        /* ASM: btst #6,(v_ssrotate+1).w ; bne.s SonicSS_DOWNsnd
         * Traducido: shift SOLO si bit 6 = 0 (ya estás en fast $80). */
        if (!(v_ssrotate & 0x0040)) {
            v_ssrotate = (uint16_t)((int16_t)v_ssrotate >> 1);
            uint8_t *p = RAM_ADDR(sonss_touchedblock_ram(o) - 1);
            *p = id_SS_UP;
        }
        Sound_Queue(sfx_SSItem, false);
        return;
    }

    /* R block? */
    if (id == id_SS_R) {
        if (sonss_timeout_r(o) != 0) return;
        sonss_timeout_r(o) = ss_timeout;

        uint8_t *a2 = SS_FindFreeAnimationSlot();
        ss_ani_id(a2) = SS_ANI_ID_REVERSE;
        ss_ani_block(a2) = sonss_touchedblock_ram(o) - 1;

        v_ssrotate = (int16_t)(-v_ssrotate);
        Sound_Queue(sfx_SSItem, false);
        return;
    }

    /* Glass block? */
    if (id >= id_SS_Glass1_Blue && id <= id_SS_Glass4_Pink) {
        uint8_t *a2 = SS_FindFreeAnimationSlot();
        ss_ani_id(a2) = SS_ANI_ID_GLASSBLOCK;
        ss_ani_block(a2) = sonss_touchedblock_ram(o) - 1;

        uint8_t *block = RAM_ADDR(sonss_touchedblock_ram(o) - 1);
       uint8_t next_id = (uint8_t)(*block + 1);
       if (next_id > id_SS_Glass4_Pink) next_id = 0;
        a2[1] = next_id;        /* ← guardar en el byte "unused", NO en a2[3] */

       Sound_Queue(sfx_SSGlass, false);
    }
}

/* --- Camera follows Sonic (SS version). --- */
static void SS_FixCamera(uint8_t *o) {
    int16_t d2 = obY(o);
    int16_t d3 = obX(o);
    int16_t d0 = (int16_t)RAM_WORD(0xF700);         /* v_screenposx low word */

    d3 = (int16_t)(d3 - 320 / 2);
    if ((uint16_t)d3 < (uint16_t)(320 / 2)) {
        /* Borrow would have been set: Sonic is left of x=160, skip X. */
    } else {
        d0 = (int16_t)(d0 - d3);
        RAM_WORD(0xF700) = (uint16_t)((int16_t)RAM_WORD(0xF700) - d0);
    }

    d0 = (int16_t)RAM_WORD(0xF704);                 /* v_screenposy low word */
    d2 = (int16_t)(d2 - 224 / 2);
    if ((uint16_t)d2 < (uint16_t)(224 / 2)) {
        /* Skip Y. */
    } else {
        d0 = (int16_t)(d0 - d2);
        RAM_WORD(0xF704) = (uint16_t)((int16_t)RAM_WORD(0xF704) - d0);
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

    fprintf(stderr, "[Sign] enter routine6: debuguse=%d status=%02X id=%02X sonicX=%d limitR=%d\n",
            v_debuguse, obStatus(player), obID(player),
            obX(player), v_limitright2);

    if (v_debuguse) return;

    if (!(obStatus(player) & (1 << 1))) {
        f_lockctrl  = 1;
        v_jpadhold2 = btnR;
        v_jpadpress2 = 0;
        fprintf(stderr, "[Sign] LOCK: lockctrl=%d hold2=%02X press2=%02X\n",
                f_lockctrl, v_jpadhold2, v_jpadpress2);
    }

    if (obID(player) == 0) { Sign_LoadEndCards(o); return; }

    int16_t d0 = obX(player);
    int16_t d1 = (int16_t)v_limitright2 + (320 - 24);
    fprintf(stderr, "[Sign] xcheck: sonicX=%d need>=%d\n", d0, d1);
    if ((uint16_t)d0 < (uint16_t)d1) return;
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
    if (OutOfRange(o, -1, 0)) {                        /* out_of_range.w DeleteObject */
        DeleteObject(o);
    }
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
    if (!OutOfRange(o, -1, 0)) {                       /* out_of_range.w .deleteBridge */
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
static int SolidObject_Heightmap(uint8_t *o, int16_t d1, int16_t d2, const uint8_t *a2);

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
    if (OutOfRange(o, -1, 0)) {                       /* out_of_range.w DeleteObject */
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
    if (d0 < d1) goto solid_left;              /* cmp.w d1,d0 / bhs.s .sonic_left */
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
    if ((uint16_t)d5 > (uint16_t)d1) goto Solid_TopBottom;
    if ((uint16_t)d1 <= 4) goto Solid_SideAir;
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

static int SolidObject_Heightmap(uint8_t *o, int16_t d1, int16_t d2,
                                  const uint8_t *a2) {
    uint8_t *a1 = RAM_ADDR(v_player);
    int16_t d0, d3, d4, d5;


    /* --- ASM: tst.b obRender(a0) ; bpl.w Solid_NoCollision --- */
    if (!(obRender(o) & 0x80)) return 0;

    /* --- X check ---
       d0 = obX(a1) - obX(a0) + d1
       bmi.w Solid_NoCollision
       d3 = d1*2 ; cmp.w d3,d0 ; bhi.w Solid_NoCollision */
    d0 = (int16_t)(obX(a1) - obX(o) + d1);
    if (d0 < 0) return 0;
    d3 = (int16_t)(d1 + d1);
    if ((uint16_t)d0 > (uint16_t)d3) return 0;

    /* --- X-flip + >>1 ---
       d5 = d0
       if xflip: d5 = ~d5 + d3
       d5 >>= 1 */
    d5 = d0;
    if (obRender(o) & sprite_xflip) {
        d5 = (int16_t)(~d5 + d3);
    }
    d5 = (int16_t)((uint16_t)d5 >> 1);

    /* --- Lee heightmap y resta baseline ---
       d3 = a2[d5]
       sub.b (a2),d3     <-- ¡BUG ARREGLADO!
       topY = obY(a0) - d3 */
    d3 = (int16_t)((int8_t)a2[d5] - (int8_t)a2[0]);
    d5 = (int16_t)(obY(o) - d3);           /* d5 = topY */

    /* --- d2 += obHeight(Sonic) --- */
    d3 = (int16_t)(int8_t)obHeight(a1);
    d2 = (int16_t)(d2 + d3);               /* d2 = halfH + sonicH */

    /* --- d3 = (sonicY - topY) + 4 + d2 ; bmi.w Solid_NoCollision --- */
    d3 = (int16_t)(obY(a1) - d5);
    d3 = (int16_t)(d3 + 4);
    d3 = (int16_t)(d3 + d2);
    if (d3 < 0) return 0;

    /* --- d4 = d2*2 ; cmp.w d4,d3 ; bhs.w Solid_NoCollision --- */
    d4 = (int16_t)(d2 + d2);
    if ((uint16_t)d3 >= (uint16_t)d4) return 0;

    /* ============ Fall-through a Solid_Collision ============ */
    if ((int8_t)f_playerctrl < 0) goto shm_nocollision;
    if ((uint8_t)obRoutine(a1) >= 6) goto shm_nocollision;
    if (v_debuguse) goto shm_nocollision;

    /* d5 = x pos de Sonic sobre el objeto (0..2*d1) */
    d5 = d0;
    if ((uint16_t)d0 > (uint16_t)d1) {          /* cmp.w d0,d1 ; bhs .sonic_left */
        d1 = (int16_t)(d1 + d1);
        d0 = (int16_t)(d0 - d1);
        d5 = (int16_t)(-d0);
    }

    /* d1 = distancia Y al borde más cercano */
    d1 = d3;
    if ((uint16_t)d3 > (uint16_t)d2) {          /* cmp.w d3,d2 ; bhs .sonic_top */
        d3 = (int16_t)(d3 - 4);
        d3 = (int16_t)(d3 - d4);
        d1 = (int16_t)(-d3);
    }

    /* cmp.w d1,d5 ; bhi.w Solid_TopBottom */
    if ((uint16_t)d5 > (uint16_t)d1) goto shm_topbottom;
    /* cmpi.w #4,d1 ; bls.s Solid_SideAir */
    if ((uint16_t)d1 <= 4) goto shm_sideair;

    /* tst.w d0 ; beq.s Solid_AlignToSide
       bmi.s Solid_OnRight */
    if (d0 == 0) goto shm_align;
    if (d0 < 0) {
        /* Solid_OnRight: bpl.s Solid_AlignToSide */
        if (obVelX(a1) >= 0) goto shm_align;
    } else {
        /* sonic_left con d0>0: bmi.s Solid_AlignToSide */
        if (obVelX(a1) < 0) goto shm_align;
    }

    /* Solid_StopX */
    obInertia(a1) = 0;
    obVelX(a1) = 0;

shm_align:
    obX(a1) = (int16_t)(obX(a1) - d0);
    if (obStatus(a1) & (1 << 1)) goto shm_sideair;
    obStatus(a1) |= (1 << 5);
    obStatus(o)  |= (1 << 5);
    return 1;

shm_sideair:
    obStatus(o)  &= ~(1 << 5);
    obStatus(a1) &= ~(1 << 5);
    return 1;

shm_topbottom:
    /* tst.w d3 ; bmi.s Solid_Below
       cmpi.w #$10,d3 ; blo.s Solid_Landed
       bra.s Solid_NoCollision */
    if (d3 < 0) goto shm_below;
    if (d3 < 0x10) goto shm_landed;
    goto shm_nocollision;

shm_below:
    /* tst.w obVelY(a1) ; beq.s Solid_Squash
       bpl.s Solid_TopBtmAir
       tst.w d3 ; bpl.s Solid_TopBtmAir
       [FixBugs=0: NO se hace sub.w d3,obY aquí]
       move.w #0,obVelY(a1) */
    if (obVelY(a1) == 0) goto shm_squash;
    if (obVelY(a1) > 0) goto shm_topair;
    if (d3 >= 0) goto shm_topair;
    obVelY(a1) = 0;

shm_topair:
    return -1;

shm_squash:
    /* btst #1,obStatus(a1) ; bne.s Solid_TopBtmAir
       KillSonic */
    if (obStatus(a1) & (1 << 1)) goto shm_topair;
    KillSonic(a1, NULL);
    return -1;

shm_landed:
    /* subq.w #4,d3
       d1 = obActWid(a0) ; d2 = d1*2
       d1 = obX(a1) + d1 - obX(a0)
       bmi.s Solid_Miss ; cmp.w d2,d1 ; bhs.s Solid_Miss
       tst.w obVelY(a1) ; bmi.s Solid_Miss
       sub.w d3,obY(a1) ; subq.w #1,obY(a1)
       bsr.s Solid_ResetFloor
       move.b #2,obSolid(a0) ; bset #3,obStatus(a0) */
    d3 = (int16_t)(d3 - 4);
    {
        int16_t w  = (int16_t)(int8_t)obActWid(o);
        int16_t w2 = (int16_t)(w + w);
        int16_t xr = (int16_t)(obX(a1) + w - obX(o));
        if (xr < 0) return 0;
        if ((uint16_t)xr >= (uint16_t)w2) return 0;
    }
    if (obVelY(a1) < 0) return 0;
    /* ASM: obY -= (d3-4) ; obY -= 1
       → obY_new = sonicY - ((sonicY - topY) + halfH + sonicH) - 1
                = topY - halfH - sonicH - 1
       Con d2 = halfH + sonicH, esto es topY - d2 - 1. */
    obY(a1) = (int16_t)(obY(a1) - d3);
    obY(a1) = (int16_t)(obY(a1) - 1);
    Solid_ResetFloor(o);
    obSolid(o) = 2;
    obStatus(o) |= (1 << 3);
    return -1;

shm_nocollision:
    if (obStatus(o) & (1 << 5)) {
        obAnim(a1) = id_Run;
    }
    obStatus(o)  &= ~(1 << 5);
    obStatus(a1) &= ~(1 << 5);
    return 0;
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
    if (OutOfRange(o, -1, 0)) {                       /* out_of_range.w DeleteObject */
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
 *  Object 12 — Spinning Light (SYZ hexagonal glass prism)
 *  Ported verbatim from _incObj/12 SYZ Search Light.asm (REV01, FixBugs=0).
 *
 *  Objeto muy simple: al inicializarse se queda con prioridad muy baja (6)
 *  y luego cicla por 6 frames cada 8 frames. Se autodestruye al salir de
 *  la pantalla con out_of_range → DeleteObject.
 * =========================================================================== */

static void Light_Main(uint8_t *o) {
    obRoutine(o) += 2;                                   /* addq.b #2 → Light_Animate */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Light;      /* move.l #Map_Light,obMap */
    obGfx(o)      = (uint16_t)ArtTile_Level;             /* move.w #ArtTile_Level,obGfx */
    obRender(o)   = sprite_cam_field;                    /* move.b #sprite_cam_field,obRender */
    obActWid(o)   = 32 / 2;                              /* move.b #32/2,obActWid */
    obPriority(o) = 6;                                   /* move.b #6,obPriority */
    /* ASM falls through into Light_Animate on the first frame. */
}

static void Light_Animate(uint8_t *o) {
    obTimeFrame(o)--;                                    /* subq.b #1,obTimeFrame(a0) */
    if ((int8_t)obTimeFrame(o) < 0) {                    /* bpl.s .chkdel (branch when time remains) */
        obTimeFrame(o) = 8 - 1;                          /* move.b #8-1,obTimeFrame */
        obFrame(o)++;                                    /* addq.b #1,obFrame */
        if (obFrame(o) >= 6) {                           /* cmpi.b #6 / blo.s .chkdel */
            obFrame(o) = 0;                              /* move.b #0,obFrame */
        }
    }

    /* .chkdel: */
    if (OutOfRange(o, -1, 0)) {                                /* out_of_range.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                                    /* bra.w DisplaySprite */
}

/* Spinning Light dispatcher — Light_Index: 0 = Main, 2 = Animate */
static void SpinningLight_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                              /* Light_Index */
        case 0: Light_Main(o);    break;
        case 2: Light_Animate(o); break;
    }
}

/* ===========================================================================
 *  Object 13 — Lava Maker (MZ, SLZ)
 *  Object 14 — Lava Ball   (MZ, SLZ)
 *  Ported verbatim from _incObj/13, 14 MZ, SLZ Fire Balls and Maker.asm
 *  (REV01, FixBugs=0).
 *
 *  LavaMaker fields:
 *    obDelayAni ($1C, byte): firing interval, loaded from LavaM_Rates
 *    obTimeFrame($1B, byte): countdown to next spawn
 *    obSubtype upper nibble : index into LavaM_Rates
 *    obSubtype lower nibble : copied to the spawned lava ball
 *
 *  LavaBall fields:
 *    lball_fromboss = objoff_29 (byte): set by MZ boss, bumps obPriority by 2
 *    lball_origY    = objoff_30 (word): original Y for the rise-and-fall types
 * =========================================================================== */

#define lball_fromboss(o) (*(uint8_t *)((uint8_t *)(o) + 0x29))
#define lball_origY(o)    (*(int16_t *)((uint8_t *)(o) + 0x30))

/* Firing interval per subtype upper-nibble. */
static const uint8_t LavaM_Rates[6] = { 30, 60, 90, 120, 150, 180 };

/* Initial vertical velocity per subtype. Types 6/7 copy it to velX. */
static const int16_t LBall_Speeds[9] = {
    -0x400, -0x500, -0x600, -0x700,
    -0x200,  0x200,
    -0x200,  0x200,
         0,
};

static void LavaM_Main(uint8_t *o);
static void LavaM_MakeLava(uint8_t *o);
static void LBall_Main(uint8_t *o);
static void LBall_Action(uint8_t *o);
static void LBall_Delete(uint8_t *o);
static void LBall_RiseAndFall(uint8_t *o);
static void LBall_Up(uint8_t *o);
static void LBall_Down(uint8_t *o);
static void LBall_Left(uint8_t *o);
static void LBall_Right(uint8_t *o);
static void LBall_DoNothing(uint8_t *o);

/* Local helper: exact port of _incObj/sub ObjFloorDist.asm ObjHitCeiling.
   Returns d1 (distance to ceiling; negative = hit). */
static int16_t LBall_ObjHitCeiling(uint8_t *o) {
    int16_t d1;
    int16_t y = (int16_t)((obY(o) - (int8_t)obHeight(o)) ^ 0xF);
    int16_t x = obX(o);
    FindFloor(y, x, 0x0E, 0x1000, -0x10, &v_anglebuffer, o, &d1);
    return d1;
}

/* --- LavaM_Main — routine 0 --- */
static void LavaM_Main(uint8_t *o) {
    obRoutine(o) += 2;                       /* → LavaM_MakeLava */

    uint8_t d0 = obSubtype(o) >> 4;
    if (d0 > 5) d0 = 5;                      /* ASM reads OOB; clamp for safety */
    obDelayAni(o)  = LavaM_Rates[d0];
    obTimeFrame(o) = obDelayAni(o);
    obSubtype(o)  &= 0x0F;                   /* clear upper subtype digit */

    /* ASM falls through into LavaM_MakeLava on the first frame */
    LavaM_MakeLava(o);
}

/* --- LavaM_MakeLava — routine 2 --- */
static void LavaM_MakeLava(uint8_t *o) {
    obTimeFrame(o)--;
    if (obTimeFrame(o) != 0) return;
    obTimeFrame(o) = obDelayAni(o);

    /* ChkObjectVisible: maker has no obActWid/obHeight set (both 0), so the
       check reduces to "is (obX, obY) inside the visible window?". */
    {
        int16_t sx = (int16_t)(obX(o) - (int16_t)v_screenposx);
        int16_t sy = (int16_t)(obY(o) - (int16_t)v_screenposy);
        if (sx < 0 || sx >= 320) return;
        if (sy < 0 || sy >= 224) return;
    }

    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (!a1) return;

    obID(a1)      = id_LavaBall;
    obX(a1)       = obX(o);
    obY(a1)       = obY(o);
    obSubtype(a1) = obSubtype(o);
}

/* --- LavaMaker dispatcher ---
   ASM (FixBugs=0): jsr LavaM_Index ; bra.w LBall_ChkDel.
   LBall_ChkDel (FixBugs=0) does out_of_range → DeleteObject, then rts.
   No DisplaySprite anywhere: LavaMaker is invisible. */
static void LavaMaker_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: LavaM_Main(o);     break;
        case 2: LavaM_MakeLava(o); break;
    }
    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
    }
}

/* --- LBall_Main — routine 0 --- */
static void LBall_Main(uint8_t *o) {
    obRoutine(o) += 2;                                  /* → LBall_Action */
    obHeight(o) = 16 / 2;
    obWidth(o)  = 16 / 2;
    obMap(o)    = (uint32_t)(uintptr_t)Map_Fire;

    obGfx(o) = (uint16_t)ArtTile_MZ_Fireball;
    if ((uint8_t)v_zone == id_SLZ) {
        obGfx(o) = (uint16_t)ArtTile_SLZ_Fireball;
    }

    obRender(o)   = sprite_cam_field;
    obPriority(o) = 3;
    obColType(o)  = (uint8_t)(col_16x16 | col_hurt);
    lball_origY(o) = obY(o);

    if (lball_fromboss(o) != 0) {
        obPriority(o) += 2;                              /* lower priority */
    }

    {
        uint8_t d0 = obSubtype(o);
        if (d0 > 8) d0 = 8;                              /* clamp for safety */
        obVelY(o) = LBall_Speeds[d0];
    }
    obActWid(o) = 16 / 2;

    if (obSubtype(o) >= 6) {
        /* Horizontal ball */
        obActWid(o) = 32 / 2;
        obAnim(o)   = 2;
        obVelX(o)   = obVelY(o);
        obVelY(o)   = 0;
    }

    Sound_Queue(sfx_Fireball, false);

    /* ASM falls through into LBall_Action on the first frame */
    LBall_Action(o);
}

/* --- LBall_RiseAndFall — subtypes 0..3 --- */
static void LBall_RiseAndFall(uint8_t *o) {
    int16_t d0;

    obVelY(o) = (int16_t)(obVelY(o) + 0x18);

    d0 = lball_origY(o);
    /* cmp.w obY(a0),d0 ; bhs.s .checkYFlip — unsigned compare */
    if ((uint16_t)d0 < (uint16_t)obY(o)) {
        /* Fell back to (or past) original Y → schedule delete next frame */
        obRoutine(o) += 2;
    }

    /* .checkYFlip */
    obStatus(o) &= ~(1 << 1);              /* face down */
    if ((int16_t)obVelY(o) < 0) {
        obStatus(o) |= (1 << 1);           /* face up */
    }
}

/* --- LBall_Up — subtype 4 --- */
static void LBall_Up(uint8_t *o) {
    obStatus(o) |= (1 << 1);               /* face up */

    if (LBall_ObjHitCeiling(o) < 0) {
        obSubtype(o) = 8;                  /* → LBall_DoNothing */
        obAnim(o)    = 1;                  /* wall-collide (vertical) */
        obVelY(o)    = 0;
    }
}

/* --- LBall_Down — subtype 5 --- */
static void LBall_Down(uint8_t *o) {
    int16_t d1, angle;

    obStatus(o) &= ~(1 << 1);              /* face down */

    ObjFloorDist(o, &d1, &angle);
    if (d1 < 0) {
        obSubtype(o) = 8;
        obAnim(o)    = 1;
        obVelY(o)    = 0;
    }
}

/* --- LBall_Left — subtype 6 --- */
static void LBall_Left(uint8_t *o) {
    obStatus(o) |= (1 << 0);               /* face left */

    if (PushB_ObjHitWallLeft(o, -8) < 0) {
        obSubtype(o) = 8;
        obAnim(o)    = 3;                  /* wall-collide (horizontal) */
        obVelX(o)    = 0;
    }
}

/* --- LBall_Right — subtype 7 --- */
static void LBall_Right(uint8_t *o) {
    obStatus(o) &= ~(1 << 0);              /* face right */

    if (PushB_ObjHitWallRight(o, 8) < 0) {
        obSubtype(o) = 8;
        obAnim(o)    = 3;
        obVelX(o)    = 0;
    }
}

/* --- LBall_DoNothing — subtype 8 --- */
static void LBall_DoNothing(uint8_t *o) { (void)o; }

/* --- LBall_Action — routine 2 --- */
static void LBall_Action(uint8_t *o) {
    switch (obSubtype(o)) {
        case 0: case 1: case 2: case 3:
            LBall_RiseAndFall(o); break;
        case 4: LBall_Up(o);          break;
        case 5: LBall_Down(o);        break;
        case 6: LBall_Left(o);        break;
        case 7: LBall_Right(o);       break;
        default: LBall_DoNothing(o);  break;   /* subtype 8 and any OOB value */
    }

    SpeedToPos(o);
    if (Ani_Fire) AnimateSprite(o, Ani_Fire);

    /* LBall_ChkDel (FixBugs=0): only the out_of_range check, no DisplaySprite */
    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
    }
}

/* --- LBall_Delete — routine 4 --- */
static void LBall_Delete(uint8_t *o) {
    DeleteObject(o);
}

/* --- LavaBall dispatcher ---
   ASM (FixBugs=0): jsr LBall_Index ; bra.w DisplaySprite.
   DisplaySprite runs even when the routine has just deleted the object; the
   sprite queue entry is harmless because BuildSprites skips obID == 0. */
static void LavaBall_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: LBall_Main(o);   break;
        case 2: LBall_Action(o); break;
        case 4: LBall_Delete(o); break;
    }
    DisplaySprite(o);
}

/* ===========================================================================
 *  Object 15 — Swinging Platforms (GHZ, MZ, SLZ) / spiked ball (SBZ)
 *  Ported verbatim from _incObj/15 Swinging Platforms.asm (REV01, FixBugs=0).
 *
 *  swing_children = obSubtype ($28): number of spawned child links
 *  swing_origY    = objoff_38 (word)
 *  swing_origX    = objoff_3A (word)
 *  swing_radius   = objoff_3C (byte): distance from pivot
 *
 *  This file also hosts Swing_UpdateSwingPosition and GBall_Move, both
 *  shared with Object 48 (Wrecking Ball).
 * =========================================================================== */

#define swing_children(o)  (*(uint8_t *)((uint8_t *)(o) + 0x28))
#define swing_origY(o)     (*(int16_t *)((uint8_t *)(o) + 0x38))
#define swing_origX(o)     (*(int16_t *)((uint8_t *)(o) + 0x3A))
#define swing_radius(o)    (*(uint8_t *)((uint8_t *)(o) + 0x3C))

/* Wrecking-ball fields, sharing the same offsets as Object 15 for the
   pivot and radius. */
#define gb_anchorpos(o)    (*(int16_t *)((uint8_t *)(o) + 0x32))
#define gb_linkdist(o)     (*(uint8_t *)((uint8_t *)(o) + 0x3C))
#define gb_swingdir(o)     (*(uint8_t *)((uint8_t *)(o) + 0x3D))
#define gb_swingspeed(o)   (*(int16_t *)((uint8_t *)(o) + 0x3E))
#define gb_anglew(o)       (*(int16_t *)((uint8_t *)(o) + 0x26))

static void Swing_Main(uint8_t *o);
static void Swing_Platform(uint8_t *o);
static void Swing_StoodOn(uint8_t *o);
static void Swing_Delete(uint8_t *o);
static void Swing_ChainLink(uint8_t *o);
static void Swing_Swinging(uint8_t *o);
static void Swing_Move(uint8_t *o);
static void GBall_Move(uint8_t *o);
static void Swing_ChkDel(uint8_t *o);

/* The actual implementation with d0 passed explicitly. */
static void Swing_UpdateSwingPosition_D0(uint8_t *o, int16_t d0) {
    int16_t s0, s1;
    CalcSine(d0, &s0, &s1);             /* s0=sin, s1=cos */
    int16_t d2 = swing_origY(o);
    int16_t d3 = swing_origX(o);

    uint8_t *a2 = (uint8_t *)((uint8_t *)o + 0x29);  /* swing_children + 1 */
    uint8_t d6 = *(a2 - 1);                          /* swing_children count */
    for (int i = d6; i >= 0; i--) {
        uint8_t idx = *a2++;
        uint8_t *a1 = (uint8_t *)Object_GetSlot((int)idx);

        uint8_t d4 = swing_radius(a1);               /* radius */
        int32_t dy = (int32_t)(int16_t)d4 * s0;      /* muls.w d0,d4 */
        int32_t dx = (int32_t)(int16_t)d4 * s1;      /* muls.w d1,d5 */
        dy >>= 8;
        dx >>= 8;
        obY(a1) = (int16_t)(d2 + (int16_t)dy);
        obX(a1) = (int16_t)(d3 + (int16_t)dx);
    }
}

/* Swing_Main — Routine 0: initialize platform + spawn child links. */
static void Swing_Main(uint8_t *o) {
    obRoutine(o) += 2;
    obMap(o)     = (uint32_t)(uintptr_t)Map_Swing_GHZ;
    obGfx(o)     = (uint16_t)(ArtTile_GHZ_MZ_Swing | Tile_Pal3);
    obRender(o)  = sprite_cam_field;
    obPriority(o)= 3;
    obActWid(o)  = 48 / 2;
    obHeight(o)  = 16 / 2;
    swing_origY(o) = obY(o);
    swing_origX(o) = obX(o);

    if ((uint8_t)v_zone == id_SLZ) {
        obMap(o)     = (uint32_t)(uintptr_t)Map_Swing_SLZ;
        obGfx(o)     = (uint16_t)(ArtTile_SLZ_Swing | Tile_Pal3);
        obActWid(o)  = 64 / 2;
        obHeight(o)  = 32 / 2;
        obColType(o) = (uint8_t)(col_64x16 | col_hurt);
    }
    if ((uint8_t)v_zone == id_SBZ) {
        obMap(o)     = (uint32_t)(uintptr_t)Map_BBall;
        obGfx(o)     = (uint16_t)ArtTile_SBZ_Swing;
        obActWid(o)  = 48 / 2;
        obHeight(o)  = 48 / 2;
        obColType(o) = (uint8_t)(col_32x32 | col_hurt);
        obRoutine(o) = 0x0C;
    }

    /* Spawn child links (same loop shape as Hel_Main). */
    uint8_t d4 = obID(o);
    uint8_t *a2 = (uint8_t *)o + 0x28;
    uint8_t d1 = *(a2);                          /* subtype */
    int16_t sp_backup = (int16_t)d1;
    uint8_t count = d1 & 0x0F;
    *(a2)++ = 0;                                 /* clear subtype */
    uint8_t d3 = (uint8_t)((count << 4) + 8);    /* parent radius */
    swing_radius(o) = d3;
    d3 = (uint8_t)(d3 - 8);

    if (obFrame(o) != 0) {                       /* main block? */
        d3 = (uint8_t)(d3 + 8);
        if (count > 0) count -= 1;
    }

    uint8_t *a1 = o;
    for (int i = 0; i <= count; i++) {         /* count + 1 iteraciones (dbf) */
        a1 = (uint8_t *)FindNextFreeObj(a1);   /* SIEMPRE nuevo slot */
        if (!a1) break;

        swing_children(o)++;
        *a2++ = (uint8_t)Object_GetIndex(a1);

        /* Link setup común */
        obRoutine(a1)  = 0x0A;                 /* Swing_ChainLink */
        obID(a1)       = d4;
        obMap(a1)      = obMap(o);
        obGfx(a1)      = (uint16_t)(obGfx(o) & ~0x0040); /* bclr #6 → pal 1 */
        obRender(a1)   = sprite_cam_field;
        obPriority(a1) = 4;
        obActWid(a1)   = 16 / 2;
        obFrame(a1)    = 1;                    /* chain */
        swing_radius(a1) = d3;
        d3 = (uint8_t)(d3 - 0x10);
        if (d3 >= 0xF0) {
            obFrame(a1)    = 2;                /* anchor */
            obPriority(a1) = 3;
            obGfx(a1) = (uint16_t)(obGfx(o) | 0x0040); /* bset #6 → pal 3 */
        }
    }

    /* Parent index stored last. */
    *a2++ = (uint8_t)Object_GetIndex(o);

    gb_anglew(o) = 0x4080;            /* $4080 word */
    gb_swingspeed(o) = (int16_t)-0x200;

    if (sp_backup & 0x10) {                  /* bit 4 → GHZ ball variant */
        obMap(o)     = (uint32_t)(uintptr_t)Map_GBall;
        obGfx(o)     = (uint16_t)(ArtTile_GHZ_Giant_Ball | Tile_Pal3);
        obFrame(o)   = 1;
        obPriority(o)= 2;
        obColType(o) = (uint8_t)(col_40x40 | col_hurt);
    }

    if ((uint8_t)v_zone == id_SBZ) {
        /* ASM: beq.s Swing_Swinging */
        Swing_Swinging(o);
        return;
    }
    /* ASM cae a Swing_Platform */
    Swing_Platform(o);
}

/* Swing_Platform — Routine 2 */
static void Swing_Platform(uint8_t *o) {
    int16_t d1 = (int16_t)obActWid(o);
    int16_t d3 = (int16_t)obHeight(o);
    PlatformObject_CustomHeight(o, d1, d3);
    /* ASM cae a Swing_Swinging */
    Swing_Swinging(o);
}

/* Swing_Swinging — Routine $C: main visual/position update. */
static void Swing_Swinging(uint8_t *o) {
    Swing_Move(o);
    DisplaySprite(o);
    Swing_ChkDel(o);
}

/* Swing_StoodOn — Routine 4 */
static void Swing_StoodOn(uint8_t *o) {
    int16_t d1 = (int16_t)obActWid(o);
    int16_t dummy;
    ExitPlatform(o, d1, &dummy);

    int16_t saved_x = obX(o);
    Swing_Move(o);

    int16_t d3 = (int16_t)(int8_t)obHeight(o);
    d3 = (int16_t)(d3 + 1);                  /* addq.b #1,d3 */
    MvSonicOnPtfm(o, saved_x, d3);

    DisplaySprite(o);
    Swing_ChkDel(o);
}

/* Swing_Delete — Routine 6/8 */
static void Swing_Delete(uint8_t *o) {
    DeleteObject(o);
}

/* Swing_ChainLink — Routine $A: just display */
static void Swing_ChainLink(uint8_t *o) {
    DisplaySprite(o);
}

/* Swing_Move — compute angle from v_oscillate and dispatch to the shared
   position updater. */
static void Swing_Move(uint8_t *o) {
    int16_t d0 = (int16_t)(int8_t)RAM_BYTE(v_oscillate + 0x1A);
    int16_t d1 = (int16_t)(0x40 * 2);
    if (obStatus(o) & sprite_xflip) {
        d0 = (int16_t)(-d0);
        d0 = (int16_t)(d0 + d1);
    }
    Swing_UpdateSwingPosition_D0(o, d0);
}

/* GBall_Move — alternate swing update for the wrecking ball. */
static void GBall_Move(uint8_t *o) {
    int16_t d0;
    if (gb_swingdir(o) != 0) {
        /* .swingCounterclockwise */
        d0 = (int16_t)(gb_swingspeed(o) - 8);
        gb_swingspeed(o) = d0;
        gb_anglew(o) = (int16_t)(gb_anglew(o) + d0);
        if (d0 == -0x200) gb_swingdir(o) = 0;
    } else {
        /* .swingClockwise */
        d0 = (int16_t)(gb_swingspeed(o) + 8);
        gb_swingspeed(o) = d0;
        gb_anglew(o) = (int16_t)(gb_anglew(o) + d0);
        if (d0 == 0x200) gb_swingdir(o) = 1;
    }

    int16_t angle = (int16_t)((uint16_t)gb_anglew(o) >> 8);
    Swing_UpdateSwingPosition_D0(o, angle);
}

/* Swing_ChkDel — delete platform and every child when off-screen. */
static void Swing_ChkDel(uint8_t *o) {
    if (!OutOfRange(o, swing_origX(o), 0)) return;

    uint8_t count = swing_children(o);
    uint8_t *entry = (uint8_t *)o + 0x29;
    uint8_t parent_id = obID(o);

    for (int i = 0; i <= count; i++) {
        uint8_t idx = entry[i];

        /* Defensa: descarta índices fuera del rango de objetos y slots
           que no son nuestros (por si el array tiene basura). */
        if (idx >= NUM_OBJECTS) continue;

        uint8_t *slot = (uint8_t *)Object_GetSlot((int)idx);
        if (obID(slot) != parent_id) continue;

        DeleteObject(slot);
    }
}

/* Object 15 dispatcher. */
static void SwingingPlatform_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0x00: Swing_Main(o);       break;
        case 0x02: Swing_Platform(o);   break;
        case 0x04: Swing_StoodOn(o);    break;
        case 0x06: case 0x08:
                   Swing_Delete(o);     break;
        case 0x0A: Swing_ChainLink(o);  break;
        case 0x0C: Swing_Swinging(o);   break;
    }
}

/* ===========================================================================
 *  Object 17 — Rotating helix of spikes on a horizontal pole (GHZ)
 *  Ported verbatim from _incObj/17 GHZ Spiked Pole Helix.asm
 *  (REV01, FixBugs=0).
 *
 *  hel_nchildren    = obSubtype ($28): number of spikes actually loaded
 *                                       (children + parent, if parent was
 *                                       placed mid-loop)
 *  hel_child_index  = $29-$38:          object-slot index per child spike
 *  hel_frame        = objoff_3E (byte): base spike frame ID (0-7)
 *
 *  Each spike displays frame (v_ani0_frame + hel_frame) & 7, and becomes
 *  harmful (col_8x32 | col_hurt) only on frame 0 — the pose where the
 *  sprite points up. v_ani0_frame comes from Sync1 of SynchroAnimate,
 *  which is ported here for the first time (see the SynchroAnimate patch
 *  below).
 * =========================================================================== */

#define hel_nchildren(obj)   (*(uint8_t *)((uint8_t *)(obj) + 0x28))   /* obSubtype */
#define hel_child_index(obj) ((uint8_t *)(obj) + 0x29)                  /* $29 onwards */
#define hel_frame(obj)       (*(uint8_t *)((uint8_t *)(obj) + 0x3E))    /* objoff_3E */

static void Hel_Main(uint8_t *o);
static void Hel_ParentSpike(uint8_t *o);
static void Hel_Delete(uint8_t *o);
static void Hel_ChildSpike(uint8_t *o);
static void Hel_RotateSpikes(uint8_t *o);
static void Hel_ChkDel(uint8_t *o);

/* Hel_RotateSpikes — set frame from Sync1 and toggle damage based on pose.
   Ported verbatim from Hel_RotateSpikes. */
static void Hel_RotateSpikes(uint8_t *o) {
    uint8_t d0 = v_ani0_frame;                    /* move.b (v_ani0_frame).w,d0 */
    obColType(o) = col_none;                      /* harmless by default */
    d0 = (uint8_t)(d0 + hel_frame(o));            /* add.b helix_frame(a0),d0 */
    d0 &= 7;                                      /* andi.b #7,d0 */
    obFrame(o) = d0;                              /* move.b d0,obFrame(a0) */
    if (d0 != 0) return;                          /* bne.s .return */
    obColType(o) = (uint8_t)(col_8x32 | col_hurt);/* hurt only on frame 0 (spike up) */
}

/* Hel_Main — Routine 0: build the helix, then fall through to Hel_ParentSpike. */
static void Hel_Main(uint8_t *o) {
    obRoutine(o) += 2;                                   /* addq.b #2 → Hel_ParentSpike */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Hel;        /* move.l #Map_Hel,obMap */
    obGfx(o)      = (uint16_t)(ArtTile_GHZ_Spike_Pole | Tile_Pal3);
    obStatus(o)   = 7;                                   /* move.b #7,obStatus (leftover) */
    obRender(o)   = sprite_cam_field;                    /* move.b #sprite_cam_field */
    obPriority(o) = 3;                                   /* move.b #3 */
    obActWid(o)   = 16 / 2;                              /* move.b #16/2 */

    int16_t d2 = obY(o);                                 /* base Y for children */
    int16_t d3 = obX(o);                                 /* base X for children */
    uint8_t d4 = obID(o);                                /* _move.b obID(a0),d4 */

    /* Read spike count from obSubtype, then clear it. a2 ends up pointing at
     * the child index array ($29). ASM: move.b (a2),d1 ; move.b #0,(a2)+ */
    uint8_t d1 = hel_nchildren(o);
    hel_nchildren(o) = 0;
    uint8_t *a2 = hel_child_index(o);

    /* Center the leftmost spike: d0 = (count / 2) * 16 */
    int16_t d0 = (int16_t)((uint16_t)d1 >> 1) << 4;       /* lsr.w #1 ; lsl.w #4 */
    d3 -= d0;                                             /* sub.w d0,d3 */

    /* subq.b #2,d1 ; bcs.s Hel_ParentSpike — branch when count < 2.
     * After the subtract, a borrow leaves bit 7 set on the byte result. */
    d1 = (uint8_t)(d1 - 2);
    if (d1 & 0x80) {
        Hel_ParentSpike(o);
        return;
    }

    uint8_t d6 = 0;                                       /* frame ID, wraps 0-7 */

    do {
        /* .loopBuildHelix */
        uint8_t *a1 = (uint8_t *)FindFreeObj();           /* FixBugs=0: plain FindFreeObj */
        if (a1 == NULL) {                                 /* bne.s Hel_ParentSpike */
            Hel_ParentSpike(o);
            return;
        }
        hel_nchildren(o)++;                               /* addq.b #1,helix_children(a0) */

        /* Store child slot index in the parent's array. ASM reconstructs the
         * index from a1's address; use Object_GetIndex directly. */
        uint8_t idx = (uint8_t)Object_GetIndex(a1);
        *a2++ = idx;                                      /* move.b d5,(a2)+ */

        /* Set up child spike */
        obRoutine(a1)  = 8;                               /* move.b #8,obRoutine(a1) → Hel_ChildSpike */
        obID(a1)       = d4;                              /* _move.b d4,obID(a1) */
        obY(a1)        = d2;                              /* copy parent Y */
        obX(a1)        = d3;                              /* position along the pole */
        obMap(a1)      = obMap(o);                        /* share parent's mappings */
        obGfx(a1)      = (uint16_t)(ArtTile_GHZ_Spike_Pole | Tile_Pal3);
        obRender(a1)   = sprite_cam_field;
        obPriority(a1) = 3;
        obActWid(a1)   = 16 / 2;

        hel_frame(a1) = d6;                               /* move.b d6,helix_frame(a1) */
        d6 = (uint8_t)((d6 + 1) & 7);                     /* addq.b #1 ; andi.b #7 */
        d3 = (int16_t)(d3 + 0x10);                        /* addi.w #$10,d3 */

        /* cmp.w obX(a0),d3 ; bne.s .next — has the sweep reached the parent? */
        if (d3 == obX(o)) {
            hel_frame(o) = d6;                            /* set parent's frame */
            d6 = (uint8_t)((d6 + 1) & 7);
            d3 = (int16_t)(d3 + 0x10);
            hel_nchildren(o)++;                           /* parent counted too */
        }

        d1 = (uint8_t)(d1 - 1);                           /* dbf d1 */
    } while ((int8_t)d1 != -1);

    Hel_ParentSpike(o);                                   /* fall through */
}

/* Hel_ChkDel — delete parent + children if offscreen. FixBugs=0: rts only
   on the on-screen branch (DisplaySprite is done by the caller). */
static void Hel_ChkDel(uint8_t *o) {
    if (!OutOfRange(o, -1, 0)) return;                          /* out_of_range.w .deleteHelix */

    /* .deleteHelix */
    uint8_t d2 = hel_nchildren(o);                        /* move.b (a2)+,d2 */
    uint8_t *a2 = hel_child_index(o);
    d2 = (uint8_t)(d2 - 2);                               /* subq.b #2,d2 */
    if (d2 & 0x80) {                                      /* bcs.s Hel_Delete (count < 2) */
        Hel_Delete(o);
        return;
    }

    do {
        /* .delLoop */
        uint8_t d0 = *a2++;                               /* move.b (a2)+,d0 */
        uint8_t *a1 = (uint8_t *)Object_GetSlot((int)d0);
        DeleteObject(a1);                                 /* bsr.w DeleteChild */
        d2 = (uint8_t)(d2 - 1);                           /* dbf d2 */
    } while ((int8_t)d2 != -1);

    Hel_Delete(o);                                        /* fall through */
}

/* Hel_ParentSpike — Routine 2 (and 4, unused). FixBugs=0: DisplaySprite
   happens BEFORE Hel_ChkDel. */
static void Hel_ParentSpike(uint8_t *o) {
    Hel_RotateSpikes(o);                                  /* bsr.w Hel_RotateSpikes */
    DisplaySprite(o);                                     /* bsr.w DisplaySprite (FixBugs=0) */
    Hel_ChkDel(o);                                        /* bra.w Hel_ChkDel */
}

/* Hel_Delete — Routine 6 (also the shared tail of Hel_ChkDel). */
static void Hel_Delete(uint8_t *o) {
    DeleteObject(o);                                      /* bsr.w DeleteObject */
}

/* Hel_ChildSpike — Routine 8. Child spikes never self-delete; the parent
   handles cleanup through Hel_ChkDel. */
static void Hel_ChildSpike(uint8_t *o) {
    Hel_RotateSpikes(o);                                  /* bsr.w Hel_RotateSpikes */
    DisplaySprite(o);                                     /* bra.w DisplaySprite */
}

/* Helix dispatcher — Hel_Index: 0=Main, 2/4=ParentSpike, 6=Delete, 8=ChildSpike. */
static void Helix_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                               /* Hel_Index */
        case 0: Hel_Main(o);        break;
        case 2: Hel_ParentSpike(o); break;
        case 4: Hel_ParentSpike(o); break;  /* never set in practice */
        case 6: Hel_Delete(o);      break;  /* never set in practice */
        case 8: Hel_ChildSpike(o);  break;
    }
}

/* ===========================================================================
 *  Object 18 — Basic platforms (GHZ, SYZ, SLZ)
 *  Ported verbatim from _incObj/18 Platforms.asm (REV01, FixBugs=0).
 *
 *  Fields:
 *    plat_rawY   = objoff_2C (word):  raw Y position (without nudge)
 *    plat_origX  = objoff_32 (word):  initial X
 *    plat_origY  = objoff_34 (word):  initial Y
 *    plat_nudge  = objoff_38 (byte):  0-$40 nudge (depression from Sonic's weight)
 *    plat_delay  = objoff_3A (word):  multi-purpose timer
 *
 *  Note: Plat_FallingDown reads plat_rawY as a 32-bit 16.16 value, using the
 *  word at objoff_2E as the subpixel fraction. We model this with two words.
 *  =========================================================================== */

#define plat_rawY(obj)      (*(int16_t  *)((uint8_t *)(obj) + 0x2C)) /* objoff_2C */
#define plat_rawY_sub(obj)  (*(uint16_t *)((uint8_t *)(obj) + 0x2E)) /* objoff_2E */
#define plat_origX(obj)     (*(int16_t  *)((uint8_t *)(obj) + 0x32)) /* objoff_32 */
#define plat_origY(obj)     (*(int16_t  *)((uint8_t *)(obj) + 0x34)) /* objoff_34 */
#define plat_nudge(obj)     (*(uint8_t  *)((uint8_t *)(obj) + 0x38)) /* objoff_38 */
#define plat_delay(obj)     (*(int16_t  *)((uint8_t *)(obj) + 0x3A)) /* objoff_3A */

static void Plat_Main(uint8_t *o);
static void Plat_Solid(uint8_t *o);
static void Plat_StoodOn(uint8_t *o);
static void Plat_Delete(uint8_t *o);
static void Plat_Action(uint8_t *o);
static void Plat_Nudge(uint8_t *o);
static void Plat_Move(uint8_t *o);
static void Plat_ChkDel(uint8_t *o);

/* --- Plat_ChangeMotion: reload oscillation variable (freq 8, mid $40) --- */
static void Plat_ChangeMotion(uint8_t *o) {
    obAngle(o) = RAM_BYTE(v_oscillate + 0x1A);
}

/* --- Plat_Nudge: depress platform by up to 4px while Sonic stands on it --- */
static void Plat_Nudge(uint8_t *o) {
    int16_t s0, s1;
    CalcSine(plat_nudge(o), &s0, &s1);                    /* bsr.w CalcSine */
    int32_t d0 = (int32_t)s0 * (int32_t)(int16_t)0x400;   /* muls.w #$400,d0 */
    int16_t high = (int16_t)((uint32_t)d0 >> 16);         /* swap d0 */
    obY(o) = (int16_t)(high + plat_rawY(o));              /* add.w plat_rawY / move.w obY */
}

/* --- Plat_Move: dispatch on (obSubtype & $F) --- */
static void Plat_Move(uint8_t *o) {
    switch (obSubtype(o) & 0x0F) {
        case 0x0: /* Plat_Stationary */
        case 0x9: /* Plat_Stationary (alias) */
            return;

        case 0x1: {
            /* Plat_RightLeft */
            int16_t d0 = plat_origX(o);
            uint8_t d1 = (uint8_t)(obAngle(o) - 0x40);    /* subi.b #$40 */
            int16_t d1w = (int16_t)(int8_t)d1;            /* ext.w d1 */
            obX(o) = (int16_t)(d0 + d1w);                 /* add.w d1,d0 / move.w obX */
            Plat_ChangeMotion(o);
            return;
        }

        case 0x5: {
            /* Plat_LeftRight */
            int16_t d0 = plat_origX(o);
            uint8_t d1 = (uint8_t)(-(int8_t)obAngle(o) + 0x40); /* neg.b / addi.b */
            int16_t d1w = (int16_t)(int8_t)d1;
            obX(o) = (int16_t)(d0 + d1w);
            Plat_ChangeMotion(o);
            return;
        }

        case 0x2: {
            /* Plat_DownUp */
            int16_t d0 = plat_origY(o);
            uint8_t d1 = (uint8_t)(obAngle(o) - 0x40);
            int16_t d1w = (int16_t)(int8_t)d1;
            plat_rawY(o) = (int16_t)(d0 + d1w);           /* move.w d0,plat_rawY */
            Plat_ChangeMotion(o);
            return;
        }

        case 0x6: {
            /* Plat_UpDown */
            int16_t d0 = plat_origY(o);
            uint8_t d1 = (uint8_t)(-(int8_t)obAngle(o) + 0x40);
            int16_t d1w = (int16_t)(int8_t)d1;
            plat_rawY(o) = (int16_t)(d0 + d1w);
            Plat_ChangeMotion(o);
            return;
        }

        case 0xB: {
            /* Plat_DownUp_Slow — v_oscillate+$E (freq 2, mid $30) */
            int16_t d0 = plat_origY(o);
            uint8_t osc = RAM_BYTE(v_oscillate + 0x0E);
            uint8_t d1 = (uint8_t)(osc - 0x30);           /* subi.b #$30 */
            int16_t d1w = (int16_t)(int8_t)d1;
            plat_rawY(o) = (int16_t)(d0 + d1w);
            Plat_ChangeMotion(o);
            return;
        }

        case 0xC: {
            /* Plat_UpDown_Slow — v_oscillate+$E (freq 2, mid $30) */
            int16_t d0 = plat_origY(o);
            uint8_t osc = RAM_BYTE(v_oscillate + 0x0E);
            uint8_t d1 = (uint8_t)(-(int8_t)osc + 0x30);
            int16_t d1w = (int16_t)(int8_t)d1;
            plat_rawY(o) = (int16_t)(d0 + d1w);
            Plat_ChangeMotion(o);
            return;
        }

        case 0xA: {
            /* Plat_DownUp_LargeGHZ2 — half amplitude */
            int16_t d0 = plat_origY(o);
            uint8_t d1 = (uint8_t)(obAngle(o) - 0x40);
            int16_t d1w = (int16_t)(int8_t)d1;            /* ext.w d1 */
            d1w = (int16_t)(d1w >> 1);                    /* asr.w #1 */
            plat_rawY(o) = (int16_t)(d0 + d1w);
            Plat_ChangeMotion(o);
            return;
        }

        case 0x3: {
            /* Plat_FallAfterStand */
            if (plat_delay(o) == 0) {
                if (obStatus(o) & (1 << 3)) {             /* btst #3 */
                    plat_delay(o) = 30;                   /* move.w #30 */
                }
                return;
            }
            /* .wait */
            plat_delay(o)--;
            if (plat_delay(o) != 0) return;
            plat_delay(o) = 32;                           /* move.w #32 */
            obSubtype(o)++;                               /* addq.b #1 -> type 4 */
            return;
        }

        case 0x4: {
            /* Plat_FallingDown */
            if (plat_delay(o) != 0) {
                plat_delay(o)--;
                if (plat_delay(o) == 0) {
                    if (obStatus(o) & (1 << 3)) {         /* btst #3,obStatus(a0) */
                        /* a1 = v_player (set by ExitPlatform earlier) */
                        uint8_t *a1 = RAM_ADDR(v_player);
                        obStatus(a1) |= (1 << 1);         /* bset #1 */
                        obStatus(a1) &= ~(1 << 3);        /* bclr #3 */
                        obRoutine(a1) = 2;                /* move.b #2 */
                        obStatus(o)  &= ~(1 << 3);        /* bclr #3,obStatus(a0) */
                        obSolid(o) = 0;                   /* clr.b obSolid */
                        obVelY(a1) = obVelY(o);           /* move.w */
                    }
                    /* .notOnPlatform */
                    obRoutine(o) = 8;                     /* move.b #8 -> Plat_Action */
                }
            }

            /* .fallingDown: 16.16 raw Y += velY<<8 */
            {
                int32_t d3 = ((int32_t)plat_rawY(o) << 16) | plat_rawY_sub(o);
                int32_t d0 = (int32_t)(int16_t)obVelY(o);
                d0 <<= 8;                                 /* asl.l #8 */
                d3 += d0;
                plat_rawY(o)     = (int16_t)((uint32_t)d3 >> 16);
                plat_rawY_sub(o) = (uint16_t)(d3 & 0xFFFF);
            }
            obVelY(o) = (int16_t)(obVelY(o) + gravity);   /* addi.w #gravity */

            {
                int16_t d0 = (int16_t)(v_limitbtm2 + 224); /* move.w / addi.w #224 */
                if ((uint16_t)d0 < (uint16_t)plat_rawY(o)) { /* cmp / bhs.s */
                    obRoutine(o) = 6;                     /* move.b #6 -> Plat_Delete */
                }
            }
            return;
        }

        case 0x7: {
            /* Plat_RiseOnSwitch */
            if (plat_delay(o) == 0) {
                uint8_t *a2 = RAM_ADDR(f_switch);
                uint8_t d0 = (uint8_t)(obSubtype(o) >> 4); /* lsr.w #4 */
                if (a2[d0] != 0) {                        /* tst.b (a2,d0.w) */
                    plat_delay(o) = 1 * 60;               /* move.w #1*60 */
                }
                return;
            }
            /* .wait */
            plat_delay(o)--;
            if (plat_delay(o) != 0) return;
            obSubtype(o)++;                               /* addq.b #1 -> type 8 */
            return;
        }

        case 0x8: {
            /* Plat_Rising — rise 2px/frame, stop $200 above origin */
            plat_rawY(o) -= 2;                            /* subq.w #2 */
            {
                int16_t d0 = (int16_t)(plat_origY(o) - 0x200);
                if (d0 == plat_rawY(o)) {
                    obSubtype(o) = 0;                     /* clr.b obSubtype -> 0 */
                }
            }
            return;
        }
    }
}

/* --- Plat_ChkDel: delete platform if out of range (FixBugs=0: rts) --- */
static void Plat_ChkDel(uint8_t *o) {
    if (OutOfRange(o, plat_origX(o), 0)) {                      /* out_of_range.s plat_origX */
        Plat_Delete(o);
    }
}

/* --- Plat_Delete — Routine 6 --- */
static void Plat_Delete(uint8_t *o) {
    DeleteObject(o);                                      /* bra.w DeleteObject */
}

/* --- Plat_Action — Routine 8 --- */
static void Plat_Action(uint8_t *o) {
    Plat_Move(o);                                         /* bsr.w Plat_Move */
    Plat_Nudge(o);                                        /* bsr.w Plat_Nudge */
    /* FixBugs=0: DisplaySprite lives in Plat_Action */
    DisplaySprite(o);                                     /* bsr.w DisplaySprite */
    Plat_ChkDel(o);                                       /* bra.w Plat_ChkDel */
}

/* --- Plat_Solid — Routine 2 (falls through into Plat_Action) --- */
static void Plat_Solid(uint8_t *o) {
    if (plat_nudge(o) != 0) {                             /* tst.b / beq.s */
        plat_nudge(o) = (uint8_t)(plat_nudge(o) - 4);     /* subq.b #4 */
    }
    /* .checkEnterPlatform */
    {
        int16_t d1 = (int16_t)obActWid(o);                /* moveq #0 / move.b obActWid */
        PlatformObject(o, d1);                            /* bsr.w PlatformObject */
    }
    /* Fall through to Plat_Action */
    Plat_Action(o);
}

/* --- Plat_StoodOn — Routine 4 --- */
static void Plat_StoodOn(uint8_t *o) {
    if (plat_nudge(o) != 0x40) {                          /* cmpi.b #$40 / beq.s */
        plat_nudge(o) = (uint8_t)(plat_nudge(o) + 4);     /* addq.b #4 */
    }
    /* .platformBehavior */
    {
        int16_t d1 = (int16_t)obActWid(o);
        int16_t dummy;
        ExitPlatform(o, d1, &dummy);                      /* bsr.w ExitPlatform */
    }

    int16_t saved_x = obX(o);                             /* move.w obX(a0),-(sp) */
    Plat_Move(o);                                         /* bsr.w Plat_Move */
    Plat_Nudge(o);                                        /* bsr.w Plat_Nudge */
    MvSonicOnPtfm2(o, saved_x);                           /* move.w (sp)+,d2 / bsr.w MvSonicOnPtfm2 */

    /* FixBugs=0: DisplaySprite lives in Plat_StoodOn */
    DisplaySprite(o);
    Plat_ChkDel(o);                                       /* bra.w Plat_ChkDel */
}

/* --- Plat_Main — Routine 0 --- */
static void Plat_Main(uint8_t *o) {
    obRoutine(o) += 2;                                    /* addq.b #2 -> Plat_Solid */

    obGfx(o)     = (uint16_t)(ArtTile_Level | Tile_Pal3); /* move.w #ArtTile_Level|Tile_Pal3 */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Plat_GHZ;     /* move.l #Map_Plat_GHZ */
    obActWid(o)  = 64 / 2;                                /* move.b #64/2 */

    if ((uint8_t)v_zone == id_SYZ) {                      /* cmpi.b #id_SYZ / bne */
        obMap(o)    = (uint32_t)(uintptr_t)Map_Plat_SYZ;
        obActWid(o) = 64 / 2;
    }

    if ((uint8_t)v_zone == id_SLZ) {                      /* cmpi.b #id_SLZ / bne */
        obMap(o)     = (uint32_t)(uintptr_t)Map_Plat_SLZ;
        obActWid(o)  = 64 / 2;
        obGfx(o)     = (uint16_t)(ArtTile_Level | Tile_Pal3);
        obSubtype(o) = 3;                                 /* force Plat_FallAfterStand */
    }

    obRender(o)   = sprite_cam_field;                     /* move.b #sprite_cam_field */
    obPriority(o) = 4;                                    /* move.b #4 */
    plat_rawY(o)  = obY(o);                               /* move.w obY,plat_rawY */
    plat_origY(o) = obY(o);                               /* move.w obY,plat_origY */
    plat_origX(o) = obX(o);                               /* move.w obX,plat_origX */
    obAngle(o)    = 0x00;                                 /* begin oscillating at center */

    uint8_t d1 = 0;
    uint8_t d0 = obSubtype(o);
    if (d0 == 0x0A) {                                     /* cmpi.b #$A / bne */
        d1++;                                             /* addq.b #1 */
        obActWid(o) = 64 / 2;
    }
    obFrame(o) = d1;                                      /* move.b d1,obFrame */

    /* Falls through into Plat_Solid */
    Plat_Solid(o);
}

/* --- Platform dispatcher — Plat_Index --- */
static void Platform_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                               /* moveq #0,d0 / move.b obRoutine */
        case 0: Plat_Main(o);    break;                   /* Plat_Main       */
        case 2: Plat_Solid(o);   break;                   /* Plat_Solid      */
        case 4: Plat_StoodOn(o); break;                   /* Plat_StoodOn    */
        case 6: Plat_Delete(o);  break;                   /* Plat_Delete     */
        case 8: Plat_Action(o);  break;                   /* Plat_Action     */
    }
}

/* ===========================================================================
   Object 1A — Collapsing Ledge (GHZ)
   Object 53 — Collapsing Floors (MZ, SLZ, SBZ)
   Ported from _incObj/1A, 53 Collapsing Ledges and Floors.asm (FixBugs=0).

   Fields:
     collapsible_timedelay = objoff_38 (byte): frames until fragment starts to fall
     collapsible_flag      = objoff_3A (byte): set when collapsing has started
   =========================================================================== */

#define collapsible_timedelay(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x38))  /* objoff_38 */
#define collapsible_flag(obj)      (*(uint8_t *)((uint8_t *)(obj) + 0x3A))  /* objoff_3A */

/* CollapseData: frames each fragment waits before it starts to fall.
   Index order matches the sprite piece order in the corresponding frame. */
static const uint8_t CollapseData_GHZLedge[25] = {
    0x1C, 0x18, 0x14, 0x10,
    0x1A, 0x16, 0x12, 0x0E, 0x0A, 0x06,
    0x18, 0x14, 0x10, 0x0C, 0x08, 0x04,
    0x16, 0x12, 0x0E, 0x0A, 0x06, 0x02,
    0x14, 0x10, 0x0C,
};

static const uint8_t CollapseData_8x2_Swipe[8] = {
    0x1E, 0x16, 0x0E, 0x06,
    0x1A, 0x12, 0x0A, 0x02,
};

static const uint8_t CollapseData_8x2_Shuffle[8] = {
    0x16, 0x1E, 0x1A, 0x12,
    0x06, 0x0E, 0x0A, 0x02,
};

/* GHZ collapsing ledge heightmap (48 entries):
     8 bytes flat ($20),
     15 ascending values $21..$2F each repeated twice,
     10 bytes flat ($30). */
static const uint8_t Ledge_SlopeData[48] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x24, 0x24,
    0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28,
    0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C,
    0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
};

static void Ledge_Main(uint8_t *o);
static void Ledge_ChkTouch(uint8_t *o);
static void Ledge_OnPlatform(uint8_t *o);
static void Ledge_FragmentPiece(uint8_t *o);
static void Ledge_Delete(uint8_t *o);
static void Ledge_WalkOff(uint8_t *o);
static void CFlo_Main(uint8_t *o);
static void CFlo_ChkTouch(uint8_t *o);
static void CFlo_OnPlatform(uint8_t *o);
static void CFlo_FragmentPiece(uint8_t *o);
static void CFlo_Delete(uint8_t *o);
static void CFlo_WalkOff(uint8_t *o);
static void Fragmentate_GHZLedge(uint8_t *o);
static void Fragmentate_GHZLedge_NoReset(uint8_t *o);
static void Fragmentate_8x2Floor(uint8_t *o);
static void Fragmentate_8x2Floor_NoReset(uint8_t *o);
static void FragmentatePlatform(uint8_t *o, const uint8_t *a4, int d1);
static void SlopeObject_AssumeStoodOn(uint8_t *o, int16_t d1, int16_t d2,
                                      const uint8_t *a2);

/* ---------------------------------------------------------------------------
 *  Ledge_Main — routine 0
 *  ------------------------------------------------------------------------- */
static void Ledge_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* addq.b #2 */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Ledge;           /* move.l #Map_Ledge */
    obGfx(o)     = (uint16_t)(ArtTile_Level | Tile_Pal3);    /* move.w */
    obRender(o) |= sprite_cam_field;                         /* ori.b #sprite_cam_field */
    obPriority(o)= 4;
    collapsible_timedelay(o) = 7;                            /* move.b #7 */
    /* FixBugs=0: 200/2 culling radius (FixBugs uses 96/2 instead). */
    obActWid(o)  = 200 / 2;
    obFrame(o)   = obSubtype(o);                             /* move.b obSubtype,obFrame */
    obHeight(o)  = 112 / 2;                                  /* move.b #112/2 */
    obRender(o) |= sprite_customheight;                      /* bset #sprite_customheight_bit */
}

/* ---------------------------------------------------------------------------
 *  Ledge_ChkTouch — routine 2
 *  ------------------------------------------------------------------------- */
static void Ledge_ChkTouch(uint8_t *o) {
    if (collapsible_flag(o) != 0) {                          /* tst.b / beq.s */
        if (collapsible_timedelay(o) == 0) {                 /* tst.b / beq.w */
            Fragmentate_GHZLedge(o);
            return;
        }
        collapsible_timedelay(o)--;                          /* subq.b #1 */
    }
    /* .chkTouch */
    SlopeObject(o, 96 / 2, Ledge_SlopeData);                 /* bsr.w SlopeObject */
    RememberState(o);                                        /* bra.w RememberState */
}

/* ---------------------------------------------------------------------------
 *  Ledge_OnPlatform — routine 4 (falls through to Ledge_WalkOff)
 *  ------------------------------------------------------------------------- */
static void Ledge_OnPlatform(uint8_t *o) {
    if (collapsible_timedelay(o) == 0) {                     /* tst.b / beq.w */
        Fragmentate_GHZLedge_NoReset(o);
        return;
    }
    collapsible_flag(o) = 1;                                 /* move.b #1 */
    collapsible_timedelay(o)--;                              /* subq.b #1 */
    Ledge_WalkOff(o);                                        /* (fall-through) */
}

/* ---------------------------------------------------------------------------
 *  Ledge_WalkOff — routine $A
 *  ------------------------------------------------------------------------- */
static void Ledge_WalkOff(uint8_t *o) {
    int16_t dummy;
    ExitPlatform(o, 96 / 2, &dummy);                         /* bsr.w ExitPlatform */
    SlopeObject_AssumeStoodOn(o, 96 / 2, obX(o), Ledge_SlopeData);
    RememberState(o);                                        /* bra.w RememberState */
}

/* ---------------------------------------------------------------------------
 *  Ledge_FragmentPiece — routine 6
 *  ------------------------------------------------------------------------- */
static void Ledge_FragmentPiece(uint8_t *o) {
    if (collapsible_timedelay(o) == 0) {                     /* tst.b / beq.s */
        /* .fragmentFall */
        ObjectFall(o);                                       /* bsr.w ObjectFall */
        /* FixBugs=0: DisplaySprite then out-of-range DeleteObject. */
        DisplaySprite(o);                                    /* bsr.w DisplaySprite */
        if ((int8_t)obRender(o) < 0) return;                 /* tst.b / bpl.s */
        Ledge_Delete(o);
        return;
    }
    if (collapsible_flag(o) != 0) goto delayCollapse;        /* tst.b / bne.w */
    collapsible_timedelay(o)--;                              /* subq.b #1 */
    DisplaySprite(o);                                        /* bra.w DisplaySprite */
    return;

delayCollapse:
    collapsible_timedelay(o)--;                              /* subq.b #1 */
    Ledge_WalkOff(o);                                        /* bsr.w Ledge_WalkOff */
    {
        uint8_t *a1 = RAM_ADDR(v_player);
        if (!(obStatus(a1) & (1 << 3))) goto startCollapse;  /* btst #3 / beq.s */
        if (collapsible_timedelay(o) != 0) return;           /* tst.b / bne.s */
        obStatus(a1) &= ~(1 << 3);                           /* bclr #3 */
        obStatus(a1) &= ~(1 << 5);                           /* bclr #5 */
        obPrevAni(a1) = id_Run;                              /* move.b #id_Run */
    }
startCollapse:
    collapsible_flag(o) = 0;                                 /* move.b #0 */
    obRoutine(o) = 6;                                        /* move.b #6 */
}

/* ---------------------------------------------------------------------------
 *  Ledge_Delete — routine 8
 *  ------------------------------------------------------------------------- */
static void Ledge_Delete(uint8_t *o) {
    DeleteObject(o);                                         /* bsr.w DeleteObject */
}

/* ===========================================================================
 *  Object 53 — Collapsing Floors (MZ / SLZ / SBZ)
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 *  CFlo_Main — routine 0
 *  ------------------------------------------------------------------------- */
static void CFlo_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* addq.b #2 */
    obMap(o)     = (uint32_t)(uintptr_t)Map_CFlo;            /* move.l #Map_CFlo */
    obGfx(o)     = (uint16_t)(ArtTile_MZ_Block | Tile_Pal3); /* move.w */

    if ((uint8_t)v_zone == id_SLZ) {                         /* cmpi.b #id_SLZ / bne.s */
        obGfx(o) = (uint16_t)(ArtTile_SLZ_Collapsing_Floor | Tile_Pal3);
        obFrame(o) += 2;                                     /* addq.b #2 */
    }
    if ((uint8_t)v_zone == id_SBZ) {                         /* cmpi.b #id_SBZ / bne.s */
        obGfx(o) = (uint16_t)(ArtTile_SBZ_Collapsing_Floor | Tile_Pal3);
    }
    obRender(o) |= sprite_cam_field;                         /* ori.b #sprite_cam_field */
    obPriority(o)= 4;
    collapsible_timedelay(o) = 7;                            /* move.b #7 */
    obActWid(o)  = 136 / 2;                                  /* move.b #136/2 */
}

/* ---------------------------------------------------------------------------
 *  CFlo_ChkTouch — routine 2
 *  ------------------------------------------------------------------------- */
static void CFlo_ChkTouch(uint8_t *o) {
    if (collapsible_flag(o) != 0) {                          /* tst.b / beq.s */
        if (collapsible_timedelay(o) == 0) {                 /* tst.b / beq.w */
            Fragmentate_8x2Floor(o);
            return;
        }
        collapsible_timedelay(o)--;                          /* subq.b #1 */
    }
    /* .solid */
    PlatformObject(o, 64 / 2);                               /* bsr.w PlatformObject */

    /* SLZ-specific flip: if subtype MSB is set, mirror the collapse pattern
       depending on which side Sonic touched. */
    if ((int8_t)obSubtype(o) < 0) {                          /* tst.b / bpl.s */
        uint8_t *a1 = RAM_ADDR(v_player);
        if (obStatus(a1) & (1 << 3)) {                       /* btst #3 / beq.s */
            obRender(o) &= ~sprite_xflip;                    /* bclr #sprite_xflip_bit */
            if ((uint16_t)obX(a1) < (uint16_t)obX(o)) {      /* sub.w / bcc.s */
                obRender(o) |= sprite_xflip;                 /* bset #sprite_xflip_bit */
            }
        }
    }
    /* .display */
    RememberState(o);                                        /* bra.w RememberState */
}

/* ---------------------------------------------------------------------------
 *  CFlo_OnPlatform — routine 4 (falls through to CFlo_WalkOff)
 *  ------------------------------------------------------------------------- */
static void CFlo_OnPlatform(uint8_t *o) {
    if (collapsible_timedelay(o) == 0) {                     /* tst.b / beq.w */
        Fragmentate_8x2Floor_NoReset(o);
        return;
    }
    collapsible_flag(o) = 1;                                 /* move.b #1 */
    collapsible_timedelay(o)--;                              /* subq.b #1 */
    CFlo_WalkOff(o);                                         /* (fall-through) */
}

/* ---------------------------------------------------------------------------
 *  CFlo_WalkOff — routine $A
 *  ------------------------------------------------------------------------- */
static void CFlo_WalkOff(uint8_t *o) {
    int16_t dummy;
    ExitPlatform(o, 64 / 2, &dummy);                         /* bsr.w ExitPlatform */
    MvSonicOnPtfm2(o, obX(o));                               /* bsr.w MvSonicOnPtfm2 */
    RememberState(o);                                        /* bra.w RememberState */
}

/* ---------------------------------------------------------------------------
 *  CFlo_FragmentPiece — routine 6
 *  ------------------------------------------------------------------------- */
static void CFlo_FragmentPiece(uint8_t *o) {
    if (collapsible_timedelay(o) == 0) {                     /* tst.b / beq.s */
        /* .fragmentFall */
        ObjectFall(o);                                       /* bsr.w ObjectFall */
        DisplaySprite(o);                                    /* bsr.w DisplaySprite */
        if ((int8_t)obRender(o) < 0) return;                 /* tst.b / bpl.s */
        CFlo_Delete(o);
        return;
    }
    if (collapsible_flag(o) != 0) goto delayCollapse;        /* tst.b / bne.w */
    collapsible_timedelay(o)--;                              /* subq.b #1 */
    DisplaySprite(o);                                        /* bra.w DisplaySprite */
    return;

delayCollapse:
    collapsible_timedelay(o)--;                              /* subq.b #1 */
    CFlo_WalkOff(o);                                         /* bsr.w CFlo_WalkOff */
    {
        uint8_t *a1 = RAM_ADDR(v_player);
        if (!(obStatus(a1) & (1 << 3))) goto startCollapse;  /* btst #3 / beq.s */
        if (collapsible_timedelay(o) != 0) return;           /* tst.b / bne.s */
        obStatus(a1) &= ~(1 << 3);
        obStatus(a1) &= ~(1 << 5);
        obPrevAni(a1) = id_Run;
    }
startCollapse:
    collapsible_flag(o) = 0;
    obRoutine(o) = 6;
}

/* ---------------------------------------------------------------------------
 *  CFlo_Delete — routine 8
 *  ------------------------------------------------------------------------- */
static void CFlo_Delete(uint8_t *o) {
    DeleteObject(o);                                         /* bsr.w DeleteObject */
}

/* ===========================================================================
 *  Fragmentate subroutines
 * =========================================================================== */

static void Fragmentate_GHZLedge(uint8_t *o) {
    collapsible_flag(o) = 0;                                 /* move.b #0 */
    Fragmentate_GHZLedge_NoReset(o);
}

static void Fragmentate_GHZLedge_NoReset(uint8_t *o) {
    obFrame(o) += 2;                                         /* addq.b #2 */
    FragmentatePlatform(o, CollapseData_GHZLedge, 25 - 1);   /* moveq #25-1,d1 */
}

static void Fragmentate_8x2Floor(uint8_t *o) {
    collapsible_flag(o) = 0;                                 /* move.b #0 */
    Fragmentate_8x2Floor_NoReset(o);
}

static void Fragmentate_8x2Floor_NoReset(uint8_t *o) {
    const uint8_t *a4 = (obSubtype(o) & 1)                   /* btst #0 / beq.s */
        ? CollapseData_8x2_Shuffle
        : CollapseData_8x2_Swipe;
    obFrame(o) += 1;                                         /* addq.b #1 */
    FragmentatePlatform(o, a4, 8 - 1);                       /* moveq #8-1,d1 */
}

/* FragmentatePlatform — spawn one fragment per sprite piece in the current
   frame. The first fragment reuses the parent object (a1 = a0); the rest
   allocate fresh slots via FindFreeObj. */
static void FragmentatePlatform(uint8_t *o, const uint8_t *a4, int d1) {
    /* .setupFrag: read mapping pointer, jump to current frame's sprite list. */
    uint8_t frame = obFrame(o);
    uint8_t *a3 = (uint8_t *)(uintptr_t)obMap(o);
    uint16_t offset = ((const uint16_t *)a3)[frame];         /* adda.w (a3,d0.w) */
    a3 = a3 + offset + 1;                                    /* addq.w #1: skip piece count */

    obRender(o) |= sprite_rawmappings;                       /* bset #sprite_rawmappings_bit */
    uint8_t d4 = obID(o);                                    /* copy ID to fragments */
    uint8_t d5 = obRender(o);                                /* copy render flags */
    uint8_t *a1 = o;                                         /* first fragment = parent */

    for (;;) {
        /* .firstFragment: initialize fragment object */
        obRoutine(a1)     = 6;
        obID(a1)          = d4;
        obMap(a1)         = (uint32_t)(uintptr_t)a3;
        obRender(a1)      = d5;
        obX(a1)           = obX(o);
        obY(a1)           = obY(o);
        obGfx(a1)         = obGfx(o);
        obPriority(a1)    = obPriority(o);
        obActWid(a1)      = obActWid(o);
        collapsible_timedelay(a1) = *a4++;                   /* move.b (a4)+,delay */

        /* FixBugs=0: fragments loaded before the parent must be displayed
           on this frame too (DisplaySprite2 in the ASM). */
        if ((uintptr_t)a1 < (uintptr_t)o) {                  /* cmpa.l a0,a1 / bhs */
            DisplaySprite(a1);
        }

        d1--;                                                /* dbf */
        if (d1 == -1) break;

        /* .loopFragments: allocate next fragment */
        uint8_t *slot = (uint8_t *)FindFreeObj();            /* bsr.w FindFreeObj */
        if (slot == NULL) break;                             /* bne.s .fragmentationDone */
        a3 += 5;                                             /* addq.w #5: next piece */
        a1 = slot;
    }

    /* .fragmentationDone */
    DisplaySprite(o);                                        /* bsr.w DisplaySprite */
    Sound_Queue(sfx_Collapse, false);                        /* jmp QueueSound2 */
}

/* ===========================================================================
 *  SlopeObject_AssumeStoodOn
 *  Aligns Sonic to the ledge's sloped surface assuming he is already standing
 *  on it (skips the usual x/y checks SlopeObject performs).
 *
 *  Inputs:
 *    d1 = platform half width
 *    d2 = platform X-position
 *    a2 = heightmap data
 * =========================================================================== */
static void SlopeObject_AssumeStoodOn(uint8_t *o, int16_t d1, int16_t d2,
                                      const uint8_t *a2) {
    uint8_t *a1 = RAM_ADDR(v_player);
    if (!(obStatus(a1) & (1 << 3))) return;                  /* btst #3 / beq.s .return */

    /* d0 = (Sonic_X - Ledge_X + half_width) >> 1, or mirrored if X-flipped */
    int16_t diff = (int16_t)((int)(obX(a1) - obX(o)) + d1);
    uint16_t d0 = (uint16_t)diff;
    d0 = (uint16_t)(d0 >> 1);                                /* lsr.w #1,d0 */
    if (obRender(o) & sprite_xflip) {                        /* btst #sprite_xflip_bit / beq.s */
        d0 = (uint16_t)((uint16_t)~d0 + (uint16_t)d1);       /* not.w d0 ; add.w d1,d0 */
    }
    /* .alignSonic */
    int16_t slope  = a2[d0];                                 /* moveq #0,d1 ; move.b (a2,d0.w),d1 */
    int16_t y      = (int16_t)(obY(o) - slope);              /* move.w obY(a0),d0 ; sub.w d1,d0 */
    int16_t height = (int16_t)obHeight(a1);                  /* moveq #0,d1 ; move.b obHeight(a1),d1 */
    y = (int16_t)(y - height);                               /* sub.w d1,d0 */
    obY(a1) = y;                                             /* move.w d0,obY(a1) */
    int16_t dx = (int16_t)(d2 - obX(o));                     /* sub.w obX(a0),d2 */
    obX(a1) = (int16_t)(obX(a1) - dx);                       /* sub.w d2,obX(a1) */
}

/* ===========================================================================
 *  Dispatch
 * =========================================================================== */

static void CollapseLedge_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                                  /* Ledge_Index: 0/2/4/6/8/$A */
        case 0x00: Ledge_Main(o);           break;
        case 0x02: Ledge_ChkTouch(o);       break;
        case 0x04: Ledge_OnPlatform(o);     break;
        case 0x06: Ledge_FragmentPiece(o);  break;
        case 0x08: Ledge_Delete(o);         break;
        case 0x0A: Ledge_WalkOff(o);        break;
    }
}

static void CollapseFloor_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                                  /* CFlo_Index: 0/2/4/6/8/$A */
        case 0x00: CFlo_Main(o);            break;
        case 0x02: CFlo_ChkTouch(o);        break;
        case 0x04: CFlo_OnPlatform(o);      break;
        case 0x06: CFlo_FragmentPiece(o);   break;
        case 0x08: CFlo_Delete(o);          break;
        case 0x0A: CFlo_WalkOff(o);         break;
    }
}

/* ===========================================================================
 *  Object 1C — Scenery (GHZ bridge stump, SLZ lava thrower)
 *  Ported verbatim from _incObj/1C GHZ, SYZ Scenery.asm (REV01, FixBugs=0).
 *
 *  Scen_Values entry layout in ROM (10 bytes = $A per entry):
 *      dc.l  map          (4 bytes)
 *      dc.w  ArtTile|Pal  (2 bytes)
 *      dc.b  frame        (1 byte)
 *      dc.b  actwid       (1 byte)
 *      dc.b  priority     (1 byte)
 *      dc.b  coltype      (1 byte)
 *
 *  The original table has the SLZ lava thrower defined three times (only
 *  the first is used in-game), then the GHZ bridge stump as subtype 3.
 *  Because Map_Scen/Map_Bri are runtime pointer variables in this port
 *  (extern const uint8_t *), they cannot appear in a static initializer,
 *  so the table lookup is inlined in Scen_Main (functionally identical).
 *  =========================================================================== */

static void Scen_Main(uint8_t *o);
static void Scen_ChkDel(uint8_t *o);

/* Scen_Main — Routine 0
 *  Setup from Scen_Values[obSubtype] and advance straight to Scen_ChkDel. */
static void Scen_Main(uint8_t *o) {
    obRoutine(o) += 2;                       /* addq.b #2,obRoutine(a0) */

    uint8_t subtype = obSubtype(o);          /* moveq #0,d0 / move.b obSubtype,d0 */

    /* Scen_Values lookup, inlined. Subtypes 0,1,2 are the SLZ lava thrower
     *      (three identical ROM entries; only subtype 0 is actually placed).
     *      Subtype 3 is the GHZ bridge stump. */
    if (subtype >= 3) {
        /* GHZ bridge stump */
        obMap(o)      = (uint32_t)(uintptr_t)Map_Bri;
        obGfx(o)      = (uint16_t)(ArtTile_GHZ_Bridge | Tile_Pal3);
        obRender(o)  |= sprite_cam_field;    /* ori.b #sprite_cam_field,obRender(a0) */
        obFrame(o)    = 1;
        obActWid(o)   = 32 / 2;
        obPriority(o) = 1;
        obColType(o)  = col_none;
    } else {
        /* SLZ lava thrower (entries 0,1,2 in Scen_Values are identical) */
        obMap(o)      = (uint32_t)(uintptr_t)Map_Scen;
        obGfx(o)      = (uint16_t)(ArtTile_SLZ_Fireball_Launcher | Tile_Pal3);
        obRender(o)  |= sprite_cam_field;    /* ori.b #sprite_cam_field,obRender(a0) */
        obFrame(o)    = 0;
        obActWid(o)   = 16 / 2;
        obPriority(o) = 2;
        obColType(o)  = col_none;
    }

    /* Fall through to Scen_ChkDel for the first frame (matches ASM). */
    Scen_ChkDel(o);
}

/* Scen_ChkDel — Routine 2
 *  out_of_range.w DeleteObject ; bra.w DisplaySprite */
static void Scen_ChkDel(uint8_t *o) {
    if (OutOfRange(o, -1, 0)) {                    /* out_of_range.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                        /* bra.w DisplaySprite */
}

/* Scenery dispatcher — Scen_Index: 0 = Main, 2 = ChkDel */
static void Scenery_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                  /* moveq #0,d0 / move.b obRoutine,d0 / move.w Scen_Index */
        case 0: Scen_Main(o);   break;
        case 2: Scen_ChkDel(o); break;
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
    if (OutOfRange(o, ring_origX(o), 0)) {
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
        a2[2 + d0] &= ~0x80;                     /* bclr #7,2(a2,d0.w) — FixBugs=0 */
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

//sonicright:
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
        if (OutOfRange(o, -1, 0)) {                    /* out_of_range.w DeleteObject */
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

            //mon2_sonicright:
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
                if (OutOfRange(o, -1, 0)) {
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
            if (OutOfRange(o, -1, 0)) {
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
                a1 += 1;                         /* addq.w #1,a1 */
                obMap(o) = (uint32_t)(uintptr_t)a1; /* move.l a1,obMap */
            }
            /* falls through to Pow_Move */
            
        }
        /* fall through */
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
        if (d0 == 3) {
            v_shoes = 1;
            shoetime(RAM_ADDR(v_player)) = 20 * 60;   /* era: RAM_WORD(v_player + 0x2A) */

            v_sonspeedmax = son_maxspeed * 2;
            v_sonspeedacc = son_acceleration * 2;
            v_sonspeeddec = son_deceleration;
            Sound_Queue(bgm_Speedup, false);
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
        if (d0 == 5) {
            v_invinc = 1;
            invtime(RAM_ADDR(v_player)) = 20 * 60;    /* era: RAM_WORD(v_player + 0x30) */

            RAM_BYTE(v_starsobj1)        = id_ShieldItem;
            obAnim(RAM_ADDR(v_starsobj1)) = 1;
            RAM_BYTE(v_starsobj2)        = id_ShieldItem;
            obAnim(RAM_ADDR(v_starsobj2)) = 2;
            RAM_BYTE(v_starsobj3)        = id_ShieldItem;
            obAnim(RAM_ADDR(v_starsobj3)) = 3;
            RAM_BYTE(v_starsobj4)        = id_ShieldItem;
            obAnim(RAM_ADDR(v_starsobj4)) = 4;

            if (f_lockscreen) goto pow_display;
            if (v_air <= 12)  goto pow_display;
            Sound_Queue(bgm_Invincible, true);
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
 *  Object 2B — Chopper enemy (GHZ, also used in SYZ)
 *  Ported verbatim from _incObj/2B Badnik - Chopper.asm (REV01, FixBugs=0).
 *
 *  chop_origY = objoff_30 (word): Y-position the Chopper was spawned at.
 *  Chopper launches upward at -$700, gravity (+$18/frame) slows it down,
 *  pulls it back to origY, then relaunches. The animation depends on
 *  how high up the Chopper currently is:
 *    - above origY - $C0 → fast  (obAnim = 1)
 *    - below that and rising   → slow  (obAnim = 0)
 *    - below that and falling  → idle  (obAnim = 2)
 *  =========================================================================== */

#define chop_origY(obj) (*(int16_t *)((uint8_t *)(obj) + 0x30)) /* objoff_30 */

static void Chop_Main(uint8_t *o);
static void Chop_ChgSpeed(uint8_t *o);

/* Chop_Main — Routine 0: initialize. */
static void Chop_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* addq.b #2 → Chop_ChgSpeed */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Chop;           /* move.l #Map_Chop,obMap */
    obGfx(o)      = (uint16_t)ArtTile_Chopper;               /* move.w #ArtTile_Chopper,obGfx */
    obRender(o)   = sprite_cam_field;                        /* move.b #sprite_cam_field,obRender */
    obPriority(o) = 4;                                       /* move.b #4,obPriority */
    obColType(o)  = (uint8_t)(col_24x32 | col_badnik);       /* move.b #col_24x32|col_badnik,obColType */
    obActWid(o)   = 32 / 2;                                  /* move.b #32/2,obActWid */
    obVelY(o)     = (int16_t)-0x700;                         /* move.w #-$700,obVelY */
    chop_origY(o) = obY(o);                                  /* move.w obY,chop_origY */
}

/* Chop_ChgSpeed — Routine 2: bob up and down, pick animation. */
static void Chop_ChgSpeed(uint8_t *o) {
    if (Ani_Chop) AnimateSprite(o, Ani_Chop);                /* lea Ani_Chop / bsr.w AnimateSprite */

        SpeedToPos(o);                                           /* bsr.w SpeedToPos */
        obVelY(o) = (int16_t)(obVelY(o) + 0x18);                 /* addi.w #$18,obVelY */

        /* Regresar a origY cuando el Chopper ya cayó por debajo.
         *      El ASM usa `bhs` (unsigned >=): se toma el salto a .chganimation
         *      cuando d0 >= obY; el cuerpo de "reset" corre sólo si d0 < obY. */
        int16_t d0 = chop_origY(o);
    if ((uint16_t)d0 < (uint16_t)obY(o)) {
        obY(o)    = d0;                                      /* move.w d0,obY */
        obVelY(o) = (int16_t)-0x700;                         /* move.w #-$700,obVelY */
    }

    /* .chganimation: elegir animación según altura y sentido de movimiento. */
    obAnim(o) = 1;                                           /* move.b #1,obAnim (fast) */
    d0 = (int16_t)(chop_origY(o) - 0xC0);                    /* subi.w #$C0,d0 */
    if ((uint16_t)d0 >= (uint16_t)obY(o)) return;            /* bhs.s .return */

        obAnim(o) = 0;                                           /* move.b #0,obAnim (slow) */
        if ((int16_t)obVelY(o) < 0) return;                      /* tst.w obVelY / bmi.s .return */
            obAnim(o) = 2;                                           /* move.b #2,obAnim (stationary) */
}

/* Chopper dispatcher — Chop_Index: 0 = Main, 2 = ChgSpeed.
 *  El ASM hace `jsr Chop_Index` y luego `bra.w RememberState`:
 *  la rutina se ejecuta y SIEMPRE se llama a RememberState después. */
static void Chopper_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0: Chop_Main(o);     break;
        case 2: Chop_ChgSpeed(o); break;
    }
    RememberState(obj);
}

/* ====================================================================================== */
/* Object 2F — Large Grass */
#define lgrass_origX(o)      (*(int16_t *)((uint8_t *)(o) + 0x2A))
#define lgrass_origY(o)      (*(int16_t *)((uint8_t *)(o) + 0x2C))

#define lgrass_nudge(o)      (*(uint8_t  *)((uint8_t *)(o) + 0x34))
#define lgrass_burning(o)    (*(uint8_t  *)((uint8_t *)(o) + 0x35))
#define lgrass_flame_list(o) ((uint8_t  *)((uint8_t *)(o) + 0x36))

/* Object 35 — Grass Fire */
#define gfire_origX(o)       (*(int16_t *)((uint8_t *)(o) + 0x2A))
#define gfire_origY(o)       (*(int16_t *)((uint8_t *)(o) + 0x2C))
#define gfire_coldata(o)     (*(const uint8_t **)((uint8_t *)(o) + 0x30))
#define gfire_platform(o)    (*(uint32_t *)((uint8_t *)(o) + 0x38))
#define gfire_nudge(o)       (*(int16_t *)((uint8_t *)(o) + 0x3C))
/* --- LGrass_Data_Symmetrical (frame 0): _/ * \_ ---
   14 flat + 15 up + 18 flat + 15 down + 14 flat = 76 bytes */
static const uint8_t LGrass_Data_Symmetrical[76] = {
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,
    0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
    0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
    0x30,0x30,
    0x2F,0x2E,0x2D,0x2C,0x2B,0x2A,0x29,0x28,
    0x27,0x26,0x25,0x24,0x23,0x22,0x21,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,
};

/* --- LGrass_Data_Column (frame 2): |**| --- 44 bytes planos */
static const uint8_t LGrass_Data_Column[44] = {
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
    0x30,0x30,0x30,0x30,
};

/* --- LGrass_Data_Asymmetrical (frame 1): 
   6 flat + 31 up + 18 flat + 15 down + 6 flat = 76 bytes */
static const uint8_t LGrass_Data_Asymmetrical[76] = {
    0x20,0x20,0x20,0x20,0x20,0x20,
    0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
    0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,
    0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
    0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
    0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
    0x40,0x40,
    0x3F,0x3E,0x3D,0x3C,0x3B,0x3A,0x39,0x38,
    0x37,0x36,0x35,0x34,0x33,0x32,0x31,
    0x30,0x30,0x30,0x30,0x30,0x30,
};

/* Tabla de setup: (heightmap, frame, actwid) por índice = subtype >> 4 */
typedef struct {
    const uint8_t *heightmap;
    uint8_t frame;
    uint8_t actwid;
} LGrassEntry;

static const LGrassEntry LGrass_Data[3] = {
    { LGrass_Data_Symmetrical, 0, 128/2 },   /* $0x */
    { LGrass_Data_Asymmetrical, 1, 128/2 },  /* $1x */
    { LGrass_Data_Column,       2, 64/2  },  /* $2x */
};

static const uint8_t *LGrass_HeightmapFor(uint8_t *platform) {
    switch (obFrame(platform)) {
        case 0: return LGrass_Data_Symmetrical;
        case 1: return LGrass_Data_Asymmetrical;
        case 2: return LGrass_Data_Column;
    }
    return LGrass_Data_Symmetrical;
}

/* Añade el índice del hijo (fire) a la lista de hijos del padre (platform). */
static void LGrass_AddChildToList(uint8_t *child, uint8_t *platform) {
    uint8_t *list = lgrass_flame_list(platform);
    uint8_t count = list[0];
    list[0] = (uint8_t)(count + 1);
    list[1 + count] = (uint8_t)Object_GetIndex(child);
}

static void LGrass_Main(uint8_t *o);
static void LGrass_Action(uint8_t *o);
static void LGrass_Stationary(uint8_t *o);
static void LGrass_Burnable(uint8_t *o);
static void LGrass_Display(uint8_t *o);
static void LGrass_ChkDel(uint8_t *o);
static void LGrass_DelFlames(uint8_t *o);



/* --- LGrass_Main — Routine 0 --- */
static void LGrass_Main(uint8_t *o) {
    obRoutine(o) += 2;
    obMap(o)    = (uint32_t)(uintptr_t)Map_LGrass;
    obGfx(o)    = (uint16_t)(ArtTile_Level | Tile_Pal3 | Tile_Prio);
    obRender(o) = sprite_cam_field;
    obPriority(o) = 5;
    lgrass_origY(o) = obY(o);
    lgrass_origX(o) = obX(o);

    int idx = (int)(obSubtype(o) >> 4);
    if (idx > 2) idx = 2;
    const LGrassEntry *data = &LGrass_Data[idx];

    obFrame(o)  = data->frame;
    obActWid(o) = data->actwid;




    obSubtype(o) &= 0x0F;
    obHeight(o) = 128 / 2;
    obRender(o) |= sprite_customheight;

    LGrass_Action(o);
}

/* --- LGrass_Action — Routine 2 --- */
static void LGrass_Action(uint8_t *o) {
    switch (obSubtype(o) & 7) {
        case 0: LGrass_Stationary(o); break;
        case 1: {
            int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 2);
            int16_t d1 = 0x10 * 2;
            if (obSubtype(o) & (1 << 3)) { d0 = (int16_t)(-d0); d0 = (int16_t)(d0 + d1); }
            obY(o) = (int16_t)(lgrass_origY(o) - d0);
            break;
        }
        case 2: {
            int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 6);
            int16_t d1 = 0x18 * 2;
            if (obSubtype(o) & (1 << 3)) { d0 = (int16_t)(-d0); d0 = (int16_t)(d0 + d1); }
            obY(o) = (int16_t)(lgrass_origY(o) - d0);
            break;
        }
        case 3: {
            int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x0A);
            int16_t d1 = 0x20 * 2;
            if (obSubtype(o) & (1 << 3)) { d0 = (int16_t)(-d0); d0 = (int16_t)(d0 + d1); }
            obY(o) = (int16_t)(lgrass_origY(o) - d0);
            break;
        }
        case 4: {
            int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x0E);
            int16_t d1 = 0x30 * 2;
            if (obSubtype(o) & (1 << 3)) { d0 = (int16_t)(-d0); d0 = (int16_t)(d0 + d1); }
            obY(o) = (int16_t)(lgrass_origY(o) - d0);
            break;
        }
        case 5: LGrass_Burnable(o); break;
    }

    if (obSolid(o) != 0) {
        int16_t d1 = (int16_t)((int8_t)obActWid(o) + sonic_solid_width);
        int16_t dummy;
        ExitPlatform(o, d1, &dummy);

        uint8_t *player = RAM_ADDR(v_player);
        if (obStatus(player) & (1 << 3)) {
            SlopeObject_AssumeStoodOn(o, d1, obX(o), LGrass_HeightmapFor(o)); /* <-- aquí */
        } else {
            obSolid(o) = 0;
        }
    } else {
        int16_t d1 = (int16_t)((int8_t)obActWid(o) + sonic_solid_width);
        int16_t d2 = (obFrame(o) == 2) ? (96 / 2) : (64 / 2);
        SolidObject_Heightmap(o, d1, d2, LGrass_HeightmapFor(o));             /* <-- aquí */
    }

    LGrass_Display(o);
}

/* --- LGrass_Stationary (tipo 0) --- */
static void LGrass_Stationary(uint8_t *o) { (void)o; }

/* --- Tipo 5: plataforma que se quema --- */
static void LGrass_Burnable(uint8_t *o) {
    int16_t d0 = (int16_t)lgrass_nudge(o);

    if (obSolid(o) != 0) {
        d0 = (int16_t)((uint8_t)d0 + 4);
        if (d0 >= 0x40) d0 = 0x40;
    } else {
        if (d0 < 2) d0 = 0;
        else        d0 = (int16_t)(d0 - 2);
    }
    lgrass_nudge(o) = (uint8_t)d0;

    int16_t s0, s1;
    CalcSine((int)(uint8_t)d0, &s0, &s1);
    int16_t y_off = (int16_t)((uint16_t)s0 >> 4);
    obY(o) = (int16_t)(y_off + lgrass_origY(o));

    if (lgrass_nudge(o) == 0x20 && lgrass_burning(o) == 0) {
        lgrass_burning(o) = 1;

        uint8_t *fire = (uint8_t *)FindNextFreeObj(o);
        if (fire) {
            obID(fire) = id_GrassFire;
            obX(fire)  = (int16_t)(obX(o) - 128/2);
            gfire_origY(fire) = (int16_t)(lgrass_origY(o) + 8 - 3);
            gfire_coldata(fire) = LGrass_HeightmapFor(o);   /* <-- aquí */
            gfire_platform(fire) = (uint32_t)Object_GetIndex(o);
            LGrass_AddChildToList(fire, o);
        }
    }

    {
        uint8_t *list = lgrass_flame_list(o);
        uint8_t count = list[0];
        for (int i = 0; i < count; i++) {
            uint8_t idx = list[1 + i];
            uint8_t *fire = (uint8_t *)Object_GetSlot((int)idx);
            gfire_nudge(fire) = y_off;
        }
    }
}

/* --- LGrass_Display --- */
static void LGrass_Display(uint8_t *o) {
    DisplaySprite(o);                            /* FixBugs=0: display aquí */
    LGrass_ChkDel(o);
}

/* --- LGrass_ChkDel --- */
static void LGrass_ChkDel(uint8_t *o) {
    if (lgrass_burning(o) != 0 && (int8_t)obRender(o) >= 0) {
        /* Plataforma quemándose y fuera de pantalla: borrar fuegos */
        LGrass_DelFlames(o);
        return;
    }
    if (OutOfRange(o, lgrass_origX(o), 0)) {
        DeleteObject(o);
    }
    /* FixBugs=0: rts */
}

/* --- LGrass_DelFlames --- */
static void LGrass_DelFlames(uint8_t *o) {
    uint8_t *list = lgrass_flame_list(o);
    uint8_t count = list[0];
    list[0] = 0;                                 /* clr.b (a2)+ */

    for (int i = 0; i < count; i++) {
        uint8_t idx = list[1 + i];
        list[1 + i] = 0;
        uint8_t *fire = (uint8_t *)Object_GetSlot((int)idx);
        DeleteObject(fire);                      /* DeleteChild = DeleteObject */
    }
    lgrass_burning(o) = 0;
    lgrass_nudge(o)   = 0;
    /* FixBugs=0: rts */
}

static void LargeGrass_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: LGrass_Main(o);   break;
        case 2: LGrass_Action(o); break;
    }
}

static void GFire_Main(uint8_t *o);
static void GFire_Spread(uint8_t *o);
static void GFire_Move(uint8_t *o);
static void GFire_Animate(uint8_t *o);

static void GFire_Main(uint8_t *o) {
    obRoutine(o) += 2;                                  /* → Spread */
    obMap(o)    = (uint32_t)(uintptr_t)Map_Fire;
    obGfx(o)    = (uint16_t)(ArtTile_MZ_Fireball| Tile_Prio);
    gfire_origX(o) = obX(o);
    obRender(o) = sprite_cam_field;
    obPriority(o) = 1;
    obColType(o) = (uint8_t)(col_16x16 | col_hurt);
    obActWid(o) = 16 / 2;

    Sound_Queue(sfx_Burning, false);

    if (obSubtype(o) == 0) {
        GFire_Spread(o);                                /* fall-through a Spread */
    } else {
        obRoutine(o) += 2;                              /* → Move */
        GFire_Move(o);
    }
}

static void GFire_Spread(uint8_t *o) {
    const uint8_t *a1 = gfire_coldata(o);
    int16_t d1 = (int16_t)(obX(o) - gfire_origX(o));
    d1 = (int16_t)(d1 + 12);
    int16_t d0 = (int16_t)(d1 >> 1);
    int16_t slope = a1[(uint16_t)d0];
    d0 = (int16_t)(-slope);
    d0 = (int16_t)(d0 + gfire_origY(o));
    int16_t d2 = d0;                                    /* para potencial spawn */
    d0 = (int16_t)(d0 + gfire_nudge(o));
    obY(o) = d0;

    if ((uint16_t)d1 >= 132) goto animate;              /* cmpi.w #132 / bhs.s */

        /* Avanzar X a 1px/frame (16.16 fixed) */
        {
            int32_t x = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
            x += 0x10000;
            obX(o) = (int16_t)(x >> 16);
            obSubpixelX(o) = (int16_t)(x & 0xFFFF);
        }

        if ((uint16_t)d1 >= 128) goto animate;

        /* Comprobar si llegó a múltiplo de 16px para spawnear hijo */
        {
            int32_t x = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
            x += 0x80000;
            if ((x & 0xFFFFF) != 0) goto animate;
        }

        {
            uint8_t *fire = (uint8_t *)FindNextFreeObj(o);
            if (!fire) goto animate;
            obID(fire) = id_GrassFire;
            obX(fire) = obX(o);
            gfire_origY(fire) = d2;
            gfire_nudge(fire) = gfire_nudge(o);
            obSubtype(fire) = 1;
            uint8_t *platform = (uint8_t *)Object_GetSlot((int)gfire_platform(o));
            LGrass_AddChildToList(fire, platform);
        }

        animate:
        GFire_Animate(o);
}

static void GFire_Move(uint8_t *o) {
    int16_t d0 = gfire_origY(o);
    d0 = (int16_t)(d0 + gfire_nudge(o));
    obY(o) = d0;
    GFire_Animate(o);
}

static void GFire_Animate(uint8_t *o) {
    if (Ani_GFire) AnimateSprite(o, Ani_GFire);
    DisplaySprite(o);
}

static void GrassFire_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: GFire_Main(o);   break;
        case 2: GFire_Spread(o); break;
        case 4: GFire_Move(o);   break;
    }
}


/* ===========================================================================
   Part 3  Object implementations, $30-$5F
   =========================================================================== */

/* ===========================================================================
 *  Object 30 — Large Green Glass Pillars (MZ)
 *  Ported from _incObj/30 MZ Large Green Glass Blocks.asm (REV01, FixBugs=0).
 *
 *  Cada pilar spawna DOS objetos en slot adyacente: el pilar (routines 2/6)
 *  y un "sheen" (reflejo, routines 4/8). El bit 3 del subtype distingue
 *  sheen (1) de pilar (0). El tipo es subtype & 7:
 *    0 = estático
 *    1 = oscila arriba/abajo (empieza arriba)
 *    2 = oscila arriba/abajo (opuesto a 1)
 *    3 = stomp (baja al pisarlo varias veces; prototipo sin terminar)
 *    4 = switch (baja cuando se pulsa el switch correspondiente)
 *
 *  Campos:
 *    glass_origY          = objoff_30 (word): Y inicial del objeto
 *    glass_distanceY      = objoff_32 (word): píxeles restantes de bajada
 *    glass_stomp_flags    = objoff_34 (byte): bit0 = Sonic encima, bit7 = bajando
 *    glass_switch_flag    = objoff_34 (byte): 1 = switch pulsado (MISMO byte que stomp)
 *    glass_stomp_first    = objoff_35 (byte): set tras el primer aterrizaje
 *    glass_stomp_distance = objoff_36 (word): píxeles restantes del stomp actual
 *    glass_stomp_delay    = objoff_38 (byte): frames de espera antes de bajar
 *    glass_parent         = objoff_3C (long): índice de slot del padre
 * =========================================================================== */

#define glass_origY(o)          (*(int16_t *)((uint8_t *)(o) + 0x30))
#define glass_distanceY(o)      (*(int16_t *)((uint8_t *)(o) + 0x32))
#define glass_stomp_flags(o)    (*(uint8_t  *)((uint8_t *)(o) + 0x34))
#define glass_switch_flag(o)    (*(uint8_t  *)((uint8_t *)(o) + 0x34))
#define glass_stomp_first(o)    (*(uint8_t  *)((uint8_t *)(o) + 0x35))
#define glass_stomp_distance(o) (*(int16_t *)((uint8_t *)(o) + 0x36))
#define glass_stomp_delay(o)    (*(uint8_t  *)((uint8_t *)(o) + 0x38))
#define glass_parent(o)         (*(uint32_t *)((uint8_t *)(o) + 0x3C))

/* Glass_Vars1: pilar alto (subtypes 0/1/2) — { routine, ydist(unused), frame } */
static const uint8_t Glass_Vars1[2][3] = {
    { 2, 0, 0 },   /* pilar */
    { 4, 0, 1 },   /* sheen */
};
/* Glass_Vars2: pilar bajo (subtypes 3+) */
static const uint8_t Glass_Vars2[2][3] = {
    { 6, 0, 2 },   /* pilar */
    { 8, 0, 1 },   /* sheen */
};
/* --- Glass_Main — routine 0: spawn pilar + sheen --- */
static void Glass_Main(uint8_t *o) {
    const uint8_t (*a2)[3] = Glass_Vars1;
    obHeight(o) = 144 / 2;

    if (obSubtype(o) >= 3) {           /* cmpi.b #3 / blo.s .IsType012 */
        a2 = Glass_Vars2;
        obHeight(o) = 112 / 2;
    }
    /* .IsType012 */

    uint8_t *a1 = o;                   /* primera iteración usa el propio slot */
    for (int i = 0; i < 2; i++) {      /* dbf d1 con d1 = 2-1 */
        if (i > 0) {
            a1 = (uint8_t *)FindNextFreeObj(a1);
            if (!a1) goto finalize;    /* bne.s .finalizePillar */
        }
        /* .makePillar */
        obRoutine(a1)   = a2[i][0];
        obID(a1)        = id_GlassBlock;
        obX(a1)         = obX(o);
        /* a2[i][1] es siempre 0 → obY = obY(a0) */
        obY(a1)         = obY(o);
        obMap(a1)       = (uint32_t)(uintptr_t)Map_Glass;
        obGfx(a1)       = (uint16_t)(ArtTile_MZ_Glass_Pillar | Tile_Pal3 | Tile_Prio);
        obRender(a1)    = sprite_cam_field;
        glass_origY(a1) = obY(a1);
        obSubtype(a1)   = obSubtype(o);
        obActWid(a1)    = 64 / 2;
        obPriority(a1)  = 4;
        obFrame(a1)     = a2[i][2];
        glass_parent(a1) = (uint32_t)Object_GetIndex(o);
    }
    /* Tras el loop, a1 = último (sheen): ajustes exclusivos del sheen */
    obActWid(a1) = 32 / 2;
    obPriority(a1) = 3;
    obSubtype(a1) = (uint8_t)((obSubtype(a1) + (1 << 3)) & 0x0F);

finalize:
    glass_distanceY(o) = 144;
    obRender(o) |= sprite_customheight;
}

/* --- Glass_UpdateY: obY = origY - d0 --- */
static void Glass_UpdateY(uint8_t *o, int16_t d0) {
    obY(o) = (int16_t)(glass_origY(o) - d0);
}

/* --- Glass_MoveSheen: tail común de Type1/Type2 --- */
static void Glass_MoveSheen(uint8_t *o, uint16_t d0, int16_t d1) {
    if (obSubtype(o) & (1 << 3)) {     /* btst #3,obSubtype → ¿es sheen? */
        d0 = (uint16_t)(-(int16_t)d0);
        d0 = (uint16_t)(d0 + (uint16_t)d1);
        /* asr/lsr.b #1: sólo el byte bajo, el resto se preserva */
        {
            uint16_t hi = d0 & 0xFF00u;
            uint16_t lo = d0 & 0x00FFu;
            d0 = (uint16_t)(hi | (lo >> 1));
        }
        d0 = (uint16_t)(d0 + 32);
    }
    Glass_UpdateY(o, (int16_t)d0);
}

/* --- Glass_Type1_UpDown (subtype 1) --- */
static void Glass_Type1_UpDown(uint8_t *o) {
    uint16_t d0 = RAM_BYTE(v_oscillate + 0x12);
    Glass_MoveSheen(o, d0, 64);
}

/* --- Glass_Type2_DownUp (subtype 2) --- */
static void Glass_Type2_DownUp(uint8_t *o) {
    int16_t d1 = 64;
    uint16_t d0 = RAM_BYTE(v_oscillate + 0x12);
    d0 = (uint16_t)(-(int16_t)d0);
    d0 = (uint16_t)(d0 + (uint16_t)d1);
    Glass_MoveSheen(o, d0, d1);
}

/* --- Glass_Type3_Stomp (subtype 3, prototipo) --- */
static void Glass_Type3_Stomp(uint8_t *o) {
    if (obSubtype(o) & (1 << 3)) {
        /* sheen: oscila, offset -16 */
        uint16_t d0 = RAM_BYTE(v_oscillate + 0x12);
        d0 = (uint16_t)(d0 - 16);
        Glass_UpdateY(o, (int16_t)d0);
        return;
    }

    /* .checkSonicStomp */
    if (obStatus(o) & (1 << 3)) {                    /* btst #3,obStatus → Sonic encima */
        if (glass_stomp_flags(o) != 0) goto checkMoveDown;
        glass_stomp_flags(o) = 1;                    /* "Sonic encima" */
        /* bset #0,glass_stomp_first — Z = valor ANTERIOR del bit */
        {
            uint8_t was = glass_stomp_first(o);
            glass_stomp_first(o) |= 1;
            if ((was & 1) == 0) {
                /* Primer aterrizaje: no arrancar el stomp todavía */
                goto checkMoveDown;
            }
        }
        /* Segundo aterrizaje o posterior: iniciar stomp */
        glass_stomp_flags(o) |= (1 << 7);            /* moving down */
        glass_stomp_distance(o) = 16;
        glass_stomp_delay(o) = 10;
        if (glass_distanceY(o) == 64) {              /* cmpi.w #64 */
            glass_stomp_distance(o) = 64;
        }
    } else {
        glass_stomp_flags(o) &= ~1;                  /* bclr #0 */
    }

checkMoveDown:
    if ((int8_t)glass_stomp_flags(o) < 0) {          /* bit 7 set: bajando */
        if (glass_stomp_delay(o) != 0) {
            glass_stomp_delay(o)--;
            if (glass_stomp_delay(o) != 0) {
                goto setYDistance;
            }
        }
        if (glass_distanceY(o) != 0) {
            glass_distanceY(o)--;
            glass_stomp_distance(o)--;
            if (glass_stomp_distance(o) != 0) {
                goto setYDistance;
            }
        }
        glass_stomp_flags(o) &= ~(1 << 7);           /* bclr #7: parar de bajar */
    }

setYDistance:
    Glass_UpdateY(o, glass_distanceY(o));
}

/* --- Glass_Type4_Switch (subtype 4) --- */
static void Glass_Type4_Switch(uint8_t *o) {
    if (obSubtype(o) & (1 << 3)) {
        /* sheen: oscila, offset -16 */
        uint16_t d0 = RAM_BYTE(v_oscillate + 0x12);
        d0 = (uint16_t)(d0 - 16);
        Glass_UpdateY(o, (int16_t)d0);
        return;
    }

    /* Glass_ChkSwitch */
    if (glass_switch_flag(o) == 0) {
        uint8_t *a2 = RAM_ADDR(f_switch);
        uint8_t d0 = (uint8_t)(obSubtype(o) >> 4);
        if (a2[d0] == 0) {
            goto setYDistance;
        }
        glass_switch_flag(o) = 1;
    }

    /* .movePillarDown: baja 2px/frame hasta distanceY == 0 */
    if (glass_distanceY(o) != 0) {
        glass_distanceY(o) -= 2;
    }

setYDistance:
    Glass_UpdateY(o, glass_distanceY(o));
}

/* --- Glass_Types: dispatcher por subtype & 7 --- */
static void Glass_Types(uint8_t *o) {
    switch (obSubtype(o) & 7) {
        case 0: /* estacionario */              return;
        case 1: Glass_Type1_UpDown(o);          return;
        case 2: Glass_Type2_DownUp(o);          return;
        case 3: Glass_Type3_Stomp(o);           return;
        case 4: Glass_Type4_Switch(o);          return;
    }
}

/* --- Glass_Pillar_UpDown — routine 2 (pilar alto) --- */
static void Glass_Pillar_UpDown(uint8_t *o) {
    Glass_Types(o);
    int16_t out_d3 = 0, out_d5 = 0;
    SolidObject(o,
                (int16_t)(64 / 2 + sonic_solid_width),   /* d1 */
                144 / 2,                                 /* d2 */
                146 / 2,                                 /* d3 */
                obX(o),                                  /* d4 */
                &out_d3, &out_d5);
}

/* --- Glass_Sheen_UpDown — routine 4 --- */
static void Glass_Sheen_UpDown(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)glass_parent(o));
    glass_distanceY(o) = glass_distanceY(parent);
    Glass_Types(o);
}

/* --- Glass_Pillar_Triggered — routine 6 (pilar bajo) --- */
static void Glass_Pillar_Triggered(uint8_t *o) {
    Glass_Types(o);
    int16_t out_d3 = 0, out_d5 = 0;
    SolidObject(o,
                (int16_t)(64 / 2 + sonic_solid_width),
                112 / 2,
                114 / 2,
                obX(o),
                &out_d3, &out_d5);
}

/* --- Glass_Sheen_Triggered — routine 8 --- */
static void Glass_Sheen_Triggered(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)glass_parent(o));
    glass_distanceY(o) = glass_distanceY(parent);
    glass_origY(o)     = obY(parent);
    Glass_Types(o);
}

/* --- Dispatcher Object 30 --- */
static void GlassBlock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Glass_Main(o);             break;
        case 2: Glass_Pillar_UpDown(o);    break;
        case 4: Glass_Sheen_UpDown(o);     break;
        case 6: Glass_Pillar_Triggered(o); break;
        case 8: Glass_Sheen_Triggered(o);  break;
    }
    /* out_of_range.w .delete ; bra.w DisplaySprite */
    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
}

/* ===========================================================================
 *  Object 31 — MZ Chained Stompers
 *  Ported from _incObj/31 MZ Chained Stompers.asm (REV01, FixBugs=0).
 *
 *  Un "main block" spawnea 4 hijos: el propio bloque (parent), las púas,
 *  la cadena y la base adosada al techo. Todos comparten subtype y posición X;
 *  el bloque los mantiene alineados verticalmente a través de cstom_current.
 *
 *  Campos:
 *    cstom_origY   = objoff_30 (word): Y inicial del objeto
 *    cstom_current = objoff_32 (word): extensión actual (16.8 fixed point; solo el byte alto se usa para Y)
 *    cstom_length  = objoff_34 (word): extensión máxima (de CStom_Lengths)
 *    cstom_rising  = objoff_36 (word): 1 si está subiendo (auto-stomp)
 *    cstom_delay   = objoff_38 (word): frames de espera antes de subir
 *    cstom_switch  = objoff_3A (byte): índice del switch que activa la subida
 *    cstom_parent  = objoff_3C (long): índice de slot del bloque padre
 *
 *  Subtypes:
 *    $00-$06: large block, tipos 0-6
 *    $10-$16: medium block (spikes), tipos 0-6
 *    $20-$26: small block SIN spikes, tipos 0-6
 *    $80/$81: switch-activated (convierte el subtype a 0)
 * =========================================================================== */

#define cstom_origY(o)   (*(int16_t *)((uint8_t *)(o) + 0x30))
#define cstom_current(o) (*(uint16_t*)((uint8_t *)(o) + 0x32))
#define cstom_length(o)  (*(int16_t *)((uint8_t *)(o) + 0x34))
#define cstom_rising(o)  (*(int16_t *)((uint8_t *)(o) + 0x36))
#define cstom_delay(o)   (*(int16_t *)((uint8_t *)(o) + 0x38))
#define cstom_switch(o)  (*(uint8_t *)((uint8_t *)(o) + 0x3A))
#define cstom_parent(o)  (*(uint32_t*)((uint8_t *)(o) + 0x3C))

/* Forward declarations de Object 31 */
static void CStom_MainBlock(uint8_t *o);
static void CStom_Types(uint8_t *o);
static void CStom_Ceiling(uint8_t *o);
static void CStom_Spikes(uint8_t *o);
static void CStom_Chain(uint8_t *o);

/* { switch ID, subtype override } × 2. El segundo byte siempre es 0. */
static const uint8_t CStom_SwchNums[2][2] = {
    { 0, 0 },  /* $80 */
    { 1, 0 },  /* $81 (unused) */
};

/* { routine, relative Y-offset (signed byte), frame } × 4 */
static const uint8_t CStom_Var[12] = {
    2, 0x00, 0,   /* main block      */
    4, 0x1C, 1,   /* spikes          */
    8, 0xCC, 3,   /* chain           */
    6, 0xF0, 2,   /* ceiling base    */
};

/* Chain lengths (indexed by subtype & 0xF). */
static const uint16_t CStom_Lengths[7] = {
    0x7000, 0xA000, 0x5000, 0x7800, 0x3800, 0x5800, 0xB800,
};

/* { obActWid, frame } × 3, indexed by (subtype >> 3) & 0xE. */
static const uint8_t CStom_Var2[3][2] = {
    { 0x70/2, 0    },   /* $0x: large  */
    { 0x60/2, 9    },   /* $1x: medium */
    { 0x20/2, 0x0A },   /* $2x: small (no spikes) */
};
/* --- CStom_UpdateBlockY: obY = origY + high byte of current --- */
static void CStom_UpdateBlockY(uint8_t *o) {
    uint8_t b = (uint8_t)(cstom_current(o) >> 8);
    obY(o) = (int16_t)((uint16_t)cstom_origY(o) + (uint16_t)b);
}

/* --- CStom_ChkDel: FixBugs=0 → out_of_range ; sin DisplaySprite extra --- */
static void CStom_ChkDel(uint8_t *o) {
    if (OutOfRange(o, -1, 0)) DeleteObject(o);
}

/* --- CStom_Main — routine 0: setup + spawn 4 objetos --- */
static void CStom_Main(uint8_t *o) {
    uint8_t d0 = obSubtype(o);

    if (d0 & 0x80) {
        /* Switch-activated */
        uint8_t idx = d0 & 0x7F;
        cstom_switch(o) = CStom_SwchNums[idx][0];
        d0 = CStom_SwchNums[idx][1];   /* subtype override, siempre 0 */
        obSubtype(o) = d0;
    }

    d0 &= 0x0F;
    uint16_t length = CStom_Lengths[d0];
    if (d0 == 0) {
        cstom_current(o) = length;     /* $x0: spawn ya extendido al máximo */
    }

    /* Spawn 4 objects (o menos si spike-less). */
    int d1 = 3;             /* contador dbf-style */
    int idx = 0;            /* offset en CStom_Var */
    uint8_t *a1 = o;
    int first = 1;

    while (d1 >= 0) {
        if (!first) {
            a1 = (uint8_t *)FindNextFreeObj(a1);
            if (!a1) goto setupMainBlock;
        }
        first = 0;

    makeStomper:
        obRoutine(a1)   = CStom_Var[idx + 0];
        obID(a1)        = id_ChainStomp;
        obX(a1)         = obX(o);
        obY(a1)         = (int16_t)(obY(o) + (int8_t)CStom_Var[idx + 1]);
        obMap(a1)       = (uint32_t)(uintptr_t)Map_CStom;
        obGfx(a1)       = (uint16_t)ArtTile_MZ_Spike_Stomper;
        obRender(a1)    = sprite_cam_field;
        cstom_origY(a1) = obY(a1);
        obSubtype(a1)   = obSubtype(o);
        obActWid(a1)    = 32 / 2;
        cstom_length(a1)= (int16_t)length;
        obPriority(a1)  = 4;
        obFrame(a1)     = CStom_Var[idx + 2];
        idx += 3;

        if (obFrame(a1) == 1) {         /* es el objeto de púas */
            d1--;                       /* subq.w #1,d1 */
            if ((obSubtype(o) & 0xF0) == 0x20) {
                /* Small spike-less: reusar slot para la cadena */
                goto makeStomper;
            }
            obActWid(a1)  = 112 / 2;
            obColType(a1) = (uint8_t)(col_80x32 | col_hurt);
            d1++;                       /* addq.w #1,d1 */
        }

        cstom_parent(a1) = (uint32_t)Object_GetIndex(o);
        d1--;
    }
    obPriority(a1) = 3;                 /* ceiling base: prioridad 3 */

setupMainBlock:
    d0 = (uint8_t)((obSubtype(o) >> 3) & 0x0E);   /* 0, 2, or 4 */
    obActWid(o) = CStom_Var2[d0 >> 1][0];
    obFrame(o)  = CStom_Var2[d0 >> 1][1];

    /* Fall through into CStom_MainBlock. */
    CStom_MainBlock(o);
}

/* --- CStom_MainBlock — routine 2 --- */
static void CStom_MainBlock(uint8_t *o) {
    CStom_Types(o);

    v_obj31ypos = (uint16_t)obY(o);

    {
        int16_t d1 = (int16_t)((int8_t)obActWid(o) + sonic_solid_width);
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, 24 / 2, 26 / 2, obX(o), &out_d3, &out_d5);
    }

    if (obStatus(o) & (1 << 3)) {         /* Sonic encima */
        /* cmpi.b #$10,cstom_current — high byte >= 0x10 */
        uint8_t hi = (uint8_t)(cstom_current(o) >> 8);
        if (hi < 0x10) {
            /* Aplastar: a0 = player, a2 = stomper */
            KillSonic(RAM_ADDR(v_player), o);
        }
    }

    DisplaySprite(o);                     /* FixBugs=0 */
    CStom_ChkDel(o);
}

/* --- CStom_Spikes — routine 4 --- */
static void CStom_Spikes(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)cstom_parent(o));
    uint8_t b = (uint8_t)(cstom_current(parent) >> 8);
    obY(o) = (int16_t)((uint16_t)cstom_origY(o) + (uint16_t)b);
    /* Fall through a CStom_Ceiling */
    CStom_Ceiling(o);
}

/* --- CStom_Ceiling — routine 6 --- */
static void CStom_Ceiling(uint8_t *o) {
    DisplaySprite(o);                     /* FixBugs=0 */
    CStom_ChkDel(o);
}

/* --- CStom_Chain — routine 8 --- */
static void CStom_Chain(uint8_t *o) {
    obHeight(o) = 256 / 2;                /* 128 */
    obRender(o) |= sprite_customheight;

    uint8_t *parent = (uint8_t *)Object_GetSlot((int)cstom_parent(o));
    uint8_t d0 = (uint8_t)(cstom_current(parent) >> 8);
    d0 = (uint8_t)(d0 >> 5);
    d0 = (uint8_t)(d0 + 3);
    obFrame(o) = d0;
    /* Fall through a CStom_Spikes (alinear + display) */
    CStom_Spikes(o);
}

/* --- CStom_SwitchActivated (type $x0) --- */
static void CStom_SwitchActivated(uint8_t *o) {
    uint8_t *a2 = RAM_ADDR(f_switch);
    if (a2[cstom_switch(o)] == 0) goto falling;

    if ((int16_t)v_obj31ypos >= 0) goto checkRising;
    /* ASM: cmpi.b #$10,cstom_current — compara el BYTE ALTO del word
       (16.8 fixed point: 0x10 = 16px por debajo del techo). El word
       completo nunca vale exactamente $10 durante la subida ($1000 →
       $0F80 → … → 0), así que comparar el word dejaría al stomper
       subir hasta el techo y empujar el PushBlock contra él. */
    if ((uint8_t)(cstom_current(o) >> 8) == 0x10) goto stop;

checkRising:
    if (cstom_current(o) == 0) goto stop;

    if ((v_vblank_byte & 0x0F) == 0 && (int8_t)obRender(o) < 0) {
        Sound_Queue(sfx_ChainRise, false);
    }

    /* .rise */
    if (cstom_current(o) >= 0x80) {
        cstom_current(o) -= 0x80;
        CStom_UpdateBlockY(o);
        return;
    }
    cstom_current(o) = 0;
    /* fall through to .stop */

stop:
    obVelY(o) = 0;
    CStom_UpdateBlockY(o);
    return;

falling:
    {
        uint16_t maxlen = (uint16_t)cstom_length(o);
        if (cstom_current(o) == maxlen) {
            CStom_UpdateBlockY(o);
            return;
        }
        int16_t v = obVelY(o);
        obVelY(o) = (int16_t)(obVelY(o) + 0x70);
        cstom_current(o) = (uint16_t)(cstom_current(o) + (uint16_t)v);

        if (maxlen > cstom_current(o)) {
            CStom_UpdateBlockY(o);
            return;
        }
        cstom_current(o) = maxlen;
        obVelY(o) = 0;
        if ((int8_t)obRender(o) < 0) {
            Sound_Queue(sfx_ChainStomp, false);
        }
        CStom_UpdateBlockY(o);
    }
}

/* --- CStom_AutoStomp (types $x1/$x2/$x4/$x6) --- */
static void CStom_AutoStomp(uint8_t *o) {
    if (cstom_rising(o) != 0) {
        if (cstom_delay(o) != 0) {
            cstom_delay(o)--;
            CStom_UpdateBlockY(o);
            return;
        }
        /* .risingSound */
        if ((v_vblank_byte & 0x0F) == 0 && (int8_t)obRender(o) < 0) {
            Sound_Queue(sfx_ChainRise, false);
        }
        /* .rise */
        if (cstom_current(o) >= 0x80) {
            cstom_current(o) -= 0x80;
            CStom_UpdateBlockY(o);
            return;
        }
        cstom_current(o) = 0;
        obVelY(o) = 0;
        cstom_rising(o) = 0;
        CStom_UpdateBlockY(o);
        return;
    }

    /* .falling */
    uint16_t maxlen = (uint16_t)cstom_length(o);
    if (cstom_current(o) == maxlen) {
        CStom_UpdateBlockY(o);
        return;
    }
    int16_t v = obVelY(o);
    obVelY(o) = (int16_t)(obVelY(o) + 0x70);
    cstom_current(o) = (uint16_t)(cstom_current(o) + (uint16_t)v);

    if (maxlen > cstom_current(o)) {
        CStom_UpdateBlockY(o);
        return;
    }
    cstom_current(o) = maxlen;
    obVelY(o) = 0;
    cstom_rising(o) = 1;
    cstom_delay(o) = 60;
    if ((int8_t)obRender(o) < 0) {
        Sound_Queue(sfx_ChainStomp, false);
    }
    CStom_UpdateBlockY(o);
}

/* --- CStom_WaitForSonic (types $x3/$x5) --- */
static void CStom_WaitForSonic(uint8_t *o) {
    int16_t d0 = obX(RAM_ADDR(v_player)) - obX(o);
    if (d0 < 0) d0 = (int16_t)(-d0);
    if ((uint16_t)d0 < 144) {
        obSubtype(o)++;
    }
    CStom_UpdateBlockY(o);
}

/* --- CStom_Types: dispatcher por subtype & 0xF --- */
static void CStom_Types(uint8_t *o) {
    switch (obSubtype(o) & 0x0F) {
        case 0: CStom_SwitchActivated(o); return;
        case 1:
        case 2:
        case 4:
        case 6: CStom_AutoStomp(o);       return;
        case 3:
        case 5: CStom_WaitForSonic(o);    return;
    }
}

/* --- Dispatcher Object 31 --- */
static void ChainStomp_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: CStom_Main(o);        return;
        case 2: CStom_MainBlock(o);   return;
        case 4: CStom_Spikes(o);      return;
        case 6: CStom_Ceiling(o);     return;
        case 8: CStom_Chain(o);       return;
    }
}

/* ===========================================================================
 *  Object 32 — Button / Switch (MZ, SYZ, LZ, SBZ)
 *  Ported from _incObj/32 Button.asm (REV01, FixBugs=0).
 *
 *  Cada botón usa un byte en f_switch[] (indexado por subtype & $F).
 *  El bit dentro del byte lo determina el bit 6 del subtype:
 *    - bit 6 = 0 → usa bit 0 del byte
 *    - bit 6 = 1 → usa bit 7 del byte (variante "alterna" sin usar en el juego)
 *
 *  Si el bit 7 del subtype está puesto Y estamos en MZ, llama a But_MZBlock
 *  para ver si un PushBlock (Object 33) está encima. En FixBugs=0 NO se
 *  comprueba la zona, así que la rutina corre en cualquier nivel donde el
 *  subtype tenga el bit 7 (esto es el bug conocido).
 * =========================================================================== */

/* Radios de detección (X, Y) del bloque empujable. Siempre los mismos. */
static void But_Pressed(uint8_t *o);
static int  But_MZBlock(uint8_t *o, int bit);
/* --- But_MZBlock: ¿hay un PushBlock (id $33) sobre el botón? -------------
 *  Devuelve 1 si sí, 0 si no. El ASM pasa d3 (bit actual 0 o 7) y lo
 *  preserva en la pila, pero en la práctica no lo usa para la lógica —
 *  el backup/restore es solo por si el llamador lo necesitara después.
 *  Lo replicamos pasándolo como parámetro por fidelidad al ASM. */
static int But_MZBlock(uint8_t *o, int bit) {
    (void)bit;   /* no afecta al cálculo */

    uint16_t d2 = (uint16_t)(obX(o) - 0x10);   /* left edge del botón */
    uint16_t d3 = (uint16_t)(obY(o) - 8);      /* top edge del botón  */
    uint16_t d4 = 0x20;                        /* rango X de detección */
    uint16_t d5 = 0x10;                        /* rango Y de detección */

    uint8_t *base = RAM_ADDR(v_lvlobjspace);
    int count = (v_lvlobjend - v_lvlobjspace) / object_size;

    for (int i = 0; i < count; i++) {
        uint8_t *a1 = base + i * object_size;
        if ((int8_t)obRender(a1) >= 0) continue;    /* bpl.s .nextObject */
        if (obID(a1) != id_PushBlock)  continue;    /* cmpi.b #id_PushBlock */
        /* .blockFound */

        /* checkX: d0 = blockX - 0x10 - buttonLeft, comparación unsigned */
        uint16_t d0 = (uint16_t)((uint16_t)obX(a1) - 0x10);
        d0 = (uint16_t)(d0 - d2);
        if (d0 & 0x8000) {
            /* blo tras sub d2 → rama blo (d0 += 0x20) */
            uint16_t sum = (uint16_t)(d0 + 0x20);
            if (sum >= 0x20) continue;              /* no wrap → fuera de rango */
            /* wrap → caer a checkY */
        } else {
            /* bhs .checkX_fromRight */
            if (d0 > d4) continue;
            /* caer a checkY */
        }

        /* checkY: mismo patrón, pero el ASM hace `add.w d1,d1` (d1=$10 → $20)
           antes del `add.w d1,d0`, así que aquí se suma/compara contra $20. */
        uint16_t e0 = (uint16_t)((uint16_t)obY(a1) - 0x10);
        e0 = (uint16_t)(e0 - d3);
        if (e0 & 0x8000) {
            uint16_t sum = (uint16_t)(e0 + 0x20);
            if (sum >= 0x20) continue;
        } else {
            if (e0 > d5) continue;
        }

        /* .blockOnTop */
        return 1;
    }
    return 0;
}

/* --- But_Main — routine 0: setup --- */
static void But_Main(uint8_t *o) {
    obRoutine(o) += 2;                              /* → But_Pressed */
    obMap(o) = (uint32_t)(uintptr_t)Map_But;

    /* Default = MZ: Tile_Pal3. Si NO es MZ, se quita la paleta. */
    obGfx(o) = (uint16_t)(ArtTile_Button_Main | Tile_Pal3);
    if ((uint8_t)v_zone != id_MZ) {
        obGfx(o) = (uint16_t)ArtTile_Button_Main;
    }

    obRender(o)  = sprite_cam_field;
    obActWid(o)  = 32 / 2;
    obPriority(o)= 4;
    obY(o) += 3;                                    /* addq.w #3,obY */
}

/* --- But_Pressed — routine 2 --- */
static void But_Pressed(uint8_t *o) {
    /* tst.b obRender ; bpl.s .display (FixBugs=0: short branch) */
    if ((int8_t)obRender(o) >= 0) goto display;

    {
        int16_t d1 = (int16_t)(32/2 + sonic_solid_width);
        int16_t d2 = 10/2;
        int16_t d3 = 10/2;
        int16_t d4 = obX(o);
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);
    }

    obFrame(o) &= (uint8_t)~1;                      /* bclr #0: unpressed frame */

    uint8_t d0 = (uint8_t)(obSubtype(o) & 0x0F);
    uint8_t *a3 = RAM_ADDR(f_switch + d0);          /* lea (a3,d0),a3 */
    int bit = 0;

    /* btst #6: alterna bit 0/7 */
    if (obSubtype(o) & (1 << 6)) {
        bit = 7;
    }

    /* .checkMZ1Block: FixBugs=0 → SIN comprobación de zona */
    if ((obSubtype(o) & 0x80) != 0) {
        if (But_MZBlock(o, bit)) {
            goto pressed;
        }
    }

    /* .checkSonicOnTop */
    if (obSolid(o) != 0) goto pressed;

    /* No está pulsado */
    *a3 &= (uint8_t)~(1u << bit);                    /* bclr d3,(a3) */
    goto handleFlashing;

pressed:
    /* .pressed */
    if (*a3 != 0) goto setPressedState;             /* tst.b (a3) ; bne */

    Sound_Queue(sfx_Switch, false);                 /* QueueSound2 */

setPressedState:
    *a3 |= (uint8_t)(1u << bit);                     /* bset d3,(a3) */
    obFrame(o) |= 1;                                /* bset #0: pressed frame */

handleFlashing:
    /* .handleFlashing: bit 5 del subtype activa el parpadeo (sin usar) */
    if (obSubtype(o) & (1 << 5)) {
        obTimeFrame(o)--;                            /* subq.b #1 */
        if ((int8_t)obTimeFrame(o) < 0) {
            obTimeFrame(o) = 7;
            obFrame(o) ^= 2;                         /* bchg #1: 0 ↔ 2 */
        }
    }

display:
    /* FixBugs=0: DisplaySprite PRIMERO, luego out_of_range */
    DisplaySprite(o);
    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
    }
}

/* --- Dispatcher Object 32 --- */
static void Button_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: But_Main(o);    break;
        case 2: But_Pressed(o); break;
    }
}

static int16_t PushB_ObjHitWallRight(uint8_t *o, int16_t d3) {
    int16_t y = obY(o);
    int16_t x = (int16_t)(obX(o) + d3);
    int16_t dist = 0;

    v_anglebuffer = 0;                                   /* move.b #0,(a4) */

    FindWall(y, x,
             0x0E,             /* d5: left/right/bottom solid */
             0x0000,           /* d6: sin eor mask         */
             0x10,             /* a3: tile width hacia derecha */
             &v_anglebuffer, o, &dist);

    /* .no_snap: si bit 0 del ángulo está puesto, snap a $C0 (pared derecha plana) */
    if (v_anglebuffer & 1) {
        v_anglebuffer = 0xC0;
    }
    return dist;
}

static int16_t PushB_ObjHitWallLeft(uint8_t *o, int16_t d3) {
    int16_t y = obY(o);
    int16_t x = (int16_t)(obX(o) + d3);
    int16_t dist = 0;

    /* FixBugs=0: el `eori.w #$F,d3` que corrige la colisión errática
       contra paredes izquierdas está entre `if FixBugs`, así que se omite. */

    v_anglebuffer = 0;                                   /* move.b #0,(a4) */

    FindWall(y, x,
             0x0E,             /* d5: left/right/bottom solid */
             0x0800,           /* d6: eor mask xflip        */
             -0x10,            /* a3: tile width hacia izquierda */
             &v_anglebuffer, o, &dist);

    if (v_anglebuffer & 1) {
        v_anglebuffer = 0x40;                            /* snap a $40 (pared izquierda plana) */
    }
    return dist;
}

/* --- ChkPartiallyVisible --------------------------------------------------
 *  Devuelve 1 si el sprite toca al menos parcialmente el viewport visible. */
static int ChkPartiallyVisible(uint8_t *o) {
    int16_t x = (int16_t)(obX(o) - (int16_t)v_screenposx);
    int16_t y = (int16_t)(obY(o) - (int16_t)v_screenposy);
    int16_t w = (int16_t)(int8_t)obActWid(o);
    int16_t h = (int16_t)(int8_t)obHeight(o);
    if (x + w < 0) return 0;
    if (x - w >= g_render_w) return 0;
    if (y + h < 0) return 0;
    if (y - h >= SCREEN_HEIGHT) return 0;
    return 1;
}

/* ===========================================================================
 *  Object 33 — Pushable Blocks (MZ; unused in LZ)
 *  Ported from _incObj/33 MZ, LZ Pushable Blocks.asm (REV01, FixBugs=0).
 *
 *  Subtypes: 0 = 1x1 block, 1 = 4x1 block (o $80+ para el bloque que va
 *  sobre el stomper en MZ1, aunque esto es por el bit 7 del subtype).
 *
 *  Campos:
 *    pblock_lavaspeed = objoff_30 (word): X-velocidad guardada antes de tocar lava
 *    pblock_onlava    = objoff_32 (byte): 1 si está flotando sobre lava
 *    pblock_origX     = objoff_34 (word): X inicial
 *    pblock_origY     = objoff_36 (word): Y inicial
 * =========================================================================== */

#define pblock_lavaspeed(o) (*(int16_t *)((uint8_t *)(o) + 0x30))
#define pblock_onlava(o)    (*(uint8_t  *)((uint8_t *)(o) + 0x32))
#define pblock_origX(o)     (*(int16_t *)((uint8_t *)(o) + 0x34))
#define pblock_origY(o)     (*(int16_t *)((uint8_t *)(o) + 0x36))

static const uint8_t PushB_Var[2][2] = {
    { 32/2,  0 },   /* 1x1 */
    { 128/2, 1 },   /* 4x1 */
};
static void PushB_Action(uint8_t *o);
static void PushB_ChkVisible(uint8_t *o);
static void PushB_OnLava(uint8_t *o);
static void PushB_Display(uint8_t *o);
static void PushB_ChkWithinOrigin(uint8_t *o);
static void PushB_SolidAction(uint8_t *o, int16_t d1, int16_t d2, int16_t d3, int16_t d4);
static void PushB_SpawnLavaGeysers(uint8_t *o);
/* --- PushB_Solid_ChkCollision: wrapper de SolidObject que devuelve d0 -----
 *  d0 = obX(player) - obX(block) + d1 ajustado para ser negativo si Sonic
 *  está en la mitad derecha (d0 > d1), positivo si está en la mitad
 *  izquierda (d0 <= d1). Igual que Solid_Collision del ASM. */
static int PushB_Solid_ChkCollision(uint8_t *o, int16_t d1, int16_t d2, int16_t d3,
                                     int16_t *out_d0) {
    int16_t out_d3 = 0, out_d5 = 0;
    uint8_t *a1 = RAM_ADDR(v_player);

    /* d0 ANTES de que SolidObject alinee a Sonic */
    int16_t d0 = (int16_t)(obX(a1) - obX(o) + d1);

    int type = SolidObject(o, d1, d2, d3, obX(o), &out_d3, &out_d5);
    if (type == 0) return 0;
    if (type < 0) return -1;

    /* Igual que Solid_Collision: transforma solo si d0 > d1 (mitad derecha),
       i.e. cuando `cmp.w d0,d1 / bhs .sonic_left` NO salta en el ASM.
       Resultado: d0 positivo = distancia al borde izquierdo (empujar a la
       derecha); d0 negativo = distancia al borde derecho (empujar a la
       izquierda). */
    if (d0 > d1) {
        d0 = (int16_t)(d0 - (int16_t)(d1 + d1));
    }
    if (out_d0) *out_d0 = d0;
    return 1;
}

/* --- PushB_Display: DisplaySprite + out_of_range ------------------------- */
static void PushB_Display(uint8_t *o) {
    if (!OutOfRange(o, -1, 0)) {
        DisplaySprite(o);
        return;
    }
    PushB_ChkWithinOrigin(o);
}

/* --- PushB_ChkWithinOrigin: si está fuera pero el original no, reset ---- */
static void PushB_ChkWithinOrigin(uint8_t *o) {
    if (OutOfRange(o, pblock_origX(o), 0)) {
        /* .deleteAndAllowRespawn */
        uint8_t *a2 = RAM_ADDR(v_objstate);
        uint8_t d0 = obRespawnNo(o);
        if (d0 != 0) {
            a2[2 + d0] &= ~1;      /* bclr #0: permite respawn */
        }
        DeleteObject(o);
        return;
    }
    /* Reset a la posición inicial */
    obX(o) = pblock_origX(o);
    obY(o) = pblock_origY(o);
    obRoutine(o) = 4;
    PushB_ChkVisible(o);
}

/* --- PushB_ChkVisible — routine 4 --- */
static void PushB_ChkVisible(uint8_t *o) {
    if (ChkPartiallyVisible(o)) {
        obRoutine(o) = 2;
        pblock_onlava(o) = 0;
        obVelX(o) = 0;
        obVelY(o) = 0;
    }
}

/* --- PushB_SolidAction_NotOnPlatform ------------------------------------- */
static void PushB_SolidAction_NotOnPlatform(uint8_t *o,
                                              int16_t d1, int16_t d2, int16_t d3) {
    int16_t d0 = 0;
    int type = PushB_Solid_ChkCollision(o, d1, d2, d3, &d0);
    if (type == 0) return;                /* no touch */
    if (type < 0) return;                 /* top/bottom */
    if (pblock_onlava(o) != 0) return;    /* on lava → no push */

    if (d0 == 0) return;
    if (d0 < 0) goto leftSide;

    /* .rightSide */
    {
        uint8_t *a1 = RAM_ADDR(v_player);
        if (obStatus(a1) & 1) return;      /* Sonic mirando a la derecha */
        
        int16_t d3h = (int16_t)(int8_t)obActWid(o);
        int16_t wall = PushB_ObjHitWallRight(o, d3h);
        if (wall < 0) return;
        obX(o) += 1;                          /* push block right */
        /* Mover Sonic 1px y fijar velocidad */
        obX(a1) += 1;
        obInertia(a1) = 0x40;
        obVelX(a1) = 0;
        Sound_Queue(sfx_Push, false);

        /* .pushBlock completo */
        goto pushCommon;
    }

leftSide:
    {
        uint8_t *a1 = RAM_ADDR(v_player);
        if (!(obStatus(a1) & 1)) return;         /* Sonic mirando a la izquierda */
        
        int16_t d3h = (int16_t)(int8_t)obActWid(o);
        int16_t wall = PushB_ObjHitWallLeft(o, (int16_t)~d3h);
        if (wall < 0) return;
        obX(o) -= 1;
        obX(a1) -= 1;
        obInertia(a1) = (int16_t)-0x40;
        obVelX(a1) = 0;
        Sound_Queue(sfx_Push, false);
    }

pushCommon:
    {
        
        int16_t dx = d0;

        if ((int8_t)obSubtype(o) < 0) return;   /* bloque sobre stomper */

        int16_t dist, angle;
        ObjFloorDist(o, &dist, &angle);
        if (dist <= 4) {
            /* .alignToFloor */
            obY(o) = (int16_t)(obY(o) + dist);
            return;
        }
        /* .setToDrop: bloque cae por un borde */
        obVelX(o) = 0x400;
        if (dx < 0) obVelX(o) = (int16_t)(-obVelX(o));
        ob2ndRout(o) = 6;
    }
}

/* --- PushB_SolidAction: dispatch por ob2ndRout --- */
static void PushB_SolidAction(uint8_t *o, int16_t d1, int16_t d2, int16_t d3, int16_t d4) {
    uint8_t state = ob2ndRout(o);

    if (state == 0) {
        PushB_SolidAction_NotOnPlatform(o, d1, d2, d3);
        return;
    }
    if (state == 2) {
        /* .sonicOnBlock: Sonic estaba encima */
        int16_t dummy;
        ExitPlatform(o, d1, &dummy);
        uint8_t *a1 = RAM_ADDR(v_player);
        if (obStatus(a1) & (1 << 3)) {
            MvSonicOnPtfm(o, d4, d3);
            return;
        }
        ob2ndRout(o) = 0;
        return;
    }
    if (state == 4) {
        SpeedToPos(o);
        obVelY(o) = (int16_t)(obVelY(o) + 0x18);

        int16_t dist, angle;
        ObjFloorDist(o, &dist, &angle);
        if (dist < 0) {
            obY(o) = (int16_t)(obY(o) + dist);
            obVelY(o) = 0;
            ob2ndRout(o) = 0;

            /* move.w (a1),d0 / andi.w #$3FF,d0 / cmpi.w #$16A,d0 / blo.s .return */
            uint16_t tile = v_last_floor_block & 0x3FF;
            if (tile >= 0x16A) {
                int16_t d0 = (int16_t)(pblock_lavaspeed(o) >> 3); /* asr.w #3 */
                obVelX(o) = d0;
                pblock_onlava(o) = 1;
                obSubpixelY(o) = 0;
            }
        }
        return;
    }
    if (state == 6) {
        /* .snapToLedge */
        SpeedToPos(o);
        int16_t x = obX(o);
        if ((x & 0xC) != 0) return;
        obX(o) = (int16_t)(obX(o) & 0xFFF0);
        pblock_lavaspeed(o) = obVelX(o);
        obVelX(o) = 0;
        ob2ndRout(o) = 4;
        return;
    }
}

/* --- PushB_OnLava: manejo mientras está flotando sobre lava --- */
static void PushB_OnLava(uint8_t *o) {
    int16_t savedX = obX(o);

    if ((uint8_t)ob2ndRout(o) < 4) {
        SpeedToPos(o);
    }

    if (obStatus(o) & (1 << 1)) {
        obVelY(o) = (int16_t)(obVelY(o) + 0x18);

        int16_t dist, angle;
        ObjFloorDist(o, &dist, &angle);
        if (dist < 0) {
            obY(o) = (int16_t)(obY(o) + dist);
            obVelY(o) = 0;
            obStatus(o) &= ~(1 << 1);

            uint16_t tile = v_last_floor_block & 0x3FF;
            if (tile >= 0x16A) {                    /* ← borra el if(0) */
                int16_t d0 = (int16_t)(pblock_lavaspeed(o) >> 3);
                obVelX(o) = d0;
                pblock_onlava(o) = 1;
                obSubpixelY(o) = 0;
            }
        }
    } else {
        /* .PushB_OnLava_CheckWall */
        if (obVelX(o) == 0) {
            /* .sinkingInLava */
            int32_t y_fp = ((uint32_t)obY(o) << 16) | (uint16_t)obSubpixelY(o);
            y_fp += 0x2001;
            obY(o) = (int16_t)((uint32_t)y_fp >> 16);
            obSubpixelY(o) = (int16_t)(y_fp & 0xFFFF);
            uint8_t sunk = (uint8_t)(obSubpixelY(o) >> 8);
            if (sunk >= 160) {
                /* .PushB_Sunken */
                uint8_t *a1 = RAM_ADDR(v_player);
                obStatus(a1) &= ~(1 << 3);
                obStatus(o)  &= ~(1 << 3);
                PushB_ChkWithinOrigin(o);
                return;
            }
        } else if (obVelX(o) < 0) {
            /* .checkWallLeft */
            int16_t d3h = (int16_t)(int8_t)obActWid(o);
            int16_t wall = PushB_ObjHitWallLeft(o, (int16_t)~d3h);
            if (wall < 0) {
                obVelX(o) = 0;
            }
        } else {
            /* .checkWallRight */
            int16_t d3h = (int16_t)(int8_t)obActWid(o);
            int16_t wall = PushB_ObjHitWallRight(o, d3h);
            if (wall < 0) {
                obVelX(o) = 0;
            }
        }
    }

    /* .PushB_LavaPlatform */
    {
        int16_t d1 = (int16_t)((int8_t)obActWid(o) + sonic_solid_width);
        int16_t d2 = 32/2;
        int16_t d3 = 34/2;
        PushB_SolidAction(o, d1, d2, d3, savedX);
        PushB_SpawnLavaGeysers(o);
        PushB_Display(o);
    }
}

#define gmake_parent(o) (*(uint32_t *)((uint8_t *)(o) + 0x3C))
/* --- PushB_SpawnLavaGeysers: hardcoded MZ2/MZ3 --- */
static void PushB_SpawnLavaGeysers(uint8_t *o) {
    uint16_t zact = RAM_U16(0xFE10);
    int16_t d2 = 0;
    int spawn = 0;

    if (zact == id_MZ_act2) {
        d2 = -32;
        int16_t x = obX(o);
        if (x == 0xDD0 || x == 0xCC0 || x == 0xBA0) spawn = 1;
    } else if (zact == id_MZ_act3) {
        d2 = 32;
        int16_t x = obX(o);
        if (x == 0x560 || x == 0x5C0) spawn = 1;
    }
    if (!spawn) return;

    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (!a1) return;

    obID(a1) = id_GeyserMaker;
    obX(a1)  = (int16_t)(obX(o) + d2);
    obY(a1)  = (int16_t)(obY(o) + 16);

    /* move.l a0,gmake_parent(a1) — pero con slot index en vez de puntero. */
    gmake_parent(a1) = (uint32_t)Object_GetIndex(o);
}

/* --- PushB_Action — routine 2 --- */
static void PushB_Action(uint8_t *o) {
    if (pblock_onlava(o) != 0) {
        PushB_OnLava(o);
        return;
    }

    {
        int16_t d1 = (int16_t)((int8_t)obActWid(o) + sonic_solid_width);
        int16_t d2 = 32/2;
        int16_t d3 = 34/2;
        PushB_SolidAction(o, d1, d2, d3, obX(o));
    }

    /* Hardcoded MZ1: alinear con el stomper (Object 31) */
    if (RAM_U16(0xFE10) == id_MZ_act1) {
        obSubtype(o) &= ~0x80;      /* bclr #7 */
        int16_t x = obX(o);
        if (x >= 0xA20 && x < 0xAA1) {
            int16_t y31 = (int16_t)v_obj31ypos;
            y31 = (int16_t)(y31 - 0x1C);
            obY(o) = y31;
            v_obj31ypos |= (1 << 7);       /* bset #7 */
            obSubtype(o) |= 0x80;          /* bset #7 */
        }
    }

    PushB_Display(o);
}

/* --- PushB_Main — routine 0 --- */
static void PushB_Main(uint8_t *o) {
    obRoutine(o) += 2;
    obHeight(o) = 30 / 2;
    obWidth(o) = 30 / 2;
    obMap(o) = (uint32_t)(uintptr_t)Map_Push;
    obGfx(o) = (uint16_t)(ArtTile_MZ_Block | Tile_Pal3);
    if ((uint8_t)v_zone == id_LZ) {
        obGfx(o) = (uint16_t)(ArtTile_LZ_Push_Block | Tile_Pal3);
    }
    obRender(o) = sprite_cam_field;
    obPriority(o) = 3;
    pblock_origX(o) = obX(o);
    pblock_origY(o) = obY(o);

    uint8_t sub = obSubtype(o);
    int idx = (sub * 2) & 0xE;
    if (idx > 2) idx = 2;               /* safety: solo 2 entradas */
    obActWid(o) = PushB_Var[idx >> 1][0];
    obFrame(o)  = PushB_Var[idx >> 1][1];

    if (sub != 0) {
        obGfx(o) = (uint16_t)(ArtTile_MZ_Block | Tile_Pal3 | Tile_Prio);
    }

    /* .chkgone: solo un bloque a la vez */
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t rno = obRespawnNo(o);
    if (rno != 0) {
        a2[2 + rno] &= (uint8_t)~0x80;    /* bclr #7 */
        uint8_t was = a2[2 + rno] & 1;
        a2[2 + rno] |= 1;                 /* bset #0 */
        if (was) {
            DeleteObject(o);
            return;
        }
    }

    /* Fall-through a PushB_Action */
    PushB_Action(o);
}

/* --- Dispatcher Object 33 --- */
static void PushBlock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: PushB_Main(o);       break;
        case 2: PushB_Action(o);     break;
        case 4: PushB_ChkVisible(o); break;
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
            if (OutOfRange(o, spikes_origX(o), 0)) {                       /* out_of_range.w DeleteObject,spikes_origX */
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
 *  Object 38 — Shield and Invincibility Stars (id_ShieldItem = $38)
 *  Ported verbatim from _incObj/38 Shield and Invincibility.asm
 *  (REV01, FixBugs=0).
 *
 *  objoff_30 (byte) = stars_lag: index of previous recorded tracking position
 *  that each star lags behind. Increments by 4 every frame, wraps at 6*4=24,
 *  so each star only updates its position once every 6 frames — a subtle
 *  jitter effect behind Sonic.
 *
 *  Role depends on obAnim at spawn:
 *    - obAnim == 0 → shield     (routine 2)
 *    - obAnim != 0 → invincibility star (routine 4), one of 4 stars; the
 *      obAnim value (1..4) selects a fixed offset within the recorded
 *      position trail (v_tracksonic).
 *
 *  Both roles share the same object slot family (v_shieldobj / v_starsobj1..4),
 *  so the ID is the same; only obAnim at spawn tells them apart.
 *  =========================================================================== */

#define shi_stars_lag(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x30)) /* objoff_30 */

static void Shi_Main(uint8_t *o);
static void Shi_Shield(uint8_t *o);
static void Shi_Stars(uint8_t *o);

/* Shi_Main — routine 0: initialize shield or star. */
static void Shi_Main(uint8_t *o) {
    obRoutine(o) += 2;                              /* addq.b #2 → Shi_Shield */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Shield;/* move.l #Map_Shield,obMap */
    obRender(o)   = sprite_cam_field;               /* move.b #sprite_cam_field,obRender */
    obPriority(o) = 1;                              /* move.b #1,obPriority */
    obActWid(o)   = 32 / 2;                         /* move.b #32/2,obActWid */

    if (obAnim(o) == 0) {                           /* tst.b obAnim / bne.s .stars */
        obGfx(o) = (uint16_t)ArtTile_Shield;        /* move.w #ArtTile_Shield,obGfx */
        return;                                     /* rts */
    }

    /* .stars */
    obRoutine(o) += 2;                              /* addq.b #2 → Shi_Stars */
    obGfx(o) = (uint16_t)ArtTile_Invincibility;     /* move.w #ArtTile_Invincibility,obGfx */
}

/* Shi_Shield — routine 2: follow Sonic while shield is active. */
static void Shi_Shield(uint8_t *o) {
    if (v_invinc != 0) {                            /* tst.b v_invinc / bne.s .hide */
        return;                                     /* .hide: rts, keep object alive, don't display */
    }
    if (v_shield == 0) {                            /* tst.b v_shield / beq.s .delete */
        DeleteObject(o);                            /* jmp (DeleteObject).l */
        return;
    }

    uint8_t *player = RAM_ADDR(v_player);
    obX(o)      = obX(player);                      /* move.w (v_player+obX),obX */
    obY(o)      = obY(player);                      /* move.w (v_player+obY),obY */
    obStatus(o) = obStatus(player);                 /* move.b (v_player+obStatus),obStatus */

    if (Ani_Shield) {                               /* lea (Ani_Shield).l,a1 */
        AnimateSprite(o, Ani_Shield);               /* jsr (AnimateSprite).l */
    }
    DisplaySprite(o);                               /* jmp (DisplaySprite).l */
}

/* Shi_Stars — routine 4: one of the four invincibility stars, trailing
 *  behind Sonic through the recorded position buffer. */
static void Shi_Stars(uint8_t *o) {
    if (v_invinc == 0) {                            /* tst.b v_invinc / beq.s Shi_Start_Delete */
        DeleteObject(o);                            /* jmp (DeleteObject).l */
        return;
    }

    /* .trail:
     *      d0 = low byte of v_trackpos; the ASM loads it as a word but every
     *      subsequent op is a byte op, so only the low byte matters (v_trackpos
     *      is bounded to $00..$FF by Sonic_RecordPosition). */
    uint8_t d0 = (uint8_t)v_trackpos;               /* move.w (v_trackpos).w,d0 */
    uint8_t d1 = (uint8_t)(obAnim(o) - 1);          /* move.b obAnim / subq.b #1 (1..4 → 0..3) */

    d1 = (uint8_t)(d1 << 3);                        /* lsl.b #3,d1   (× 8) */
    {
        uint8_t d2 = d1;                            /* move.b d1,d2 */
        d1 = (uint8_t)(d1 + d1);                    /* add.b d1,d1   (× 2) */
        d1 = (uint8_t)(d1 + d2);                    /* add.b d2,d1   (× 3 total) */
    }
    d1 = (uint8_t)(d1 + 4);                         /* addq.b #4,d1 (next slot) */
    d0 = (uint8_t)(d0 - d1);                        /* sub.b d1,d0  (base index for this star) */

    /* Small jitter: only update position every 6 frames. */
    d1 = shi_stars_lag(o);                          /* move.b stars_lag(a0),d1 */
    d0 = (uint8_t)(d0 - d1);                        /* sub.b d1,d0  (use earlier data to create trail) */
    d1 = (uint8_t)(d1 + 4);                         /* addq.b #4,d1 */
    if (d1 >= 6 * 4) {                              /* cmpi.b #6*4,d1 / blo.s .updateLag */
        d1 = 0;                                     /* moveq #0,d1 */
    }
    shi_stars_lag(o) = d1;                          /* move.b d1,stars_lag(a0) */

    /* .updateStars */
    uint8_t *a1 = RAM_ADDR(v_tracksonic + d0);      /* lea (v_tracksonic).w,a1 ; lea (a1,d0.w),a1 */
    obX(o) = *(int16_t *)a1; a1 += 2;               /* move.w (a1)+,obX */
    obY(o) = *(int16_t *)a1;                        /* move.w (a1)+,obY */

    uint8_t *player = RAM_ADDR(v_player);
    obStatus(o) = obStatus(player);                 /* move.b (v_player+obStatus),obStatus */

    if (Ani_Shield) {                               /* lea (Ani_Shield).l,a1 */
        AnimateSprite(o, Ani_Shield);               /* jsr (AnimateSprite).l */
    }
    DisplaySprite(o);                               /* jmp (DisplaySprite).l */
}

/* ShieldItem dispatcher — Shi_Index: 0 = Main, 2 = Shield, 4 = Stars */
static void ShieldItem_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Shi_Main(o);   break;
        case 2: Shi_Shield(o); break;
        case 4: Shi_Stars(o);  break;
    }
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
    /* Demo build: if finishing Green Hill Act 3, go to End Demo screen (Not Necesary Anymore)*/
    //if (v_zone == id_GHZ && v_act == act3) {
    //    v_gamemode = GM_EndDemo;
    //    DisplaySprite(o);
    //    return;
    //}

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
        case 0x04: Got_Wait(o); break;
        case 0x06: Got_Bonus(o); break;
        case 0x08: Got_Wait(o); break;
        case 0x0A: Got_NextLevel(o); break;
        case 0x0C: Got_Wait(o); break;
        case 0x0E: Got_SBZ2_MoveOut(o); break;
        case 0x10: Got_SBZ2_Boundary(o); break;
    }
}

/* ===========================================================================
 *  Object 3C — Smashable Wall (GHZ, SLZ)
 *  Ported verbatim from _incObj/3C GHZ, SLZ Smashable Wall.asm
 *  + _incObj/sub SmashObject.asm (REV01, FixBugs=0).
 *
 *  smash_speed = objoff_30 (word): Sonic's horizontal velocity captured
 *                                   before SolidObject (which can modify it).
 * =========================================================================== */

#define smash_speed(obj) (*(int16_t *)((uint8_t *)(obj) + 0x30))

/* Smash_FragSpd1: fragment velocities when the wall is broken from the left
 * (fragments fly rightward). 8 pairs of (velX, velY). */
static const int16_t Smash_FragSpd1[16] = {
     0x400, -0x500,
     0x600, -0x100,
     0x600,  0x100,
     0x400,  0x500,
     0x600, -0x600,
     0x800, -0x200,
     0x800,  0x200,
     0x600,  0x600,
};

/* Smash_FragSpd2: fragment velocities when the wall is broken from the right
 * (fragments fly leftward). */
static const int16_t Smash_FragSpd2[16] = {
    -0x600, -0x600,
    -0x800, -0x200,
    -0x800,  0x200,
    -0x600,  0x600,
    -0x400, -0x500,
    -0x600, -0x100,
    -0x600,  0x100,
    -0x400,  0x500,
};


/* SmashObject — shared subroutine (_incObj/sub SmashObject.asm).
 *
 * Converts the parent object (o) into fragment #0 and spawns d1 additional
 * fragments in free slots. Each fragment's obMap points at ONE sprite piece
 * of the parent's current frame (the "raw mappings" trick), advancing 5
 * bytes per fragment. Every fragment inherits obID from the parent, so the
 * dispatcher routes them into SmashWall/Smash_Fragment. No Points object
 * is spawned here.
 *
 * a4 = fragment velocity table (indexed by obFrame(o) * 2 words)
 * d2 = extra gravity applied to each fragment's initial velY (gravity*2). */
static void SmashObject(uint8_t *o, const int16_t *a4, int d1, int16_t d2) {
    /* moveq #0,d0 / move.b obFrame(a0),d0
     * add.w d0,d0 / adda.w (a3,d0.w),a3
     * The mapping file starts with a table of word offsets, one per frame. */
    uint8_t frame = obFrame(o);
    const uint8_t *map_base = (const uint8_t *)(uintptr_t)obMap(o);
    uint16_t frame_off = *(const uint16_t *)(map_base + frame * 2);
    const uint8_t *a3 = map_base + frame_off;
    a3 += 1;                                    /* addq.w #1: skip piece count byte */

    /* bset #sprite_rawmappings_bit,obRender(a0)
     * move.b obRender(a0),d5 — copy AFTER the bset, so fragments inherit it. */
    obRender(o) |= sprite_rawmappings;
    uint8_t render_flags = obRender(o);
    uint8_t frag_id = obID(o);                  /* _move.b obID(a0),d4 */

    /* First fragment reuses the parent's slot (movea.l a0,a1). */
    uint8_t *frag = o;

    for (int i = 0; i <= d1; i++) {
        if (i != 0) {
            frag = (uint8_t *)FindFreeObj();    /* FixBugs=0 uses plain FindFreeObj */
            if (frag == NULL) goto play_sound;  /* bne.s .playSmashSound */
            a3 += 5;                            /* addq.w #5: next sprite piece */
        }

        /* .loadFirstFrag */
        obRoutine(frag)   = 4;                  /* Smash_Fragment */
        obID(frag)        = frag_id;            /* _move.b d4,obID(a1) */
        obMap(frag)       = (uint32_t)(uintptr_t)a3; /* raw piece pointer */
        obRender(frag)    = render_flags;       /* includes raw-mappings bit */
        obX(frag)         = obX(o);
        obY(frag)         = obY(o);
        obGfx(frag)       = obGfx(o);
        obPriority(frag)  = obPriority(o);
        obActWid(frag)    = obActWid(o);
        obVelX(frag)      = a4[0];
        obVelY(frag)      = a4[1];
        a4 += 2;

        /* FixBugs=0: if the fragment sits BEFORE the parent in RAM, then
         * ExecuteObjects already passed that slot this frame. Manually run
         * one step of Smash_Fragment so it doesn't fall behind. */
        if ((uintptr_t)frag < (uintptr_t)o) {
            SpeedToPos(frag);
            obVelY(frag) = (int16_t)(obVelY(frag) + d2);
            DisplaySprite(frag);                /* ASM: DisplaySprite2 */
        }
    }

play_sound:
    Sound_Queue(sfx_WallSmash, false);          /* move.w #sfx_WallSmash,d0 / jmp QueueSound2 */
}

/* Smash_Main — routine 0 */
static void Smash_Main(uint8_t *o) {
    obRoutine(o) += 2;                              /* addq.b #2 → Smash_Solid */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Smash;
    obGfx(o)      = (uint16_t)(ArtTile_GHZ_SLZ_Smashable_Wall | Tile_Pal3);
    obRender(o)   = sprite_cam_field;
    obActWid(o)   = 32 / 2;
    obPriority(o) = 4;
    obFrame(o)    = obSubtype(o);                   /* 0=left, 1=middle, 2=right */
    /* ASM falls through into Smash_Solid. */
    Smash_Solid(o);
}

/* Smash_Solid — routine 2 */
static void Smash_Solid(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_player);

    /* Remember Sonic's speed BEFORE SolidObject gets a chance to modify it. */
    smash_speed(o) = obVelX(a1);

    {
        int16_t d1 = (int16_t)(32 / 2 + sonic_solid_width);
        int16_t d2 = 64 / 2;
        int16_t d3 = 64 / 2;
        int16_t d4 = obX(o);
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);
    }

    /* btst #5,obStatus(a0) / bne.s .chkroll */
    if (!(obStatus(o) & (1 << 5))) return;

    /* .chkroll: cmpi.b #id_Roll,obAnim(a1) / bne.s .return */
    if (obAnim(a1) != id_Roll) return;

    {
        int16_t d0 = smash_speed(o);
        if (d0 < 0) d0 = (int16_t)(-d0);            /* bpl.s .chkspeed / neg.w d0 */
        if ((uint16_t)d0 < 0x480) return;           /* cmpi.w #$480 / blo.s .return */
    }

    /* Restore Sonic's pre-impact speed and nudge him so the break looks
     * seamless as he passes through. */
    obVelX(a1) = smash_speed(o);
    obX(a1) = (int16_t)(obX(a1) + 4);               /* addq.w #4 */

    const int16_t *a4 = Smash_FragSpd1;
    /* cmp.w obX(a1),d0 / blo.s .smash — wall strictly left of Sonic?
     *   yes → fragments fly right (Spd1)
     *   no  → nudge Sonic back left and use Spd2 */
    if ((uint16_t)obX(o) >= (uint16_t)obX(a1)) {
        obX(a1) = (int16_t)(obX(a1) - 8);           /* subq.w #4*2 */
        a4 = Smash_FragSpd2;
    }

    /* .smash */
    obInertia(a1) = obVelX(a1);
    obStatus(o)  &= ~(1 << 5);                      /* bclr #5,obStatus(a0) */
    obStatus(a1) &= ~(1 << 5);                      /* bclr #5,obStatus(a1) */

    SmashObject(o, a4, 7, (int16_t)(gravity * 2));  /* moveq #8-1,d1 ; move.w #gravity*2,d2 */

    /* ASM falls through: SmashObject converted the parent into fragment #0,
     * so obRoutine(parent) is now 4 and Smash_Fragment runs on this frame. */
    Smash_Fragment(o);
}

/* Smash_Fragment — routine 4 */
static void Smash_Fragment(uint8_t *o) {
    SpeedToPos(o);
    obVelY(o) = (int16_t)(obVelY(o) + gravity * 2); /* addi.w #gravity*2 */

    /* FixBugs=0 order: DisplaySprite first, then delete if offscreen.
     * rts returns to SmashWall, which then calls RememberState — that
     * double-queues the fragment when on-screen, exactly as the original. */
    DisplaySprite(o);
    if (!(obRender(o) & 0x80)) {
        DeleteObject(o);                            /* bpl.w DeleteObject */
    }
}

/* SmashWall dispatcher — Smash_Index: 0/2/4, then RememberState. */
static void SmashWall_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0: Smash_Main(o);     break;
        case 2: Smash_Solid(o);    break;
        case 4: Smash_Fragment(o); break;
    }
    RememberState(obj);   /* bra.w RememberState */
}

/* ===========================================================================
 *  Object 3D — Eggman (GHZ) boss
 *  Ported verbatim from _incObj/3D, 48 Boss - GHZ Main and Wrecking Ball.asm
 *  (REV01, FixBugs=0).
 *
 *  bghz_parentobj = $34 (4-byte slot index, not a pointer)
 *  bghz_timer     = $3C (word)
 *  bghz_sine      = $3F (byte)
 *  obBossX/Y      = $30 / $38 (long, 16.16 fixed-point)
 *  obBossFlash    = $3E (byte, moved from $32 to avoid overlap with X high word)
 *  obBossHits     = $3A (byte, as used by ReactToItem)
 * =========================================================================== */

#define bghz_parentobj(o) (*(uint32_t *)((uint8_t *)(o) + 0x34))
#define bghz_timer(o)     (*(int16_t *)((uint8_t *)(o) + 0x3C))
#define bghz_sine(o)      (*(uint8_t *)((uint8_t *)(o) + 0x3F))

static void BGHZ_Main(uint8_t *o);
static void BGHZ_ShipMain(uint8_t *o);
static void BGHZ_FaceMain(uint8_t *o);
static void BGHZ_FlameMain(uint8_t *o);

/* BGHZ_ObjData: { routine, animation } × 3 */
static const uint8_t BGHZ_ObjData[6] = {
    2, 0,
    4, 1,
    6, 7,
};

/* BGHZ_Main — Routine 0: spawn ship + face + flame. The first iteration
   reuses the parent slot; the other two allocate new slots. */
static void BGHZ_Main(uint8_t *o) {
    const uint8_t *a2 = BGHZ_ObjData;
    uint8_t *a1 = o;

    for (int i = 0; i < 3; i++) {
        if (i > 0) {
            a1 = (uint8_t *)FindNextFreeObj(a1);
            if (!a1) break;
        }
        obRoutine(a1)  = *a2++;
        obID(a1)       = id_BossGreenHill;
        obX(a1)        = obX(o);
        obY(a1)        = obY(o);
        obMap(a1)      = (uint32_t)(uintptr_t)Map_Eggman;
        obGfx(a1)      = (uint16_t)ArtTile_Eggman;
        obRender(a1)   = sprite_cam_field;
        obActWid(a1)   = 64 / 2;
        obPriority(a1) = 3;
        obAnim(a1)     = *a2++;

        /* Store parent's slot INDEX (addresses don't survive 32-bit fields). */
        bghz_parentobj(a1) = (uint32_t)Object_GetIndex(o);
    }

    /* BGHZ_Done */
    obBossX(o) = ((int32_t)(int16_t)obX(o)) << 16;
    obBossY(o) = ((int32_t)(int16_t)obY(o)) << 16;
    obColType(o) = (uint8_t)(col_48x48 | col_boss);
    obBossHits(o) = 8;
}

/* BGHZ_ShipMain — Routine 2 */
static void BGHZ_ShipMain(uint8_t *o) {
    switch (ob2ndRout(o)) {
        case 0x00: { /* BGHZ_ShipStart */
            obVelY(o) = 0x100;
            BossMove(o);
            if ((int16_t)(obBossY(o) >> 16) != (int16_t)(boss_ghz_y + 0x38)) {
                /* fall through to ShipUpdate */
            } else {
                obVelY(o) = 0;
                ob2ndRout(o) += 2;
            }
            break;
        }
        case 0x02: { /* BGHZ_MakeBall */
            obVelX(o) = -0x100;
            obVelY(o) = -0x40;
            BossMove(o);
            if ((int16_t)(obBossX(o) >> 16) != (int16_t)(boss_ghz_x + 0xA0)) {
                break;
            }
            obVelX(o) = 0;
            obVelY(o) = 0;
            ob2ndRout(o) += 2;
            uint8_t *a1 = (uint8_t *)FindNextFreeObj(o);
            if (a1) {
                obID(a1) = id_BossBall;
                obX(a1)  = (int16_t)(obBossX(o) >> 16);
                obY(a1)  = (int16_t)(obBossY(o) >> 16);
                bghz_parentobj(a1) = (uint32_t)Object_GetIndex(o);
            }
            bghz_timer(o) = 120 - 1;
            break;
        }
        case 0x04: { /* BGHZ_ShipMove */
            bghz_timer(o)--;
            if ((int16_t)bghz_timer(o) < 0) {
                ob2ndRout(o) += 2;
                bghz_timer(o) = 0x40 - 1;
                obVelX(o) = 0x100;
                if ((int16_t)(obBossX(o) >> 16) == (int16_t)(boss_ghz_x + 0xA0)) {
                    bghz_timer(o) = (0x40 * 2) - 1;
                    obVelX(o) = 0x40;
                }
            }
            /* BGHZ_Reverse — corre SIEMPRE, en ambos caminos */
            if (!(obStatus(o) & 1)) obVelX(o) = (int16_t)(-obVelX(o));
            break;
        }
        case 0x06: { /* BGHZ_ChgDir */
            bghz_timer(o)--;
            if ((int16_t)bghz_timer(o) < 0) {
                obStatus(o) ^= sprite_xflip;
                bghz_timer(o) = 64 - 1;
                ob2ndRout(o) -= 2;
                obVelX(o) = 0;
            } else {
                BossMove(o);
            }
            break;
        }
        case 0x08: { /* BGHZ_Explode */
            bghz_timer(o)--;
            if ((int16_t)bghz_timer(o) < 0) {
                obStatus(o) |= sprite_xflip;
                obStatus(o) &= ~(1 << 7);
                obVelX(o) = 0;
                ob2ndRout(o) += 2;
                bghz_timer(o) = -38;
                if (!v_bossstatus) v_bossstatus = 1;
                break;
            }
            BossDefeated(o);
            break;
        }
        case 0x0A: { /* BGHZ_Recover */
            bghz_timer(o)++;
            int16_t t = bghz_timer(o);
            if (t == 0) {
                obVelY(o) = 0;
            } else if (t < 0) {
                obVelY(o) = (int16_t)(obVelY(o) + 0x18);
            } else if (t < 0x30) {
                obVelY(o) = (int16_t)(obVelY(o) - 8);
            } else if (t == 0x30) {
                obVelY(o) = 0;
                Sound_Queue(bgm_GHZ, false);
            } else if (t < 0x38) {
                /* nothing */
            } else {
                ob2ndRout(o) += 2;
            }
            BossMove(o);
            break;
        }
        case 0x0C: { /* BGHZ_Escape */
            obVelX(o) = 0x400;
            obVelY(o) = -0x40;
            if (v_limitright2 != boss_ghz_end) {
                v_limitright2 += 2;
            } else if (!(obRender(o) & 0x80)) {
                DeleteObject(o);
                return;
            }
            BossMove(o);
            break;
        }
    }

    /* BGHZ_ShipUpdate (common tail) */
    {
        int16_t s0, s1;
        CalcSine(bghz_sine(o), &s0, &s1);
        int16_t bob = (int16_t)(s0 >> 6);
        bob = (int16_t)(bob + (int16_t)(obBossY(o) >> 16));
        obY(o) = bob;
        obX(o) = (int16_t)(obBossX(o) >> 16);
        bghz_sine(o) += 2;

                if (ob2ndRout(o) < 8) {
            if ((int8_t)obStatus(o) < 0) {          /* defeated flag */
                AddPoints(1000);
                ob2ndRout(o) = 8;
                bghz_timer(o) = 0xB3;
            } else if (obColType(o) == 0) {         /* SOLO cuando ya fue golpeado */
                if (obBossFlash(o) == 0) {
                    obBossFlash(o) = 0x20;
                    Sound_Queue(sfx_HitBoss, false);
                }
                /* .flash — siempre corre una vez dentro del bloque */
                uint16_t *pal = (uint16_t *)RAM_ADDR(v_palette + 0x22);
                uint16_t col = (*pal != 0) ? 0 : cWhite;
                *pal = col;
                obBossFlash(o)--;
                if (obBossFlash(o) == 0) {
                    obColType(o) = (uint8_t)(col_48x48 | col_boss);
                }
            }
        }
    }

    if (Ani_Eggman) AnimateSprite(o, Ani_Eggman);
    /* Copy flip bits from status to render. */
    uint8_t d0 = obStatus(o) & 3;
    obRender(o) = (uint8_t)((obRender(o) & ~(sprite_xflip | sprite_yflip)) | d0);
    DisplaySprite(o);
}

/* BGHZ_FaceMain — Routine 4 */
static void BGHZ_FaceMain(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));
    int8_t d0 = (int8_t)ob2ndRout(a1);        /* ← signed */
    int d1 = 1;                               /* face normal */

    d0 -= 4;
    if (d0 == 0 && (int16_t)(obBossX(a1) >> 16) == (int16_t)(boss_ghz_x + 0xA0)) {
        d1 = 4;                               /* face laugh at default pos */
    }
    d0 -= 6;
    if (d0 >= 0) {                            /* bmi.s .checkHitState */
        d1 = 0xA;                             /* defeated */
    } else if (obColType(a1) == 0) {
        d1 = 5;                               /* face hit */
    } else if (obRoutine(RAM_ADDR(v_player)) >= 4) {
        d1 = 4;                               /* face laugh */
    }

    obAnim(o) = (uint8_t)d1;

    d0 -= 2;
    if (d0 == 0) {                            /* Escape state */
        obAnim(o) = 6;
        if (!(obRender(o) & 0x80)) {
            DeleteObject(o);
            return;
        }
    }
    /* BGHZ_Display (shared with flame) */
    uint8_t *p = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));
    obX(o)      = obX(p);
    obY(o)      = obY(p);
    obStatus(o) = obStatus(p);
    if (Ani_Eggman) AnimateSprite(o, Ani_Eggman);
    uint8_t dd = obStatus(o) & 3;
    obRender(o) = (uint8_t)((obRender(o) & ~(sprite_xflip | sprite_yflip)) | dd);
    DisplaySprite(o);
}

/* BGHZ_FlameMain — Routine 6 */
static void BGHZ_FlameMain(uint8_t *o) {
    obAnim(o) = 7;
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));
    if (ob2ndRout(a1) == 0x0C) {
        obAnim(o) = 0xB;
        if (!(obRender(o) & 0x80)) {
            DeleteObject(o);
            return;
        }
    } else if (obVelX(a1) == 0) {
        /* no move → no flame anim */
    } else {
        obAnim(o) = 8;
    }

    uint8_t *p = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));
    obX(o)      = obX(p);
    obY(o)      = obY(p);
    obStatus(o) = obStatus(p);
    if (Ani_Eggman) AnimateSprite(o, Ani_Eggman);
    uint8_t dd = obStatus(o) & 3;
    obRender(o) = (uint8_t)((obRender(o) & ~(sprite_xflip | sprite_yflip)) | dd);
    DisplaySprite(o);
}

static void BossGreenHill_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: BGHZ_Main(o);      break;
        case 2: BGHZ_ShipMain(o);  break;
        case 4: BGHZ_FaceMain(o);  break;
        case 6: BGHZ_FlameMain(o); break;
    }
}

/* ===========================================================================
 *  Object 48 — Wrecking Ball (GHZ boss chain)
 *  Ported verbatim from _incObj/3D, 48 Boss - GHZ Main and Wrecking Ball.asm
 *  (REV01, FixBugs=0). Shares swing_origX/Y/radius fields with Object 15.
 *
 *  gb_anchorpos  = objoff_32 (word)
 *  gb_linkdist   = objoff_3C (byte) — reuses swing_radius offset
 *  gb_swingdir   = objoff_3D (byte)
 *  gb_swingspeed = objoff_3E (word)
 * =========================================================================== */

static const uint8_t GBall_PosData[6] = { 0, 0x10, 0x20, 0x30, 0x40, 0x60 };

static void GBall_Main(uint8_t *o);
static void GBall_Base(uint8_t *o);
static void GBall_Base2(uint8_t *o);
static void GBall_Link(uint8_t *o);
static void GBall_Ball(uint8_t *o);
static void GBall_UpdateBase(uint8_t *o);

/* GBall_Main — Routine 0: spawn 6 chain links (including the parent). */
static void GBall_Main(uint8_t *o) {
    obRoutine(o) += 2;
    gb_anglew(o) = (int16_t)0x4080;                       /* low byte of move.w #$4080 */
    gb_swingspeed(o) = (int16_t)-0x200;      /* GBall_Swing_Speed = -$200 */
    obMap(o)     = (uint32_t)(uintptr_t)Map_BossItems;
    obGfx(o)     = (uint16_t)ArtTile_Eggman_Weapons;

    uint8_t *a2 = (uint8_t *)o + 0x28;
    *a2++ = 0;                               /* clear obSubtype (count) */

    /* 6 objetos: el padre en la posición 0 y 5 hijos nuevos. */
    uint8_t *a1 = o;
    for (int i = 0; i < 6; i++) {
        if (i > 0) {
            a1 = (uint8_t *)FindNextFreeObj(a1);
            if (!a1) break;

            /* GBall_MakeLinks setup */
            obX(a1)      = obX(o);
            obY(a1)      = obY(o);
            obID(a1)     = id_BossBall;
            obRoutine(a1)= 6;
            obMap(a1)    = (uint32_t)(uintptr_t)Map_Swing_GHZ;
            obGfx(a1)    = (uint16_t)ArtTile_GHZ_MZ_Swing;
            obFrame(a1)  = 1;

            swing_children(o)++;             /* sólo los hijos nuevos */
        }

        /* GBall_LinkSetup común */
        *a2++ = (uint8_t)Object_GetIndex(a1);
        obRender(a1)   = sprite_cam_field;
        obActWid(a1)   = 16 / 2;
        obPriority(a1) = 6;
        bghz_parentobj(a1) = bghz_parentobj(o);
    }

    /* GBall_MakeBall: el último objeto se convierte en la bola. */
    if (a1) {
        obRoutine(a1) = 8;
        obMap(a1)     = (uint32_t)(uintptr_t)Map_GBall;
        obGfx(a1)     = (uint16_t)(ArtTile_GHZ_Giant_Ball | Tile_Pal3);
        obFrame(a1)   = 1;
        obPriority(a1)= 5;
        obColType(a1) = (uint8_t)(col_40x40 | col_hurt);
    }
}

/* GBall_Base — Routine 2 */
static void GBall_Base(uint8_t *o) {
    const uint8_t *a3 = GBall_PosData;
    uint8_t *a2 = (uint8_t *)o + 0x28;
    uint8_t d6 = *(a2);                  /* child count (excluding parent) */
    uint8_t *a1 = o;

    for (int i = 0; i <= d6; i++) {
        uint8_t idx = a2[1 + i];
        a1 = (uint8_t *)Object_GetSlot((int)idx);
        uint8_t target = *a3++;
        if (gb_linkdist(a1) != target) {
            gb_linkdist(a1)++;
        }
    }

    uint8_t target = *(a3 - 1);
    if (gb_linkdist(a1) == target) {
        uint8_t *parent = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));
        if (ob2ndRout(parent) == 6) {
            obRoutine(o) += 2;           /* → GBall_Base2 */
        }
    }

    /* .checkAnchor */
    if (gb_anchorpos(o) != 32) gb_anchorpos(o)++;

    GBall_UpdateBase(o);
        int16_t angle = (int16_t)((uint16_t)gb_anglew(o) >> 8);
    Swing_UpdateSwingPosition_D0(o, angle);
    DisplaySprite(o);
}

/* GBall_Base2 — Routine 4 */
static void GBall_Base2(uint8_t *o) {
    GBall_UpdateBase(o);
    GBall_Move(o);
    DisplaySprite(o);
}

/* GBall_Link — Routine 6: swap to explosion when boss defeated. */
static void GBall_Link(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));
    if ((int8_t)obStatus(a1) < 0) {
        obID(o) = id_Explosion;
        obRoutine(o) = 0;
    }
    DisplaySprite(o);
}

/* GBall_Ball — Routine 8: ball vanish/explode on defeat. */
static void GBall_Ball(uint8_t *o) {
    int d0 = 0;
    if (obFrame(o) == 0) d0 = 1;
    obFrame(o) = (uint8_t)d0;

    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));
    if ((int8_t)obStatus(a1) < 0) {
        obColType(o) = col_none;
        BossDefeated(o);
        uint8_t *p = (uint8_t *)o;
        bghz_timer(p)--;                  /* reuse timer from parent layout; see note */
        if ((int16_t)bghz_timer(p) < 0) {
            obID(o) = id_Explosion;
            obRoutine(o) = 0;
        }
    }
    DisplaySprite(o);
}

/* GBall_UpdateBase — shared animation + parent-follow logic. */
static void GBall_UpdateBase(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bghz_parentobj(o));

    /* Animate: swap obFrame bit 0 every $C0 frames. */
    uint8_t prev = obAniFrame(o);
    obAniFrame(o) = (uint8_t)(prev + 32);
    if (obAniFrame(o) < prev) {           /* byte wrap $C0 → 0 */
        obFrame(o) ^= 1;
    }

    /* swing_origX/Y act as the ball's pivot anchor here. */
    swing_origX(o) = obX(a1);
    int16_t d0 = (int16_t)(obY(a1) + gb_anchorpos(o));
    swing_origY(o) = d0;
    obStatus(o) = obStatus(a1);

    if ((int8_t)obStatus(a1) < 0) {       /* boss defeated */
        obID(o) = id_Explosion;
        obRoutine(o) = 0;
    }
}

static void BossBall_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: GBall_Main(o);   break;
        case 2: GBall_Base(o);   break;
        case 4: GBall_Base2(o);  break;
        case 6: GBall_Link(o);   break;
        case 8: GBall_Ball(o);   break;
    }
}

/* ===========================================================================
 *  Object 3E — Prison Capsule (id_Prison = $3E)
 *  Ported verbatim from _incObj/3E Prison Capsule.asm (REV01, FixBugs=0).
 *
 *  pri_origY = objoff_30 (word): initial Y-position
 *
 *  Subtypes (from Pri_Var):
 *    0 = capsule body  (routine 2)
 *    1 = switch        (routine 4)
 *    2 = unused        (routine 6)
 *    3 = unused        (routine 8)
 * =========================================================================== */

#define pri_origY(obj) (*(int16_t *)((uint8_t *)(obj) + 0x30))

/* Pri_Var: routine, display width, priority, frame per subtype */
static const uint8_t Pri_Var[4][4] = {
    { 2, 64/2, 4, 0 },   /* subtype 0 - capsule   */
    { 4, 24/2, 5, 1 },   /* subtype 1 - switch    */
    { 6, 32/2, 4, 3 },   /* subtype 2 - unused    */
    { 8, 32/2, 3, 5 },   /* subtype 3 - unused    */
};

static void Pri_Main(uint8_t *o);
static void Pri_BodyMain(uint8_t *o);
static void Pri_Switch(uint8_t *o);
static void Pri_Explosion(uint8_t *o);
static void Pri_SpawnAnimals(uint8_t *o);
static void Pri_Animals(uint8_t *o);
static void Pri_EndAct(uint8_t *o);

/* Pri_Main — Routine 0 */
static void Pri_Main(uint8_t *o) {
    obMap(o)     = (uint32_t)(uintptr_t)Map_Pri;
    obGfx(o)     = (uint16_t)ArtTile_Prison_Capsule;
    obRender(o)  = sprite_cam_field;
    pri_origY(o) = obY(o);

    uint8_t sub = obSubtype(o);
    if (sub >= 4) sub = 0;                     /* host safety (unreachable) */
        const uint8_t *v = Pri_Var[sub];
    obRoutine(o)  = v[0];
    obActWid(o)   = v[1];
    obPriority(o) = v[2];
    obFrame(o)    = v[3];

    /* Leftover from the deleted subtypes — only subtype 2 (unused). */
    if (sub == 2) {
        obColType(o)  = (uint8_t)(col_32x32 | col_boss);
        obBossHits(o) = 8;
    }
}

/* Pri_BodyMain — Routine 2: capsule body */
static void Pri_BodyMain(uint8_t *o) {
    if ((uint8_t)v_bossstatus == 2) goto openCapsule;

    {
        int16_t d1 = (int16_t)(64/2 + sonic_solid_width);
        int16_t d2 = 48/2;
        int16_t d3 = 48/2;
        int16_t d4 = obX(o);
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);
    }
    return;

    openCapsule:
    /* Was Sonic standing on the capsule as it opened? */
    if (obSolid(o) != 0) {
        obSolid(o) = 0;
        uint8_t *player = RAM_ADDR(v_player);
        obStatus(player) &= ~(1 << 3);        /* clear on-platform flag */
        obStatus(player) |=  (1 << 1);        /* set in-air flag */
    }
    obFrame(o) = 2;                            /* destroyed prison frame */
}

/* Pri_Switch — Routine 4: capsule switch (stepping on it opens the capsule) */
static void Pri_Switch(uint8_t *o) {
    {
        int16_t d1 = (int16_t)(24/2 + sonic_solid_width);
        int16_t d2 = 16/2;
        int16_t d3 = 16/2;
        int16_t d4 = obX(o);
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);
    }

    if (Ani_Pri) AnimateSprite(o, Ani_Pri);
    obY(o) = pri_origY(o);                     /* force Y back to initial */

    if (obSolid(o) == 0) return;               /* not stepped on yet */

        /* Sonic stepped on the switch — open capsule */
        obY(o) = (int16_t)(obY(o) + 8);
    obRoutine(o) = 0x0A;                       /* → Pri_Explosion */
    *(uint16_t *)((uint8_t *)o + 0x1E) = 1 * 60;   /* obTimeFrame as word */
    f_timecount  = 0;                          /* stop time counter */
    f_lockscreen = 0;                          /* lock screen position */
    f_lockctrl   = 1;                          /* lock controls */
    v_jpadhold2  = btnR;                       /* simulate holding right */
    obSolid(o)   = 0;
    {
        uint8_t *player = RAM_ADDR(v_player);
        obStatus(player) &= ~(1 << 3);
        obStatus(player) |=  (1 << 1);
    }
}

/* Pri_Explosion — Routine $A (also 6/8, but those are unused) */
static void Pri_Explosion(uint8_t *o) {
    /* Spawn an explosion every 8 frames, based on VBlank frame counter */
    if (((uint8_t)v_vblank_byte & 7) == 0) {
        uint8_t *a1 = (uint8_t *)FindFreeObj();
        if (a1) {
            obID(a1) = id_Explosion;
            obX(a1)  = obX(o);
            obY(a1)  = obY(o);

            /* Random X/Y offset around the prison */
            uint16_t r = RandomNumber();
            int16_t dx = (int16_t)((uint8_t)r >> 2);   /* lsr.b #2 */
            dx = (int16_t)(dx - 32);                   /* subi.w #32 */
            obX(a1) = (int16_t)(obX(a1) + dx);

            uint8_t hi = (uint8_t)(r >> 8);            /* lsr.w #8 */
            hi = (uint8_t)(hi >> 3);                   /* lsr.b #3 */
            obY(a1) = (int16_t)(obY(a1) + (int16_t)hi);
        }
    }

    /* Timer: when it expires, replace explosions with animals */
    uint16_t *timer = (uint16_t *)((uint8_t *)o + 0x1E);
    *timer = (uint16_t)(*timer - 1);
    if (*timer == 0) {
        Pri_SpawnAnimals(o);
    }
}

/* Pri_SpawnAnimals: replaces the switch with 8 animals */
static void Pri_SpawnAnimals(uint8_t *o) {
    v_bossstatus = 2;                          /* mark prison as opened */
    obRoutine(o) = 0x0C;                       /* → Pri_Animals */
    obFrame(o)   = 6;                          /* hide switch */
    *(uint16_t *)((uint8_t *)o + 0x1E) = (2 * 60) + 30;
    obY(o) = (int16_t)(obY(o) + 32);           /* load animals 32px below */

    /* 8 animals with staggered hop-out delays (roughly 2.5s start) */
    int16_t  d4 = -28;                         /* start X-offset */
    uint16_t d5 = (2 * 60) + 34;               /* start hop-out delay */
    for (int i = 0; i < 8; i++) {
        uint8_t *a1 = (uint8_t *)FindFreeObj();
        if (!a1) return;                       /* object RAM full */
            obID(a1) = id_Animals;
        obX(a1)  = obX(o);
        obY(a1)  = obY(o);
        obX(a1)  = (int16_t)(obX(a1) + d4);
        d4 = (int16_t)(d4 + 7);
        animal_prisondelay(a1) = d5;
        d5 = (uint16_t)(d5 - 8);
    }
}

/* Pri_Animals — Routine $C: continue spawning animals until timer expires */
static void Pri_Animals(uint8_t *o) {
    /* Spawn one animal every 8 frames */
    if (((uint8_t)v_vblank_byte & 7) == 0) {
        uint8_t *a1 = (uint8_t *)FindFreeObj();
        if (a1) {
            obID(a1) = id_Animals;
            obX(a1)  = obX(o);
            obY(a1)  = obY(o);

            uint16_t r = RandomNumber();
            int16_t d0 = (int16_t)(r & 0x1F);  /* andi.w #$1F */
            d0 = (int16_t)(d0 - 6);            /* subq.w #6 */
            /* ASM: tst.w d1 ; bpl.s .setX. After RandomNumber, d1 = seed
             *              whose low word is 0, so bpl is always taken — the neg.w d0
             *              branch is dead code. Skipped here for the same reason. */
            obX(a1) = (int16_t)(obX(a1) + d0);
            animal_prisondelay(a1) = 12;       /* hop out almost instantly */
        }
    }

    /* Timer until we start checking for remaining animals */
    uint16_t *timer = (uint16_t *)((uint8_t *)o + 0x1E);
    *timer = (uint16_t)(*timer - 1);
    if (*timer != 0) return;

    obRoutine(o) += 2;                         /* → Pri_EndAct */
    /* FixBugs=0: leftover from prototype — sets a 3-second delay that is
     *      never read. Kept for 1:1 fidelity. */
    *timer = 3 * 60;
}

/* Pri_EndAct — Routine $E: wait for all animals to despawn, then end act */
static void Pri_EndAct(uint8_t *o) {
    /* FixBugs=0: the loop range is nonsensical (only covers the first half
     *      of object RAM starting at slot 1, missing most of lvlobjspace where
     *      animals actually live). Ported as-is for fidelity. */
    int count = (int)((v_objspace_end - (v_objspace + object_size * 1))
    / object_size) / 2 - 1;
    uint8_t *a1 = RAM_ADDR(v_objspace + object_size * 1);

    for (int i = 0; i <= count; i++) {
        if (obID(a1) == id_Animals) return;    /* still has animals → wait */
            a1 += object_size;
    }

    /* No animals left — launch end-of-level cards and delete the prison */
    GotThroughAct();
    DeleteObject(o);
}

/* Prison dispatcher — Pri_Index: 0/2/4/6/8/A/C/E
 * ASM: jsr Pri_Index ; out_of_range.s .delete ; jmp DisplaySprite
 *      .delete: jmp DeleteObject */
static void Prison_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00: Pri_Main(o);       break;
        case 0x02: Pri_BodyMain(o);   break;
        case 0x04: Pri_Switch(o);     break;
        case 0x06:
        case 0x08:
        case 0x0A: Pri_Explosion(o);  break;
        case 0x0C: Pri_Animals(o);    break;
        case 0x0E: Pri_EndAct(o);     break;
    }

    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
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
    if (OutOfRange(o, -1, 0)) {                                 /* out_of_range.w DeleteObject */
        DeleteObject(o);
    }
}

/* ===========================================================================
 *  Object 42 — Newtron enemy (GHZ, también colocado en SYZ)
 *  Ported verbatim from _incObj/42 Badnik - Newtron.asm (REV01, FixBugs=0).
 *
 *  newt_fired = objoff_32 (byte): flag que se activa cuando el Newtron verde
 *                                  ya disparó su misil (evita re-disparar)
 *
 *  Subtype:
 *    0 = Newtron azul  (cae, vuela en línea recta, no dispara)
 *    1 = Newtron verde (dispara misiles al llegar al frame 2)
 *
 *  Notas de traducción:
 *    - La rama `Newt_Action_WaitDrop → Newt_Action_Drop` es un tail-call en
 *      el ASM (bhs.s fuera del cuerpo). Se traduce como llamada + return.
 *    - Los dos bloques `if FixBugs=0 … endif` de Newt_Action_Drop están
 *      deshabilitados por compilación con FixBugs=0, así que se omiten.
 * =========================================================================== */

#define newt_fired(obj) (*(uint8_t *)((uint8_t *)(obj) + 0x32)) /* objoff_32 */

static void Newt_Main(uint8_t *o);
static void Newt_Action(uint8_t *o);
static void Newt_GreenDelete(uint8_t *o);
static void Newt_Action_ChkDistance(uint8_t *o);
static void Newt_Action_WaitDrop(uint8_t *o);
static void Newt_Action_Drop(uint8_t *o);
static void Newt_Action_MoveOnFloor(uint8_t *o);
static void Newt_Action_MoveInAir(uint8_t *o);
static void Newt_Action_GreenNewtron(uint8_t *o);

/* Newt_Main — Routine 0 */
static void Newt_Main(uint8_t *o) {
    obRoutine(o) += 2;                             /* addq.b #2 → Newt_Action */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Newt; /* move.l #Map_Newt,obMap */
    obGfx(o)      = (uint16_t)ArtTile_Newtron;     /* move.w #ArtTile_Newtron */
    obRender(o)   = sprite_cam_field;              /* move.b #sprite_cam_field */
    obPriority(o) = 4;                             /* move.b #4 */
    obActWid(o)   = 40 / 2;                        /* move.b #40/2 */
    obHeight(o)   = 32 / 2;                        /* move.b #32/2 */
    obWidth(o)    = 16 / 2;                        /* move.b #16/2 */
}

/* Newt_Action — Routine 2: dispatch por ob2ndRout, luego anima y recuerda. */
static void Newt_Action(uint8_t *o) {
    switch (ob2ndRout(o)) {                        /* Newt_ActIndex */
        case 0: Newt_Action_ChkDistance(o);  break;
        case 2: Newt_Action_WaitDrop(o);     break;
        case 4: Newt_Action_MoveOnFloor(o);  break;
        case 6: Newt_Action_MoveInAir(o);    break;
        case 8: Newt_Action_GreenNewtron(o); break;
    }

    if (Ani_Newt) AnimateSprite(o, Ani_Newt);      /* lea (Ani_Newt).l,a1 */
        RememberState(o);                              /* bra.w RememberState */
}

/* Newt_Action_ChkDistance — ¿Sonic está a menos de 128 px? Si sí, arranca. */
static void Newt_Action_ChkDistance(uint8_t *o) {
    obStatus(o) |= (1 << 0);                       /* bset #0 */
    int16_t d0 = (int16_t)(obX(RAM_ADDR(v_player)) - obX(o));
    if (d0 < 0) {                                  /* bhs.s .chkDistance (salta si >= 0) */
        d0 = (int16_t)(-d0);                       /* neg.w d0 */
        obStatus(o) &= (uint8_t)~(1 << 0);         /* bclr #0 */
    }
    /* .chkDistance */
    if ((uint16_t)d0 >= 128u) return;              /* cmpi.w #128 / bhs.s .return */

        ob2ndRout(o) += 2;                             /* addq.b #2 → WaitDrop */
        obAnim(o) = 1;                                 /* move.b #1,obAnim (.drop) */

        if (obSubtype(o) == 0) return;                 /* tst.b obSubtype / beq.s .return */
            obGfx(o) = (uint16_t)(ArtTile_Newtron | Tile_Pal2);
    ob2ndRout(o) = 8;                              /* move.b #8 → GreenNewtron */
    obAnim(o) = 4;                                 /* move.b #4,obAnim (.fires) */
}

/* Newt_Action_WaitDrop — espera que la animación de aparición llegue a frame 4 */
static void Newt_Action_WaitDrop(uint8_t *o) {
    if ((uint8_t)obFrame(o) >= 4u) {               /* cmpi.b #4 / bhs.s Newt_Action_Drop */
        Newt_Action_Drop(o);
        return;
    }
    obStatus(o) |= (1 << 0);                       /* bset #0 */
    int16_t d0 = (int16_t)(obX(RAM_ADDR(v_player)) - obX(o));
    if (d0 < 0) {                                  /* bhs.s .return (salta si >= 0) */
        obStatus(o) &= (uint8_t)~(1 << 0);         /* bclr #0 */
    }
}

/* Newt_Action_Drop — cae hasta el suelo, luego vuela en horizontal */
static void Newt_Action_Drop(uint8_t *o) {
    /* FixBugs=0: el bloque "frame 1 → col_40x32" está deshabilitado. */

    ObjectFall(o);                                 /* bsr.w ObjectFall */
    int16_t d1, d3;
    ObjFloorDist(o, &d1, &d3);                     /* bsr.w ObjFloorDist */
    if (d1 >= 0) return;                           /* tst.w d1 / bpl.s .return */
        obY(o) = (int16_t)(obY(o) + d1);               /* add.w d1,obY: aterriza */
        obVelY(o) = 0;                                 /* move.w #0,obVelY */
        ob2ndRout(o) += 2;                             /* addq.b #2 → MoveOnFloor */
        obAnim(o) = 2;                                 /* move.b #2,obAnim (.fly1) */

        /* FixBugs=0: el bloque "Newtron verde → .fly2" está deshabilitado. */

        obColType(o) = (uint8_t)(col_40x16 | col_badnik); /* destruible, 40x16 */
        obVelX(o) = 0x200;                             /* move.w #$200: vuela a la derecha */
        if (obStatus(o) & 1) return;                   /* btst #0 / bne.s .return */
            obVelX(o) = (int16_t)(-obVelX(o));             /* neg.w: vuela a la izquierda */
}

/* Newt_Action_MoveOnFloor — vuela en horizontal, alineado al piso */
static void Newt_Action_MoveOnFloor(uint8_t *o) {
    SpeedToPos(o);                                 /* bsr.w SpeedToPos */

    int16_t d1, d3;
    ObjFloorDist(o, &d1, &d3);                     /* bsr.w ObjFloorDist */
    if (d1 < -8 || d1 >= 0x0C) {                   /* cmpi.w #-8 blt / cmpi.w #$C bge */
        ob2ndRout(o) += 2;                         /* .detach → MoveInAir */
        return;
    }
    obY(o) = (int16_t)(obY(o) + d1);               /* add.w d1,obY: pegado al piso */
}

/* Newt_Action_MoveInAir — sigue volando sin alinearse al piso */
static void Newt_Action_MoveInAir(uint8_t *o) {
    SpeedToPos(o);                                 /* bsr.w SpeedToPos */
}

/* Newt_Action_GreenNewtron — comportamiento del Newtron verde */
static void Newt_Action_GreenNewtron(uint8_t *o) {
    if ((uint8_t)obFrame(o) == 1) {                /* cmpi.b #1 / bne.s .chkFire */
        obColType(o) = (uint8_t)(col_40x32 | col_badnik);
    }

    /* .chkFire */
    if ((uint8_t)obFrame(o) != 2) return;          /* cmpi.b #2 / bne.s .return */
        if (newt_fired(o) != 0) return;                /* tst.b newt_fired / bne.s .return */
            newt_fired(o) = 1;                             /* move.b #1,newt_fired */

            uint8_t *a1 = (uint8_t *)FindFreeObj();        /* bsr.w FindFreeObj */
            if (!a1) return;                               /* bne.s .return */

                obID(a1)   = id_Missile;                       /* _move.b #id_Missile,obID */
                obX(a1)    = obX(o);                           /* move.w obX(a0),obX(a1) */
                obY(a1)    = obY(o);                           /* move.w obY(a0),obY(a1) */
                obY(a1)    = (int16_t)(obY(a1) - 8);           /* subq.w #8,obY(a1) */
                obVelX(a1) = 0x200;                            /* move.w #$200,obVelX(a1) */

                int16_t d0 = 0x14;                             /* move.w #$14,d0 */
                if ((obStatus(o) & 1) == 0) {                  /* btst #0 / bne.s .alignX */
                    d0 = (int16_t)(-d0);                       /* neg.w d0 */
                    obVelX(a1) = (int16_t)(-obVelX(a1));       /* neg.w obVelX(a1) */
                }
                /* .alignX */
                obX(a1) = (int16_t)(obX(a1) + d0);             /* add.w d0,obX(a1) */

                obStatus(a1)  = obStatus(o);                   /* copia X-flip al misil */
                obSubtype(a1) = 1;                             /* "from Newtron" */
}

/* Newt_GreenDelete — Routine 4: el Newtron verde se autodestruye al terminar
 *  su ciclo de animación (activado por afRoutine $FC en Ani_Newt). */
static void Newt_GreenDelete(uint8_t *o) {
    DeleteObject(o);                               /* bra.w DeleteObject */
}

/* Newtron dispatcher — Newt_Index: 0=Main, 2=Action, 4=GreenDelete */
static void Newtron_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0: Newt_Main(o);        break;
        case 2: Newt_Action(o);      break;
        case 4: Newt_GreenDelete(o); break;
    }
}

/* ===========================================================================
 *  Object 43 — Roller enemy (SYZ)
 *  Ported verbatim from _incObj/43 Badnik - Roller.asm (REV01, FixBugs=0).
 * =========================================================================== */

/* Bandera usada por Roll_Action_FromLeft para replicar el `addq.l #4,sp`
   que en el ASM salta el return a Roll_Action. Reset cada frame desde
   Roll_Action. */
static int _roll_skip_action = 0;

#define roll_waitunfolded(o) (*(int16_t *)((uint8_t *)(o) + 0x30))
#define roll_stateflags(o)   (*(uint8_t *)((uint8_t *)(o) + 0x32))

static void Roll_Action(uint8_t *o);
static void Roll_Action_FromLeft(uint8_t *o);
static void Roll_Action_Unfolded(uint8_t *o);
static void Roll_Action_Rolling(uint8_t *o);
static void Roll_Action_Jumping(uint8_t *o);
static void Roll_Action_StopAndUnfold(uint8_t *o);

/* --- Roll_Main — Routine 0 ------------------------------------------- */
static void Roll_Main(uint8_t *o) {
    obHeight(o) = 28 / 2;
    obWidth(o)  = 16 / 2;

    ObjectFall(o);
    int16_t d1, d3;
    ObjFloorDist(o, &d1, &d3);
    if (d1 >= 0) return;

    obY(o) = (int16_t)(obY(o) + d1);
    obVelY(o) = 0;
    obRoutine(o) += 2;
    obMap(o)      = (uint32_t)(uintptr_t)Map_Roll;
    obGfx(o)      = (uint16_t)ArtTile_Roller;
    obRender(o)   = sprite_cam_field;
    obPriority(o) = 4;
    obActWid(o)   = 32 / 2;
}

/* --- Roll_Action — Routine 2 ---------------------------------------- */
static void Roll_Action(uint8_t *o) {
    _roll_skip_action = 0;                                   /* reset por frame */

    switch (ob2ndRout(o)) {                                  /* Roll_ActIndex */
        case 0: Roll_Action_FromLeft(o); break;
        case 2: Roll_Action_Unfolded(o); break;
        case 4: Roll_Action_Rolling(o);  break;
        case 6: Roll_Action_Jumping(o);  break;
    }

    /* Roll_Action_FromLeft pudo pedir "skip" (replica del addq.l #4,sp). */
    if (_roll_skip_action) return;

    if (Ani_Roll) AnimateSprite(o, Ani_Roll);

    /* Chequeo offscreen FixBugs=0 (bgt signed en lugar de bhi unsigned). */
    {
        uint16_t d0 = (uint16_t)obX(o);
        d0 &= 0xFF80;
        uint16_t d1 = (uint16_t)RAM_WORD(0xF700);            /* v_screenposx low word */
        d1 = (uint16_t)(d1 - 0x80);
        d1 &= 0xFF80;
        d0 = (uint16_t)(d0 - d1);
        if ((int16_t)d0 > 0x280) {                           /* bgt signed */
            uint8_t *a2 = RAM_ADDR(v_objstate);
            uint8_t idx = obRespawnNo(o);
            if (idx != 0) {
                a2[2 + idx] &= (uint8_t)~0x80;
            }
            DeleteObject(o);
            return;
        }
        DisplaySprite(o);
    }
}

/* --- Roll_Action_FromLeft — sub-rutina 0 ---------------------------- */
static void Roll_Action_FromLeft(uint8_t *o) {
    uint8_t *player = RAM_ADDR(v_player);
    uint16_t sonicX = (uint16_t)obX(player);

    /* subi.w #256,d0 ; blo.s .returnAndSkip  (borrow real, no bit 15) */
    if (sonicX < 256u) { _roll_skip_action = 1; return; }
    uint16_t d0 = (uint16_t)(sonicX - 256u);

    /* sub.w obX(a0),d0 ; blo.s .returnAndSkip */
    if (d0 < (uint16_t)obX(o)) { _roll_skip_action = 1; return; }

    ob2ndRout(o) += 4;
    obAnim(o) = 2;
    obVelX(o) = 0x700;
    obColType(o) = (uint8_t)(col_28x28 | col_hurt);

    _roll_skip_action = 1;   /* camino de activación también salta */
}

/* --- Roll_Action_Unfolded — sub-rutina 2 ---------------------------- */
static void Roll_Action_Unfolded(uint8_t *o) {
    if (obAnim(o) == 2) {
        ob2ndRout(o) += 2;
        return;
    }
    roll_waitunfolded(o)--;
    if (roll_waitunfolded(o) >= 0) return;

    obAnim(o) = 1;
    obVelX(o) = 0x700;
    obColType(o) = (uint8_t)(col_28x28 | col_hurt);
}

/* --- Roll_Action_Rolling — sub-rutina 4 ----------------------------- */
static void Roll_Action_Rolling(uint8_t *o) {
    Roll_Action_StopAndUnfold(o);
    SpeedToPos(o);

    int16_t d1, d3;
    ObjFloorDist(o, &d1, &d3);
    if (d1 < -8) goto ledge_hit;
    if (d1 >= 0x0C) goto ledge_hit;
    obY(o) = (int16_t)(obY(o) + d1);
    return;

ledge_hit:
    ob2ndRout(o) += 2;
    {
        uint8_t was = roll_stateflags(o) & 1;
        roll_stateflags(o) |= 1;
        if (!was) return;
    }
    obVelY(o) = (int16_t)-0x600;
}

/* --- Roll_Action_Jumping — sub-rutina 6 ----------------------------- */
static void Roll_Action_Jumping(uint8_t *o) {
    ObjectFall(o);

    if ((int16_t)obVelY(o) < 0) return;

    int16_t d1, d3;
    ObjFloorDist(o, &d1, &d3);
    if (d1 >= 0) return;

    obY(o) = (int16_t)(obY(o) + d1);
    ob2ndRout(o) -= 2;
    obVelY(o) = 0;
}

/* --- Roll_Action_StopAndUnfold -------------------------------------- */
static void Roll_Action_StopAndUnfold(uint8_t *o) {
    if ((int8_t)roll_stateflags(o) < 0) return;

    uint8_t *player = RAM_ADDR(v_player);
    uint16_t sonicX = (uint16_t)obX(player);

    /* subi.w #48,d0 ; blo si Sonic_X < 48 (borrow) */
    uint16_t a = (uint16_t)(sonicX - 48u);
    if (sonicX < 48u) return;                /* underflow → no trigger */

    /* sub.w obX(a0),d0 ; bhs.s .return  ⇔  si a >= obX, salir */
    if (a >= (uint16_t)obX(o)) return;

    obAnim(o) = 0;
    obColType(o) = (uint8_t)(col_28x28 | col_badnik);
    obVelX(o) = 0;
    roll_waitunfolded(o) = 2 * 60;
    ob2ndRout(o) = 2;
    roll_stateflags(o) |= 0x80;
}

/* --- Dispatcher Object 43 ------------------------------------------- */
static void Roller_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Roll_Main(o);   break;
        case 2: Roll_Action(o); break;
    }
}

/* ===========================================================================
 *  Object 46 — Solid blocks and blocks that fall from the ceiling (MZ)
 *  Ported verbatim from _incObj/46 MZ Bricks.asm (REV01, FixBugs=0).
 *
 *  Fields:
 *    brick_origY = objoff_30 (word): initial Y-position used by wobble effect
 *    (objoff_32 = $5C0 es un leftover de FixBugs=0, sin efecto)
 * =========================================================================== */

#define brick_origY(o) (*(int16_t *)((uint8_t *)(o) + 0x30))

static void Brick_Action(uint8_t *o);
static void Brick_Type00(uint8_t *o);
static void Brick_Type01(uint8_t *o);
static void Brick_Type02(uint8_t *o);
static void Brick_Type03(uint8_t *o);
static void Brick_Type04(uint8_t *o);

/* --- Brick_Main — Routine 0 --- */
static void Brick_Main(uint8_t *o) {
    obRoutine(o) += 2;                                  /* → Brick_Action */
    obHeight(o)  = 30 / 2;
    obWidth(o)   = 30 / 2;
    obMap(o)     = (uint32_t)(uintptr_t)Map_Brick;
    obGfx(o)     = (uint16_t)(ArtTile_Level | Tile_Pal3);
    obRender(o)  = sprite_cam_field;
    obPriority(o)= 3;
    obActWid(o)  = 32 / 2;
    brick_origY(o) = obY(o);

    /* FixBugs=0 leftover sin propósito conocido */
    *(int16_t *)((uint8_t *)o + 0x32) = 0x5C0;

    /* Fall-through a Brick_Action */
    Brick_Action(o);
}

/* --- Brick_Action — Routine 2 --- */
static void Brick_Action(uint8_t *o) {
    if ((int8_t)obRender(o) >= 0) goto chkdel;          /* tst.b obRender / bpl.s .chkdel */

    {
        uint8_t d0 = (uint8_t)(obSubtype(o) & 7);       /* andi.w #7,d0 */
        switch (d0) {
            case 0: Brick_Type00(o); break;
            case 1: Brick_Type01(o); break;
            case 2: Brick_Type02(o); break;
            case 3: Brick_Type03(o); break;
            case 4: Brick_Type04(o); break;
            /* 5..7 no tienen entrada en Brick_TypeIndex. En el ASM leerían
             * fuera de la tabla; en la práctica no se usan. */
        }
    }

    {
        int16_t d1 = (int16_t)(32 / 2 + sonic_solid_width);
        int16_t d2 = 32 / 2;
        int16_t d3 = 34 / 2;
        int16_t d4 = obX(o);
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);
    }

    chkdel:
    /* REV01 / FixBugs=0: out_of_range primero, DisplaySprite después. */
    if (OutOfRange(o, -1, 0)) {                               /* out_of_range.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                                   /* bra.w DisplaySprite */
}

/* --- Tipo 00: bloque estático --- */
static void Brick_Type00(uint8_t *o) {
    (void)o;                                            /* rts */
}

/* --- Tipo 01: wobble rápido (techo) --- */
static void Brick_Type01(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x16); /* freq 8, mid $40 */
    if (obSubtype(o) & (1 << 3)) {                      /* btst #3,obSubtype */
        d0 = (int16_t)(-d0);                            /* neg.w d0 */
        d0 = (int16_t)(d0 + 0x10);                      /* addi.w #$10,d0 */
    }
    obY(o) = (int16_t)(brick_origY(o) - d0);            /* sub.w d0,d1 ; move.w d1,obY */
}

/* --- Tipo 02: cae cuando Sonic se acerca --- */
static void Brick_Type02(uint8_t *o) {
    int16_t d0 = (int16_t)(obX(RAM_ADDR(v_player)) - obX(o));
    if (d0 < 0) d0 = (int16_t)(-d0);                    /* neg.w d0 */
        if ((uint16_t)d0 >= 0x90) {                         /* cmpi.w #$90 / bhs.s */
            Brick_Type01(o);                                /* no está cerca: seguir wobble */
            return;
        }
        obSubtype(o) = 3;                                   /* cae */
}

/* --- Tipo 03: cae hasta tocar el piso --- */
static void Brick_Type03(uint8_t *o) {
    SpeedToPos(o);
    obVelY(o) = (int16_t)(obVelY(o) + 0x18);            /* addi.w #$18 */

    int16_t dist, angle;
    ObjFloorDist(o, &dist, &angle);                     /* bsr.w ObjFloorDist */
    if (dist >= 0) return;                              /* tst.w d1 / bpl.w .return */

        obY(o) = (int16_t)(obY(o) + dist);                  /* add.w d1,obY: alinear al piso */
        obVelY(o) = 0;                                      /* clr.w obVelY */
        brick_origY(o) = obY(o);                            /* move.w obY,brick_origY */
        obSubtype(o) = 4;                                   /* → wobble lento sobre lava */

        /* FixBugs=0 / REV01: comprueba si el bloque 16×16 bajo el brick es lava.
         * Después de ObjFloorDist, v_last_floor_block contiene el word del bloque
         * que el ASM deja en a1 (mismo efecto secundario, misma semántica de
         * "última llamada a FindNearestTile gana"). */
        {
            uint16_t block_id = (uint16_t)(v_last_floor_block & 0x3FF); /* andi.w #$3FF */
            if (block_id >= 0x16A) return;                  /* cmpi.w #$16A / bhs.s .return */
                obSubtype(o) = 0;                               /* no es lava → estático */
        }
}

/* --- Tipo 04: wobble lento (sobre lava) --- */
static void Brick_Type04(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x12); /* freq 8, mid $40 */
    d0 = (int16_t)((uint16_t)d0 >> 3);                  /* lsr.w #3,d0 */
    obY(o) = (int16_t)(brick_origY(o) - d0);            /* sub.w d0,d1 ; move.w d1,obY */
}

/* --- Dispatcher Object 46 --- */
static void MarbleBrick_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                             /* Brick_Index */
        case 0: Brick_Main(o);   break;
        case 2: Brick_Action(o); break;
    }
}

/* ===========================================================================
 *  Object 47 — Pinball Bumper (SYZ)
 *  Ported verbatim from _incObj/47 SYZ Bumper.asm (REV01, FixBugs=0).
 *
 *  ReactToItem incrementa obColProp cuando Sonic toca el bumper
 *  (col_16x16_alt | col_special → React_Special). Bump_Hit detecta el flag,
 *  calcula el ángulo Sonic→bumper, y lanza a Sonic en dirección opuesta con
 *  una fuerza de $700 (16.8 fixed point).
 *
 *  El respawn byte cuenta los impactos; al llegar a $8A (bit 7 + 10 en
 *  los bits bajos, según la convención del ASM) deja de dar puntos.
 * =========================================================================== */

static void Bump_Main(uint8_t *o);
static void Bump_Hit(uint8_t *o);
static void Bump_Display(uint8_t *o);

/* --- Bump_Main — Routine 0 ------------------------------------------- */
static void Bump_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* → Bump_Hit */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Bump;
    obGfx(o)      = (uint16_t)ArtTile_SYZ_Bumper;
    obRender(o)   = sprite_cam_field;
    obActWid(o)   = 32 / 2;
    obPriority(o) = 1;                                       /* encima de Sonic */
    obColType(o)  = (uint8_t)(col_16x16_alt | col_special);  /* ReactToItem → D7 */
    /* ASM falls through into Bump_Hit on the first frame. */
    Bump_Hit(o);
}

/* --- Bump_Hit — Routine 2 -------------------------------------------- */
static void Bump_Hit(uint8_t *o) {
    if (obColProp(o) == 0) {                                 /* tst.b obColProp / beq.w Bump_Display */
        Bump_Display(o);
        return;
    }
    obColProp(o) = 0;                                        /* clr.b obColProp: consumir flag */

    uint8_t *player = RAM_ADDR(v_player);

    /* d1 = bumper_x - sonic_x, d2 = bumper_y - sonic_y */
    int16_t d1 = (int16_t)(obX(o) - obX(player));
    int16_t d2 = (int16_t)(obY(o) - obY(player));

    /* Ángulo Sonic→bumper, y su seno/coseno */
    uint8_t angle = CalcAngle(d1, d2);
    int16_t s0, s1;
    CalcSine(angle, &s0, &s1);                               /* s0=sin, s1=cos */

    /* Fuerza de rebote $700 en 16.8 fixed, con signo negativo (alejar). */
    int16_t vx = (int16_t)(((int32_t)s1 * -0x700) >> 8);     /* cos * -$700 */
    int16_t vy = (int16_t)(((int32_t)s0 * -0x700) >> 8);     /* sin * -$700 */
    obVelX(player) = vx;
    obVelY(player) = vy;

    obStatus(player) |=  (1 << 1);                           /* bset #1: airborne */
    obStatus(player) &= ~(1 << 4);                           /* bclr #4: roll-jump */
    obStatus(player) &= ~(1 << 5);                           /* bclr #5: push */
    jumping(player) = 0;                                     /* clr.b jumping */

    obAnim(o) = 1;                                           /* bumper: animación "hit" */
    Sound_Queue(sfx_Bumper, false);                          /* QueueSound2 */

    /* Puntuación condicional: cuenta impactos en el respawn byte */
    {
        uint8_t *a2 = RAM_ADDR(v_objstate);
        uint8_t d0 = obRespawnNo(o);
        if (d0 != 0) {
            /* cmpi.b #10+$80, 2(a2,d0.w) → si >= $8A, no dar más puntos */
            if (a2[2 + d0] >= 0x8A) {
                Bump_Display(o);
                return;
            }
            a2[2 + d0] = (uint8_t)(a2[2 + d0] + 1);
        }
    }
    AddPoints(1);                                            /* moveq #1,d0 → 10 pts */

    /* Spawn de objeto Points con frame "10" */
    {
        uint8_t *slot = (uint8_t *)FindFreeObj();
        if (slot != NULL) {
            obID(slot)    = id_Points;
            obX(slot)     = obX(o);
            obY(slot)     = obY(o);
            obFrame(slot) = 4;                               /* frame "10" */
        }
    }

    Bump_Display(o);
}

/* --- Bump_Display ---------------------------------------------------- */
static void Bump_Display(uint8_t *o) {
    if (Ani_Bump) AnimateSprite(o, Ani_Bump);                /* lea (Ani_Bump).l,a1 */

    if (OutOfRange(o, -1, 0)) {                                    /* out_of_range.s .delete */
        /* .delete: limpiar flag de respawn-block y borrar */
        uint8_t *a2 = RAM_ADDR(v_objstate);
        uint8_t d0 = obRespawnNo(o);
        if (d0 != 0) {
            a2[2 + d0] &= (uint8_t)~0x80;                    /* bclr #7: permitir respawn */
        }
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                                        /* bra.w DisplaySprite */
}

/* --- Dispatcher Object 47 — Bump_Index: 0 = Main, 2 = Hit ----------- */
static void Bumper_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                                  /* Bump_Index */
        case 0: Bump_Main(o); break;
        case 2: Bump_Hit(o);  break;
    }
}

/* ===========================================================================
   Object 4B — Giant Ring (entry to Special Stage)
   Object 7C — Giant Ring flash
   Ported from _incObj/4B, 7C Giant Ring and Flash.asm (REV01, FixBugs=0).

   gring_parent = objoff_3C (long): 4-byte field. Igual que msl_parent en
   el port de Buzz Bomber, almacenamos el ÍNDICE DE SLOT del padre (word,
   < 128) porque los punteros de x86-64 no caben en un campo de 32 bits.
   =========================================================================== */

#define gring_parent(obj) (*(uint32_t *)((uint8_t *)(obj) + 0x3C)) /* objoff_3C */

static void GRing_Main(uint8_t *o);
static void GRing_Animate(uint8_t *o);
static void GRing_Collect(uint8_t *o);
static void GRing_Delete(uint8_t *o);
static void Flash_Main(uint8_t *o);
static void Flash_ChkDel(uint8_t *o);
static void Flash_Delete(uint8_t *o);
static void Flash_Collect(uint8_t *o);

/* --- GRing_Main (Routine 0) --------------------------------------------- */
static void GRing_Main(uint8_t *o) {
    obMap(o) = (uint32_t)(uintptr_t)Map_GRing;          /* move.l #Map_GRing */
    obGfx(o) = (uint16_t)(ArtTile_Giant_Ring | Tile_Pal2);
    obRender(o) |= sprite_cam_field;                    /* ori.b #sprite_cam_field */
    obActWid(o) = 128 / 2;                              /* move.b #128/2 */

    /* tst.b obRender / bpl.s GRing_Animate: ring off-screen, just animate */
    if (!(obRender(o) & sprite_rendered)) {
        GRing_Animate(o);
        return;
    }
    /* cmpi.b #ss_emeralds_num,(v_emeralds).w / beq.w GRing_Delete */
    if (v_emeralds == ss_emeralds_num) {
        GRing_Delete(o);
        return;
    }
    /* cmpi.w #ss_giantring_rings,(v_rings).w / bhs.s GRing_Okay
     *   bhs → rings >= 50 → mostrar; si no, rts (no mostrar). */
    if ((uint16_t)v_rings < (uint16_t)ss_giantring_rings) {
        return;
    }

    /* --- GRing_Okay --- */
    obRoutine(o) += 2;                                  /* addq.b #2 → GRing_Animate */
    obPriority(o) = 2;                                  /* move.b #2 */
    obColType(o) = (uint8_t)(col_16x32 | col_item);     /* move.b #col_16x32|col_item */
    v_gfxbigring = (uint16_t)Art_BigRing_size;          /* trigger AniArt_GiantRing */

    /* ASM cae a GRing_Animate */
    GRing_Animate(o);
}

/* --- GRing_Animate (Routine 2) ------------------------------------------ */
static void GRing_Animate(uint8_t *o) {
    obFrame(o) = (uint8_t)v_ani1_frame;                 /* move.b (v_ani1_frame).w */
    if (OutOfRange(o, -1, 0)) {                               /* out_of_range.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                                   /* bra.w DisplaySprite */
}

/* --- GRing_Collect (Routine 4) ----------------------------------------- */
static void GRing_Collect(uint8_t *o) {
    obRoutine(o) -= 2;                                  /* subq.b #2 → GRing_Animate */
    obColType(o) = col_none;                            /* move.b #col_none */

    uint8_t *a1 = (uint8_t *)FindFreeObj();             /* bsr.w FindFreeObj */
    if (a1) {                                           /* bne.w GRing_PlaySnd: RAM llena */
        obID(a1) = id_RingFlash;                        /* _move.b #id_RingFlash */
        obX(a1)  = obX(o);                              /* copia X */
        obY(a1)  = obY(o);                              /* copia Y */
        gring_parent(a1) = (uint32_t)Object_GetIndex(o);/* move.l a0,gring_parent(a1) */

        /* move.w (v_player+obX).w,d0 / cmp.w obX(a0),d0 / blo.s GRing_PlaySnd
         *   Sonic X < ring X → no flip; Sonic X >= ring X → bset xflip. */
        if ((uint16_t)obX(RAM_ADDR(v_player)) >= (uint16_t)obX(o)) {
            obRender(a1) |= sprite_xflip;               /* bset #sprite_xflip_bit */
        }
    }

    /* GRing_PlaySnd: */
    Sound_Queue(sfx_GiantRing, false);                  /* jsr QueueSound2 */

    /* bra.s GRing_Animate: mantiene la animación hasta que el flash lo borre */
    GRing_Animate(o);
}

/* --- GRing_Delete (Routine 6) ------------------------------------------ */
static void GRing_Delete(uint8_t *o) {
    DeleteObject(o);                                    /* bra.w DeleteObject */
}

/* --- Dispatcher Object 4B ---------------------------------------------- */
static void GiantRing_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                             /* GRing_Index */
        case 0: GRing_Main(o);    break;
        case 2: GRing_Animate(o); break;
        case 4: GRing_Collect(o); break;
        case 6: GRing_Delete(o);  break;
    }
}

/* ===========================================================================
   Object 7C — Giant Ring flash
   =========================================================================== */

/* --- Flash_Main (Routine 0) -------------------------------------------- */
static void Flash_Main(uint8_t *o) {
    obRoutine(o) += 2;                                  /* addq.b #2 → Flash_ChkDel */
    obMap(o) = (uint32_t)(uintptr_t)Map_Flash;          /* move.l #Map_Flash */
    obGfx(o) = (uint16_t)(ArtTile_Giant_Ring_Flash | Tile_Pal2);
    obRender(o) |= sprite_cam_field;                    /* ori.b #sprite_cam_field */
    obPriority(o) = 0;                                  /* move.b #0: máxima prioridad */
    obActWid(o) = 64 / 2;                               /* move.b #64/2 */
    obFrame(o) = 0xFF;                                  /* move.b #-1: primer Flash_Collect lo pasa a 0 */
}

/* --- Flash_Collect ------------------------------------------------------ */
/* Avanza la animación del flash; en el frame 3 borra el Giant Ring padre
   y oculta a Sonic; en el frame 8 borra a Sonic del todo.                */
static void Flash_Collect(uint8_t *o) {
    /* subq.b #1,obTimeFrame ; bpl.s .return
     *   Trabajamos con bytes: un valor de 0-1 = $FF → N=1 → NO se salta. */
    int8_t tf = (int8_t)(uint8_t)(obTimeFrame(o) - 1);
    obTimeFrame(o) = (uint8_t)tf;
    if (tf >= 0) return;                                /* bpl.s .return */

    obTimeFrame(o) = 1;                                 /* move.b #1: reset a 2 frames */
    uint8_t frame = (uint8_t)(obFrame(o) + 1);          /* addq.b #1 */
    obFrame(o) = frame;
    if (frame >= 8) {                                   /* cmpi.b #8 / bhs.s .deleteSonic */
        obRoutine(o) += 2;                              /* addq.b #2 → Flash_Delete */
        RAM_WORD(v_player) = 0;                         /* move.w #0,(v_player).w: borrar Sonic */
        return;
    }
    if (frame != 3) return;                             /* cmpi.b #3 / bne.s .return */

    /* 3er frame: matar el giant ring padre y ocultar a Sonic */
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)gring_parent(o)); /* movea.l gring_parent(a0),a1 */
    obRoutine(a1) = 6;                                  /* move.b #6,obRoutine(a1): borrar el ring */
    obAnim(RAM_ADDR(v_player)) = id_Null;               /* move.b #id_Null,(v_player+obAnim).w */
    f_bigring = 1;                            /* move.b #1,(f_bigring).w */
    v_invinc  = 0;                            /* clr.b (v_invinc).w */
    v_shield  = 0;                            /* clr.b (v_shield).w */
}

/* --- Flash_ChkDel (Routine 2) ------------------------------------------ */
static void Flash_ChkDel(uint8_t *o) {
    Flash_Collect(o);                                   /* bsr.s Flash_Collect */
    if (OutOfRange(o, -1, 0)) {                               /* out_of_range.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                                   /* bra.w DisplaySprite */
}

/* --- Flash_Delete (Routine 4) ------------------------------------------ */
static void Flash_Delete(uint8_t *o) {
    DeleteObject(o);                                    /* bra.w DeleteObject */
}

/* --- Dispatcher Object 7C ---------------------------------------------- */
static void RingFlash_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                             /* Flash_Index */
        case 0: Flash_Main(o);   break;
        case 2: Flash_ChkDel(o); break;
        case 4: Flash_Delete(o); break;
    }
}

/* ===========================================================================
 *  Object 4C — Lava Geyser Maker  (MZ)
 *  Object 4D — Lava Geyser / Lavafall (MZ)
 *  Ported verbatim from _incObj/4C, 4D MZ Lava Geyser and Maker.asm
 *  (REV01, FixBugs=0).
 *
 *  GeyserMaker fields:
 *    gmake_timer  = objoff_32 (word): current countdown
 *    gmake_time   = objoff_34 (word): reload value
 *    gmake_parent = objoff_3C (4 bytes): parent PushBlock SLOT INDEX
 *                                         (ASM stored a pointer)
 *
 *  LavaGeyser fields:
 *    geyser_origY = objoff_30 (word): initial Y
 *    gmake_parent = objoff_3C: parent SLOT INDEX (top tip for the wall,
 *                              maker for the bottom tip of lavafall)
 *
 *  Layout of the group (see .configureLavaObjects):
 *    top tip  → spawned by maker (subtype 0 in MZ1, 1 for lavafall)
 *    wall     → 2nd object spawned by Geyser_Main; routine 4 (BigLavaWall);
 *               follows the top tip's Y + $60
 *    bottom   → only for lavafall: routine 2 (Action); follows top tip's Y+$100
 * =========================================================================== */

#define gmake_timer(o)   (*(int16_t *)((uint8_t *)(o) + 0x32))
#define gmake_time(o)    (*(int16_t *)((uint8_t *)(o) + 0x34))
#define gmake_parent(o)  (*(uint32_t *)((uint8_t *)(o) + 0x3C))

#define geyser_origY(o)  (*(int16_t *)((uint8_t *)(o) + 0x30))

static const int16_t Geyser_Speeds[2] = { -0x500, 0 };

static void GMake_Main(uint8_t *o);
static void GMake_Wait(uint8_t *o);
static void GMake_ChkType(uint8_t *o);
static void GMake_MakeLava(uint8_t *o);
static void GMake_Display(uint8_t *o);
static void GMake_Delete(uint8_t *o);

static void Geyser_Main(uint8_t *o);
static void Geyser_Action(uint8_t *o);
static void Geyser_BigLavaWall(uint8_t *o);
static void Geyser_Delete(uint8_t *o);

/* --- Geyser_MakeChild: port del .makeLava interno.
       Setup de un nuevo LavaGeyser hijo copiando X/Y/subtype de a0.
       FixBugs=0: obActWid = 64/2 (el wall "desaparece" un poco antes de
       salir de pantalla — bug replicado). */
static void Geyser_MakeChild(uint8_t *a1, uint8_t *a0) {
    obID(a1)       = id_LavaGeyser;
    obMap(a1)      = (uint32_t)(uintptr_t)Map_Geyser;
    obGfx(a1)      = (uint16_t)(ArtTile_MZ_Lava | Tile_Pal4);
    obRender(a1)   = sprite_cam_field;
    obActWid(a1)   = 64 / 2;
    obX(a1)        = obX(a0);
    obY(a1)        = obY(a0);
    obSubtype(a1)  = obSubtype(a0);
    obPriority(a1) = 1;

    obAnim(a1) = 5;                        /* ".bubble4" (geyser) */
    if (obSubtype(a0) != 0) {
        obAnim(a1) = 2;                    /* ".end" (lavafall) */
    }
}

/* ===========================================================================
 *  GeyserMaker (Object 4C)
 * =========================================================================== */

static void GMake_Main(uint8_t *o) {
    obRoutine(o) += 2;
    obMap(o)    = (uint32_t)(uintptr_t)Map_Geyser;
    obGfx(o)    = (uint16_t)(ArtTile_MZ_Lava | Tile_Pal4 | Tile_Prio);
    obRender(o) = sprite_cam_field;
    obPriority(o) = 1;
    obActWid(o)   = 112 / 2;
    gmake_time(o) = 120;
    /* ASM falls through to GMake_Wait on the same frame. */
    GMake_Wait(o);
}

static void GMake_Wait(uint8_t *o) {
    gmake_timer(o)--;
    if (gmake_timer(o) >= 0) return;

    gmake_timer(o) = gmake_time(o);

    int16_t son_y   = obY(RAM_ADDR(v_player));
    int16_t maker_y = obY(o);
    /* cmp.w d1,d0 / bhs → skip when sonY >= makerY (Sonic below) */
    if ((uint16_t)son_y >= (uint16_t)maker_y) return;
    int16_t ceiling = (int16_t)(maker_y - 0x170);
    /* cmp.w d1,d0 / blo → skip when sonY < ceiling (too far above) */
    if ((uint16_t)son_y < (uint16_t)ceiling) return;

    obRoutine(o) += 2;                     /* → GMake_ChkType (4) */
}

static void GMake_ChkType(uint8_t *o) {
    if (obSubtype(o) == 0) {
        /* Geyser: falls through to Display, routine STAYS at 4 until the
           bubbling animation (Ani_Geyser) finishes and advances it via
           afRoutine ($FC) to routine 6. */
        GMake_Display(o);
        return;
    }
    obRoutine(o) += 2;                     /* lavafall → GMake_MakeLava (6) */
}

static void GMake_MakeLava(uint8_t *o) {
    obRoutine(o) += 2;                     /* → GMake_Display (8) */

    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (a1 != NULL) {
        obID(a1)       = id_LavaGeyser;
        obX(a1)        = obX(o);
        obY(a1)        = obY(o);
        obSubtype(a1)  = obSubtype(o);
        gmake_parent(a1) = (uint32_t)Object_GetIndex(o);
    }

    obAnim(o) = 1;                         /* ".bubble2" */
    if (obSubtype(o) != 0) {
        obAnim(o) = 4;                     /* ".blank" for lavafall */
        GMake_Display(o);
        return;
    }

    /* .shootUpPushBlock: only for subtype 0.
       gmake_parent(maker) points to the PushBlock that spawned this maker. */
    {
        uint8_t *block = (uint8_t *)Object_GetSlot((int)gmake_parent(o));
        obStatus(block) |= (1 << 1);       /* bset #1 */
        obVelY(block) = (int16_t)-0x580;
    }

    GMake_Display(o);
}

static void GMake_Display(uint8_t *o) {
    if (Ani_Geyser) AnimateSprite(o, Ani_Geyser);
    DisplaySprite(o);
}

static void GMake_Delete(uint8_t *o) {
    obAnim(o)    = 0;
    obRoutine(o) = 2;                      /* → GMake_Wait */
    if (obSubtype(o) == 0) {
        DeleteObject(o);                   /* geyser self-deletes after one shot */
    }
    /* lavafall stays alive indefinitely */
}

/* Dispatcher — FixBugs=0: jsr routine, then Geyser_ChkDel
   (out_of_range → DeleteObject, no DisplaySprite at dispatcher level). */
static void GeyserMaker_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0x00: GMake_Main(o);     break;
        case 0x02: GMake_Wait(o);     break;
        case 0x04: GMake_ChkType(o);  break;
        case 0x06: GMake_MakeLava(o); break;
        case 0x08: GMake_Display(o);  break;
        case 0x0A: GMake_Delete(o);   break;
    }
    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
    }
}

/* ===========================================================================
 *  LavaGeyser (Object 4D)
 * =========================================================================== */

static void Geyser_Main(uint8_t *o) {
    obRoutine(o) += 2;
    geyser_origY(o) = obY(o);

    if (obSubtype(o) != 0) {
        /* Lavafall sits $250px above the maker's Y */
        obY(o) = (int16_t)(obY(o) - 0x250);
    }

    {
        uint8_t s = obSubtype(o);
        if (s > 1) s = 1;                  /* clamp (ASM reads OOB for safety) */
        obVelY(o) = Geyser_Speeds[s];
    }

    uint8_t *tip = o;

    /* bsr .makeLava: a1 = a0 (top tip).
       dbf d1 (1 → 0) loops once: FindNextFreeObj + .makeLava → wall. */
    Geyser_MakeChild(tip, tip);
    uint8_t *wall = (uint8_t *)FindNextFreeObj(tip);
    if (wall != NULL) {
        Geyser_MakeChild(wall, tip);
    }

    /* .configureLavaObjects: a0 = top tip, a1 = wall */
    if (wall != NULL) {
        obY(wall)           = (int16_t)(obY(wall) + 0x60);
        geyser_origY(wall)  = (int16_t)(geyser_origY(tip) + 0x60);
        obColType(wall)     = (uint8_t)(col_64x224 | col_hurt);
        obHeight(wall)      = 256 / 2;
        obRender(wall)     |= sprite_customheight;
        obRoutine(wall)    += 4;           /* → Geyser_BigLavaWall */
        gmake_parent(wall)  = (uint32_t)Object_GetIndex(tip);
    }

    /* Lavafall only: third object (bottom tip) */
    if (obSubtype(tip) != 0) {
        uint8_t *after   = (wall != NULL) ? wall : tip;
        uint8_t *bottom  = (uint8_t *)FindNextFreeObj(after);
        if (bottom != NULL) {
            Geyser_MakeChild(bottom, tip);
            obRoutine(bottom) += 2;        /* → Geyser_Action (2) */

            /* The original disassembly used this line, but it causes issues in the port;
               commenting it out fixes the problem.
               However, the visual fidelity isn't quite the same, though the difference
               is barely noticeable unless you examine it frame by frame. */

            //obGfx(bottom)     |= 0x10;     /* bset #4,obGfx (custom height) */

            obY(bottom)        = (int16_t)(obY(bottom) + 0x100);
            obPriority(bottom) = 0;
            geyser_origY(bottom) = geyser_origY(tip);
            /* ASM: move.l gmake_parent(a0),gmake_parent(a1)
               copies the TIP's parent (= maker slot) into the bottom tip.
               The comment says "top tip" but the code copies the parent. */
            gmake_parent(bottom) = gmake_parent(tip);
            obSubtype(tip) = 0;            /* top tip is now a plain geyser */
        }
    }

    Sound_Queue(sfx_Burning, false);

    /* ASM falls through into Geyser_Action on the same frame. */
    Geyser_Action(o);
}

static void Geyser_Action(uint8_t *o) {
    uint8_t s = obSubtype(o);
    if (s > 1) s = 1;

    if (s == 0) {
        /* Geyser_Type00 */
        obVelY(o) = (int16_t)(obVelY(o) + 0x18);
        if ((uint16_t)geyser_origY(o) < (uint16_t)obY(o)) {
            obRoutine(o) += 4;             /* → Geyser_Delete */
            uint8_t *parent = (uint8_t *)Object_GetSlot((int)gmake_parent(o));
            obAnim(parent) = 3;            /* tell parent (maker) to show ".bubble3" */
        }
    } else {
        /* Geyser_Type01 */
        obVelY(o) = (int16_t)(obVelY(o) + 0x18);
        if ((uint16_t)geyser_origY(o) < (uint16_t)obY(o)) {
            obRoutine(o) += 4;
            uint8_t *parent = (uint8_t *)Object_GetSlot((int)gmake_parent(o));
            obAnim(parent) = 1;            /* ".bubble1" */
        }
    }

    SpeedToPos(o);
    if (Ani_Geyser) AnimateSprite(o, Ani_Geyser);

    /* Geyser_ChkDel (FixBugs=0) */
    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
    }
}

static void Geyser_BigLavaWall(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)gmake_parent(o));
    if (obRoutine(parent) == 6) {
        DeleteObject(o);                   /* parent marked for delete → die too */
        return;
    }

    int16_t d0 = (int16_t)(obY(parent) + 0x60);
    obY(o) = d0;

    d0 = (int16_t)(d0 - geyser_origY(o));
    d0 = (int16_t)(-d0);                   /* neg.w d0 */

    int16_t d1 = 8;                        /* medium frame base */
    if (d0 < 0x40) {
        d1 = 0x0B;                         /* short */
    }
    if (d0 > 0x80) {
        d1 = 0x0E;                         /* long */
    }

    obTimeFrame(o)--;
    if ((int8_t)obTimeFrame(o) < 0) {
        obTimeFrame(o) = 8 - 1;
        obAniFrame(o)++;
        if (obAniFrame(o) >= 2) {
            obAniFrame(o) = 0;
        }
    }

    d0 = (int16_t)(int8_t)obAniFrame(o);
    d0 = (int16_t)((int8_t)d1 + (int8_t)d0);
    obFrame(o) = (uint8_t)d0;

    /* Geyser_ChkDel (FixBugs=0) */
    if (OutOfRange(o, -1, 0)) {
        DeleteObject(o);
    }
}

static void Geyser_Delete(uint8_t *o) {
    DeleteObject(o);
}

/* Dispatcher — FixBugs=0: jsr routine, then bra DisplaySprite. */
static void LavaGeyser_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Geyser_Main(o);        break;
        case 2: Geyser_Action(o);      break;
        case 4: Geyser_BigLavaWall(o); break;
        case 6: Geyser_Delete(o);      break;
    }
    DisplaySprite(o);
}

/* ===========================================================================
 *  Object 4E — Wall of Lava (MZ Act 2)
 *  Ported verbatim from _incObj/4E MZ Wall of Lava.asm (REV01, FixBugs=0).
 *
 *  lwall_flag   = objoff_36 (byte): set when the lava wall is moving
 *  lwall_parent = objoff_3C (4 bytes): parent SLOT INDEX
 *                                     (ASM stored an address)
 *
 *  The object is created as a group: the root slot becomes the parent
 *  (routine 4, "ChkSonic") and a second slot is allocated as the child
 *  (routine 6, "BackChild"). The child mirrors the parent's X minus 128.
 *
 *  MZ2 hardcodes the stop coordinate $6A0; the wall stops moving when it
 *  reaches that X.
 * =========================================================================== */

#define lwall_flag(o)   (*(uint8_t  *)((uint8_t *)(o) + 0x36))
#define lwall_parent(o) (*(uint32_t *)((uint8_t *)(o) + 0x3C))

static void LWall_Main(uint8_t *o);
static void LWall_Solid(uint8_t *o);
static void LWall_ChkSonic(uint8_t *o);
static void LWall_BackChild(uint8_t *o);
static void LWall_Delete(uint8_t *o);

/* --- LWall_Main — routine 0 --- */
static void LWall_Main(uint8_t *o) {
    obRoutine(o) += 4;              /* → LWall_ChkSonic (4) */

    uint8_t *a1 = o;
    int first_iter = 1;

    /* ASM: moveq #2-1,d1 ; bra.s .make ; .loop: FindNextFreeObj ;
       bne.s .next ; .make: setup ; .next: dbf d1,.loop
       The first iteration reuses this slot (no FindNextFreeObj); the
       second allocates a fresh slot for the child. If FindNextFreeObj
       fails on the second iteration, a1 stays on the parent and the
       parent's routine gets bumped by 6 (replicating the ASM edge case). */
    for (int d1 = 2 - 1; d1 >= 0; d1--) {
        if (!first_iter) {
            uint8_t *slot = (uint8_t *)FindNextFreeObj(a1);
            if (!slot) continue;    /* ASM: bne.s .next → skip .make */
            a1 = slot;
        }
        first_iter = 0;

        /* .make */
        obID(a1)         = id_LavaWall;
        obMap(a1)        = (uint32_t)(uintptr_t)Map_LWall;
        obGfx(a1)        = (uint16_t)(ArtTile_MZ_Lava | Tile_Pal4);
        obRender(a1)     = sprite_cam_field;
        obActWid(a1)     = 160 / 2;
        obX(a1)          = obX(o);
        obY(a1)          = obY(o);
        obPriority(a1)   = 1;
        obAnim(a1)       = 0;
        obColType(a1)    = (uint8_t)(col_128x64 | col_hurt);
        lwall_parent(a1) = (uint32_t)Object_GetIndex(o);
    }

    /* Child gets routine 6 and frame 4. */
    obRoutine(a1) += 6;             /* → LWall_BackChild (6) */
    obFrame(a1)   = 4;              /* ".lava_back" */

    /* ASM falls through into LWall_ChkSonic. */
    LWall_ChkSonic(o);
}

/* --- LWall_ChkSonic — routine 4 --- */
static void LWall_ChkSonic(uint8_t *o) {
    int16_t d0;

    /* Horizontal range check: |sonic_x - lava_x| < 192? */
    d0 = (int16_t)(obX(RAM_ADDR(v_player)) - obX(o));
    if (d0 < 0) d0 = (int16_t)(-d0);
    if ((uint16_t)d0 < 192) {
        /* Vertical range check: |sonic_y - lava_y| < 96? */
        d0 = (int16_t)(obY(RAM_ADDR(v_player)) - obY(o));
        if (d0 < 0) d0 = (int16_t)(-d0);
        if ((uint16_t)d0 < 96) {
            lwall_flag(o) = 1;      /* Sonic is close: arm the lava wall */
            LWall_Solid(o);
            return;
        }
    }

    /* .checkMoveStart */
    if (lwall_flag(o) != 0) {
        obVelX(o) = 0x180;          /* start sliding to the right */
        obRoutine(o) -= 2;          /* → LWall_Solid (2) */
    }
    /* ASM falls through to LWall_Solid in both paths. */
    LWall_Solid(o);
}

/* --- LWall_Solid — routine 2 --- */
static void LWall_Solid(uint8_t *o) {
    /* ASM saves obRoutine to the stack because SolidObject clobbers d0.
       In C, obRoutine lives in the object struct and SolidObject doesn't
       touch it, but we replicate the save/restore for fidelity. */
    uint8_t saved_routine = obRoutine(o);

    {
        int16_t d1 = (int16_t)(64 / 2 + sonic_solid_width);
        int16_t d2 = 48 / 2;
        int16_t d3 = (int16_t)(d2 + 1);   /* stood-on height = initial+1 */
        int16_t d4 = obX(o);
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);
    }

    obRoutine(o) = saved_routine;

    /* Hardcoded MZ2 stop coordinate: freeze at X = $6A0. */
    if ((uint16_t)obX(o) == 0x6A0) {
        obVelX(o) = 0;
        lwall_flag(o) = 0;
    }

    /* .animateAndMove */
    if (Ani_LWall) AnimateSprite(o, Ani_LWall);

    /* Skip SpeedToPos while Sonic is hurt/dying so the wall doesn't slide
       into him mid-animation. */
    if ((uint8_t)obRoutine(RAM_ADDR(v_player)) < 4) {
        SpeedToPos(o);
    }

    /* FixBugs=0: DisplaySprite BEFORE the out_of_range delete check. */
    DisplaySprite(o);
    if (lwall_flag(o) != 0) return;     /* bne.s .show */

    if (OutOfRange(o, -1, 0)) {
        /* .startDelete
         * FixBugs=0 does NOT check obRespawnNo != 0 before bclr;
         * that's the known bug where placement in debug mode can corrupt
         * an unrelated respawn byte. Replicated verbatim. */
        uint8_t *a2 = RAM_ADDR(v_objstate);
        uint8_t d0 = obRespawnNo(o);
        a2[2 + d0] &= 0x7F;             /* bclr #7,2(a2,d0.w) */
        obRoutine(o) = 8;               /* → LWall_Delete */
    }
}

/* --- LWall_BackChild — routine 6 --- */
static void LWall_BackChild(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)lwall_parent(o));

    if (obRoutine(a1) == 8) {           /* parent marked for deletion? */
        LWall_Delete(o);
        return;
    }

    obX(o) = (int16_t)(obX(a1) - 128);  /* child is 128 px to the left */
    DisplaySprite(o);
}

/* --- LWall_Delete — routine 8 --- */
static void LWall_Delete(uint8_t *o) {
    DeleteObject(o);
}

/* --- Dispatcher Object 4E --- */
static void LavaWall_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: LWall_Main(o);      break;
        case 2: LWall_Solid(o);     break;
        case 4: LWall_ChkSonic(o);  break;
        case 6: LWall_BackChild(o); break;
        case 8: LWall_Delete(o);    break;
    }
}

/* ===========================================================================
 *  Object 50 — Yadrin enemy (MZ [unused], SYZ)
 *  Ported verbatim from _incObj/50 Badnik - Yadrin.asm (REV01, FixBugs=0).
 *
 *  yad_timedelay = objoff_30 (word): delay before turning around
 *
 *  FixBugs=0: se omite el chequeo "delete si cae por debajo de $7FF" del
 *  Yad_Main (presente solo con FixBugs=1).
 *
 *  La subrutina ChkHitLeftRightWall viene del mismo archivo; en el prototipo
 *  era compartida con Splats (Obj4F), pero en el juego final solo la usa
 *  Yadrin. Se implementa como función estática local.
 * =========================================================================== */

#define yad_timedelay(o) (*(int16_t *)((uint8_t *)(o) + 0x30))

static void Yad_Main(uint8_t *o);
static void Yad_Action(uint8_t *o);
static void Yad_Action_Wait(uint8_t *o);
static void Yad_Action_Move(uint8_t *o);
static int  Yad_ChkHitLeftRightWall(uint8_t *o);

/* --- Yad_ChkHitLeftRightWall -------------------------------------------
 * Chequeo de pared izquierda/derecha, ejecutado solo 1 de cada 4 frames
 * (d0 = (v_framecount + d7) & 3; si != 0, sale). El "d7" del ASM es el
 * enumerador OST descendente (127..0) que ExecuteObjects tiene en d7
 * durante su bucle dbf. En el port, donde iteramos ascendente, d7 se
 * reconstruye como (NUM_OBJECTS - 1) - índice.
 * ---------------------------------------------------------------------- */
static int Yad_ChkHitLeftRightWall(uint8_t *o) {
    uint8_t d7 = (uint8_t)((NUM_OBJECTS - 1) - Object_GetIndex(o));

    uint16_t d0 = (uint16_t)v_framecount + d7;               /* add.w d7,d0 */
    d0 &= 3;                                                 /* andi.w #3,d0 */
    if (d0 != 0) return 0;                                   /* bne.s .noWallHit */

    int16_t d3 = (int16_t)(int8_t)obActWid(o);               /* moveq #0,d3 / move.b obActWid,d3 */

    if ((int16_t)obVelX(o) < 0) {                            /* tst.w obVelX / bmi.s .chkLeftWall */
        /* .chkLeftWall */
        d3 = (int16_t)~d3;                                   /* not.w d3: mirar a la izquierda */
        if (PushB_ObjHitWallLeft(o, d3) < 0) return 1;       /* bmi.s .wallHit */
        return 0;                                            /* .noWallHit */
    }
    /* Camino derecho */
    if (PushB_ObjHitWallRight(o, d3) < 0) return 1;
    return 0;
}

/* --- Yad_Main — Routine 0: setup + caer hasta el suelo ---------------- */
static void Yad_Main(uint8_t *o) {
    obMap(o)      = (uint32_t)(uintptr_t)Map_Yad;
    obGfx(o)      = (uint16_t)(ArtTile_Yadrin | Tile_Pal2);
    obRender(o)   = sprite_cam_field;
    obPriority(o) = 4;
    obActWid(o)   = 40 / 2;
    obHeight(o)   = 34 / 2;
    obWidth(o)    = 16 / 2;
    obColType(o)  = (uint8_t)(col_40x32 | col_special);

    /* Cae hasta tocar el suelo (invisible mientras cae) */
    ObjectFall(o);                                           /* bsr.w ObjectFall */
    int16_t d1, d3;
    ObjFloorDist(o, &d1, &d3);                               /* bsr.w ObjFloorDist */
    if (d1 >= 0) return;                                     /* bpl.s .hide: aún no toca el suelo */

    obY(o) = (int16_t)(obY(o) + d1);                         /* add.w d1,obY: pegar al suelo */
    obVelY(o) = 0;                                           /* move.w #0,obVelY */
    obRoutine(o) += 2;                                       /* addq.b #2 → Yad_Action */
    obStatus(o) ^= sprite_xflip;                             /* bchg #0: spawn mirando a la izquierda */

    /* FixBugs=0: se omite el chequeo "delete si obY > $7FF". */
}

/* --- Yad_Action — Routine 2 ------------------------------------------ */
static void Yad_Action(uint8_t *o) {
    switch (ob2ndRout(o)) {                                  /* Yad_ActIndex */
        case 0: Yad_Action_Wait(o); break;
        case 2: Yad_Action_Move(o); break;
    }
    if (Ani_Yad) AnimateSprite(o, Ani_Yad);                  /* lea (Ani_Yad).l,a1 */
    RememberState(o);                                        /* bra.w RememberState */
}

/* --- Yad_Action_Wait — sub-rutina 0: pausa y luego gira --------------- */
static void Yad_Action_Wait(uint8_t *o) {
    yad_timedelay(o)--;                                      /* subq.w #1,yad_timedelay */
    if (yad_timedelay(o) >= 0) return;                       /* bpl.s .return: aún esperando */

    ob2ndRout(o) += 2;                                       /* addq.b #2 → Yad_Action_Move */
    obVelX(o) = (int16_t)-0x100;                             /* move.w #-$100,obVelX: mirar izquierda */
    obAnim(o) = 1;                                           /* move.b #1,obAnim: walk */
    obStatus(o) ^= sprite_xflip;                             /* bchg #0,obStatus: invertir orientación */
    /* bne.s .return saltaba si el bit ANTERIOR era 1; tras el XOR,
       "current bit = 1" ⟺ "old bit = 0". */
    if (obStatus(o) & sprite_xflip) {
        obVelX(o) = (int16_t)(-obVelX(o));                   /* neg.w obVelX: derecha */
    }
}

/* --- Yad_Action_Move — sub-rutina 2: caminar y detectar bordes ------- */
static void Yad_Action_Move(uint8_t *o) {
    SpeedToPos(o);                                           /* bsr.w SpeedToPos */

    int16_t d1, d3;
    ObjFloorDist(o, &d1, &d3);                               /* bsr.w ObjFloorDist */
    if (d1 < -8) goto pause;                                 /* cmpi.w #-8 / blt.s .pause */
    if (d1 >= 0x0C) goto pause;                              /* cmpi.w #$C / bge.s .pause */
    obY(o) = (int16_t)(obY(o) + d1);                         /* add.w d1,obY: pegar al piso */

    if (Yad_ChkHitLeftRightWall(o)) goto pause;              /* bne.s .pause */
    return;

pause:
    ob2ndRout(o) -= 2;                                       /* subq.b #2 → Yad_Action_Wait */
    yad_timedelay(o) = 60 - 1;                               /* move.w #60-1: 1 segundo de pausa */
    obVelX(o) = 0;                                           /* move.w #0,obVelX: parar */
    obAnim(o) = 0;                                           /* move.b #0,obAnim: wait */
}

/* --- Dispatcher Object 50 — Yad_Index: 0 = Main, 2 = Action ---------- */
static void Yadrin_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                                  /* Yad_Index */
        case 0: Yad_Main(o);   break;
        case 2: Yad_Action(o); break;
    }
}

/* ===========================================================================
 *  Object 51 — Smashable Green Block (MZ)
 *  Ported verbatim from _incObj/51 MZ Smashable Green Block.asm
 *  (REV01, FixBugs=0).
 *
 *  smab_sonani = objoff_32 (byte): backup of Sonic's animation before
 *                                   SolidObject (which can clobber it)
 *  smab_combo  = objoff_34 (word): combo score chain cached before
 *                                   SolidObject (Sonic_ResetOnFloor zeroes
 *                                   v_itembonus inside SolidObject)
 *
 *  Reuses SmashObject() from Object 3C (GHZ Smashable Wall). The parent slot
 *  is converted into fragment #0, and obID stays id_SmashBlock, so fragments
 *  are dispatched back into Smab_Fragment via routine 4.
 * =========================================================================== */

#define smab_sonani(o) (*(uint8_t  *)((uint8_t *)(o) + 0x32))
#define smab_combo(o)  (*(int16_t  *)((uint8_t *)(o) + 0x34))

static void Smab_Main(uint8_t *o);
static void Smab_Solid(uint8_t *o);
static void Smab_Fragment(uint8_t *o);

/* Fragment launch velocities (x, y). Word pairs, indexed by a4+2*n. */
static const int16_t Smab_Speeds[8] = {
    -0x200, -0x200,   /* fragment 1 */
    -0x100, -0x100,   /* fragment 2 */
     0x200, -0x200,   /* fragment 3 */
     0x100, -0x100,   /* fragment 4 */
};

/* Points awarded per block (index = combo count / 2).
   The 16th and subsequent smashes are hardcoded to 10000 pts. */
static const uint16_t Smab_Scores[4] = { 10, 20, 50, 100 };

/* --- Smab_Main — routine 0 --- */
static void Smab_Main(uint8_t *o) {
    obRoutine(o) += 2;                      /* → Smab_Solid */
    obMap(o)     = (uint32_t)(uintptr_t)Map_Smab;
    obGfx(o)     = (uint16_t)(ArtTile_MZ_Block | Tile_Pal3);
    obRender(o)  = sprite_cam_field;
    obActWid(o)  = 32 / 2;
    obPriority(o)= 4;

    /* FixBugs=0: leftover from a copy-paste of Object 3C. Uses subtype as
       frame ID (0 = two-piece frame, 1 = four-piece, unused). Kept as-is. */
    obFrame(o)   = obSubtype(o);

    /* ASM falls through to Smab_Solid on the first frame. */
    Smab_Solid(o);
}

/* --- Smab_Solid — routine 2 --- */
static void Smab_Solid(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_player);
    int16_t d1;
    int16_t d2;
    int16_t d3;

    /* Cache the combo chain before SolidObject (Sonic_ResetOnFloor inside
       SolidObject would zero v_itembonus if Sonic lands on the block). */
    smab_combo(o)  = v_itembonus;
    smab_sonani(o) = obAnim(a1);

    d1 = (int16_t)(32 / 2 + sonic_solid_width);
    d2 = 32 / 2;
    d3 = 34 / 2;
    {
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, obX(o), &out_d3, &out_d5);
    }

    /* btst #3,obStatus(a0) / bne.s .smash */
    if (!(obStatus(o) & (1 << 3))) {
        return;                             /* .return: rts */
    }

    /* .smash: only smash if Sonic is rolling/jumping */
    if (smab_sonani(o) != id_Roll) {
        return;                             /* .return */
    }

    /* Restore the cached combo chain (Sonic_ResetOnFloor zeroed it). */
    v_itembonus = smab_combo(o);

    /* Force Sonic to remain rolling after the rebound. */
    obStatus(a1) |=  (1 << 2);              /* bset #2: rolling flag */
    obHeight(a1)  = sonic_roll_height;
    obWidth(a1)   = sonic_roll_width;
    obAnim(a1)    = id_Roll;
    obVelY(a1)    = (int16_t)-0x300;        /* bounce up */
    obStatus(a1) |=  (1 << 1);              /* bset #1: airborne */
    obStatus(a1) &= ~(1 << 3);              /* bclr #3: not on platform */
    obRoutine(a1) = 2;                      /* → Sonic_Control */
    obStatus(o)  &= ~(1 << 3);              /* clear block's stood-on flag */
    obSolid(o)    = 0;                      /* clear block solidity */

    /* Switch to the four-piece frame for a nicer fragmentation. */
    obFrame(o) = 1;

    /* Smash into four fragments. d2 (gravity) is the counter-gravity applied
       to the first fragment when the parent is before Sonic in RAM. */
    SmashObject(o, Smab_Speeds, 4 - 1, (int16_t)gravity);

    /* Spawn floating points object. */
    {
        uint8_t *slot = (uint8_t *)FindFreeObj();
        if (slot != NULL) {
            obID(slot) = id_Points;
            obX(slot)  = obX(o);
            obY(slot)  = obY(o);

            uint16_t d2b = v_itembonus;              /* old combo */
            v_itembonus += 2;                        /* increment by 1*2 */
            if (d2b >= 3 * 2) d2b = 3 * 2;           /* cap index */

            int16_t d0 = (int16_t)Smab_Scores[d2b / 2];
            if ((uint16_t)v_itembonus >= 16 * 2) {
                d0 = 1000;
                d2b = 5 * 2;                         /* frame 5 = 10000 pts */
            }
            AddPoints(d0);

            d2b >>= 1;
            obFrame(slot) = (uint8_t)d2b;
        }
    }

    /* ASM falls through into Smab_Fragment. The parent object has been
       converted into fragment #0 by SmashObject, so calling it here is
       exactly what the original does. */
    Smab_Fragment(o);
}

/* --- Smab_Fragment — routine 4 --- */
static void Smab_Fragment(uint8_t *o) {
    SpeedToPos(o);
    obVelY(o) = (int16_t)(obVelY(o) + gravity);

    /* FixBugs=0: DisplaySprite first, then delete-if-offscreen, then rts.
       The dispatcher's trailing RememberState (see SmashBlock_Main) will
       re-queue the fragment on the same frame, exactly as the original. */
    DisplaySprite(o);
    if (!(obRender(o) & 0x80)) {
        DeleteObject(o);
    }
}

/* --- Dispatcher Object 51 ---
 * ASM (FixBugs=0): jsr Smab_Index ; bra.w RememberState.
 * RememberState runs after every routine, including after Smab_Fragment
 * (which may have just deleted the object; harmless because RememberState
 * checks obID == 0 in the sprite queue). */
static void SmashBlock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: Smab_Main(o);     break;
        case 2: Smab_Solid(o);    break;
        case 4: Smab_Fragment(o); break;
    }
    RememberState(obj);
}

/* ===========================================================================
 *  Object 52 — Moving platform blocks (MZ, LZ, SBZ)
 *  Ported verbatim from _incObj/52 Moving Blocks.asm (REV01, FixBugs=0).
 *
 *  Fields:
 *    mblock_origX        = objoff_30 (word): initial X-position
 *    mblock_origY        = objoff_32 (word): initial Y-position
 *    mblock_slide_wait   = objoff_34 (word): delay before red sliding floor moves back
 *    mblock_slide_goback = objoff_36 (word): set if red sliding floor is moving back
 * =========================================================================== */

#define mblock_origX(o)        (*(int16_t *)((uint8_t *)(o) + 0x30))
#define mblock_origY(o)        (*(int16_t *)((uint8_t *)(o) + 0x32))
#define mblock_slide_wait(o)   (*(int16_t *)((uint8_t *)(o) + 0x34))
#define mblock_slide_goback(o) (*(int16_t *)((uint8_t *)(o) + 0x36))

/* MBlock_Var aplanado: 2 bytes por entrada (width, frame). El ASM lee con
 * `(a2)+` usando un byte-offset en d0 = (subtype >> 3) & $1E, así que el
 * offset en bytes es exactamente 2 * (subtype >> 4) para los subtipos válidos. */
static const uint8_t MBlock_Var[5 * 2] = {
    32/2,  0,   /* $0x - MZ single block / LZ small raft */
    64/2,  1,   /* $1x - MZ double block (unused)      */
    64/2,  2,   /* $2x - SBZ short (yellow/black)      */
    128/2, 3,   /* $3x - SBZ long (red sliding floors) */
    96/2,  4,   /* $4x - MZ triple block               */
};

/* Helper — port de ObjHitWallRight (sub ObjHitWallRight.asm). Recibe d3 =
 * píxeles a mirar hacia adelante desde obX; devuelve d1 = distancia a la
 * pared (negativo si choca). Misma lógica que PushB_ObjHitWallRight. */
static int16_t MBlock_ObjHitWallRight(uint8_t *o, int16_t d3) {
    int16_t y = obY(o);
    int16_t x = (int16_t)(obX(o) + d3);
    int16_t dist = 0;
    v_anglebuffer = 0;
    FindWall(y, x, 0x0E, 0x0000, 0x10, &v_anglebuffer, o, &dist);
    if (v_anglebuffer & 1) v_anglebuffer = 0xC0;
    return dist;
}

/* Forward declarations */
static void MBlock_Main(uint8_t *o);
static void MBlock_Platform(uint8_t *o);
static void MBlock_StandOn(uint8_t *o);
static int  MBlock_Move(uint8_t *o);
static void MBlock_DisplayOrDelete(uint8_t *o);

/* --- MBlock_Main — Routine 0 --- */
static void MBlock_Main(uint8_t *o) {
    obRoutine(o) += 2;                                  /* → MBlock_Platform */

    obMap(o) = (uint32_t)(uintptr_t)Map_MBlock;         /* MZ/SBZ mappings */
    obGfx(o) = (uint16_t)(ArtTile_MZ_Block | Tile_Pal3);

    if ((uint8_t)v_zone == id_LZ) {                     /* LZ overrides */
        obMap(o) = (uint32_t)(uintptr_t)Map_MBlockLZ;
        obGfx(o) = (uint16_t)(ArtTile_LZ_Moving_Block | Tile_Pal3);
        obHeight(o) = 14 / 2;
    }

    if ((uint8_t)v_zone == id_SBZ) {                    /* SBZ overrides */
        obGfx(o) = (uint16_t)(ArtTile_SBZ_Moving_Block_Short | Tile_Pal2);
        if (obSubtype(o) != 0x28) {                     /* no es corta → larga */
            obGfx(o) = (uint16_t)(ArtTile_SBZ_Moving_Block_Long | Tile_Pal3);
        }
    }

    obRender(o) = sprite_cam_field;
    obPriority(o) = 4;

    /* MBlock_Var lookup: d0 = (subtype >> 3) & $1E */
    {
        uint8_t d0 = (uint8_t)((obSubtype(o) >> 3) & 0x1E);
        obActWid(o) = MBlock_Var[d0];
        obFrame(o)  = MBlock_Var[d0 + 1];
    }

    mblock_origX(o) = obX(o);
    mblock_origY(o) = obY(o);

    obSubtype(o) &= 0x0F;                               /* clear upper digit */

    /* Fall-through a MBlock_Platform */
    MBlock_Platform(o);
}

/* --- MBlock_Platform — Routine 2 --- */
static void MBlock_Platform(uint8_t *o) {
    /* El ASM usa `bsr.w MBlock_Move`; si el tipo 7 ejecuta `addq.l #4,sp`,
     * el rts retorna al caller de MBlock_Platform saltándose el resto.
     * Modelamos ese caso con MBlock_Move devolviendo 1. */
    if (MBlock_Move(o)) return;

    int16_t d1 = (int16_t)obActWid(o);
    PlatformObject(o, d1);

    /* bra.s MBlock_DisplayOrDelete */
    MBlock_DisplayOrDelete(o);
}

/* --- MBlock_StandOn — Routine 4 --- */
static void MBlock_StandOn(uint8_t *o) {
    int16_t d1 = (int16_t)obActWid(o);
    int16_t dummy;
    ExitPlatform(o, d1, &dummy);

    /* El ASM guarda obX en el stack antes de MBlock_Move para pasarlo
     * como d2 a MvSonicOnPtfm2. En FixBugs=1 se usa scratch RAM para
     * evitar que MBlock_SecretLZ1Raft corrompa el stack; en FixBugs=0
     * usa la pila. En C una variable local es equivalente en FixBugs=0
     * y no rompe en el caso del raft. */
    int16_t saved_x = obX(o);

    if (MBlock_Move(o)) return;                         /* tipo 7: skip */

        MvSonicOnPtfm2(o, saved_x);

    /* Fall-through a MBlock_DisplayOrDelete */
    MBlock_DisplayOrDelete(o);
}

/* --- MBlock_DisplayOrDelete --- */
static void MBlock_DisplayOrDelete(uint8_t *o) {
    if (OutOfRange(o, mblock_origX(o), 0)) {                  /* out_of_range.w DeleteObject,mblock_origX */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);                                   /* bra.w DisplaySprite */
}

/* --- Sub-rutinas de movimiento --- */

/* Tipo 0: estacionario */
static void MBlock_Stationary(uint8_t *o) {
    (void)o;                                            /* rts */
}

/* Tipo 1: izquierda/derecha continuo (freq 2, mid $30) */
static void MBlock_LeftRight(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x0E);
    if (obStatus(o) & 1) {                              /* btst #0,obStatus */
        d0 = (int16_t)(-d0);                            /* neg.w d0 */
        d0 = (int16_t)(d0 + 0x60);                      /* add.w d1,d0 */
    }
    obX(o) = (int16_t)(mblock_origX(o) - d0);           /* sub.w d0,d1 ; move.w d1,obX */
}

/* Tipos 2/4/9: estacionario, avanza de subtipo cuando Sonic lo pisa */
static void MBlock_NextWhenStoodOn(uint8_t *o) {
    if (obRoutine(o) == 4) {                            /* cmpi.b #4,obRoutine */
        obSubtype(o) += 1;                              /* addq.b #1,obSubtype */
    }
}

/* Tipo 3: se mueve a la derecha, se detiene al chocar pared */
static void MBlock_Right_StopOnWall(uint8_t *o) {
    int16_t d3 = (int16_t)obActWid(o);
    int16_t d1 = MBlock_ObjHitWallRight(o, d3);
    if (d1 < 0) {                                       /* bmi.s .stopPlatform */
        obSubtype(o) = 0;                               /* clr.b obSubtype */
        return;
    }
    obX(o) += 1;                                        /* addq.w #1,obX */
    mblock_origX(o) = obX(o);                           /* move.w obX,mblock_origX */
}

/* Tipo 5: se mueve a la derecha, cae al chocar pared */
static void MBlock_Right_FallOnWall(uint8_t *o) {
    int16_t d3 = (int16_t)obActWid(o);
    int16_t d1 = MBlock_ObjHitWallRight(o, d3);
    if (d1 < 0) {                                       /* bmi.s .fallDown */
        obSubtype(o) += 1;                              /* → tipo 6 */
        return;
    }
    obX(o) += 1;
    mblock_origX(o) = obX(o);
}

/* Tipo 6: cae, se detiene al tocar el piso */
static void MBlock_FallingDown(uint8_t *o) {
    SpeedToPos(o);                                      /* bsr.w SpeedToPos */
    obVelY(o) = (int16_t)(obVelY(o) + 0x18);            /* addi.w #$18 */

    int16_t dist, angle;
    ObjFloorDist(o, &dist, &angle);                     /* bsr.w ObjFloorDist */
    if (dist < 0) {                                     /* tst.w d1 / bpl.s .return */
        obY(o) = (int16_t)(obY(o) + dist);              /* add.w d1,obY */
        obVelY(o) = 0;                                  /* clr.w obVelY */
        obSubtype(o) = 0;                               /* clr.b obSubtype */
    }
}

/* Tipo 7: raft secreto de LZ1 (switch ID 2). Devuelve 1 para señalizar
 * al caller que debe saltarse PlatformObject/MvSonicOnPtfm2/DisplaySprite,
 * replicando el `addq.l #4,sp` del ASM. */
static int MBlock_SecretLZ1Raft(uint8_t *o) {
    if (RAM_BYTE(f_switch + 2) != 0) {                  /* tst.b (f_switch+2) */
        obSubtype(o) -= 3;                              /* subq.b #3 → tipo 4 */
    }
    /* .hidePlatform: addq.l #4,sp ; out_of_range.w DeleteObject,mblock_origX */
    if (OutOfRange(o, mblock_origX(o), 0)) {
        DeleteObject(o);
    }
    return 1;
}

/* Tipo 8: arriba/abajo continuo (freq 4, mid $40) */
static void MBlock_UpDown(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x1E);
    if (obStatus(o) & 1) {                              /* btst #0,obStatus */
        d0 = (int16_t)(-d0);
        d0 = (int16_t)(d0 + 0x80);                      /* add.w d1,d0 */
    }
    obY(o) = (int16_t)(mblock_origY(o) - d0);
}

/* Tipo A: slide rápido a la derecha y vuelve (red sliding floors SBZ) */
static void MBlock_SlideFast(uint8_t *o) {
    int16_t d3 = (int16_t)(int8_t)obActWid(o);
    d3 = (int16_t)(d3 + d3);                            /* add.w d3,d3: ancho total */
    int16_t d1 = 8;                                     /* moveq #8,d1 */
    if (obStatus(o) & 1) {                              /* btst #0,obStatus */
        d1 = (int16_t)(-d1);                            /* neg.w d1 */
        d3 = (int16_t)(-d3);                            /* neg.w d3 */
    }

    if (mblock_slide_goback(o) != 0) {                  /* tst.w / bne.s .goingBack */
        /* .goingBack */
        int16_t d0 = (int16_t)(obX(o) - mblock_origX(o));
        if (d0 == 0) {                                  /* beq.s .reset */
            mblock_slide_goback(o) = 0;                 /* clr.w */
            obSubtype(o) -= 1;                          /* subq.b #1 → tipo 9 */
            return;
        }
        obX(o) = (int16_t)(obX(o) - d1);                /* sub.w d1,obX */
        return;
    }

    /* .slide */
    {
        int16_t d0 = (int16_t)(obX(o) - mblock_origX(o));
        if (d0 == d3) {                                 /* cmp.w d3,d0 / beq.s .waiting */
            /* .waiting */
            mblock_slide_wait(o)--;
            if (mblock_slide_wait(o) == 0) {            /* bne.s .return */
                mblock_slide_goback(o) = 1;             /* move.w #1 */
            }
            return;
        }
        obX(o) = (int16_t)(obX(o) + d1);                /* add.w d1,obX */
        mblock_slide_wait(o) = 5 * 60;                  /* move.w #5*60 */
    }
}

/* --- MBlock_Move — dispatcher por subtype & $F ---
 * Devuelve 1 si el caller debe saltarse el resto de su procesamiento
 * (solo para tipo 7, replicando el `addq.l #4,sp` del ASM). */
static int MBlock_Move(uint8_t *o) {
    switch (obSubtype(o) & 0x0F) {
        case 0x0: MBlock_Stationary(o);         return 0;
        case 0x1: MBlock_LeftRight(o);          return 0;
        case 0x2:
        case 0x4:
        case 0x9: MBlock_NextWhenStoodOn(o);    return 0;
        case 0x3: MBlock_Right_StopOnWall(o);   return 0;
        case 0x5: MBlock_Right_FallOnWall(o);   return 0;
        case 0x6: MBlock_FallingDown(o);        return 0;
        case 0x7: return MBlock_SecretLZ1Raft(o);
        case 0x8: MBlock_UpDown(o);             return 0;
        case 0xA: MBlock_SlideFast(o);          return 0;
    }
    return 0;
}

/* --- Dispatcher Object 52 --- */
static void MovingBlock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                             /* MBlock_Index */
        case 0: MBlock_Main(o);     break;
        case 2: MBlock_Platform(o); break;
        case 4: MBlock_StandOn(o);  break;
    }
}

/* ===========================================================================
 *  Object 54 — Invisible lava tag / hurt marker (MZ)
 *  Ported verbatim from _incObj/54 MZ Invisible Lava Tag.asm
 *  (REV01, FixBugs=0).
 *
 *  No tiene campos propios: solo configura obColType según el subtype y
 *  deja que ReactToItem lo detecte. obRender lleva sprite_rendered para
 *  que ReactToItem no lo saltee (ver comentario en el ASM).
 * =========================================================================== */

/* LTag_ColTypes: una entrada de 1 byte por subtype (0..2). */
static const uint8_t LTag_ColTypes[3] = {
    (uint8_t)(col_64x64  | col_hurt),   /* subtype 00 - pequeño  */
    (uint8_t)(col_128x64 | col_hurt),   /* subtype 01 - mediano  */
    (uint8_t)(col_256x64 | col_hurt),   /* subtype 02 - grande   */
};

static void LTag_Main(uint8_t *o);

/* --- LTag_ChkDel — Routine 2 --- */
static void LTag_ChkDel(uint8_t *o) {
    /* out_of_range.w DeleteObject,obX(a0),1 ; rts
     * El objeto se borra si está fuera de rango, pero NUNCA se muestra
     * (mappings en blanco, y no hay DisplaySprite). */
    if (OutOfRange(o, -1, 1)) {                            /* usa obX(o), bmicheck=1 (Macros.asm 289-292) */
        DeleteObject(o);
    }
}

/* --- LTag_Main — Routine 0 --- */
static void LTag_Main(uint8_t *o) {
    obRoutine(o) += 2;                                  /* → LTag_ChkDel */

    {
        uint8_t d0 = obSubtype(o);                      /* moveq #0,d0 ; move.b obSubtype */
        obColType(o) = LTag_ColTypes[d0];               /* move.b LTag_ColTypes(pc,d0.w),obColType */
    }

    obMap(o)    = (uint32_t)(uintptr_t)Map_LTag;        /* mappings en blanco */
    obRender(o) = sprite_rendered | sprite_cam_field;   /* $80 | $04: visible para ReactToItem */

    /* Fall-through a LTag_ChkDel (no hay rts al final de LTag_Main en el ASM) */
    LTag_ChkDel(o);
}

/* --- Dispatcher Object 54 --- */
static void LavaTag_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                             /* LTag_Index */
        case 0: LTag_Main(o);   break;
        case 2: LTag_ChkDel(o); break;
    }
}

/* ===========================================================================
 *  Object 55 — Basaran enemy (MZ)
 *  Ported verbatim from _incObj/55 Badnik - Basaran.asm (REV01, FixBugs=0).
 *
 *  bas_sonicY = objoff_36 (word): copia de la Y de Sonic en el instante en
 *                                 que el Basaran empezó a bajar; se usa para
 *                                 detener su caída a esa altura
 * =========================================================================== */

#define bas_sonicY(o) (*(int16_t *)((uint8_t *)(o) + 0x36))

static void Bas_Main(uint8_t *o);
static void Bas_Action(uint8_t *o);
static void Bas_Action_ChkSonic(uint8_t *o);
static void Bas_Action_DropDown(uint8_t *o);
static void Bas_Action_Fly(uint8_t *o);
static void Bas_Action_BackToCeiling(uint8_t *o);

/* Helper local — port de ObjHitCeiling (_incObj/sub ObjFloorDist.asm).
 * Igual que Sonic_FindCeiling_Quick pero usando obHeight del objeto en
 * lugar de sonic_quick_size. Devuelve d1 = distancia al techo. */
static int16_t Bas_ObjHitCeiling(uint8_t *o) {
    int16_t d1;
    int16_t y = (int16_t)((obY(o) - (int8_t)obHeight(o)) ^ 0xF);
    int16_t x = obX(o);
    FindFloor(y, x, 0x0E, 0x1000, -0x10, &v_anglebuffer, o, &d1);
    return d1;
}

/* --- Bas_CheckDistanceAndFaceSonic ---
 * Entrada: d2 = tamaño de la zona de trigger (128).
 * Salida:  obStatus bit 0 ajustado (X-flip).
 *          *out_d1 = velocidad horizontal (0x100 o -0x100).
 *          Devuelve 1 si Sonic está dentro de la zona (C=1 en ASM),
 *          0 si está fuera (C=0 → bhs tomado en el ASM). */
static int Bas_CheckDistanceAndFaceSonic(uint8_t *o, int16_t d2, int16_t *out_d1) {
    int16_t d1 = 0x100;                             /* move.w #$100,d1 */
    obStatus(o) |= (1 << 0);                        /* bset #0: face right */
    int16_t d0 = (int16_t)(obX(RAM_ADDR(v_player)) - obX(o));
    if (d0 < 0) {                                   /* bhs.s .checkDistance */
        d0 = (int16_t)(-d0);                        /* neg.w d0 */
        d1 = (int16_t)(-d1);                        /* neg.w d1 */
        obStatus(o) &= (uint8_t)~(1 << 0);          /* bclr #0: face left */
    }
    if (out_d1) *out_d1 = d1;
    /* cmp.w d2,d0: C=1 si d0 < d2 (in range) */
    return (uint16_t)d0 < (uint16_t)d2;
}

/* --- Bas_Main — Routine 0 --- */
static void Bas_Main(uint8_t *o) {
    obRoutine(o) += 2;                              /* → Bas_Action */
    obMap(o)      = (uint32_t)(uintptr_t)Map_Bas;
    obGfx(o)      = (uint16_t)(ArtTile_Basaran | Tile_Prio);
    obRender(o)   = sprite_cam_field;
    obHeight(o)   = 24 / 2;
    obPriority(o) = 2;
    obColType(o)  = (uint8_t)(col_16x16 | col_badnik);
    obActWid(o)   = 32 / 2;

    /* Fall-through a Bas_Action (el ASM no tiene rts al final de Bas_Main) */
    Bas_Action(o);
}

/* --- Bas_Action — Routine 2 --- */
static void Bas_Action(uint8_t *o) {
    switch (ob2ndRout(o)) {                         /* Bas_ActIndex */
        case 0: Bas_Action_ChkSonic(o);        break;
        case 2: Bas_Action_DropDown(o);        break;
        case 4: Bas_Action_Fly(o);             break;
        case 6: Bas_Action_BackToCeiling(o);   break;
    }
    if (Ani_Bas) AnimateSprite(o, Ani_Bas);
    RememberState(o);
}

/* --- Bas_Action_ChkSonic — sub-rutina 0 --- */
static void Bas_Action_ChkSonic(uint8_t *o) {
    /* .dropcheck: comprueba distancia horizontal (128px) */
    if (!Bas_CheckDistanceAndFaceSonic(o, 128, NULL)) return;   /* bhs.s .return */

        int16_t sonicY = (int16_t)obY(RAM_ADDR(v_player));          /* move.w (v_player+obY),d0 */
        int16_t basY   = obY(o);
    bas_sonicY(o) = sonicY;                                     /* move.w d0,bas_sonicY */

    int16_t d0 = (int16_t)(sonicY - basY);                      /* sub.w obY(a0),d0 */
    if ((uint16_t)sonicY < (uint16_t)basY) return;              /* blo.s .return: Sonic arriba */
        if ((uint16_t)d0 >= 128) return;                            /* cmpi.w #128 / bhs.s .return */
            if (v_debuguse) return;                                     /* tst.w (v_debuguse).w / bne.s .return */

                /* Stagger por índice de objeto + VBlank byte: sólo cae cada 8 frames */
                uint8_t d7 = (uint8_t)((NUM_OBJECTS - 1) - Object_GetIndex(o));
            uint8_t v  = (uint8_t)(v_vblank_byte + d7);
        if (v & 7) return;                                          /* andi.b #7 / bne.s .return */

            obAnim(o) = 1;                                              /* .drop animation */
            ob2ndRout(o) += 2;                                          /* → Bas_Action_DropDown */
}

/* --- Bas_Action_DropDown — sub-rutina 2 --- */
static void Bas_Action_DropDown(uint8_t *o) {
    int16_t d1;                                                 /* guardará velocidad de Bas_Check... */
    SpeedToPos(o);                                              /* bsr.w SpeedToPos */
    obVelY(o) = (int16_t)(obVelY(o) + 0x18);                    /* addi.w #$18,obVelY */

    /* .dropfly: la distancia se ignora pero d1 (velocidad) se usa después */
    Bas_CheckDistanceAndFaceSonic(o, 128, &d1);

    int16_t d0 = (int16_t)(bas_sonicY(o) - obY(o));             /* sub.w obY(a0),d0 */
    if ((uint16_t)bas_sonicY(o) < (uint16_t)obY(o)) {
        /* blo.s Bas_Action_DropDown_Delete — dead code path (Sonic "arriba") */
        if ((int8_t)obRender(o) >= 0) {                         /* bpl.w DeleteObject */
            DeleteObject(o);
        }
        return;                                                 /* .return: rts */
    }
    if ((uint16_t)d0 >= 16) return;                             /* cmpi.w #16 / bhs.s .return */

        obVelX(o) = d1;                                             /* move.w d1,obVelX */
        obVelY(o) = 0;                                              /* move.w #0,obVelY */
        obAnim(o) = 2;                                              /* .fly animation */
        ob2ndRout(o) += 2;                                          /* → Bas_Action_Fly */
}

/* --- Bas_Action_Fly — sub-rutina 4 --- */
static void Bas_Action_Fly(uint8_t *o) {
    /* .flapsound: sfx cada 16 frames */
    if ((v_vblank_byte & 0x0F) == 0) {                          /* andi.b #$F / bne.s .move */
        Sound_Queue(sfx_Basaran, false);                        /* jsr QueueSound2 */
    }

    /* .move */
    SpeedToPos(o);

    int16_t d0 = (int16_t)(obX(RAM_ADDR(v_player)) - obX(o));   /* move.w (v_player+obX),d0 ; sub.w obX */
    if (d0 < 0) d0 = (int16_t)(-d0);                            /* neg.w d0 */
        if ((uint16_t)d0 < 128) return;                             /* cmpi.w #128 / blo.s .return */

            /* Stagger por índice de objeto + VBlank byte */
            uint8_t d7 = (uint8_t)((NUM_OBJECTS - 1) - Object_GetIndex(o));
        uint8_t v  = (uint8_t)(v_vblank_byte + d7);
    if (v & 7) return;                                          /* andi.b #7 / bne.s .return */

        ob2ndRout(o) += 2;                                          /* → Bas_Action_BackToCeiling */
}

/* --- Bas_Action_BackToCeiling — sub-rutina 6 --- */
static void Bas_Action_BackToCeiling(uint8_t *o) {
    SpeedToPos(o);                                              /* bsr.w SpeedToPos */
    obVelY(o) = (int16_t)(obVelY(o) - 0x18);                    /* subi.w #$18,obVelY */

    int16_t d1 = Bas_ObjHitCeiling(o);                          /* bsr.w ObjHitCeiling */
    if (d1 >= 0) return;                                        /* tst.w d1 / bpl.s .return */

        obY(o) = (int16_t)(obY(o) - d1);                            /* sub.w d1,obY: alinear al techo */
        obX(o) = (int16_t)(obX(o) & 0xFFF8);                        /* andi.w #$FFF8: snap a múltiplo de 8 */
        obVelX(o) = 0;                                              /* clr.w obVelX */
        obVelY(o) = 0;                                              /* clr.w obVelY */
        obAnim(o) = 0;                                              /* clr.b obAnim: .hang */
        ob2ndRout(o) = 0;                                           /* clr.b ob2ndRout → vuelve a ChkSonic */
}

/* --- Dispatcher Object 55 --- */
static void Basaran_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                                     /* Bas_Index */
        case 0: Bas_Main(o);   break;
        case 2: Bas_Action(o); break;
    }
}

/* ===========================================================================
 *  Object 56 — Floating Blocks (SYZ/SLZ) and Large Doors (LZ)
 *  Ported verbatim from _incObj/56 SYZ, SLZ Floating Blocks and LZ Doors.asm
 *  (REV01, Revision<>0). FixBugs=0.
 *
 *  Campos (los mismos offsets que en el ASM):
 *    fb_origY    = objoff_30 (word): Y original
 *    fb_origX    = objoff_34 (word): X original
 *    fb_moving   = objoff_38 (byte): flag "está moviéndose"
 *    fb_distance = objoff_3A (word): distancia restante a recorrer
 *    fb_switch   = objoff_3C (byte): ID del switch que lo activa (LZ)
 *
 *  Dispatch por (obSubtype & $F) → FBlock_TypeIndex. SolidObject se aplica
 *  siempre (salvo off-screen). El borrado final (out_of_range) tiene el
 *  hack REV01 para el bloque horizontal de SYZ3 (subtype $37).
 * =========================================================================== */

#define fb_origY(o)    (*(int16_t  *)((uint8_t *)(o) + 0x30))
#define fb_origX(o)    (*(int16_t  *)((uint8_t *)(o) + 0x34))
#define fb_moving(o)   (*(uint8_t  *)((uint8_t *)(o) + 0x38))
#define fb_distance(o) (*(uint16_t *)((uint8_t *)(o) + 0x3A))
#define fb_switch(o)   (*(uint8_t  *)((uint8_t *)(o) + 0x3C))

/* FBlock_Var: { half-width, half-height } por tipo. El índice es
   (subtype >> 3) & $E, o sea (subtype >> 4) * 2 como byte offset.
   Ocho entradas: $0x/$8x..$7x/$Fx. */
static const uint8_t FBlock_Var[8][2] = {
    { 32/2,  32/2 },   /* $0x/$8x — SYZ 1x1 block                        */
    { 64/2,  64/2 },   /* $1x/$9x — SYZ 2x2 square up/down               */
    { 32/2,  64/2 },   /* $2x/$Ax — SYZ 1x2 door                         */
    { 64/2,  52/2 },   /* $3x/$Bx — SYZ special horizontally-moving block*/
    { 32/2,  78/2 },   /* $4x/$Cx — (unused)                              */
    { 32/2,  32/2 },   /* $5x/$Dx — SLZ rotating stairway block          */
    { 16/2,  64/2 },   /* $6x/$Ex — LZ small vertical door               */
    { 128/2, 32/2 },   /* $7x/$Fx — LZ large sideways 4x1 block          */
};

static void FBlock_Action(uint8_t *o);
static void FBlock_Stationary(uint8_t *o);
static void FBlock_LeftRight_Small(uint8_t *o);
static void FBlock_LeftRight_Large(uint8_t *o);
static void FBlock_MoveLR(uint8_t *o, int16_t d0, int16_t d1);
static void FBlock_UpDown_Small(uint8_t *o);
static void FBlock_UpDown_Large(uint8_t *o);
static void FBlock_MoveUD(uint8_t *o, int16_t d0, int16_t d1);
static void FBlock_LZSmallDoor_Open(uint8_t *o);
static void FBlock_LZSmallDoor_Close(uint8_t *o);
static void FBlock_HorizontalSYZ3(uint8_t *o);
static void FBlock_SLZStair_Smallest(uint8_t *o);
static void FBlock_SLZStair_Small(uint8_t *o);
static void FBlock_SLZStair_Large(uint8_t *o);
static void FBlock_SLZStair_Largest(uint8_t *o);
static void FBlock_SLZStair_MoveSquare(uint8_t *o, int16_t d0, int16_t d1, int16_t d3);
static void FBlock_LZHorizDoor_Open(uint8_t *o);
static void FBlock_LZHorizDoor_Close(uint8_t *o);

/* --- FBlock_Main — Routine 0 ----------------------------------------- */
static void FBlock_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* → FBlock_Action */
    obMap(o)     = (uint32_t)(uintptr_t)Map_FBlock;

    obGfx(o) = (uint16_t)(ArtTile_Level | Tile_Pal3);
    if ((uint8_t)v_zone == id_LZ) {
        obGfx(o) = (uint16_t)(ArtTile_LZ_Door | Tile_Pal3);
    }

    obRender(o)   = sprite_cam_field;
    obPriority(o) = 3;

    uint8_t sub = obSubtype(o);
    uint8_t d0  = (uint8_t)(sub >> 3);                       /* lsr.w #3,d0 */
    d0 = (uint8_t)(d0 & 0x0E);                               /* andi.w #$E,d0 */

    obActWid(o) = FBlock_Var[d0 >> 1][0];                    /* move.b (a2)+,obActWid */
    obHeight(o) = FBlock_Var[d0 >> 1][1];                    /* move.b (a2),obHeight   */
    obFrame(o)  = (uint8_t)(d0 >> 1);                        /* lsr.w #1,d0 → frame ID */

    fb_origX(o) = obX(o);
    fb_origY(o) = obY(o);

    fb_distance(o) = (uint16_t)(FBlock_Var[d0 >> 1][1] * 2); /* d0 = 2 * half-height */

    /* ---- Revision<>0: hack especial para el bloque horizontal de SYZ3 ---- */
    if (sub == 0x37) {
        /* Dos de estos bloques existen en el layout de SYZ3:
           X=$1BB8 → bloque "real" del túnel.
           X=$1F38 → bloque "fake" que cierra el túnel.
           Solo uno de los dos se muestra, según el estado de f_obj56. */
        if (obX(o) == 0x1BB8) {
            /* .realBlock */
            if (f_obj56) {
                DeleteObject(o);
                return;
            }
        } else {
            /* .fakeBlock */
            obSubtype(o) = 0;                                /* forzar FBlock_Stationary */
            if (!f_obj56) {
                DeleteObject(o);
                return;
            }
        }
    }

    /* ---- Setup SLZ staircase / LZ door ---- */
    if ((uint8_t)v_zone != id_LZ) {
        d0 = (uint8_t)((sub & 0x0F) - 8);                    /* subq.w #8,d0 */
        if ((d0 & 0x80) == 0) {                              /* bcs → skip si < 8 */
            d0 = (uint8_t)(d0 << 2);                         /* lsl.w #2: 4 bytes por entrada */
            int16_t osc = (int16_t)RAM_WORD(v_oscillate + 0x2A + 2 + d0);
            if (osc < 0) {                                   /* bpl.s .setupLZDoor */
                obStatus(o) ^= 1;                            /* bchg #0: invertir X-flip */
            }
        }
    }

    /* .setupLZDoor */
    d0 = obSubtype(o);
    if ((d0 & 0x80) == 0) {                                  /* bpl.s FBlock_Action */
        FBlock_Action(o);
        return;
    }

    /* Subtipo $80+ → LZ door activado por switch */
    d0 &= 0x0F;
    fb_switch(o) = d0;
    obSubtype(o) = 5;                                        /* FBlock_LZSmallDoor_Open */
    if (obFrame(o) == 7) {
        obSubtype(o) = 0x0C;                                 /* FBlock_LZHorizDoor_Open */
        fb_distance(o) = 128;
    }

    /* .chkState: comprueba si la puerta ya se abrió antes */
    {
        uint8_t respawn = obRespawnNo(o);
        if (respawn != 0) {
            uint8_t *a2 = RAM_ADDR(v_objstate);
            a2[2 + respawn] &= (uint8_t)~0x80;               /* bclr #7: quitar flag de bloqueo */
            if (a2[2 + respawn] & 1) {                       /* btst #0: ¿ya abierta? */
                obSubtype(o)++;                              /* saltar a Close (ya abierta) */
                fb_distance(o) = 0;                          /* no queda distancia por recorrer */
            }
        }
    }

    FBlock_Action(o);
}

/* --- FBlock_Action — Routine 2 --------------------------------------- */
static void FBlock_Action(uint8_t *o) {
    int16_t prev_x = obX(o);                                 /* move.w obX(a0),-(sp) */

    uint8_t type = (uint8_t)(obSubtype(o) & 0x0F);
    switch (type) {                                          /* FBlock_TypeIndex */
        case 0x0: FBlock_Stationary(o);             break;
        case 0x1: FBlock_LeftRight_Small(o);        break;
        case 0x2: FBlock_LeftRight_Large(o);        break;
        case 0x3: FBlock_UpDown_Small(o);           break;
        case 0x4: FBlock_UpDown_Large(o);           break;
        case 0x5: FBlock_LZSmallDoor_Open(o);       break;
        case 0x6: FBlock_LZSmallDoor_Close(o);      break;
        case 0x7: FBlock_HorizontalSYZ3(o);         break;
        case 0x8: FBlock_SLZStair_Smallest(o);      break;
        case 0x9: FBlock_SLZStair_Small(o);         break;
        case 0xA: FBlock_SLZStair_Large(o);         break;
        case 0xB: FBlock_SLZStair_Largest(o);       break;
        case 0xC: FBlock_LZHorizDoor_Open(o);       break;
        case 0xD: FBlock_LZHorizDoor_Close(o);      break;
    }

    /* SolidObject (solo si el objeto está on-screen) */
    if ((int8_t)obRender(o) < 0) {                           /* bpl.s .chkDel */
        int16_t d1 = (int16_t)((int16_t)obActWid(o) + sonic_solid_width);
        int16_t d2 = (int16_t)obHeight(o);
        int16_t d3 = (int16_t)(d2 + 1);                      /* addq.w #1,d3 */
        int16_t out_d3 = 0, out_d5 = 0;
        SolidObject(o, d1, d2, d3, prev_x, &out_d3, &out_d5);
    }

    /* .chkDel (REV01) */
    if (OutOfRange(o, fb_origX(o), 0)) {
        /* out_of_range → .checkSYZSpecial */
        if (type == 0x7 && fb_moving(o) != 0) {
            /* El bloque horizontal de SYZ3 en movimiento nunca se borra:
               aunque esté fuera de rango, sigue avanzando hasta destino. */
        } else {
            DeleteObject(o);
            return;
        }
    }
    DisplaySprite(o);                                        /* .display */
}

/* --- Type 0 — estacionario ------------------------------------------- */
static void FBlock_Stationary(uint8_t *o) { (void)o; }

/* --- Type 1 — L/R pequeño (freq 2, mid $20) -------------------------- */
static void FBlock_LeftRight_Small(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x0A);
    FBlock_MoveLR(o, d0, 0x20 * 2);
}

/* --- Type 2 — L/R grande (freq 4, mid $40) --------------------------- */
static void FBlock_LeftRight_Large(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x1E);
    FBlock_MoveLR(o, d0, 0x40 * 2);
}

static void FBlock_MoveLR(uint8_t *o, int16_t d0, int16_t d1) {
    if (obStatus(o) & 1) {                                   /* btst #0,obStatus */
        d0 = (int16_t)(-d0);                                 /* neg.w d0 */
        d0 = (int16_t)(d0 + d1);                             /* add.w d1,d0 */
    }
    int16_t x = (int16_t)(fb_origX(o) - d0);                 /* sub.w d0,d1 */
    obX(o) = x;
}

/* --- Type 3 — U/D pequeño (freq 2, mid $20) -------------------------- */
static void FBlock_UpDown_Small(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x0A);
    FBlock_MoveUD(o, d0, 0x20 * 2);
}

/* --- Type 4 — U/D grande (freq 4, mid $40) --------------------------- */
static void FBlock_UpDown_Large(uint8_t *o) {
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x1E);
    FBlock_MoveUD(o, d0, 0x40 * 2);
}

static void FBlock_MoveUD(uint8_t *o, int16_t d0, int16_t d1) {
    if (obStatus(o) & 1) {
        d0 = (int16_t)(-d0);
        d0 = (int16_t)(d0 + d1);
    }
    int16_t y = (int16_t)(fb_origY(o) - d0);
    obY(o) = y;
}

/* --- Type 5 — LZ small door: abrir al pulsar switch ------------------ */
static void FBlock_LZSmallDoor_Open(uint8_t *o) {
    if (fb_moving(o) == 0) {
        /* .checkSwitch */
        if (RAM_U16(0xFE10) == id_LZ_act1 && fb_switch(o) == 3) {
            /* Caso especial LZ1: gestiona f_wtunneldisallow según posición */
            f_wtunneldisallow = 0;
            int16_t son_x = obX(RAM_ADDR(v_player));
            if (son_x < obX(o)) {                            /* blo → Sonic a la izq */
                f_wtunneldisallow = 1;
            }
        }

        {
            uint8_t *a2 = RAM_ADDR(f_switch);
            uint8_t sw = fb_switch(o);
            if (!(a2[sw] & 1)) {                             /* btst #0 / beq.s .updatePosition */
                goto update_position;
            }
            if (RAM_U16(0xFE10) == id_LZ_act1 && sw == 3) {
                f_wtunneldisallow = 0;                       /* permitir que el túnel succione a Sonic */
            }
        }
        fb_moving(o) = 1;
    }

    /* .opening */
    if (fb_distance(o) != 0) {
        fb_distance(o) = (uint16_t)(fb_distance(o) - 2);
    } else {
        /* .fullyOpened */
        obSubtype(o)++;
        fb_moving(o) = 0;
        uint8_t respawn = obRespawnNo(o);
        if (respawn != 0) {
            uint8_t *a2 = RAM_ADDR(v_objstate);
            a2[2 + respawn] |= 1;                            /* bset #0 */
        }
    }

update_position:
    {
        int16_t d0 = (int16_t)fb_distance(o);
        if (obStatus(o) & 1) d0 = (int16_t)(-d0);
        int16_t y = (int16_t)(fb_origY(o) + d0);
        obY(o) = y;
    }
}

/* --- Type 6 — LZ small door: cerrar (ver nota de fidelidad) ---------- */
static void FBlock_LZSmallDoor_Close(uint8_t *o) {
    if (fb_moving(o) == 0) {
        uint8_t *a2 = RAM_ADDR(f_switch);
        uint8_t sw = fb_switch(o);
        if ((int8_t)a2[sw] < 0) {                            /* tst.b / bpl.s .updatePosition */
            fb_moving(o) = 1;
        } else {
            goto update_position;
        }
    }

    /* .closing */
    {
        int16_t full = (int16_t)(obHeight(o) * 2);           /* add.w d0,d0 */
        if ((int16_t)fb_distance(o) != full) {
            fb_distance(o) = (uint16_t)(fb_distance(o) + 2);
        } else {
            /* .fullyClosed */
            obSubtype(o)--;
            fb_moving(o) = 0;
            uint8_t respawn = obRespawnNo(o);
            if (respawn != 0) {
                uint8_t *a2 = RAM_ADDR(v_objstate);
                a2[2 + respawn] &= (uint8_t)~1;              /* bclr #0 */
            }
        }
    }

update_position:
    {
        int16_t d0 = (int16_t)fb_distance(o);
        if (obStatus(o) & 1) d0 = (int16_t)(-d0);
        int16_t y = (int16_t)(fb_origY(o) + d0);
        obY(o) = y;
    }
}

/* --- Type 7 — bloque horizontal especial de SYZ3 -------------------- */
static void FBlock_HorizontalSYZ3(uint8_t *o) {
    if (fb_moving(o) == 0) {
        if (RAM_BYTE(f_switch + 0x0F) == 0) return;          /* tst.b (f_switch+$F) */
        fb_moving(o) = 1;
        fb_distance(o) = 0;
    }

    /* .moveRight */
    obX(o) = (int16_t)(obX(o) + 1);                          /* addq.w #1,obX */
    fb_origX(o) = obX(o);
    fb_distance(o) = (uint16_t)(fb_distance(o) + 1);
    if (fb_distance(o) != 0x380) return;                     /* cmpi.w #$380 / bne.s .return */

    /* Llegó al destino ($1BB8 + $380 = $1F38) */
    f_obj56 = 1;                                             /* marca túnel como cerrado permanentemente */
    fb_moving(o) = 0;
    obSubtype(o) = 0;                                        /* FBlock_Stationary */
}

/* --- Types 8/B — SLZ square stairway blocks -------------------------- */
static void FBlock_SLZStair_Smallest(uint8_t *o) {
    int16_t d1 = 0x10;
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x2A);
    d0 = (int16_t)(d0 >> 1);                                 /* lsr.w #1 */
    int16_t d3 = (int16_t)RAM_WORD(v_oscillate + 0x2C);
    FBlock_SLZStair_MoveSquare(o, d0, d1, d3);
}

static void FBlock_SLZStair_Small(uint8_t *o) {
    int16_t d1 = 0x30;
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x2E);
    int16_t d3 = (int16_t)RAM_WORD(v_oscillate + 0x30);
    FBlock_SLZStair_MoveSquare(o, d0, d1, d3);
}

static void FBlock_SLZStair_Large(uint8_t *o) {
    int16_t d1 = 0x50;
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x32);
    int16_t d3 = (int16_t)RAM_WORD(v_oscillate + 0x34);
    FBlock_SLZStair_MoveSquare(o, d0, d1, d3);
}

static void FBlock_SLZStair_Largest(uint8_t *o) {
    int16_t d1 = 0x70;
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x36);
    int16_t d3 = (int16_t)RAM_WORD(v_oscillate + 0x38);
    FBlock_SLZStair_MoveSquare(o, d0, d1, d3);
}

static void FBlock_SLZStair_MoveSquare(uint8_t *o, int16_t d0, int16_t d1, int16_t d3) {
    if (d3 == 0) {                                           /* tst.w d3 / bne.s .checkFlipped */
        obStatus(o) = (uint8_t)((obStatus(o) + 1) & 3);      /* rota los 4 estados flip */
    }

    /* .checkFlipped */
    uint8_t d2 = (uint8_t)(obStatus(o) & 3);
    if (d2 == 0) {
        /* Sin flips: movimiento hacia la derecha por el borde superior */
        int16_t x = (int16_t)(d0 - d1 + fb_origX(o));        /* sub.w d1,d0 / add.w fb_origX,d0 */
        obX(o) = x;
        int16_t y = (int16_t)(fb_origY(o) - d1);             /* neg.w d1 / add.w fb_origY,d1 */
        obY(o) = y;
        return;
    }
    d2 = (uint8_t)(d2 - 1);
    if (d2 == 0) {
        /* X-flip: movimiento hacia abajo por el borde derecho */
        d1 = (int16_t)(d1 - 1);
        int16_t y = (int16_t)(-(d0 - d1) + fb_origY(o));     /* sub.w d1,d0 / neg.w d0 / add fb_origY */
        obY(o) = y;
        d1 = (int16_t)(d1 + 1);
        int16_t x = (int16_t)(d1 + fb_origX(o));
        obX(o) = x;
        return;
    }
    d2 = (uint8_t)(d2 - 1);
    if (d2 == 0) {
        /* Y-flip: movimiento hacia la izquierda por el borde inferior */
        d1 = (int16_t)(d1 - 1);
        int16_t x = (int16_t)(-(d0 - d1) + fb_origX(o));
        obX(o) = x;
        d1 = (int16_t)(d1 + 1);
        int16_t y = (int16_t)(d1 + fb_origY(o));
        obY(o) = y;
        return;
    }
    /* XY-flip: movimiento hacia arriba por el borde izquierdo */
    int16_t y = (int16_t)((d0 - d1) + fb_origY(o));
    obY(o) = y;
    int16_t x = (int16_t)(fb_origX(o) - d1);
    obX(o) = x;
}

/* --- Type C — LZ large sideways 4x1 door: abrir ---------------------- */
static void FBlock_LZHorizDoor_Open(uint8_t *o) {
    if (fb_moving(o) == 0) {
        uint8_t *a2 = RAM_ADDR(f_switch);
        uint8_t sw = fb_switch(o);
        if (!(a2[sw] & 1)) goto update_position;
        fb_moving(o) = 1;
    }

    /* .opening */
    if (fb_distance(o) != 0) {
        fb_distance(o) = (uint16_t)(fb_distance(o) - 2);
    } else {
        /* .fullyOpened */
        obSubtype(o)++;
        fb_moving(o) = 0;
        uint8_t respawn = obRespawnNo(o);
        if (respawn != 0) {
            uint8_t *a2 = RAM_ADDR(v_objstate);
            a2[2 + respawn] |= 1;
        }
    }

update_position:
    {
        int16_t d0 = (int16_t)fb_distance(o);
        if (obStatus(o) & 1) {
            d0 = (int16_t)(-d0);
            d0 = (int16_t)(d0 + 128);
        }
        int16_t x = (int16_t)(fb_origX(o) + d0);
        obX(o) = x;
    }
}

/* --- Type D — LZ large sideways 4x1 door: cerrar -------------------- */
static void FBlock_LZHorizDoor_Close(uint8_t *o) {
    if (fb_moving(o) == 0) {
        uint8_t *a2 = RAM_ADDR(f_switch);
        uint8_t sw = fb_switch(o);
        if ((int8_t)a2[sw] < 0) {
            fb_moving(o) = 1;
        } else {
            goto update_position;
        }
    }

    /* .closing */
    if ((int16_t)fb_distance(o) != 128) {
        fb_distance(o) = (uint16_t)(fb_distance(o) + 2);
    } else {
        /* .fullyClosed */
        obSubtype(o)--;
        fb_moving(o) = 0;
        uint8_t respawn = obRespawnNo(o);
        if (respawn != 0) {
            uint8_t *a2 = RAM_ADDR(v_objstate);
            a2[2 + respawn] &= (uint8_t)~1;
        }
    }

update_position:
    {
        int16_t d0 = (int16_t)fb_distance(o);
        if (obStatus(o) & 1) {
            d0 = (int16_t)(-d0);
            d0 = (int16_t)(d0 + 128);
        }
        int16_t x = (int16_t)(fb_origX(o) + d0);
        obX(o) = x;
    }
}

/* --- Dispatcher Object 56 — FBlock_Index: 0 = Main, 2 = Action ------- */
static void FloatingBlock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: FBlock_Main(o);   break;
        case 2: FBlock_Action(o); break;
    }
}

/* ===========================================================================
 *  Object 57 — Spiked Ball and Chain (SYZ, LZ)
 *  Ported verbatim from _incObj/57 SYZ, LZ Spiked Ball and Chain.asm
 *  (REV01, FixBugs=0).
 *
 *  Campos (offsets ASM):
 *    sball_children   = objoff_29 (byte): número de hijos spawneados
 *    sball_children_ram = objoff_2A..: índices de los hijos (1 byte c/u),
 *                                       seguidos del índice del propio padre
 *    sball_origY      = objoff_38 (word)
 *    sball_origX      = objoff_3A (word)
 *    sball_radius     = objoff_3C (byte): radio de este eslabón
 *    sball_speed      = objoff_3E (word): velocidad de giro (nibble alto << 3)
 *
 *  FixBugs=0: usa FindFreeObj (FixBugs usa FindNextFreeObj).
 * =========================================================================== */

#define sball_children(o) (*(uint8_t *)((uint8_t *)(o) + 0x29))
#define sball_origY(o)    (*(int16_t *)((uint8_t *)(o) + 0x38))
#define sball_origX(o)    (*(int16_t *)((uint8_t *)(o) + 0x3A))
#define sball_radius(o)   (*(uint8_t *)((uint8_t *)(o) + 0x3C))
#define sball_speed(o)    (*(int16_t *)((uint8_t *)(o) + 0x3E))

static void SBall_Move(uint8_t *o);
static void SBall_Child(uint8_t *o);
static void Sball_Twirl(uint8_t *o);
static void SBall_ChkDel(uint8_t *o);

/* --- SBall_Main — Routine 0 ------------------------------------------ */
static void SBall_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* → SBall_Move */
    obMap(o)      = (uint32_t)(uintptr_t)Map_SBall;
    obGfx(o)      = (uint16_t)ArtTile_SYZ_Spikeball_Chain;
    obRender(o)   = sprite_cam_field;
    obPriority(o) = 4;
    obActWid(o)   = 16 / 2;
    sball_origX(o) = obX(o);
    sball_origY(o) = obY(o);

    /* SYZ: la cadena daña (col_8x8|col_hurt). LZ: no daña y usa otro art/map. */
    obColType(o) = (uint8_t)(col_8x8 | col_hurt);
    if ((uint8_t)v_zone == id_LZ) {
        obColType(o) = col_none;
        obGfx(o)     = (uint16_t)ArtTile_LZ_Spikeball_Chain;
        obMap(o)     = (uint32_t)(uintptr_t)Map_SBall2;
    }

    uint8_t sub = obSubtype(o);

    /* Velocidad de giro: nibble alto << 3 con sign-extend (igual que 58). */
    {
        int16_t d1 = (int16_t)(sub & 0xF0);
        d1 = (int16_t)(int8_t)d1;                            /* ext.w d1 */
        d1 = (int16_t)(d1 << 3);                             /* asl.w #3 */
        sball_speed(o) = d1;
    }

    /* Ángulo inicial desde obStatus: ror.b #2 + andi.b #$C0. */
    {
        uint8_t d0 = obStatus(o);
        d0 = (uint8_t)((d0 >> 2) | (d0 << 6));               /* ror.b #2 */
        d0 &= 0xC0;                                          /* andi.b #%11000000 */
        obAngle(o) = d0;
    }

    /* ---- Spawn de la cadena ---- */
    {
        uint8_t *a2 = (uint8_t *)o + 0x29;                   /* sball_children */
        int16_t child_count = (int16_t)(sub & 7);
        int16_t d3 = (int16_t)(child_count << 4);            /* radius = count * $10 */
        *a2++ = 0;                                           /* sball_children = 0, a2 → 0x2A */
        sball_radius(o) = (uint8_t)d3;

        int16_t d1 = (int16_t)(child_count - 1);             /* subq.w #1,d1 */
        if (d1 < 0) goto finalize_tip;                       /* bcs.s .finalizeTip */
        if (sub & (1 << 3)) {                                /* btst #3,obSubtype */
            d1--;                                            /* subq.w #1,d1 */
            if (d1 < 0) goto finalize_tip;                   /* bcs.s .finalizeTip */
        }

        for (;;) {
            uint8_t *a1 = (uint8_t *)FindFreeObj();          /* bsr.w FindFreeObj */
            if (!a1) goto finalize_tip;                      /* bne.s .finalizeTip */

            sball_children(o)++;                             /* addq.b #1,sball_children */
            *a2++ = (uint8_t)Object_GetIndex(a1);            /* guardar índice del hijo */

            obRoutine(a1)  = 4;                              /* SBall_Child */
            obID(a1)       = obID(o);
            obMap(a1)      = obMap(o);
            obGfx(a1)      = obGfx(o);
            obRender(a1)   = obRender(o);
            obPriority(a1) = obPriority(o);
            obActWid(a1)   = obActWid(o);
            obColType(a1)  = obColType(o);

            d3 = (int16_t)(d3 - 0x10);                       /* subi.b #$10,d3 */
            sball_radius(a1) = (uint8_t)d3;

            if ((uint8_t)v_zone == id_LZ && d3 == 0) {
                obFrame(a1) = 2;                             /* LZ stem (el último eslabón pegado al piso) */
            }

            if (--d1 == -1) break;                           /* dbf d1 */
        }

finalize_tip:
        *a2++ = (uint8_t)Object_GetIndex(o);                 /* índice del padre al final */
    }

    if ((uint8_t)v_zone == id_LZ) {
        obColType(o) = (uint8_t)(col_16x16 | col_hurt);      /* solo la punta daña en LZ */
        obFrame(o)   = 1;
    }
}

/* --- SBall_Move — Routine 2 ----------------------------------------- */
static void SBall_Move(uint8_t *o) {
    Sball_Twirl(o);                                          /* bsr.w Sball_Twirl */
    SBall_ChkDel(o);                                         /* bra.w SBall_ChkDel */
}

/* --- Sball_Twirl — gira el padre y toda la cadena ------------------- */
static void Sball_Twirl(uint8_t *o) {
    /* Endianness: el ASM hace `add.w speed,obAngle` (word BE en 0x26-0x27)
       y luego lee SOLO el byte 0x26, que en 68k es el byte ALTO del word.
       En nuestro port little-endian hay que hacer la aritmética BE
       manualmente para no alternar 0x00/0x80 cada frame. */
    {
        uint8_t *p = (uint8_t *)o;
        uint16_t w = ((uint16_t)p[0x26] << 8) | p[0x27];
        w = (uint16_t)(w + (uint16_t)sball_speed(o));
        p[0x26] = (uint8_t)(w >> 8);
        p[0x27] = (uint8_t)(w & 0xFF);
    }

    uint8_t angle = obAngle(o);                              /* byte 0x26 */
    int16_t s0, s1;
    CalcSine(angle, &s0, &s1);                               /* s0=sin, s1=cos */

    int16_t d2 = sball_origY(o);
    int16_t d3 = sball_origX(o);

    uint8_t *a2 = (uint8_t *)o + 0x29;                       /* sball_children */
    uint8_t d6 = *a2++;                                      /* count */

    do {
        uint8_t idx = *a2++;
        uint8_t *a1 = (uint8_t *)Object_GetSlot((int)idx);

        int16_t d4 = (int16_t)sball_radius(a1);
        int16_t d5 = d4;

        d4 = (int16_t)(((int32_t)d4 * s0) >> 8);             /* muls.w d0,d4 / asr.l #8 */
        d5 = (int16_t)(((int32_t)d5 * s1) >> 8);             /* muls.w d1,d5 / asr.l #8 */

        d4 = (int16_t)(d4 + d2);                             /* add.w d2,d4 */
        d5 = (int16_t)(d5 + d3);                             /* add.w d3,d5 */

        obY(a1) = d4;
        obX(a1) = d5;
    } while (--d6 != 0xFF);                                  /* dbf d6 */
}

/* --- SBall_ChkDel — display o borra toda la cadena ------------------ */
static void SBall_ChkDel(uint8_t *o) {
    if (!OutOfRange(o, sball_origX(o), 0)) {                       /* out_of_range.w .delete */
        DisplaySprite(o);                                    /* bra.w DisplaySprite */
        return;
    }

    /* .delete: borra todos los hijos Y el padre (el padre está al final) */
    uint8_t *a2 = (uint8_t *)o + 0x29;
    uint8_t d2 = *a2++;                                      /* count */

    do {
        uint8_t idx = *a2++;
        uint8_t *a1 = (uint8_t *)Object_GetSlot((int)idx);
        DeleteObject(a1);                                    /* bsr.w DeleteChild */
    } while (--d2 != 0xFF);                                  /* dbf d2 */
}

/* --- SBall_Child — Routine 4: solo se dibuja ------------------------ */
static void SBall_Child(uint8_t *o) {
    DisplaySprite(o);                                        /* bra.w DisplaySprite */
}

/* --- Dispatcher Object 57 ------------------------------------------- */
static void SpikeBall_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                                  /* SBall_Index */
        case 0: SBall_Main(o);   break;
        case 2: SBall_Move(o);   break;
        case 4: SBall_Child(o);  break;
    }
}

/* ===========================================================================
 *  Object 58 — Big Spiked Metal Ball (SYZ)
 *  Ported verbatim from _incObj/58 SYZ Big Spiked Ball.asm (REV01, FixBugs=0).
 *
 *  Campos (offsets ASM):
 *    bball_origY  = objoff_38 (word)
 *    bball_origX  = objoff_3A (word)
 *    bball_radius = objoff_3C (byte): radio del círculo (subtype $x3 only)
 *    bball_speed  = objoff_3E (word): velocidad de rotación (subtype $x3 only)
 *
 *  Dispatcher por (obSubtype & 7): 0=estático, 1=izq/der, 2=arriba/abajo,
 *  3=círculo. El círculo guarda el ángulo en obAngle (byte en 0x26) y lo
 *  actualiza con suma de word (obAngle es un word de 2 bytes en el ASM
 *  original; el byte bajo es el ángulo real y el alto va aparte).
 * =========================================================================== */

#define bball_origY(o)  (*(int16_t *)((uint8_t *)(o) + 0x38))
#define bball_origX(o)  (*(int16_t *)((uint8_t *)(o) + 0x3A))
#define bball_radius(o) (*(uint8_t *)((uint8_t *)(o) + 0x3C))
#define bball_speed(o)  (*(int16_t *)((uint8_t *)(o) + 0x3E))



static void BBall_Move(uint8_t *o);
static void BBall_Type0_Stationary(uint8_t *o);
static void BBall_Type1_LeftRight(uint8_t *o);
static void BBall_Type2_UpDown(uint8_t *o);
static void BBall_Type3_Circling(uint8_t *o);

/* --- BBall_Main — Routine 0 ------------------------------------------ */
static void BBall_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* → BBall_Move */
    obMap(o)      = (uint32_t)(uintptr_t)Map_BBall;
    obGfx(o)      = (uint16_t)ArtTile_SYZ_Big_Spikeball;
    obRender(o)   = sprite_cam_field;
    obPriority(o) = 4;
    obActWid(o)   = 48 / 2;
    bball_origX(o) = obX(o);
    bball_origY(o) = obY(o);
    obColType(o)  = (uint8_t)(col_32x32 | col_hurt);         /* dañino al tocar */

    /* Velocidad de rotación: subtype & $F0 << 3 */
    uint8_t sub = obSubtype(o);
    int16_t d1 = (int16_t)((sub & 0xF0) >> 4);               /* parte alta: 0..$F */
    d1 = (int16_t)((d1 << 4) << 3);                          /* asl.w #3 sobre el valor original:
                                                                el ASM extiende un byte ($Fx) y
                                                                luego <<3, así que el resultado es
                                                                $F0 << 3 = $780 máx. Para replicar
                                                                esto uso el byte completo: */
    /* Recalculo exacto según el ASM: */
    d1 = (int16_t)(sub & 0xF0);                              /* move.b obSubtype / andi.b #$F0 */
    d1 = (int16_t)(int8_t)d1;                                /* ext.w d1 (sign-extend) */
    d1 = (int16_t)(d1 << 3);                                 /* asl.w #3 */
    bball_speed(o) = d1;

    /* Ángulo inicial desde obStatus: ror.b #2, andi.b #$C0 → (status & 3) << 6 */
    uint8_t d0 = obStatus(o);
    d0 = (uint8_t)((d0 >> 2) | (d0 << 6));                   /* ror.b #2 */
    d0 &= 0xC0;                                              /* andi.b #%11000000 */
    obAngle(o) = d0;

    bball_radius(o) = 0x50;                                  /* move.b #$50,bball_radius */

    /* ASM falls through into BBall_Move */
    BBall_Move(o);
}

/* --- BBall_Move — Routine 2 ----------------------------------------- */
static void BBall_Move(uint8_t *o) {
    switch (obSubtype(o) & 7) {                              /* BBall_Types */
        case 0: BBall_Type0_Stationary(o); break;
        case 1: BBall_Type1_LeftRight(o);  break;
        case 2: BBall_Type2_UpDown(o);     break;
        case 3: BBall_Type3_Circling(o);   break;
    }

    if (OutOfRange(o, bball_origX(o), 0)) {                        /* out_of_range.w DeleteObject,bball_origX */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
}

/* --- Subtype $x0: estacionario --------------------------------------- */
static void BBall_Type0_Stationary(uint8_t *o) { (void)o; }

/* --- Subtype $x1: izquierda/derecha --------------------------------- */
static void BBall_Type1_LeftRight(uint8_t *o) {
    int16_t d1 = 0x60;                                       /* move.w #$60,d1 */
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x0E);      /* moveq #0,d0 / move.b (v_oscillate+$E),d0 */

    if (obStatus(o) & 1) {                                   /* btst #0,obStatus */
        d0 = (int16_t)(-d0);                                 /* neg.w d0 */
        d0 = (int16_t)(d0 + d1);                             /* add.w d1,d0 */
    }
    /* .setX */
    d1 = bball_origX(o);                                     /* move.w bball_origX(a0),d1 */
    d1 = (int16_t)(d1 - d0);                                 /* sub.w d0,d1 */
    obX(o) = d1;
}

/* --- Subtype $x2: arriba/abajo --------------------------------------- */
static void BBall_Type2_UpDown(uint8_t *o) {
    int16_t d1 = 0x60;                                       /* move.w #$60,d1 */
    int16_t d0 = (int16_t)RAM_BYTE(v_oscillate + 0x0E);

    if (obStatus(o) & 1) {                                   /* btst #0,obStatus */
        d0 = (int16_t)(-d0);                                 /* neg.w d0 */
        d0 = (int16_t)(d0 + 0x60 + 0x20);                    /* addi.w #$60+$20,d0 */
    }
    /* .setY */
    d1 = bball_origY(o);                                     /* move.w bball_origY(a0),d1 */
    d1 = (int16_t)(d1 - d0);                                 /* sub.w d0,d1 */
    obY(o) = d1;
}

/* --- Subtype $x3: círculo -------------------------------------------- */
static void BBall_Type3_Circling(uint8_t *o) {
    /* El ASM hace add.w al par 0x26-0x27 e interpreta el byte 0x26 como
       el byte ALTO del word (68k big-endian). Como el port es little-endian,
       hay que hacer la aritmética BE manualmente. Si sumamos con nativo LE,
       el ángulo alterna 0x00/0x80 en vez de incrementar de 1 en 1. */
    uint8_t *p = (uint8_t *)o;
    uint16_t w = ((uint16_t)p[0x26] << 8) | p[0x27];
    w = (uint16_t)(w + (uint16_t)bball_speed(o));
    p[0x26] = (uint8_t)(w >> 8);
    p[0x27] = (uint8_t)(w & 0xFF);

    uint8_t angle = obAngle(o);                              /* byte 0x26 */
    int16_t s0, s1;
    CalcSine(angle, &s0, &s1);                               /* s0=sin, s1=cos */

    int16_t d2 = bball_origY(o);
    int16_t d3 = bball_origX(o);

    int16_t d4 = (int16_t)bball_radius(o);
    int16_t d5 = d4;

    d4 = (int16_t)(((int32_t)d4 * s0) >> 8);
    d5 = (int16_t)(((int32_t)d5 * s1) >> 8);

    d4 = (int16_t)(d4 + d2);
    d5 = (int16_t)(d5 + d3);

    obY(o) = d4;
    obX(o) = d5;
}

/* --- Dispatcher Object 58 — BBall_Index: 0 = Main, 2 = Move ---------- */
static void BigSpikeBall_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {                                  /* BBall_Index */
        case 0: BBall_Main(o); break;
        case 2: BBall_Move(o); break;
    }
}


/* ===========================================================================
   Part 4  Object implementations, $60-$8F
   =========================================================================== */

/* ===========================================================================
 *  Object 73 — Eggman (MZ)
 *  Object 74 — Lava ball dropped by Eggman (MZ)
 *  Ported verbatim from _incObj/73, 74 Boss - MZ Main and Fire.asm
 *  (REV01, FixBugs=0).
 *
 *  BossMarble fields:
 *    bmz_lavatimer   = objoff_34 (byte): countdown between LavaBall spawns
 *                                        (shares the high byte of bmz_parentobj)
 *    bmz_parentobj   = objoff_34 (4 bytes): parent SLOT INDEX
 *    bmz_generictimer= objoff_3C (word): action timer
 *    bmz_sinecounter = objoff_3F (byte): sine phase for bobbing
 *    obBossX/Y       = objoff_30 / $38 (long)
 *    obBossFlash     = objoff_3E (byte)
 *    obBossHits      = objoff_3A (byte)
 *
 *  BossFire fields:
 *    bfire_timer     = objoff_29 (byte): drop wait / burn timer
 *    bfire_spreadx   = objoff_32 (word): last spawn X for edge detection
 * =========================================================================== */

#define bmz_parentobj(o)    (*(uint32_t *)((uint8_t *)(o) + 0x34))
#define bmz_lavatimer(o)    (*(uint8_t  *)((uint8_t *)(o) + 0x34))
#define bmz_generictimer(o) (*(int16_t  *)((uint8_t *)(o) + 0x3C))
#define bmz_sinecounter(o)  (*(uint8_t  *)((uint8_t *)(o) + 0x3F))

#define bfire_timer(o)      (*(uint8_t  *)((uint8_t *)(o) + 0x29))
#define bfire_spreadx(o)    (*(int16_t  *)((uint8_t *)(o) + 0x32))

/* BossMarble_ObjData: {routine, anim, priority} × 4 */
static const uint8_t BossMarble_ObjData[4][3] = {
    { 2, 0, 4 },   /* Ship  */
    { 4, 1, 4 },   /* Face  */
    { 6, 7, 4 },   /* Flame */
    { 8, 0, 3 },   /* Tube  */
};

static void BossMarble_ShipMain(uint8_t *o);
static void BossMarble_FaceMain(uint8_t *o);
static void BossMarble_FlameMain(uint8_t *o);
static void BossMarble_TubeMain(uint8_t *o);
static void BossMarble_ShipUpdate(uint8_t *o);
static void BossMarble_Display(uint8_t *o);
static void BossMarble_SetBits(uint8_t *o);
static void BMZ_ShipStart(uint8_t *o);
static void BMZ_ShipMove(uint8_t *o);
static void BMZ_ChgDir(uint8_t *o);
static void BMZ_DropFire(uint8_t *o);
static void BMZ_Explode(uint8_t *o);
static void BMZ_Recover(uint8_t *o);
static void BMZ_Escape(uint8_t *o);
static void BMZ_Defeated(uint8_t *o);
static void BMZ_MakeLava(uint8_t *o);

/* ===========================================================================
 *  BossMarble (Object 73)
 * =========================================================================== */

/* --- BossMarble_Main — routine 0: spawn ship + face + flame + tube --- */
static void BMZ_Main_Init(uint8_t *o) {
    obBossX(o) = ((int32_t)(int16_t)obX(o)) << 16;
    obBossY(o) = ((int32_t)(int16_t)obY(o)) << 16;
    obColType(o) = (uint8_t)(col_48x48 | col_boss);
    obBossHits(o) = 8;

    const uint8_t *a2 = (const uint8_t *)BossMarble_ObjData;
    uint8_t *a1 = o;

    /* 4 iterations (dbf d1 con d1=3). La primera reutiliza el slot actual. */
    for (int d1 = 3; d1 >= 0; d1--) {
        if (d1 < 3) {                          /* .loop: FindNextFreeObj */
            uint8_t *slot = (uint8_t *)FindNextFreeObj(a1);
            if (slot == NULL) {
                /* ASM: bne.s BossMarble_ShipMain → no hay slot libre */
                BossMarble_ShipMain(o);
                return;
            }
            obID(slot) = id_BossMarble;
            obX(slot)  = obX(o);
            obY(slot)  = obY(o);
            a1 = slot;
        }

        /* BossMarble_LoadBoss */
        obStatus(o) &= (uint8_t)~0x01;          /* bclr #0 en a0 (el main) */
        ob2ndRout(a1) = 0;
        obRoutine(a1) = a2[0];
        obAnim(a1)    = a2[1];
        obPriority(a1)= a2[2];
        a2 += 3;
        obMap(a1)     = (uint32_t)(uintptr_t)Map_Eggman;
        obGfx(a1)     = (uint16_t)ArtTile_Eggman;
        obRender(a1)  = sprite_cam_field;
        obActWid(a1)  = 64 / 2;
        bmz_parentobj(a1) = (uint32_t)Object_GetIndex(o);
    }

    /* Fall-through a ShipMain. */
    BossMarble_ShipMain(o);
}

/* --- BossMarble_ShipMain — routine 2 --- */
static void BossMarble_ShipMain(uint8_t *o) {
    switch (ob2ndRout(o)) {
        case 0x00: BMZ_ShipStart(o); break;
        case 0x02: BMZ_ShipMove(o);  break;
        case 0x04: BMZ_Explode(o);   break;
        case 0x06: BMZ_Recover(o);   break;
        case 0x08: BMZ_Escape(o);    break;
    }

    if (Ani_Eggman) AnimateSprite(o, Ani_Eggman);

    /* Mover los bits de X/Y-flip de obStatus a obRender. */
    uint8_t d0 = obStatus(o) & (sprite_xflip | sprite_yflip);
    obRender(o) &= (uint8_t)~(sprite_xflip | sprite_yflip);
    obRender(o) |= d0;

    DisplaySprite(o);
}

/* --- BossMarble_ShipUpdate — cola común de ShipStart / ShipMove / etc. --- */
static void BossMarble_ShipUpdate(uint8_t *o) {
    /* Copiar posición del boss a obX/obY (high word). */
    obY(o) = (int16_t)(obBossY(o) >> 16);
    obX(o) = (int16_t)(obBossX(o) >> 16);

    if (ob2ndRout(o) >= 4) return;              /* Explode/Recover/Escape: nada */
    if ((int8_t)obStatus(o) < 0) {              /* bit 7 = defeated */
        BMZ_Defeated(o);
        return;
    }
    if (obColType(o) != 0) return;              /* no golpeado: salir */

    /* .flash */
    if (obBossFlash(o) == 0) {
        obBossFlash(o) = 0x28;
        Sound_Queue(sfx_HitBoss, false);
    }
    {
        uint16_t *pal = (uint16_t *)((uint8_t *)RAM_ADDR(v_palette) + 0x22);
        uint16_t col = (*pal != 0) ? 0 : cWhite;
        *pal = col;
    }
    obBossFlash(o)--;
    if (obBossFlash(o) == 0) {
        obColType(o) = (uint8_t)(col_48x48 | col_boss);
    }
}

/* --- BMZ_ShipStart — sub-rutina 0 del ShipIndex --- */
static void BMZ_ShipStart(uint8_t *o) {
    uint8_t old = bmz_sinecounter(o);
    bmz_sinecounter(o) += 2;

    int16_t s0, s1;
    CalcSine(old, &s0, &s1);
    obVelY(o) = (int16_t)(s0 >> 2);
    obVelX(o) = (int16_t)-0x100;

    BossMove(o);

    if ((int16_t)(obBossX(o) >> 16) == (int16_t)(boss_mz_x + 0x110)) {
        /* Llegamos al borde: pasar a ShipMove (ob2ndRout = 2). */
        ob2ndRout(o) += 2;
        obSubtype(o) = 0;
        obVelX(o) = 0;
        obVelY(o) = 0;
    }

    /* .continue: obtener nuevo timer aleatorio para spawn de LavaBall. */
    bmz_lavatimer(o) = (uint8_t)RandomNumber();

    BossMarble_ShipUpdate(o);
}

/* --- BMZ_ShipMove — sub-rutina 2 del ShipIndex (dispatch por obSubtype) --- */
static void BMZ_ShipMove(uint8_t *o) {
    switch (obSubtype(o)) {
        case 0: case 4: BMZ_ChgDir(o);   break;
        case 2: case 6: BMZ_DropFire(o); break;
        default: break;                          /* valores inválidos: no-op */
    }
    obSubtype(o) &= 0x06;
    BossMarble_ShipUpdate(o);
}

/* --- BMZ_ChgDir — sub-rutina 0/2 de ShipMove_Index --- */
static void BMZ_ChgDir(uint8_t *o) {
    if (obVelX(o) == 0) {
        int16_t d0 = 0x40;
        int16_t cur_y = (int16_t)(obBossY(o) >> 16);
        int16_t target = (int16_t)(boss_mz_y + 0x1C);

        if (cur_y == target) {
            /* .swoop */
            obVelX(o) = 0x200;
            obVelY(o) = 0x100;
            if (!(obStatus(o) & 1)) {
                obVelX(o) = (int16_t)(-obVelX(o));
            }
        } else {
            if (cur_y < target) {
                /* .above: d0 positivo → mover hacia abajo */
            } else {
                d0 = (int16_t)(-d0);
            }
            obVelY(o) = d0;
            BossMove(o);
        }
    }

    /* .skip */
    if ((uint8_t)obBossFlash(o) < 0x18) {
        BossMove(o);
        obVelY(o) = (int16_t)(obVelY(o) - 4);
    }

    /* BossMarble_MakeLava (inline en el ASM) */
    BMZ_MakeLava(o);
}

/* --- BMZ_DropFire — sub-rutina 1/3 de ShipMove_Index --- */
static void BMZ_DropFire(uint8_t *o) {
    BossMove(o);

    int16_t d0 = (int16_t)((int16_t)(obBossY(o) >> 16) - (int16_t)(boss_mz_y + 0x1C));
    if (d0 > 0) return;                          /* todavía subiendo */

    if (obVelY(o) != 0) {
        obVelY(o) = 0;
        bmz_generictimer(o) = 80;
        obStatus(o) ^= 1;

        uint8_t *slot = (uint8_t *)FindFreeObj();
        if (slot != NULL) {
            obX(slot) = (int16_t)(obBossX(o) >> 16);
            obY(slot) = (int16_t)((obBossY(o) >> 16) + 0x18);
            obID(slot) = id_BossFire;
            obSubtype(slot) = 1;
        }
    }

    bmz_generictimer(o)--;
    if (bmz_generictimer(o) == 0) {
        obSubtype(o) += 2;
    }
}

/* --- BMZ_MakeLava: spawn de LavaBall con timer aleatorio --- */
static void BMZ_MakeLava(uint8_t *o) {
    /* bcc.s .checkRight: saltar si el timer NO expiró (old != 0). */
    if (bmz_lavatimer(o)-- != 0) goto check_right;

    {
        uint8_t *slot = (uint8_t *)FindFreeObj();
        if (slot != NULL) {
            obID(slot) = id_LavaBall;
            obY(slot)  = (int16_t)(boss_mz_y + 0xD8);

            uint16_t r = RandomNumber();
            obX(slot)  = (int16_t)(boss_mz_x + 0x78 + (r % 0x50));

            /* ASM: move.w #$FF,obSubtype(a1) → obSubtype=0, objoff_29=0xFF */
            obSubtype(slot) = 0;
            lball_fromboss(slot) = 0xFF;
        }
    }

    /* .generateTimer */
    {
        uint16_t r = RandomNumber();
        bmz_lavatimer(o) = (uint8_t)((r & 0x1F) + 0x40);
    }

check_right:
    if (obStatus(o) & 1) {
        /* Facing right */
        if ((int16_t)(obBossX(o) >> 16) < (int16_t)(boss_mz_x + 0x110)) return;
        obBossX(o) = (int32_t)((int16_t)(boss_mz_x + 0x110)) << 16;
    } else {
        if ((int16_t)(obBossX(o) >> 16) > (int16_t)(boss_mz_x + 0x30)) return;
        obBossX(o) = (int32_t)((int16_t)(boss_mz_x + 0x30)) << 16;
    }

    /* .rise */
    obVelX(o) = 0;
    obVelY(o) = (int16_t)-0x180;
    if ((int16_t)(obBossY(o) >> 16) < (int16_t)(boss_mz_y + 0x1C)) {
        obVelY(o) = (int16_t)(-obVelY(o));
    }
    obSubtype(o) += 2;
}

/* --- BMZ_Explode — sub-rutina 2 del ShipIndex --- */
static void BMZ_Explode(uint8_t *o) {
    bmz_generictimer(o)--;
    if (bmz_generictimer(o) < 0) {
        /* .transition */
        obStatus(o) |= 1;
        obStatus(o) &= (uint8_t)~0x80;
        obVelX(o) = 0;
        ob2ndRout(o) += 2;
        bmz_generictimer(o) = -38;
        if (v_bossstatus == 0) {
            v_bossstatus = 1;
            obVelY(o) = 0;
        }
        return;
    }
    BossDefeated(o);
}

/* --- BMZ_Recover — sub-rutina 3 del ShipIndex --- */
static void BMZ_Recover(uint8_t *o) {
    bmz_generictimer(o)++;
    int16_t t = bmz_generictimer(o);

    if (t == 0) {
        obVelY(o) = 0;
        bmz_generictimer(o) = 0;
        goto exit_recover;
    }
    if (t > 0) {
        if (t >= 48) {
            if (t == 48) {
                obVelY(o) = 0;
                Sound_Queue(bgm_MZ, false);
                goto exit_recover;
            }
            if (t < 56) goto exit_recover;
            ob2ndRout(o) += 2;
            goto exit_recover;
        }
        /* t in (0, 48): .rise */
        obVelY(o) = (int16_t)(obVelY(o) - 8);
        goto exit_recover;
    }

    /* t < 0 */
    if ((uint16_t)(obBossY(o) >> 16) >= (uint16_t)(boss_mz_y + 0x60)) {
        obVelY(o) = 0;
        bmz_generictimer(o) = 0;
        goto exit_recover;
    }
    obVelY(o) = (int16_t)(obVelY(o) + 0x18);

exit_recover:
    BossMove(o);
    BossMarble_ShipUpdate(o);
}

/* --- BMZ_Escape — sub-rutina 4 del ShipIndex --- */
static void BMZ_Escape(uint8_t *o) {
    obVelX(o) = 0x500;
    obVelY(o) = (int16_t)-0x40;

    if ((uint16_t)v_limitright2 >= (uint16_t)boss_mz_end) {
        /* .checkOffScreen */
        if ((int8_t)obRender(o) >= 0) {
            DeleteObject(o);
            return;
        }
    } else {
        v_limitright2 += 2;
    }

    /* .flee */
    BossMove(o);
    BossMarble_ShipUpdate(o);
}

/* --- BMZ_Defeated: helper llamado desde ShipUpdate cuando bit 7 de status --- */
static void BMZ_Defeated(uint8_t *o) {
    AddPoints(100);
    ob2ndRout(o) = 4;
    bmz_generictimer(o) = 180;
    obVelX(o) = 0;
    obVelY(o) = 0;
}

/* --- BossMarble_FaceMain — routine 4 --- */
static void BossMarble_FaceMain(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bmz_parentobj(o));
    int16_t d0 = (int16_t)(int8_t)ob2ndRout(a1);
    int16_t d1 = 1;                          /* facenormal1 */

    d0 -= 2;
    if (d0 == 0) {
        /* Estamos en ShipMove */
        if (obSubtype(a1) & 2) {
            if (obVelY(a1) == 0) {
                d1 = 4;                      /* facelaugh */
            }
        }
    } else {
        d0 -= 2;
        if (d0 >= 0) {
            d1 = 0x0A;                       /* defeated */
        } else {
            /* Chequear hit / Sonic hurt */
            if (obColType(a1) == 0) {
                d1 = 5;                      /* facehit */
            } else if ((uint8_t)obRoutine(RAM_ADDR(v_player)) >= 4) {
                d1 = 4;                      /* facelaugh */
            }
        }
    }

    obAnim(o) = (uint8_t)d1;

    /* Escape: facepanic */
    d0 -= 4;
    if (d0 == 0) {
        obAnim(o) = 6;
        if ((int8_t)obRender(o) >= 0) {
            DeleteObject(o);
            return;
        }
    }

    BossMarble_Display(o);
}

/* --- BossMarble_FlameMain — routine 6 --- */
static void BossMarble_FlameMain(uint8_t *o) {
    obAnim(o) = 7;
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bmz_parentobj(o));

    if (ob2ndRout(a1) >= 8) {
        /* Escape: thruster */
        obAnim(o) = 0xB;
        if ((int8_t)obRender(o) >= 0) {
            DeleteObject(o);
            return;
        }
        BossMarble_Display(o);
        return;
    }

    if (obVelX(a1) != 0) {
        obAnim(o) = 8;                       /* flame visible */
    }
    BossMarble_Display(o);
}

/* --- BossMarble_TubeMain — routine 8 --- */
static void BossMarble_TubeMain(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bmz_parentobj(o));

    if (ob2ndRout(a1) == 8) {
        if ((int8_t)obRender(o) >= 0) {
            DeleteObject(o);
            return;
        }
    }

    obMap(o)     = (uint32_t)(uintptr_t)Map_BossItems;
    obGfx(o)     = (uint16_t)(ArtTile_Eggman_Weapons | Tile_Pal2);
    obFrame(o)   = 4;
    BossMarble_SetBits(o);
}

/* --- BossMarble_Display --- */
static void BossMarble_Display(uint8_t *o) {
    if (Ani_Eggman) AnimateSprite(o, Ani_Eggman);
    BossMarble_SetBits(o);
}

/* --- BossMarble_SetBits: sincroniza posición y flips con el padre --- */
static void BossMarble_SetBits(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)bmz_parentobj(o));
    obX(o)      = obX(a1);
    obY(o)      = obY(a1);
    obStatus(o) = obStatus(a1);

    uint8_t d0 = obStatus(o) & (sprite_xflip | sprite_yflip);
    obRender(o) &= (uint8_t)~(sprite_xflip | sprite_yflip);
    obRender(o) |= d0;
    DisplaySprite(o);
}

/* --- Dispatcher Object 73 --- */
static void BossMarble_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: BMZ_Main_Init(o);      break;  /* no recurse — ver nota abajo */
        case 2: BossMarble_ShipMain(o);  break;
        case 4: BossMarble_FaceMain(o);  break;
        case 6: BossMarble_FlameMain(o); break;
        case 8: BossMarble_TubeMain(o);  break;
    }
}

/* ===========================================================================
 *  BossFire (Object 74)
 * =========================================================================== */

static void BF_Drop(uint8_t *o);
static void BF_MakeFlame(uint8_t *o);
static void BF_Duplicate(uint8_t *o);
static void BF_Duplicate2(uint8_t *o);
static void BF_FallEdge(uint8_t *o);
static void BossFire_TempFire(uint8_t *o);
static void BossFire_Action(uint8_t *o);

/* --- BossFire_Main — routine 0 --- */
static void BF_Main_Init(uint8_t *o) {
    obHeight(o) = 16 / 2;
    obWidth(o)  = 16 / 2;
    obMap(o)    = (uint32_t)(uintptr_t)Map_Fire;
    obGfx(o)    = (uint16_t)ArtTile_MZ_Fireball;
    obRender(o) = sprite_cam_field;
    obPriority(o) = 5;
    obBossY(o)  = ((int32_t)(int16_t)obY(o)) << 16;
    obActWid(o) = 16 / 2;
    obRoutine(o) += 2;

    if (obSubtype(o) == 0) {
        /* Temp fire: salta a routine 4 */
        obColType(o) = (uint8_t)(col_16x16 | col_hurt);
        obRoutine(o) += 2;
        BossFire_TempFire(o);
        return;
    }

    /* Real fire */
    bfire_timer(o) = 30;
    Sound_Queue(sfx_Fireball, false);

    /* Fall-through a BossFire_Action */
    BossFire_Action(o);
}

/* --- BossFire_Action — routine 2 --- */
static void BossFire_Action(uint8_t *o) {
    switch (ob2ndRout(o)) {
        case 0: BF_Drop(o);      break;
        case 2: BF_MakeFlame(o); break;
        case 4: BF_Duplicate(o); break;
        case 6: BF_FallEdge(o);  break;
    }

    SpeedToPos(o);
    if (Ani_Fire) AnimateSprite(o, Ani_Fire);

    if ((uint16_t)obY(o) > (uint16_t)(boss_mz_y + 0xD8)) {
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
}

/* --- BF_Drop — sub-rutina 0 --- */
static void BF_Drop(uint8_t *o) {
    obStatus(o) |= (1 << 1);                 /* face up */
    bfire_timer(o)--;

    if ((int8_t)bfire_timer(o) >= 0) return; /* todavía esperando */

    obColType(o) = (uint8_t)(col_16x16 | col_hurt);
    obSubtype(o) = 0;
    obVelY(o) = (int16_t)(obVelY(o) + 0x18);
    obStatus(o) &= ~(1 << 1);                /* face down */

    {
        int16_t dist, angle;
        ObjFloorDist(o, &dist, &angle);
        if (dist < 0) {
            ob2ndRout(o) += 2;
        }
    }
}

/* --- BF_MakeFlame — sub-rutina 2 --- */
static void BF_MakeFlame(uint8_t *o) {
    obY(o) = (int16_t)(obY(o) - 2);
    obGfx(o) |= 0x8000;                      /* bset #7: prioridad alta */
    obVelX(o) = 0xA0;
    obVelY(o) = 0;
    obBossX(o) = ((int32_t)(int16_t)obX(o)) << 16;
    obBossY(o) = ((int32_t)(int16_t)obY(o)) << 16;
    bfire_timer(o) = 3;

    uint8_t *slot = (uint8_t *)FindNextFreeObj(o);
    if (slot != NULL) {
        memcpy(slot, o, OBJECT_SIZE);
        obVelX(slot) = (int16_t)(-obVelX(slot));
        ob2ndRout(slot) += 2;
    }

    ob2ndRout(o) += 2;
}

/* --- BF_Duplicate2: spawn de un temp fire extra --- */
static void BF_Duplicate2(uint8_t *o) {
    uint8_t *slot = (uint8_t *)FindNextFreeObj(o);
    if (slot == NULL) return;

    obX(slot) = obX(o);
    obY(slot) = obY(o);
    obID(slot) = id_BossFire;
    /* ASM: move.w #103,obSubtype(a1) → obSubtype=0, bfire_timer=103 */
    obSubtype(slot) = 0;
    bfire_timer(slot) = 103;
}

/* --- BF_Duplicate — sub-rutina 4 --- */
static void BF_Duplicate(uint8_t *o) {
    int16_t dist, angle;
    ObjFloorDist(o, &dist, &angle);
    if (dist >= 0) {                         /* no toca suelo: ir a FallEdge */
        ob2ndRout(o) += 2;
        return;
    }

    int16_t cur_x = obX(o);
    if (cur_x > (int16_t)(boss_mz_x + 0x140)) {
        obRoutine(o) += 2;                   /* preparar para borrar */
        return;
    }

    int16_t last_x = (int16_t)(obBossX(o) >> 16);
    if (cur_x == last_x) goto copy_position;

    if ((cur_x & 0x10) != (last_x & 0x10)) {
        BF_Duplicate2(o);
        bfire_spreadx(o) = obX(o);
    }

copy_position:
    obBossX(o) = ((int32_t)(int16_t)obX(o)) << 16;
}

/* --- BF_FallEdge — sub-rutina 6 --- */
static void BF_FallEdge(uint8_t *o) {
    obStatus(o) &= ~(1 << 1);
    obVelY(o) = (int16_t)(obVelY(o) + 0x24);

    int16_t d0 = (int16_t)(obX(o) - bfire_spreadx(o));
    if (d0 < 0) d0 = (int16_t)(-d0);

    if (d0 == 0x12) {
        obGfx(o) &= (uint16_t)~0x8000;       /* bclr #7: prioridad baja */
    }

    {
        int16_t dist, angle;
        ObjFloorDist(o, &dist, &angle);
        if (dist >= 0) return;
    }

    bfire_timer(o)--;
    if (bfire_timer(o) == 0) {
        DeleteObject(o);
        return;
    }

    obVelY(o) = 0;
    obX(o) = bfire_spreadx(o);
    obY(o) = (int16_t)(obBossY(o) >> 16);
    obGfx(o) |= 0x8000;
    ob2ndRout(o) -= 2;
}

/* --- BossFire_TempFire — routine 4 --- */
static void BossFire_TempFire(uint8_t *o) {
    obGfx(o) |= 0x8000;
    bfire_timer(o)--;

    if (bfire_timer(o) == 0) {
        obAnim(o) = 1;
        obY(o) = (int16_t)(obY(o) - 4);
        obColType(o) = col_none;
    }

    if (Ani_Fire) AnimateSprite(o, Ani_Fire);
    DisplaySprite(o);
}

/* --- BossFire_TempFireDel — routine 6 --- */
static void BossFire_TempFireDel(uint8_t *o) {
    DeleteObject(o);
}

/* --- Dispatcher Object 74 --- */
static void BossFire_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: BF_Main_Init(o);     break;  /* no recurse — ver nota */
        case 2: BossFire_Action(o);   break;
        case 4: BossFire_TempFire(o); break;
        case 6: BossFire_TempFireDel(o); break;
    }
    DisplaySprite(o);
}

/* ===========================================================================
 *  Object 75 — Eggman (SYZ)
 *  Object 76 — Blocks that Eggman picks up (SYZ)
 *  Ported from _incObj/75, 76 Boss - SYZ Main and Blocks.asm (REV01, FixBugs=0).
 *
 *  BossSpringYard object fields:
 *    bsyz_childcmd  = objoff_29 (byte): child command (0=normal, -1=grabbed,
 *                                       $A=break)
 *    bsyz_parentobj = objoff_34 (byte): parent SLOT INDEX (port-specific)
 *    bsyz_blockidx  = objoff_34 (byte): alias; current block index
 *    bsyz_objptr    = objoff_36 (word): SLOT INDEX of block/spike object
 *    bsyz_timer     = objoff_3C (word): generic action timer
 *    bsyz_phase     = low byte of bsyz_timer (ASM alias at objoff_3D)
 *    bsyz_sinecnt   = objoff_3F (byte): sine phase
 *
 *  The ASM aliases GenericTimer (0x3C, word) with PhaseTimer (0x3D, byte):
 *  in big-endian, byte 0x3D is the LOW byte of the word. In this little-
 *  endian port, the equivalent byte is the low byte of bsyz_timer, so all
 *  `btst`/`tst`/`clr`/`move.b` operations on PhaseTimer are done via the
 *  timer value itself:
 *      tst.b  PhaseTimer    ⇔  (bsyz_timer(o) & 0xFF) != 0
 *      btst #0, PhaseTimer  ⇔  bsyZ_timer(o) & 0x0001
 *      btst #1, PhaseTimer  ⇔  bsyZ_timer(o) & 0x0002
 *      clr.b  PhaseTimer    ⇔  bsyZ_timer(o) &= 0xFF00
 *      move.b #-1,PhaseTimer⇔  bsyZ_timer(o) |= 0x00FF
 * =========================================================================== */

#define bsyz_childcmd(o)  (*(uint8_t  *)((uint8_t *)(o) + 0x29))
#define bsyz_parentobj(o) (*(uint8_t  *)((uint8_t *)(o) + 0x34))
#define bsyz_blockidx(o)  (*(uint8_t  *)((uint8_t *)(o) + 0x34))
#define bsyz_objptr(o)    (*(uint16_t *)((uint8_t *)(o) + 0x36))
#define bsyz_timer(o)     (*(int16_t  *)((uint8_t *)(o) + 0x3C))
#define bsyz_sinecnt(o)   (*(uint8_t  *)((uint8_t *)(o) + 0x3F))

/* --- BossSpringYard_ObjData: {routine, animation, priority} × 4 --------- */
static const uint8_t BSYZ_ObjData[4][3] = {
    { 2, 0, 5 },   /* ship  */
    { 4, 1, 5 },   /* face  */
    { 6, 7, 5 },   /* flame */
    { 8, 0, 5 },   /* spike */
};

/* --- BossBlock fragment tables ------------------------------------------ */
static const int16_t BBlock_FragSpeed[8] = {
    -0x180, -0x200,   /* top left     */
     0x180, -0x200,   /* top right    */
    -0x100, -0x100,   /* bottom left  */
     0x100, -0x100,   /* bottom right */
};
static const int16_t BBlock_FragPos[8] = {
    -8,   -8,     /* top left     */
     0x10, 0,     /* top right    */
     0,    0x10,  /* bottom left  */
     0x10, 0x10,  /* bottom right */
};

/* --- Forward declarations ----------------------------------------------- */
static void BSYZ_Main(uint8_t *o);
static void BSYZ_ShipMain(uint8_t *o);
static void BSYZ_FaceMain(uint8_t *o);
static void BSYZ_FlameMain(uint8_t *o);
static void BSYZ_SpikeMain(uint8_t *o);
static void BSYZ_MoveUpdate(uint8_t *o);
static void BSYZ_StatusUpdate(uint8_t *o);
static void BSYZ_CalcSine(uint8_t *o);
static void BSYZ_ShipStart(uint8_t *o);
static void BSYZ_ShipMove(uint8_t *o);
static void BSYZ_Attack(uint8_t *o);
static void BSYZ_Explode(uint8_t *o);
static void BSYZ_Recover(uint8_t *o);
static void BSYZ_Escape(uint8_t *o);
static void BSYZ_Defeated(uint8_t *o);
static void BSYZ_FindBlocks(uint8_t *o);
static void BSYZ_Descend(uint8_t *o);
static void BSYZ_Lift(uint8_t *o);
static void BSYZ_LiftStop(uint8_t *o);
static void BSYZ_BreakBlock(uint8_t *o);
static void BSYZ_SetupAnim(uint8_t *o);
static void BSYZ_Display(uint8_t *o);
static int16_t BSYZ_Face_ChkHit(uint8_t *parent, int16_t d1);

static void BBlock_Main(uint8_t *o);
static void BBlock_Action(uint8_t *o);
static void BBlock_Frag(uint8_t *o);
static void BBlock_Solid(uint8_t *o);
static void BBlock_Break(uint8_t *o);

/* ===========================================================================
 *  Object 75 — Eggman (SYZ)
 * =========================================================================== */

/* --- BSYZ_Main — Routine 0: setup ship + face + flame + spike ----------- */
static void BSYZ_Main(uint8_t *o) {
    obX(o) = (int16_t)(boss_syz_x + 0x1B0);
    obY(o) = (int16_t)(boss_syz_y + 0x0E);
    /* ASM: move.w obX,obBossX — writes the high word of the 32-bit field.
       Equivalent in C: integer part = obX, fraction = 0. */
    obBossX(o) = ((int32_t)obX(o)) << 16;
    obBossY(o) = ((int32_t)obY(o)) << 16;
    obColType(o) = (uint8_t)(col_48x48 | col_boss);
    obBossHits(o) = 8;

    const uint8_t *a2 = (const uint8_t *)BSYZ_ObjData;
    uint8_t *a1 = o;

    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            a1 = (uint8_t *)FindNextFreeObj(a1);
            if (!a1) { BSYZ_ShipMain(o); return; }
            obID(a1) = id_BossSpringYard;
            obX(a1)  = obX(o);
            obY(a1)  = obY(o);
        }
        /* BossSpringYard_LoadBoss */
        obStatus(o) &= (uint8_t)~(1 << 0);
        ob2ndRout(a1) = 0;
        obRoutine(a1) = a2[0];
        obAnim(a1)    = a2[1];
        obPriority(a1)= a2[2];
        a2 += 3;
        obMap(a1)     = (uint32_t)(uintptr_t)Map_Eggman;
        obGfx(a1)     = (uint16_t)ArtTile_Eggman;
        obRender(a1)  = sprite_cam_field;
        obActWid(a1)  = 64 / 2;
        bsyz_parentobj(a1) = (uint8_t)Object_GetIndex(o);
    }

    BSYZ_ShipMain(o);
}

/* --- BSYZ_ShipMain — Routine 2 ------------------------------------------ */
static void BSYZ_ShipMain(uint8_t *o) {
    switch (ob2ndRout(o)) {
        case 0x00: BSYZ_ShipStart(o); break;
        case 0x02: BSYZ_ShipMove(o);  break;
        case 0x04: BSYZ_Attack(o);    break;
        case 0x06: BSYZ_Explode(o);   break;
        case 0x08: BSYZ_Recover(o);   break;
        case 0x0A: BSYZ_Escape(o);    break;
    }
    if (obID(o) == 0) return;              /* sub-routine may have deleted us */

    if (Ani_Eggman) AnimateSprite(o, Ani_Eggman);
    uint8_t d0 = obStatus(o) & (sprite_xflip | sprite_yflip);
    obRender(o) = (uint8_t)((obRender(o) & ~(sprite_xflip | sprite_yflip)) | d0);
    DisplaySprite(o);
}

/* --- BSYZ_ShipStart — sub-rutina 0 -------------------------------------- */
static void BSYZ_ShipStart(uint8_t *o) {
    obVelX(o) = (int16_t)-0x100;
    if ((int16_t)(obBossX(o) >> 16) < (int16_t)(boss_syz_x + 0x138)) {
        ob2ndRout(o) += 2;
    }
    BSYZ_CalcSine(o);
}

/* --- BSYZ_CalcSine ------------------------------------------------------ */
static void BSYZ_CalcSine(uint8_t *o) {
    uint8_t d0 = bsyz_sinecnt(o);
    bsyz_sinecnt(o) = (uint8_t)(bsyz_sinecnt(o) + 2);
    int16_t s0, s1;
    CalcSine(d0, &s0, &s1);
    obVelY(o) = (int16_t)(s0 >> 2);
    BSYZ_MoveUpdate(o);
}

/* --- BSYZ_MoveUpdate ---------------------------------------------------- */
static void BSYZ_MoveUpdate(uint8_t *o) {
    BossMove(o);
    obY(o) = (int16_t)(obBossY(o) >> 16);
    obX(o) = (int16_t)(obBossX(o) >> 16);
    BSYZ_StatusUpdate(o);
}

/* --- BSYZ_StatusUpdate -------------------------------------------------- */
static void BSYZ_StatusUpdate(uint8_t *o) {
    int16_t d0 = (int16_t)(obX(o) - boss_syz_x);
    d0 = (int16_t)((uint16_t)d0 >> 5);
    bsyz_blockidx(o) = (uint8_t)d0;

    if (ob2ndRout(o) >= 6) return;         /* exploding/escaping */

    if ((int8_t)obStatus(o) < 0) {
        BSYZ_Defeated(o);
        return;
    }
    if (obColType(o) != 0) return;
    if (obBossFlash(o) == 0) {
        obBossFlash(o) = 0x20;
        Sound_Queue(sfx_HitBoss, false);
    }
    {
        uint16_t *pal = (uint16_t *)RAM_ADDR(v_palette + 0x22);
        uint16_t col = (*pal != 0) ? 0 : cWhite;
        *pal = col;
    }
    obBossFlash(o)--;
    if (obBossFlash(o) == 0) {
        obColType(o) = (uint8_t)(col_48x48 | col_boss);
    }
}

/* --- BSYZ_Defeated ------------------------------------------------------ */
static void BSYZ_Defeated(uint8_t *o) {
    AddPoints(100);
    ob2ndRout(o) = 6;
    bsyz_timer(o) = 180;
    obVelX(o) = 0;
}

/* --- BSYZ_ShipMove — sub-rutina 2 --------------------------------------- */
static void BSYZ_ShipMove(uint8_t *o) {
    int16_t d0 = (int16_t)(obBossX(o) >> 16);
    obVelX(o) = 0x140;
    if (!(obStatus(o) & 1)) {
        obVelX(o) = (int16_t)-obVelX(o);
        if (d0 > (int16_t)(boss_syz_x + 8)) goto drop_setup;
        /* .flip */
        obStatus(o) ^= 1;
        bsyz_timer(o) &= (int16_t)0xFF00;      /* clr.b PhaseTimer */
    } else {
        if (d0 < (int16_t)(boss_syz_x + 0x138)) goto drop_setup;
        obStatus(o) ^= 1;
        bsyz_timer(o) &= (int16_t)0xFF00;
    }

drop_setup:
    d0 = (int16_t)(d0 - (boss_syz_x + 0x10));
    d0 = (int16_t)((uint16_t)d0 & 0x1F);
    d0 = (int16_t)(d0 - 0x1F);
    if (d0 < 0) d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 - 1);
    if (d0 > 0) { BSYZ_CalcSine(o); return; }
    if ((bsyz_timer(o) & 0xFF) != 0) { BSYZ_CalcSine(o); return; }

    {
        int16_t d1 = (int16_t)(obX(RAM_ADDR(v_player)) - boss_syz_x);
        d1 = (int16_t)((uint16_t)d1 >> 5);
        if ((uint8_t)d1 != bsyz_blockidx(o)) { BSYZ_CalcSine(o); return; }
    }
    {
        int16_t block_idx = (int16_t)bsyz_blockidx(o);
        int16_t new_x = (int16_t)((block_idx << 5) + (boss_syz_x + 0x10));
        obBossX(o) = ((int32_t)new_x) << 16;
    }
    BSYZ_FindBlocks(o);
    ob2ndRout(o) += 2;
    obSubtype(o)   = 0;
    bsyz_childcmd(o) = 0;
    obVelX(o) = 0;
    BSYZ_CalcSine(o);
}

/* --- BSYZ_Attack — sub-rutina 4 (dispatch por obSubtype) ---------------- */
static void BSYZ_Attack(uint8_t *o) {
    switch (obSubtype(o)) {
        case 0: BSYZ_Descend(o);    break;
        case 2: BSYZ_Lift(o);       break;
        case 4: BSYZ_LiftStop(o);   break;
        case 6: BSYZ_BreakBlock(o); break;
    }
}

/* --- BSYZ_Descend — sub-rutina 0 ---------------------------------------- */
static void BSYZ_Descend(uint8_t *o) {
    obVelY(o) = 0x180;
    if ((uint16_t)(obBossY(o) >> 16) < (uint16_t)(boss_syz_y + 0x8A)) {
        BSYZ_MoveUpdate(o);
        return;
    }
    obBossY(o) = ((int32_t)(boss_syz_y + 0x8A)) << 16;
    bsyz_timer(o) = 0;                     /* clr.w: clears timer AND PhaseTimer */

    if (bsyz_objptr(o) != 0) {
        uint8_t *block = (uint8_t *)Object_GetSlot((int)bsyz_objptr(o));
        bsyz_childcmd(block)  = (uint8_t)-1;
        bsyz_childcmd(o)      = (uint8_t)-1;
        bsyz_parentobj(block) = (uint8_t)Object_GetIndex(o);
        bsyz_timer(o)         = 50;
    }
    obVelY(o) = 0;
    obSubtype(o) += 2;
    BSYZ_MoveUpdate(o);
}

/* --- BSYZ_Lift — sub-rutina 2 ------------------------------------------- */
static void BSYZ_Lift(uint8_t *o) {
    bsyz_timer(o)--;
    int16_t d0 = 0;

    if ((int16_t)bsyz_timer(o) < 0) {
        obSubtype(o) += 2;
        obVelY(o) = (int16_t)-0x800;
        if (bsyz_objptr(o) == 0) {
            obVelY(o) = (int16_t)(obVelY(o) >> 1);
        }
    } else {
        if ((int16_t)bsyz_timer(o) <= 30) {
            d0 = 2;
            if (!(bsyz_timer(o) & 0x0002)) d0 = 0;
            else                            d0 = -2;
        }
    }
    obY(o) = (int16_t)((int16_t)(obBossY(o) >> 16) + d0);
    obX(o) = (int16_t)(obBossX(o) >> 16);
    BSYZ_StatusUpdate(o);
}

/* --- BSYZ_LiftStop — sub-rutina 4 --------------------------------------- */
static void BSYZ_LiftStop(uint8_t *o) {
    int16_t d0 = (int16_t)(boss_syz_y + 0x0E);
    if (bsyz_objptr(o) != 0) d0 = (int16_t)(d0 - 0x18);

    if ((int16_t)d0 < (int16_t)(obBossY(o) >> 16)) {
        /* .checkSpeed */
        if ((int16_t)obVelY(o) >= (int16_t)-0x40) { BSYZ_MoveUpdate(o); return; }
        obVelY(o) = (int16_t)(obVelY(o) + 0x0C);
        BSYZ_MoveUpdate(o);
        return;
    }
    bsyz_timer(o) = 8;
    if (bsyz_objptr(o) != 0) bsyz_timer(o) = 45;
    obSubtype(o) += 2;
    obVelY(o) = 0;
    BSYZ_MoveUpdate(o);
}

/* --- BSYZ_BreakBlock — sub-rutina 6 ------------------------------------- */
static void BSYZ_BreakBlock(uint8_t *o) {
    bsyz_timer(o)--;

    if ((int16_t)bsyz_timer(o) > 0) goto update_position;
    if ((int16_t)bsyz_timer(o) < 0) {
        /* .endAttack */
        if (bsyz_timer(o) == -30) {
            bsyz_childcmd(o) = 0;
            ob2ndRout(o) -= 2;
            bsyz_timer(o) |= (int16_t)0x00FF;  /* move.b #-1,PhaseTimer */
        }
        BSYZ_StatusUpdate(o);
        return;
    }
    /* timer == 0 */
    if (bsyz_objptr(o) != 0) {
        uint8_t *block = (uint8_t *)Object_GetSlot((int)bsyz_objptr(o));
        bsyz_childcmd(block) = 0x0A;
    }
    bsyz_objptr(o) = 0;

update_position:
    {
        int16_t d0 = 1;
        if (bsyz_objptr(o) != 0) d0 = 2;

        int16_t boss_y = (int16_t)(obBossY(o) >> 16);
        int16_t rest_y = (int16_t)(boss_syz_y + 0x0E);

        if (boss_y == rest_y) {
            /* skip */
        } else if (boss_y < rest_y) {
            obBossY(o) += ((int32_t)d0) << 16;
        } else {
            d0 = (int16_t)-d0;
            obBossY(o) += ((int32_t)d0) << 16;
        }
    }
    /* .shakeOffset */
    {
        int16_t d0 = 0;
        if (bsyz_objptr(o) != 0) {
            d0 = 2;
            if (bsyz_timer(o) & 0x0001) d0 = (int16_t)-d0;
        }
        obY(o) = (int16_t)((int16_t)(obBossY(o) >> 16) + d0);
        obX(o) = (int16_t)(obBossX(o) >> 16);
    }
    BSYZ_StatusUpdate(o);
}

/* --- BSYZ_Explode — sub-rutina 6 ---------------------------------------- */
static void BSYZ_Explode(uint8_t *o) {
    bsyz_timer(o)--;
    if ((int16_t)bsyz_timer(o) < 0) {
        /* .transition */
        ob2ndRout(o) += 2;
        obVelY(o) = 0;
        obStatus(o) |= (1 << 0);
        obStatus(o) &= (uint8_t)~(1 << 7);
        obVelX(o) = 0;
        bsyz_timer(o) = -1;
        if (v_bossstatus == 0) v_bossstatus = 1;
        BSYZ_StatusUpdate(o);
        return;
    }
    BossDefeated(o);
}

/* --- BSYZ_Recover — sub-rutina 8 ---------------------------------------- */
static void BSYZ_Recover(uint8_t *o) {
    bsyz_timer(o)++;
    int16_t t = bsyz_timer(o);

    if (t == 0) {
        obVelY(o) = 0;
    } else if (t < 0) {
        obVelY(o) = (int16_t)(obVelY(o) + 0x18);
    } else if (t < 32) {
        obVelY(o) = (int16_t)(obVelY(o) - 8);
    } else if (t == 32) {
        obVelY(o) = 0;
        Sound_Queue(bgm_SYZ, false);
    } else if (t >= 42) {
        ob2ndRout(o) += 2;
    }
    /* else 33..41: nothing */
    BSYZ_MoveUpdate(o);
}

/* --- BSYZ_Escape — sub-rutina $A ---------------------------------------- */
static void BSYZ_Escape(uint8_t *o) {
    obVelX(o) = 0x400;
    obVelY(o) = (int16_t)-0x40;

    if ((uint16_t)v_limitright2 >= (uint16_t)boss_syz_end) {
        /* .checkOffScreen */
        if ((int8_t)obRender(o) >= 0) {
            DeleteObject(o);
            return;
        }
    } else {
        v_limitright2 += 2;
    }
    BossMove(o);
    BSYZ_CalcSine(o);
}

/* --- BSYZ_FindBlocks ---------------------------------------------------- */
static void BSYZ_FindBlocks(uint8_t *o) {
    bsyz_objptr(o) = 0;
    /* FixBugs=0: covers only the first half of object RAM. */
    uint8_t *a1 = RAM_ADDR(v_objspace + object_size * 1);
    int count = (int)((v_objspace_end - (v_objspace + object_size * 1))
                      / object_size) / 2 - 1;
    uint8_t d1 = id_BossBlock;
    uint8_t d2 = bsyz_blockidx(o);

    for (int i = 0; i <= count; i++) {
        if (obID(a1) == d1 && obSubtype(a1) == d2) {
            bsyz_objptr(o) = (uint16_t)Object_GetIndex(a1);
            return;
        }
        a1 += object_size;
    }
}

/* --- BSYZ_FaceMain — Routine 4 ------------------------------------------ */
static void BSYZ_FaceMain(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)bsyz_parentobj(o));
    int16_t d1 = 1;                         /* facenormal1 */

    switch (ob2ndRout(parent)) {
        case 0: case 2:
            d1 = BSYZ_Face_ChkHit(parent, d1);
            break;
        case 4:
            if (obSubtype(parent) == 2) d1 = 6;
            d1 = BSYZ_Face_ChkHit(parent, d1);
            break;
        case 6: case 8:
            d1 = 0x0A;                      /* defeated */
            break;
        case 10:
            d1 = 6;                         /* panic/escape */
            break;
    }
    obAnim(o) = (uint8_t)d1;

    if (obID(o) != obID(parent)) {
        DeleteObject(o);
        return;
    }
    BSYZ_SetupAnim(o);
}

/* --- BSYZ_Face_ChkHit: helper ------------------------------------------- */
static int16_t BSYZ_Face_ChkHit(uint8_t *parent, int16_t d1) {
    if (obColType(parent) != 0) {
        if ((uint8_t)obRoutine(RAM_ADDR(v_player)) >= 4) d1 = 4;
    } else {
        d1 = 5;                             /* facehit */
    }
    return d1;
}

/* --- BSYZ_FlameMain — Routine 6 ----------------------------------------- */
static void BSYZ_FlameMain(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)bsyz_parentobj(o));
    obAnim(o) = 7;

    if (ob2ndRout(parent) == 0x0A) {
        obAnim(o) = 0x0B;
        if ((int8_t)obRender(o) >= 0) { DeleteObject(o); return; }
    } else if (obVelX(parent) != 0) {
        obAnim(o) = 8;
    }
    BSYZ_SetupAnim(o);
}

/* --- BSYZ_SpikeMain — Routine 8 ----------------------------------------- */
static void BSYZ_SpikeMain(uint8_t *o) {
    obMap(o)  = (uint32_t)(uintptr_t)Map_BossItems;
    obGfx(o)  = (uint16_t)(ArtTile_Eggman_Weapons | Tile_Pal2);
    obFrame(o)= 5;

    uint8_t *parent = (uint8_t *)Object_GetSlot((int)bsyz_parentobj(o));
    if (ob2ndRout(parent) == 0x0A) {
        if ((int8_t)obRender(o) >= 0) { DeleteObject(o); return; }
    }
    obX(o) = obX(parent);
    obY(o) = obY(parent);

    int16_t d0 = bsyz_timer(o);
    if (ob2ndRout(parent) == 4) {
        if (obSubtype(parent) == 6) {
            if ((int16_t)bsyz_timer(parent) < 0) goto retract;
            /* apply as-is */
        } else if (obSubtype(parent) != 0) {
            /* apply as-is */
        } else {
            if (d0 < 0x94) d0 = (int16_t)(d0 + 7);
        }
    } else {
    retract:
        if (d0 > 0) d0 = (int16_t)(d0 - 5);
    }
    bsyz_timer(o) = d0;
    d0 = (int16_t)(d0 >> 2);
    obY(o) = (int16_t)(obY(o) + d0);

    obActWid(o)  = 16 / 2;
    obHeight(o)  = 24 / 2;
    obColType(o) = 0;

    if (obColType(parent) != 0 && bsyz_childcmd(parent) == 0) {
        obColType(o) = (uint8_t)(col_8x32 | col_hurt);
    }
    BSYZ_Display(o);
}

/* --- BSYZ_SetupAnim ----------------------------------------------------- */
static void BSYZ_SetupAnim(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)bsyz_parentobj(o));
    if (Ani_Eggman) AnimateSprite(o, Ani_Eggman);
    obX(o) = obX(parent);
    obY(o) = obY(parent);
    BSYZ_Display(o);
}

/* --- BSYZ_Display ------------------------------------------------------- */
static void BSYZ_Display(uint8_t *o) {
    uint8_t *parent = (uint8_t *)Object_GetSlot((int)bsyz_parentobj(o));
    obStatus(o) = obStatus(parent);
    uint8_t d0 = obStatus(o) & (sprite_xflip | sprite_yflip);
    obRender(o) = (uint8_t)((obRender(o) & ~(sprite_xflip | sprite_yflip)) | d0);
    DisplaySprite(o);
}

/* --- Dispatcher Object 75 ---------------------------------------------- */
static void BossSpringYard_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: BSYZ_Main(o);      break;
        case 2: BSYZ_ShipMain(o);  break;
        case 4: BSYZ_FaceMain(o);  break;
        case 6: BSYZ_FlameMain(o); break;
        case 8: BSYZ_SpikeMain(o); break;
    }
}

/* ===========================================================================
 *  Object 76 — Blocks picked up by Eggman (SYZ)
 * =========================================================================== */

/* --- BBlock_Main — Routine 0: spawn 10 blocks in a row ------------------ */
static void BBlock_Main(uint8_t *o) {
    int16_t d4 = 0;                         /* index: 0, 0x101, 0x202, ... */
    int16_t d5 = (int16_t)(boss_syz_x + 0x10);
    uint8_t *a1 = o;

    for (int i = 0; i < 10; i++) {
        if (i > 0) {
            a1 = (uint8_t *)FindFreeObj();
            if (!a1) return;                /* exit loop */
        }
        obID(a1)       = id_BossBlock;
        obMap(a1)      = (uint32_t)(uintptr_t)Map_BossBlock;
        obGfx(a1)      = (uint16_t)(ArtTile_Level | Tile_Pal3);
        obRender(a1)   = sprite_cam_field;
        obActWid(a1)   = 32 / 2;
        obHeight(a1)   = 32 / 2;
        obPriority(a1) = 3;
        obX(a1)        = d5;
        obY(a1)        = 0x582;
        /* ASM: move.w d4,obSubtype — writes both obSubtype and childcmd
           with the same value (0, 1, 2, ..., 9). */
        obSubtype(a1)    = (uint8_t)d4;
        bsyz_childcmd(a1) = (uint8_t)d4;
        d4 = (int16_t)(d4 + 0x101);
        d5 = (int16_t)(d5 + 0x20);
        obRoutine(a1) += 2;
    }
}

/* --- BBlock_Action — Routine 2 ------------------------------------------ */
static void BBlock_Action(uint8_t *o) {
    uint8_t cmd = bsyz_childcmd(o);
    if (cmd == obSubtype(o)) { BBlock_Solid(o); return; }
    if ((int8_t)cmd < 0) {
        /* .blockGrabbed */
        uint8_t *parent = (uint8_t *)Object_GetSlot((int)bsyz_parentobj(o));
        if (obBossHits(parent) == 0) { BBlock_Break(o); return; }
        obX(o) = obX(parent);
        obY(o) = obY(parent);
        obY(o) = (int16_t)(obY(o) + 44);
        if ((uintptr_t)parent < (uintptr_t)o) {
            /* parent already updated, just display */
        } else {
            int32_t v = (int32_t)obVelY(parent);
            v >>= 8;
            obY(o) = (int16_t)(obY(o) + (int16_t)v);
        }
        DisplaySprite(o);
        return;
    }
    /* .break */
    BBlock_Break(o);
    DisplaySprite(o);
}

/* --- BBlock_Frag — Routine 4 -------------------------------------------- */
static void BBlock_Frag(uint8_t *o) {
    if (!(obRender(o) & 0x80)) { DeleteObject(o); return; }
    ObjectFall(o);
    DisplaySprite(o);
}

/* --- BBlock_Solid ------------------------------------------------------- */
static void BBlock_Solid(uint8_t *o) {
    int16_t d1 = (int16_t)(16 + sonic_solid_width);
    int16_t d2 = 16;
    int16_t d3 = 17;
    int16_t d4 = obX(o);
    int16_t out_d3 = 0, out_d5 = 0;
    SolidObject(o, d1, d2, d3, d4, &out_d3, &out_d5);
    DisplaySprite(o);
}

/* --- BBlock_Break ------------------------------------------------------- */
static void BBlock_Break(uint8_t *o) {
    const int16_t *a4 = BBlock_FragSpeed;
    const int16_t *a5 = BBlock_FragPos;
    int16_t d4 = 1;

    obRoutine(o) += 2;
    obActWid(o)   = 16 / 2;
    obHeight(o)   = 16 / 2;

    uint8_t *a1 = o;
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            a1 = (uint8_t *)FindNextFreeObj(a1);
            if (!a1) break;
            memcpy(a1, o, OBJECT_SIZE);
        }
        obVelX(a1) = *a4++;
        obVelY(a1) = *a4++;
        obX(a1)    = (int16_t)(obX(a1) + *a5++);
        obY(a1)    = (int16_t)(obY(a1) + *a5++);
        obFrame(a1)= (uint8_t)d4;
        d4++;
    }
    Sound_Queue(sfx_WallSmash, false);
}

/* --- Dispatcher Object 76 ---------------------------------------------- */
static void BossBlock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0: BBlock_Main(o);   break;
        case 2: BBlock_Action(o); break;
        case 4: BBlock_Frag(o);   break;
    }
}

/* ===========================================================================
 *  Object 78 — Caterkiller enemy (MZ, SBZ)
 *  Ported verbatim from _incObj/78 Badnik - Caterkiller.asm (REV01, FixBugs=0).
 *
 *  cat_waittime  = objoff_2A (byte): time between actions
 *  cat_mode      = objoff_2B (byte): bit 4 = mouth open/closed, bit 7 = update anim
 *  cat_floormap  = objoff_2C (16 bytes): floor height map
 *  cat_parent    = objoff_3C (4 bytes): parent SLOT INDEX (ASM stored an address)
 *  cat_segmentpos: In the ASM this shares the high byte of cat_parent; here we
 *                  stash it in obSubtype's byte since Caterkiller never uses
 *                  obSubtype.
 * =========================================================================== */

#define cat_waittime(o)   (*(uint8_t  *)((uint8_t *)(o) + 0x2A))
#define cat_mode(o)       (*(uint8_t  *)((uint8_t *)(o) + 0x2B))
#define cat_floormap(o)   ((uint8_t  *)((uint8_t *)(o) + 0x2C))
#define cat_parent(o)     (*(uint32_t *)((uint8_t *)(o) + 0x3C))
#define cat_segmentpos(o) (*(uint8_t  *)((uint8_t *)(o) + 0x28))  /* reuses obSubtype */

static const int16_t Cat_FragSpeed[4] = {
    -0x200, -0x180, 0x180, 0x200
};

/* Ani_Cat — NON-STANDARD animation script (128 bytes of frame IDs).
   Indexed by (obAngle & $7F). $FF means "stop animating". */
static const uint8_t Ani_Cat[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,
    6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0xFF, 7, 7, 0xFF,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6,
    6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4,
    4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0, 0, 0xFF
};

static void Cat_Main(uint8_t *o);
static void Cat_Head(uint8_t *o);
static void Cat_BodySeg1(uint8_t *o);
static void Cat_BodySeg2(uint8_t *o);
static void Cat_Delete(uint8_t *o);
static void Cat_Fragment(uint8_t *o);
static void Cat_Despawn(uint8_t *o);
static void Cat_Undulate(uint8_t *o);
static void Cat_Floor(uint8_t *o);
static void Cat_FragmentateBody(uint8_t *o);
static void Cat_FragmentateBody_NotifyHead(uint8_t *o);

/* Cat_Despawn — Cat_ChkGone: clear respawn block, then mark for deletion */
static void Cat_Despawn(uint8_t *o) {
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t d0 = obRespawnNo(o);
    if (d0 != 0) {
        a2[2 + d0] &= 0x7F;    /* bclr #7,2(a2,d0.w) */
    }
    obRoutine(o) = 0x0A;       /* → Cat_Delete */
}

/* Cat_Delete — routine $A */
static void Cat_Delete(uint8_t *o) {
    DeleteObject(o);
}

/* Cat_Main — routine 0 */
static void Cat_Main(uint8_t *o) {
    int16_t d1, d3, d5, d2;
    uint8_t d6, d4;
    uint8_t *a2;
    int loop;

    obHeight(o) = 14 / 2;
    obWidth(o)  = 16 / 2;

    /* Fall until floor collision */
    ObjectFall(o);
    ObjFloorDist(o, &d1, &d3);
    if (d1 >= 0) {
        /* Cat_HideForInit with FixBugs: delete if fallen below $7FF */
        if ((uint16_t)obY(o) > 0x7FF) Cat_Despawn(o);
        return;
    }
    obY(o) = (int16_t)(obY(o) + d1);
    obVelY(o) = 0;
    obRoutine(o) += 2;
    obMap(o) = (uint32_t)(uintptr_t)Map_Cat;

    obGfx(o) = (uint16_t)(ArtTile_SBZ_Caterkiller | Tile_Pal2);
    if ((uint8_t)v_zone != id_SBZ) {
        obGfx(o) = (uint16_t)(ArtTile_MZ_SYZ_Caterkiller | Tile_Pal2);
    }

    obRender(o) &= (sprite_xflip | sprite_yflip);
    obRender(o) |= sprite_cam_field;
    obStatus(o) = obRender(o);
    obPriority(o) = 4;
    obActWid(o)   = 16 / 2;
    obColType(o)  = (uint8_t)(col_16x16 | col_badnik);

    /* Spawn 3 body segments */
    d2 = obX(o);
    d5 = 12;
    if (obStatus(o) & 1) d5 = (int16_t)-d5;

    d6 = 4;                  /* start routine for first segment: Cat_BodySeg1 */
    d4 = 4;                  /* segmentpos for first segment */
    a2 = o;                  /* parent = head */
    loop = 3 - 1;            /* 3 segments */

    for (;;) {
        uint8_t *a1 = (uint8_t *)FindNextFreeObj(a2);
        if (!a1) {
            /* REV01: delete the whole Caterkiller if object RAM is full */
            Cat_Despawn(o);
            return;
        }
        obID(a1) = id_Caterkiller;
        obRoutine(a1) = d6;
        d6 = (uint8_t)(d6 + 2);
        obMap(a1) = obMap(o);
        obGfx(a1) = obGfx(o);
        obPriority(a1) = 5;
        obActWid(a1)   = 16 / 2;
        obColType(a1)  = (uint8_t)(col_16x16 | col_special);
        d2 = (int16_t)(d2 + d5);
        obX(a1) = d2;
        obY(a1) = obY(o);
        obStatus(a1) = obStatus(o);
        obRender(a1) = obStatus(o);
        obFrame(a1)  = 8;
        cat_parent(a1) = (uint32_t)Object_GetIndex(a2);
        cat_segmentpos(a1) = d4;
        d4 = (uint8_t)(d4 + 4);
        a2 = a1;             /* next segment's parent is this one */

        if (--loop == -1) break;
    }

    cat_waittime(o) = 8 - 1;
    cat_segmentpos(o) = 0;
}

/* Cat_Undulate — Cat_Head .wait */
static void Cat_Undulate(uint8_t *o) {
    uint8_t was_set;

    cat_waittime(o)--;
    if ((int8_t)cat_waittime(o) >= 0) return;

    /* .move */
    ob2ndRout(o) += 2;
    cat_waittime(o) = 17 - 1;
    obVelX(o) = (int16_t)-0xC0;
    obInertia(o) = 0x40;

    /* bchg #4,cat_mode ; bne.s .updateHeadSprite.
       bne branches if the OLD bit was set (Z=0). */
    was_set = cat_mode(o) & 0x10;
    cat_mode(o) ^= 0x10;
    if (!was_set) {
        obVelX(o) = 0;
        obInertia(o) = (int16_t)(-obInertia(o));
    }
    /* .updateHeadSprite */
    cat_mode(o) |= 0x80;
}

/* Cat_Floor — Cat_Head .move */
static void Cat_Floor(uint8_t *o) {
    int32_t d2, d3;
    int16_t d0, old_x, dist, angle;
    uint8_t d0b;

    cat_waittime(o)--;
    if ((int8_t)cat_waittime(o) < 0) {
        /* .undulateNext */
        ob2ndRout(o) -= 2;
        cat_waittime(o) = 8 - 1;
        obVelX(o) = 0;
        obInertia(o) = 0;                  /* REV01 */
        return;
    }

    /* REV01: skip all horizontal work when head isn't moving */
    if (obVelX(o) == 0) return;

    d2 = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
    d3 = d2;                                /* backup of X before update */
    d0 = obVelX(o);
    if (obStatus(o) & 1) d0 = (int16_t)-d0;
    d2 += ((int32_t)d0) << 8;
    obX(o) = (int16_t)((uint32_t)d2 >> 16);
    obSubpixelX(o) = (int16_t)(d2 & 0xFFFF);

    old_x = (int16_t)((uint32_t)d3 >> 16);
    if (old_x == obX(o)) return;            /* didn't move this frame */

    ObjFloorDist(o, &dist, &angle);
    if (dist < -8 || dist >= 0x0C) {
        /* .ledgeHit */
        d0b = cat_segmentpos(o);
        cat_floormap(o)[d0b] = 0x80;
        obSubpixelX(o) = (int16_t)(-obSubpixelX(o));
        if (obSubpixelX(o) != 0 && (obStatus(o) & 1)) {
            obX(o) = (int16_t)(obX(o) - 1);
            cat_segmentpos(o) = (uint8_t)(cat_segmentpos(o) + 1);
            cat_floormap(o)[cat_segmentpos(o) & 0x0F] = 0;
        }
        /* .faceLeft */
        obStatus(o) ^= 1;
        obRender(o) = obStatus(o);
        cat_segmentpos(o) = (uint8_t)((cat_segmentpos(o) + 1) & 0x0F);
        return;
    }

    /* Align to floor + record height */
    obY(o) = (int16_t)(obY(o) + dist);
    d0b = cat_segmentpos(o);
    cat_segmentpos(o) = (uint8_t)((cat_segmentpos(o) + 1) & 0x0F);
    cat_floormap(o)[d0b] = (uint8_t)dist;
}

/* Cat_Head — routine 2 */
static void Cat_Head(uint8_t *o) {
    uint8_t d0, d1, idx, frame, mask;

    if ((int8_t)obStatus(o) < 0) {
        Cat_FragmentateBody(o);
        return;
    }

    d0 = ob2ndRout(o);
    if (d0 == 0)      Cat_Undulate(o);
    else if (d0 == 2) Cat_Floor(o);

    d1 = cat_mode(o);
    if ((int8_t)d1 >= 0) goto display;

    /* Animation update (special non-standard script) */
    idx = obAngle(o) & 0x7F;
    obAngle(o) = (uint8_t)(obAngle(o) + 4);
    frame = Ani_Cat[idx];
    if ((int8_t)frame < 0) {
        cat_mode(o) &= (uint8_t)~0x80;      /* disable animation */
        goto display;
    }
    mask = d1 & 0x10;                        /* mouth open bit adds $10 to frame */
    frame = (uint8_t)(frame + mask);
    obFrame(o) = frame;

display:
    if (OutOfRange(o, -1, 0)) {
        Cat_Despawn(o);
        return;
    }
    DisplaySprite(o);
}

/* Cat_BodySeg1 — routines 4, 8 */
static void Cat_BodySeg1(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)cat_parent(o));
    int16_t d0, old_x;     
    int32_t d2, d3;
    uint8_t d0b, d1b;

    if ((int8_t)obStatus(o) < 0) {
        Cat_FragmentateBody_NotifyHead(o);
        return;
    }

    cat_mode(o) = cat_mode(a1);
    ob2ndRout(o) = ob2ndRout(a1);
    if (ob2ndRout(o) == 0) goto chk_broken;

    obInertia(o) = obInertia(a1);
    d0 = obVelX(a1);
    /* REV01: add self's inertia (was parent's in REV00) */
    d0 = (int16_t)(d0 + obInertia(o));
    obVelX(o) = d0;

    d2 = ((uint32_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);
    d3 = d2;
    d0 = obVelX(o);
    if (obStatus(o) & 1) d0 = (int16_t)-d0;
    d2 += ((int32_t)d0) << 8;
    obX(o) = (int16_t)((uint32_t)d2 >> 16);
    obSubpixelX(o) = (int16_t)(d2 & 0xFFFF);

    old_x = (int16_t)((uint32_t)d3 >> 16);
    if (old_x == obX(o)) goto chk_broken;

    d0b = cat_segmentpos(o);
    d1b = cat_floormap(a1)[d0b];
    if (d1b == 0x80) {
        /* Wall/drop ahead: mirror subpixel handling of REV01 */
        cat_floormap(o)[d0b] = d1b;
        obSubpixelX(o) = (int16_t)(-obSubpixelX(o));
        if (obSubpixelX(o) != 0 && (obStatus(o) & 1) && obVelX(o) == (int16_t)-0xC0) {
            obX(o) = (int16_t)(obX(o) - 1);
            cat_segmentpos(o) = (uint8_t)(cat_segmentpos(o) + 1);
            cat_floormap(o)[cat_segmentpos(o) & 0x0F] = 0;
        }
        /* .faceLeft */
        obStatus(o) ^= 1;
        obRender(o) = obStatus(o);
        cat_segmentpos(o) = (uint8_t)((cat_segmentpos(o) + 1) & 0x0F);
        goto chk_broken;
    }

    /* .alignToFloor */
    obY(o) = (int16_t)(obY(o) + (int8_t)d1b);
    cat_segmentpos(o) = (uint8_t)((cat_segmentpos(o) + 1) & 0x0F);
    cat_floormap(o)[d0b] = d1b;

chk_broken:
    if (obRoutine(a1) == 0x0C) {
        Cat_FragmentateBody_NotifyHead(o);
        return;
    }
    if (obID(a1) == id_ExplosionItem) goto delete_self;
    if (obRoutine(a1) == 0x0A)         goto delete_self;

    DisplaySprite(o);
    return;

delete_self:
    obRoutine(o) = 0x0A;
    /* FixBugs=0: fall through and display this frame */
    DisplaySprite(o);
}

/* Cat_BodySeg2 — routine 6 */
static void Cat_BodySeg2(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)cat_parent(o));
    uint8_t d0, frame;

    cat_mode(o) = cat_mode(a1);
    if ((int8_t)cat_mode(o) >= 0) {
        Cat_BodySeg1(o);
        return;
    }

    /* Animation update (special: skips $FF bytes 4 ahead) */
    d0 = obAngle(o) & 0x7F;
    obAngle(o) = (uint8_t)(obAngle(o) + 4);
    if ((int8_t)Ani_Cat[(d0 + 4) & 0x7F] < 0) {
        obAngle(o) = (uint8_t)(obAngle(o) + 4);   /* skip another step */
    }
    frame = Ani_Cat[d0];
    frame = (uint8_t)(frame + 8);                 /* body frames start at +8 */
    obFrame(o) = frame;

    Cat_BodySeg1(o);
}

/* Cat_FragmentateBody_NotifyHead — set parent's broken flag, then fragment self */
static void Cat_FragmentateBody_NotifyHead(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)cat_parent(o));
    obStatus(a1) |= 0x80;                         /* bset #7 */
    Cat_FragmentateBody(o);
}

/* Cat_FragmentateBody — Cat_FragmentateBody:
   X-speed is picked by obRoutine: 2→-0x200, 4→-0x180, 6→+0x180, 8→+0x200
   (inverted by X-flip). Launches upward at -$400. */
static void Cat_FragmentateBody(uint8_t *o) {
    uint8_t r = obRoutine(o);
    int16_t d0 = Cat_FragSpeed[(r - 2) / 2];
    if (obStatus(o) & 1) d0 = (int16_t)-d0;
    obVelX(o) = d0;
    obVelY(o) = (int16_t)-0x400;
    obRoutine(o) = 0x0C;
    obFrame(o) &= 0xF8;                           /* reset to frame 0 per segment */
    /* ASM falls through to Cat_Fragment */
    Cat_Fragment(o);
}

/* Cat_Fragment — routine $C: bounce and eventually despawn */
static void Cat_Fragment(uint8_t *o) {
    int16_t dist, angle;

    ObjectFall(o);
    if ((int16_t)obVelY(o) >= 0) {
        ObjFloorDist(o, &dist, &angle);
        if (dist < 0) {
            obY(o) = (int16_t)(obY(o) + dist);
            obVelY(o) = (int16_t)-0x400;          /* bounce */
        }
    }
    if ((int8_t)obRender(o) >= 0) {               /* offscreen? */
        Cat_Despawn(o);
        return;
    }
    DisplaySprite(o);
}

/* Caterkiller dispatcher — Cat_Index */
static void Caterkiller_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    switch (obRoutine(o)) {
        case 0x00: Cat_Main(o);     break;
        case 0x02: Cat_Head(o);     break;
        case 0x04: Cat_BodySeg1(o); break;
        case 0x06: Cat_BodySeg2(o); break;
        case 0x08: Cat_BodySeg1(o); break;
        case 0x0A: Cat_Delete(o);   break;
        case 0x0C: Cat_Fragment(o); break;
    }
}

/* ===========================================================================
   Object 7E — Special Stage results screen
   Object 7F — Chaos Emeralds from the Special Stage results screen
   (disasm/_incObj/7E, 7F Special Stage Results and Chaos Emeralds.asm)

   The elements live in the fixed ObjRAM slots v_ssrescard..v_ssrescontinue
   (slots 23-27) and the emeralds at v_ssresemeralds (slot 32); GM_Special
   sets RAM_BYTE(v_ssrescard) = id_SSResult before the SS_NormalExit loop,
   exactly like the ASM.
   =========================================================================== */

/* SSR_ItemData: start X, target X, Y, routine, frame */
typedef struct {
    int16_t start_x;
    int16_t main_x;
    int16_t y;
    uint8_t routine;
    uint8_t frame;
} SSR_Item;

static const SSR_Item SSR_ItemData[] = {
    /* Header text */      { 0x020, 0x120, 0xC4, 2, 0 },  /* custom frame, see SSR_Main */
    /* Score tally */      { 0x320, 0x120, 0x118, 2, 1 },
    /* Ring Bonus tally */ { 0x360, 0x120, 0x128, 2, 2 },
    /* Blue oval */        { 0x1EC, 0x11C, 0xC4, 2, 3 },
    /* Continue tally */   { 0x3A0, 0x120, 0x138, 2, 6 },
};

/* SSRC_PosData: emerald X-positions in order of collection (pseudo-interlaced) */
static const int16_t SSRC_PosData[] = { 0x110, 0x128, 0xF8, 0x140, 0xE0, 0x158 };

/* SSR_ChkPLC (routine 0) -> SSR_Main: once the PLC queue is empty, set up
   the card elements back-to-back in the v_ssrescard..v_ssrescontinue slots. */
static void SSR_Main(uint8_t *o) {
    uint8_t *a1 = o;

    /* moveq #4-1,d1: header, score, ring, oval; +1 if >= 50 rings */
    int d1 = 4 - 1;
    if ((uint16_t)v_rings >= (uint16_t)ss_continue_rings) { /* cmpi.w #ss_continue_rings,(v_rings).w / blo.s SSR_Loop */
        d1 += 1;                                             /* addq.w #1,d1 */
    }

    for (int i = 0; i <= d1; i++) {                          /* SSR_Loop: dbf d1 */
        const SSR_Item *it = &SSR_ItemData[i];
        obID(a1)       = id_SSResult;                        /* _move.b #id_SSResult,obID(a1) */
        obX(a1)        = it->start_x;                        /* move.w (a2)+,obX(a1) */
        ssr_mainX(a1)  = it->main_x;                         /* move.w (a2)+,ssr_mainX(a1) */
        obScreenY(a1)  = it->y;                              /* move.w (a2)+,obScreenY(a1) */
        obRoutine(a1)  = it->routine;                        /* move.b (a2)+,obRoutine(a1) */
        obFrame(a1)    = it->frame;                          /* move.b (a2)+,obFrame(a1) */
        obMap(a1)      = (uint32_t)(uintptr_t)Map_SSR;       /* move.l #Map_SSR,obMap(a1) */
        obGfx(a1)      = (uint16_t)(ArtTile_Title_Card | Tile_Prio); /* move.w #ArtTile_Title_Card|Tile_Prio,obGfx(a1) */
        obRender(a1)   = sprite_cam_screen;                  /* move.b #sprite_cam_screen,obRender(a1) */
        a1 += object_size;                                   /* lea object_size(a1),a1 */
    }

    /* Header text frame: SPECIAL STAGE / CHAOS EMERALDS / SONIC GOT THEM ALL */
    int d0 = 7;                                              /* moveq #7,d0 */
    if (v_emeralds != 0) {                                   /* move.b (v_emeralds).w,d1 / beq.s .setFrame */
        d0 = 0;                                              /* moveq #0,d0 */
        if (v_emeralds == ss_emeralds_num) {                 /* cmpi.b #ss_emeralds_num,d1 / bne.s .setFrame */
            d0 = 8;                                          /* "SONIC GOT THEM ALL" */
            obX(o)       = 0x18;                             /* move.w #$18,obX(a0) */
            ssr_mainX(o) = 0x118;                            /* move.w #$118,ssr_mainX(a0) */
        }
    }
    obFrame(o) = (uint8_t)d0;                                /* .setFrame: move.b d0,obFrame(a0) */
}

static void SSR_ChkPLC(uint8_t *o) {
    if (PLC_IsEmpty()) {                                     /* tst.l (v_plc_buffer).w / beq.s SSR_Main */
        SSR_Main(o);
    }
}

/* SSR_Move (routine 2): slide each element toward ssr_mainX at 0x10 px/frame;
   the ring bonus element (frame 2) controls the sequence when it lands. */
static void SSR_Move(uint8_t *o) {
    int16_t d1 = 0x10;                                       /* moveq #$10,d1 */
    int16_t d0 = ssr_mainX(o);                               /* move.w ssr_mainX(a0),d0 */

    if (d0 == obX(o)) {                                      /* cnt.w obX(a0),d0 / beq.s .reachedXTarget */
        if (obFrame(o) == 2) {                               /* cmpi.b #2,obFrame(a0) / bne.s .checkOffScreen */
            obRoutine(o) += 2;                               /* addq.b #2 -> SSR_Wait (4) */
            ssr_timeframe(o) = 3 * 60;                       /* move.w #3*60,obTimeFrame(a0) */
            RAM_BYTE(v_ssresemeralds) = id_SSRChaos;         /* move.b #id_SSRChaos,(v_ssresemeralds).w */
        }
        goto check_offscreen;                                /* falls through to .checkOffScreen */
    }

    if (d0 < obX(o)) {                                       /* bge.s .updateXPos / neg.w d1 */
        d1 = -d1;
    }
    obX(o) += d1;                                            /* .updateXPos: add.w d1,obX(a0) */

check_offscreen:                                             /* .checkOffScreen */
    d0 = obX(o);                                             /* move.w obX(a0),d0 */
    if (d0 < 0) return;                                      /* bmi.s .return */
    if ((uint16_t)d0 >= 0x80 + 320 + 64) return;             /* cmpi.w #$80+320+64,d0 / bhs.s .return */
    DisplaySprite(o);                                        /* bra.w DisplaySprite */
}

/* SSR_Wait (routines 4, 8, $C, $10): wait out the timer, then advance routine */
static void SSR_Wait(uint8_t *o) {
    ssr_timeframe(o) -= 1;                                   /* subq.w #1,obTimeFrame(a0) */
    if (ssr_timeframe(o) == 0) {                             /* bne.s .display */
        obRoutine(o) += 2;                                   /* addq.b #2,obRoutine(a0) */
    }
    DisplaySprite(o);                                        /* .display: bra.w DisplaySprite */
}

/* SSR_RingBonus (routine 6): count the ring bonus down by 10 (100 points)
   each tally step, then wait and show the continue if >= 50 rings. */
static void SSR_RingBonus(uint8_t *o) {
    DisplaySprite(o);                                        /* bsr.w DisplaySprite */
    f_endactbonus = 1;                                       /* move.b #1,(f_endactbonus).w */

    if (v_ringbonus != 0) {                                  /* tst.w (v_ringbonus).w / beq.s .finished */
        v_ringbonus -= 10;                                   /* subi.w #10,(v_ringbonus).w */
        AddPoints(10);                                       /* moveq #10,d0 / jsr (AddPoints).l */

        if ((v_vblank_byte & 3) == 0) {                      /* move.b (v_vblank_byte).w,d0 / andi.b #3,d0 / bne.s .return */
            Sound_Queue(sfx_Switch, false);                  /* move.w #sfx_Switch,d0 / jmp (QueueSound2).l */
        }
        return;                                              /* .return: rts */
    }

    /* .finished */
    Sound_Queue(sfx_Cash, false);                            /* move.w #sfx_Cash,d0 / jsr (QueueSound2).l */
    obRoutine(o) += 2;                                       /* addq.b #2 -> SSR_Wait (8) */
    ssr_timeframe(o) = 3 * 60;                               /* move.w #3*60,obTimeFrame(a0) */

    if ((uint16_t)v_rings >= (uint16_t)ss_continue_rings) {  /* cmpi.w #ss_continue_rings,(v_rings).w / blo.s .return */
        ssr_timeframe(o) = 1 * 60;                           /* move.w #1*60,obTimeFrame(a0) */
        obRoutine(o) += 4;                                   /* addq.b #4 -> SSR_Wait ($C) */
    }
}

/* SSR_Exit (routines $A, $12): signal SS_NormalExit to return to the level */
static void SSR_Exit(uint8_t *o) {
    f_restart = 1;                                           /* move.w #1,(f_restart).w */
    DisplaySprite(o);                                        /* bra.w DisplaySprite */
}

/* SSR_Continue (routine $E): show the mini-Sonic continue tally and play the
   continue jingle. */
static void SSR_Continue(uint8_t *o) {
    uint8_t *cont = RAM_ADDR(v_ssrescontinue);
    obFrame(cont)   = 4;                                     /* move.b #4,(v_ssrescontinue+obFrame).w */
    obRoutine(cont) = 0x14;                                  /* move.b #$14,(v_ssrescontinue+obRoutine).w -> SSR_ContAni */
    Sound_Queue(sfx_Continue, false);                        /* move.w #sfx_Continue,d0 / jsr (QueueSound2).l */
    obRoutine(o) += 2;                                       /* addq.b #2 -> SSR_Wait ($10) */
    ssr_timeframe(o) = 6 * 60;                               /* move.w #6*60,obTimeFrame(a0) */
    DisplaySprite(o);                                        /* bra.w DisplaySprite */
}

/* SSR_ContAni (routine $14): make mini-Sonic alternate frames 4/5 every
   16 frames (foot tapping). */
static void SSR_ContAni(uint8_t *o) {
    if ((v_vblank_byte & 0x0F) == 0) {                       /* move.b (v_vblank_byte).w,d0 / andi.b #$F,d0 / bne.s .display */
        obFrame(o) ^= 1;                                     /* bchg #0,obFrame(a0) */
    }
    DisplaySprite(o);                                        /* .display: bra.w DisplaySprite */
}

/* Object 7E dispatcher */
static void SSResult_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00: SSR_ChkPLC(o); break;
        case 0x02: SSR_Move(o); break;
        case 0x04:
        case 0x08:
        case 0x0C:
        case 0x10: SSR_Wait(o); break;
        case 0x06: SSR_RingBonus(o); break;
        case 0x0A:
        case 0x12: SSR_Exit(o); break;
        case 0x0E: SSR_Continue(o); break;
        case 0x14: SSR_ContAni(o); break;
    }
}

/* ===========================================================================
   Object 7F — Chaos Emeralds from the results screen
   =========================================================================== */

/* SSRC_Main (routine 0): spawn one SSRChaos per collected emerald into the
   slots right after v_ssresemeralds, reading v_emldlist for the colors. */
static void SSRC_Main(uint8_t *o) {
    uint8_t *a1 = o;

    /* moveq #0,d2 (v_emldlist index); d1 = v_emeralds-1 */
    int d2 = 0;
    int d1 = v_emeralds - 1;                                 /* move.b (v_emeralds).w,d1 / subq.b #1,d1 */
    if (d1 < 0) {                                            /* bcs.w DeleteObject */
        DeleteObject(o);
        return;
    }

    for (int i = 0; i <= d1; i++) {                          /* SSRC_Loop: dbf d1 */
        obID(a1)       = id_SSRChaos;                        /* _move.b #id_SSRChaos,obID(a1) */
        obX(a1)        = SSRC_PosData[i];                    /* move.w (a2)+,obX(a1) */
        obScreenY(a1)  = 0xF0;                               /* move.w #$F0,obScreenY(a1) */

        uint8_t d3 = RAM_BYTE(v_emldlist + d2);              /* lea (v_emldlist).w,a3 / move.b (a3,d2.w),d3 */
        obFrame(a1) = d3;                                    /* move.b d3,obFrame(a1) */
        obAnim(a1)  = d3;                                    /* move.b d3,obAnim(a1) */
        d2 += 1;                                             /* addq.b #1,d2 */

        obRoutine(a1) += 2;                                  /* addq.b #2 -> SSRC_Flash */
        obMap(a1)     = (uint32_t)(uintptr_t)Map_SSRC;       /* move.l #Map_SSRC,obMap(a1) */
        obGfx(a1)     = (uint16_t)(ArtTile_SS_Results_Emeralds | Tile_Prio); /* move.w #ArtTile_SS_Results_Emeralds|Tile_Prio,obGfx(a1) */
        obRender(a1)  = sprite_cam_screen;                   /* move.b #sprite_cam_screen,obRender(a1) */
        a1 += object_size;                                   /* lea object_size(a1),a1 */
    }
}

/* SSRC_Flash (routine 2): alternate each emerald between its visible frame
   and the blank frame (frame 6) every frame. */
static void SSRC_Flash(uint8_t *o) {
    uint8_t d0 = obFrame(o);                                 /* move.b obFrame(a0),d0 */
    obFrame(o) = ss_emeralds_num;                            /* move.b #ss_emeralds_num,obFrame(a0) (blank) */
    if (d0 == ss_emeralds_num) {                             /* cmpi.b #ss_emeralds_num,d0 / bne.s .display */
        obFrame(o) = obAnim(o);                              /* move.b obAnim(a0),obFrame(a0) */
    }
    DisplaySprite(o);                                        /* .display: bra.w DisplaySprite */
}

/* Object 7F dispatcher */
static void SSRChaos_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00: SSRC_Main(o); break;
        case 0x02: SSRC_Flash(o); break;
    }
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
   Part 5  Labyrinth Zone objects ($0A-$0C, $16, $1B, $2C, $2D, $60-$65, $77)
   ---------------------------------------------------------------------------
   Ported 1:1 from the REV01 disassembly (FixBugs=0) of:

     disasm/_incObj/0A LZ Drowning Countdown.asm
     disasm/_incObj/0B LZ Pole that Breaks.asm
     disasm/_incObj/0C LZ Flapping Door.asm
     disasm/_incObj/16 LZ Harpoon.asm
     disasm/_incObj/1B LZ Water Surface.asm
     disasm/_incObj/2C Badnik - Jaws.asm
     disasm/_incObj/2D Badnik - Burrobot.asm
     disasm/_incObj/60 Badnik - Orbinaut.asm
     disasm/_incObj/61 LZ Blocks.asm
     disasm/_incObj/62 LZ Gargoyle.asm
     disasm/_incObj/63 LZ Conveyor.asm
     disasm/_incObj/64 LZ Air Bubbles.asm
     disasm/_incObj/65 LZ Waterfalls.asm
     disasm/_incObj/77 Boss - LZ Main.asm

   plus the shared subroutines they need:
     disasm/_incObj/sub ChkObjectVisible.asm
     disasm/_incObj/Sonic Collision.asm (ObjHitCeiling)
     disasm/_incObj/sub DeleteObject.asm (DeleteChild)
   =========================================================================== */

/* ---------------------------------------------------------------------------
   Shared Labyrinth Zone helpers
   --------------------------------------------------------------------------- */

/* ObjHitCeiling — disasm/_incObj/Sonic Collision.asm 450-471.
   Local copy of the ceiling probe; returns d1 (negative = ceiling hit). */
static int16_t LZ_ObjHitCeiling(uint8_t *o) {
    int16_t d1;
    int16_t y = (int16_t)((obY(o) - (int8_t)obHeight(o)) ^ 0xF);
    FindFloor(y, obX(o), 0x0E, 0x1000, -0x10, &v_anglebuffer, o, &d1);
    return d1;
}

/* ChkObjectVisible — disasm/_incObj/sub ChkObjectVisible.asm 9-29.
   Returns 0 when the object is on screen, 1 when it is off screen (the
   routine's d0, which callers test with `bne`). */
static int LZ_ChkObjectVisible(uint8_t *o) {
    int16_t d0 = (int16_t)(obX(o) - (int16_t)RAM_WORD(0xF700));   /* v_screenposx */
    if (d0 < 0) return 1;
    if (d0 >= 320) return 1;
    int16_t d1 = (int16_t)(obY(o) - (int16_t)RAM_WORD(0xF704));   /* v_screenposy */
    if (d1 < 0) return 1;
    if (d1 >= 224) return 1;
    return 0;
}

/* Drown_WobbleData — disasm/_incObj/0A LZ Drowning Countdown.asm 178-201.
   Revision 1 repeats the 64-byte ripple table twice, so this is the 128-byte
   REV01 form (the first "4" of the third line already became a "3"). */
static const int8_t Drown_WobbleData[128] = {
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2,
    2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -3, -3, -3, -3, -3,
    -3, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,
    -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3,
    -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1,
};

/* ---------------------------------------------------------------------------
   Object $0A - LZ Drowning Countdown
   disasm/_incObj/0A LZ Drowning Countdown.asm
   --------------------------------------------------------------------------- */
#define drown_restarttime(o)  (*(int16_t *)((uint8_t *)(o) + 0x2C)) /* objoff_2C */
#define drown_origX(o)        (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */
#define drown_displaytime(o)  (*(uint8_t *)((uint8_t *)(o) + 0x32)) /* objoff_32 */
#define drown_type(o)         (*(uint8_t *)((uint8_t *)(o) + 0x33)) /* objoff_33 */
#define drown_extrabubbles(o) (*(uint8_t *)((uint8_t *)(o) + 0x34)) /* objoff_34 */
#define drown_extrabubflag(o) (*(int16_t *)((uint8_t *)(o) + 0x36)) /* objoff_36 */
#define drown_numtime(o)      (*(int16_t *)((uint8_t *)(o) + 0x38)) /* objoff_38 */
#define drown_delaytime(o)    (*(int16_t *)((uint8_t *)(o) + 0x3A)) /* objoff_3A */

static void Drown_Main(uint8_t *o);
static void Drown_Animate(uint8_t *o);
static void Drown_ChkWater(uint8_t *o);
static void Drown_Display(uint8_t *o);
static void Drown_Countdown(uint8_t *o);
static void Drown_AirLeft(uint8_t *o);
static void Drown_ShowNumber(uint8_t *o);
static void Drown_CheckSpawnExtraBubbles(uint8_t *o);
static void Drown_SpawnExtraBubbles(uint8_t *o);
static void Drown_DecrementExtraBubbles(uint8_t *o);

/* Drown_ShowNumber — routine shared by Drown_ChkWater.wobble and
   Drown_Display; converts a floating number bubble into the screen-fixed
   countdown bubble and hands it over to Drown_AirLeft. */
static void Drown_ShowNumber(uint8_t *o) {
    if (drown_numtime(o) == 0) return;              /* tst.w / beq.s .return */
    drown_numtime(o) = (int16_t)(drown_numtime(o) - 1);
    if (drown_numtime(o) != 0) return;             /* bne.s .return */
    if (obAnim(o) >= 7) return;                     /* cmpi.b #7 / bhs.s .return */

    drown_numtime(o) = 15;                          /* move.w #15 */
    obVelY(o) = 0;
    obRender(o) = sprite_rendered;                  /* screen-fixed positioning */

    int16_t d0 = (int16_t)(obX(o) - (int16_t)RAM_WORD(0xF700));
    d0 = (int16_t)(d0 + 0x80);
    obX(o) = d0;
    d0 = (int16_t)(obY(o) - (int16_t)RAM_WORD(0xF704));
    d0 = (int16_t)(d0 + 0x80);
    obScreenY(o) = d0;                              /* same field as obSubpixelX */

    obRoutine(o) = 0x0C;                            /* -> Drown_AirLeft */
}

/* Drown_Main — routine 0 */
static void Drown_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* -> Drown_Animate */
    obMap(o) = (uint32_t)(uintptr_t)Map_Bub;
    obGfx(o) = (uint16_t)(ArtTile_LZ_Bubbles | Tile_Prio);
    obRender(o) = (uint8_t)(sprite_rendered | sprite_cam_field);
    obActWid(o) = 32 / 2;
    obPriority(o) = 1;

    uint16_t d0 = obSubtype(o);
    if (!(d0 & 0x80)) {                                       /* bpl.s .numberBubble */
        /* .numberBubble: a plain bubble rising out of Sonic's mouth */
        obAnim(o) = (uint8_t)d0;
        drown_origX(o) = obX(o);
        obVelY(o) = (int16_t)-0x88;
        Drown_Animate(o);                                    /* fall through */
        return;
    }

    obRoutine(o) = 0x0A;                             /* -> Drown_Countdown */
    /* FixBugs=0: the countdown object is left pointing at the unused Sonic
       face mappings (it is invisible anyway; only the numbers are shown). */
    obMap(o) = (uint32_t)(uintptr_t)Map_UnusedFace;
    obGfx(o) = (uint16_t)ArtTile_LZ_UnusedFace;
    d0 = (uint16_t)(d0 & 0x7F);
    drown_type(o) = (uint8_t)d0;
    Drown_Countdown(o);                                      /* bra.w */
}

/* Drown_Animate — routine 2 */
static void Drown_Animate(uint8_t *o) {
    AnimateSprite(o, Ani_Drown);      /* afRoutine advances to Drown_ChkWater */
}

/* Drown_ChkWater — routine 4 */
static void Drown_ChkWater(uint8_t *o) {
    if ((uint16_t)v_waterpos1 < (uint16_t)obY(o)) {           /* blo.s .wobble */
        /* .wobble */
        if (f_wtunnelmode) drown_origX(o) = (int16_t)(drown_origX(o) + 4);

        uint8_t d0 = obAngle(o);
        obAngle(o) = (uint8_t)(d0 + 1);
        d0 = (uint8_t)(d0 & 0x7F);
        d0 = (uint8_t)Drown_WobbleData[d0];
        int16_t wx = (int16_t)(int16_t)(int8_t)d0;
        wx = (int16_t)(wx + drown_origX(o));
        obX(o) = wx;

        Drown_ShowNumber(o);

        SpeedToPos(o);
        if (!(obRender(o) & 0x80)) {                          /* bpl.s .delete */
            DeleteObject(o);
            return;
        }
        DisplaySprite(o);
        return;
    }

    obRoutine(o) = 6;                                         /* -> Drown_Display */
    obAnim(o) = (uint8_t)(obAnim(o) + 7);
    /* FixBugs=0: `cmpi.b #$D,obAnim` is dead — beq and bra share the target. */
    Drown_Display(o);
}

/* Drown_Display — routine 6 and routine $E */
static void Drown_Display(uint8_t *o) {
    Drown_ShowNumber(o);
    AnimateSprite(o, Ani_Drown);
    DisplaySprite(o);
}

/* Drown_AirLeft — routine $C */
static void Drown_AirLeft(uint8_t *o) {
    if ((uint16_t)v_air > 12) {                               /* bhi.s .delete */
        DeleteObject(o);
        return;
    }

    drown_numtime(o) = (int16_t)(drown_numtime(o) - 1);
    if (drown_numtime(o) != 0) {                              /* bne.s .display */
        AnimateSprite(o, Ani_Drown);
        if (!(obRender(o) & 0x80)) { DeleteObject(o); return; }
        DisplaySprite(o);
        return;
    }

    obRoutine(o) = 0x0E;                             /* -> Drown_Display (2nd) */
    obAnim(o) = (uint8_t)(obAnim(o) + 7);
    Drown_Display(o);
}

/* Drown_DecrementExtraBubbles — shared tail of the bubble-spawn sequence. */
static void Drown_DecrementExtraBubbles(uint8_t *o) {
    drown_extrabubbles(o) = (uint8_t)(drown_extrabubbles(o) - 1);
    if ((int8_t)drown_extrabubbles(o) >= 0) return;          /* bpl.s .return */
    drown_extrabubflag(o) = 0;                                /* clr.w */
}

/* Drown_CheckSpawnExtraBubbles — decide whether a new extra bubble is due. */
static void Drown_CheckSpawnExtraBubbles(uint8_t *o) {
    if (drown_extrabubflag(o) == 0) return;                   /* beq.w .return */
    drown_delaytime(o) = (int16_t)(drown_delaytime(o) - 1);
    if (drown_delaytime(o) >= 0) return;                      /* bpl.w .return */
    Drown_SpawnExtraBubbles(o);                               /* fall through */
}

/* Drown_Countdown — routine $A: the actual air countdown / drowning logic. */
static void Drown_Countdown(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_player);

    if (drown_restarttime(o) != 0) {                          /* bne .sonicIsDrowning */
        /* .sonicIsDrowning */
        drown_restarttime(o) = (int16_t)(drown_restarttime(o) - 1);
        if (drown_restarttime(o) == 0) {
            obRoutine(a1) = 6;                                /* -> Sonic_Death */
            return;
        }
        /* .sinking */
        SpeedToPos(a1);
        obVelY(a1) = (int16_t)(obVelY(a1) + 0x10);
        Drown_CheckSpawnExtraBubbles(o);
        return;
    }

    if (obRoutine(a1) >= 6) return;                           /* bhs.w .return */
    if (!(obStatus(a1) & (1 << 6))) return;                   /* beq.w .return */

    drown_numtime(o) = (int16_t)(drown_numtime(o) - 1);
    if (drown_numtime(o) >= 0) {                              /* bpl */
        Drown_CheckSpawnExtraBubbles(o);
        return;
    }
    drown_numtime(o) = 60 - 1;

    drown_extrabubflag(o) = 1;
    uint16_t r = RandomNumber();
    drown_extrabubbles(o) = (uint8_t)(r & 1);

    int16_t air = v_air;
    int warn = 0;
    if (air == 25 || air == 20 || air == 15) warn = 1;
    if ((uint16_t)air > 12) {
        /* .reduceAir */
    } else if (air == 12) {
        Sound_Queue(bgm_Drowning, true);             /* jsr (QueueSound1).l */
        /* .countdown */
        drown_displaytime(o) = (uint8_t)(drown_displaytime(o) - 1);
        if ((int8_t)drown_displaytime(o) < 0) {
            drown_displaytime(o) = drown_type(o);
            drown_extrabubflag(o) = (int16_t)(drown_extrabubflag(o) | 0x8000);
        }
    } else {
        /* .countdown */
        drown_displaytime(o) = (uint8_t)(drown_displaytime(o) - 1);
        if ((int8_t)drown_displaytime(o) < 0) {
            drown_displaytime(o) = drown_type(o);
            drown_extrabubflag(o) = (int16_t)(drown_extrabubflag(o) | 0x8000);
        }
    }
    if (warn) Sound_Queue(sfx_Warning, false);        /* .warningSound */

    /* .reduceAir: subq.w #1,(v_air).w */
    {
        int16_t before = v_air;
        v_air = (int16_t)(before - 1);
        if (before >= 1) {                           /* bhs .gotoSpawnExtraBubbles */
            Drown_SpawnExtraBubbles(o);
            return;
        }
    }

    /* Sonic drowns here */
    ResumeMusic();
    f_playerctrl = 0x81;
    Sound_Queue(sfx_Drown, false);
    drown_extrabubbles(o) = 10;
    drown_extrabubflag(o) = 1;
    drown_restarttime(o) = 2 * 60;

    Sonic_ResetOnFloor(a1);
    obAnim(a1) = id_SonDrown;                 /* id_Drown */
    obStatus(a1) = (uint8_t)(obStatus(a1) | 0x02);
    obGfx(a1) = (uint16_t)(obGfx(a1) | 0x8000);  /* bset #7 on the word */
    obVelY(a1) = 0;
    obVelX(a1) = 0;
    obInertia(a1) = 0;
    f_nobgscroll = 1;
    /* FixBugs=0 omits `move.b #2,obRoutine(a1)` and `clr.b (f_timecount).w`. */
    return;
}

/* Drown_SpawnExtraBubbles — spawn one bubble, then the shared tail. */
static void Drown_SpawnExtraBubbles(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_player);

    uint16_t r = RandomNumber();
    r = (uint16_t)(r & 0xF);
    drown_delaytime(o) = (int16_t)r;

    uint8_t *a2 = (uint8_t *)FindFreeObj();
    if (!a2) return;                                          /* bne.w .return */

    obID(a2) = id_DrownCount;
    obX(a2) = obX(a1);
    int16_t d0 = 6;                                           /* moveq #6,d0 */
    if (obStatus(a1) & 0x01) {                                /* btst #0 */
        d0 = (int16_t)-d0;
        obAngle(a2) = 0x40;
    }
    obX(a2) = (int16_t)(obX(a2) + d0);
    obY(a2) = obY(a1);
    obSubtype(a2) = 6;                                        /* "small bubble" */

    if (drown_restarttime(o) != 0) {
        /* Sonic is drowning: bubbles come out of his mouth. */
        drown_delaytime(o) = (int16_t)(drown_delaytime(o) & 7);
        /* addi.w #0,drown_delaytime — no-op left over in the ASM */
        d0 = (int16_t)(obY(a1) - 12);
        obY(a2) = d0;
        obAngle(a2) = (uint8_t)RandomNumber();
        if (((uint16_t)v_framecount & 3) == 0) obSubtype(a2) = 0x0E;
        Drown_DecrementExtraBubbles(o);
        return;
    }

    /* .checkNumberBubble */
    if (drown_extrabubflag(o) & 0x8000) {
        int16_t d2 = (int16_t)((uint16_t)v_air >> 1);         /* number bubble ID */
        if ((RandomNumber() & 3) == 0) {
            uint16_t old = (uint16_t)drown_extrabubflag(o);
            drown_extrabubflag(o) = (int16_t)(old | 0x4000); /* bset #6 */
            if (!(old & 0x4000)) {                            /* bne: old bit clear */
                obSubtype(a2) = (uint8_t)d2;
                drown_numtime(a2) = 28;
            }
        }
        /* .secondTry */
        if (drown_extrabubbles(o) == 0) {
            uint16_t old = (uint16_t)drown_extrabubflag(o);
            drown_extrabubflag(o) = (int16_t)(old | 0x4000);
            if (!(old & 0x4000)) {
                obSubtype(a2) = (uint8_t)d2;
                drown_numtime(a2) = 28;
            }
        }
    }

    Drown_DecrementExtraBubbles(o);
}

/* Object $0A dispatcher */
static void DrownCount_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00: Drown_Main(o); break;
        case 0x02: Drown_Animate(o); break;
        case 0x04: Drown_ChkWater(o); break;
        case 0x06:                                         /* fall through */
        case 0x0E: Drown_Display(o); break;
        case 0x08:                                         /* fall through */
        case 0x10: DeleteObject(o); break;
        case 0x0A: Drown_Countdown(o); break;
        case 0x0C: Drown_AirLeft(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $0B - LZ Pole that Breaks
   disasm/_incObj/0B LZ Pole that Breaks.asm
   --------------------------------------------------------------------------- */
#define pole_breaktime(o) (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */
#define pole_grabbed(o)   (*(uint8_t *)((uint8_t *)(o) + 0x32)) /* objoff_32 */

static void Pole_Action(uint8_t *o);
static void Pole_Display(uint8_t *o);

/* Pole_Main — routine 0 */
static void Pole_Main(uint8_t *o) {
    obRoutine(o) += 2;                                       /* -> Pole_Action */
    obMap(o) = (uint32_t)(uintptr_t)Map_Pole;
    obGfx(o) = (uint16_t)(ArtTile_LZ_Pole | Tile_Pal3);
    obRender(o) = sprite_cam_field;
    obActWid(o) = 16 / 2;
    obPriority(o) = 4;

    obColType(o) = (uint8_t)(col_8x64 | col_special);
    uint32_t d0 = (uint32_t)obSubtype(o) * 60;               /* mulu.w #60 */
    pole_breaktime(o) = (int16_t)d0;
    Pole_Action(o);                                          /* fall through */
}

/* Pole_Action — routine 2 */
static void Pole_Action(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_player);
    int16_t d0;

    if (pole_grabbed(o)) {
        if (pole_breaktime(o) != 0) {
            pole_breaktime(o) = (int16_t)(pole_breaktime(o) - 1);
            if (pole_breaktime(o) == 0) {
                obFrame(o) = 1;                              /* "broken pole" */
                goto release;
            }
        }

        /* .checkMoveUp */
        d0 = (int16_t)(obY(o) - 24);
        if (v_jpadhold1 & btnUp) {
            obY(a1) = (int16_t)(obY(a1) - 1);
            if ((uint16_t)obY(a1) <= (uint16_t)d0) obY(a1) = d0;
        }

        /* .checkMoveDown */
        d0 = (int16_t)(d0 + 12 + 24);
        if (v_jpadhold1 & btnDn) {
            obY(a1) = (int16_t)(obY(a1) + 1);
            if ((uint16_t)obY(a1) > (uint16_t)d0) obY(a1) = d0;
        }

        /* .checkLetGo */
        if (!(v_jpadpress2 & btnABC)) {                      /* beq.s Pole_Display */
            Pole_Display(o);
            return;
        }
    } else {
        /* .checkGrab */
        if (obColProp(o) == 0) {                              /* beq.s Pole_Display */
            Pole_Display(o);
            return;
        }
        d0 = (int16_t)(obX(o) + 20);
        if ((uint16_t)d0 >= (uint16_t)obX(a1)) {             /* cmp.w obX(a1),d0 */
            Pole_Display(o);
            return;
        }
        obColProp(o) = 0;
        if (obRoutine(a1) >= 4) {                             /* cmpi.b #4 / bhs */
            Pole_Display(o);
            return;
        }
        obVelX(a1) = 0;
        obVelY(a1) = 0;
        d0 = (int16_t)(obX(o) + 20);
        obX(a1) = d0;
        obStatus(a1) = (uint8_t)(obStatus(a1) & ~0x01);
        obAnim(a1) = id_SonHang;                    /* id_Hang, $11 */
        f_playerctrl = 1;
        f_wtunneldisallow = 1;
        pole_grabbed(o) = 1;
        Pole_Display(o);
        return;
    }

release:
    obColType(o) = 0;
    obRoutine(o) = (uint8_t)(obRoutine(o) + 2);   /* -> Pole_Display */
    f_playerctrl = 0;
    f_wtunneldisallow = 0;
    pole_grabbed(o) = 0;
    Pole_Display(o);
}

/* Pole_Display — routine 4 */
static void Pole_Display(uint8_t *o) {
    RememberState(o);
}

/* Object $0B dispatcher */
static void Pole_ObjectMain(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00: Pole_Main(o); break;
        case 0x02: Pole_Action(o); break;
        case 0x04: Pole_Display(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $0C - LZ Flapping Door
   disasm/_incObj/0C LZ Flapping Door.asm
   --------------------------------------------------------------------------- */
#define flap_wait(o) (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */
#define flap_time(o) (*(int16_t *)((uint8_t *)(o) + 0x32)) /* objoff_32 */

static void Flap_OpenClose(uint8_t *o) {
    flap_wait(o) = (int16_t)(flap_wait(o) - 1);
    if (flap_wait(o) >= 0) goto animate;                      /* bpl.s */
    flap_wait(o) = flap_time(o);
    obAnim(o) = (uint8_t)(obAnim(o) ^ 0x01);                 /* bchg #0 */
    if (obRender(o) & 0x80) Sound_Queue(sfx_Door, false);   /* bpl → skip */

animate:
    AnimateSprite(o, Ani_Flap);
    f_wtunneldisallow = 0;

    if (obFrame(o) != 0) goto display;                       /* bne.s .display */
    if ((uint16_t)obX(RAM_ADDR(v_player)) >= (uint16_t)obX(o)) goto display;

    f_wtunneldisallow = 1;
    {
        int16_t d1 = (int16_t)(16 / 2 + sonic_solid_width);
        int16_t d2 = 64 / 2;
        int16_t d3 = (int16_t)(d2 + 1);
        int16_t d4 = obX(o);
        int16_t d3out = 0, d5out = 0;
        SolidObject(o, d1, d2, d3, d4, &d3out, &d5out);
    }
display:
    RememberState(o);
}

/* Object $0C dispatcher */
static void FlapDoor_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00:
            obRoutine(o) += 2;                               /* -> Flap_OpenClose */
            obMap(o) = (uint32_t)(uintptr_t)Map_Flap;
            obGfx(o) = (uint16_t)(ArtTile_LZ_Flapping_Door | Tile_Pal3);
            obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
            obActWid(o) = 80 / 2;
            flap_time(o) = (int16_t)((uint32_t)obSubtype(o) * 60); /* mulu.w #60 */
            Flap_OpenClose(o);
            break;
        case 0x02: Flap_OpenClose(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $16 - LZ Harpoon
   disasm/_incObj/16 LZ Harpoon.asm
   --------------------------------------------------------------------------- */
#define harp_time(o) (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */

/* Harp_ColTypes — disasm/_incObj/16 LZ Harpoon.asm 41-47, indexed by obFrame. */
static const uint8_t Harp_ColTypes[6] = {
    (uint8_t)(col_16x8 | col_hurt),   /* frame 0, sideways, retracted  */
    (uint8_t)(col_48x8 | col_hurt),   /* frame 1, sideways, moving     */
    (uint8_t)(col_80x8 | col_hurt),   /* frame 2, sideways, extended  */
    (uint8_t)(col_8x16  | col_hurt),  /* frame 3, upright,  retracted  */
    (uint8_t)(col_8x48  | col_hurt),  /* frame 4, upright,  moving     */
    (uint8_t)(col_8x80  | col_hurt),  /* frame 5, upright,  extended   */
};

static void Harp_Move(uint8_t *o) {
    AnimateSprite(o, Ani_Harp);
    uint8_t d0 = obFrame(o);
    if (d0 > 5) d0 = 5;                 /* ASM reads OOB; clamp for safety */
    obColType(o) = Harp_ColTypes[d0];
    RememberState(o);
}

static void Harp_Wait(uint8_t *o) {
    harp_time(o) = (int16_t)(harp_time(o) - 1);
    if (harp_time(o) >= 0) {                            /* bpl.s .display */
        RememberState(o);
        return;
    }
    harp_time(o) = 60;
    obRoutine(o) = (uint8_t)(obRoutine(o) - 2);         /* back to Harp_Move */
    obAnim(o) = (uint8_t)(obAnim(o) ^ 0x01);           /* bchg #0 */
    RememberState(o);
}

/* Object $16 dispatcher */
static void Harpoon_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00:
            obRoutine(o) += 2;                           /* -> Harp_Move */
            obMap(o) = (uint32_t)(uintptr_t)Map_Harp;
            obGfx(o) = (uint16_t)ArtTile_LZ_Harpoon;
            obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
            obPriority(o) = 4;
            obAnim(o) = obSubtype(o);            /* 0 = sideways, 2 = upright */
            obActWid(o) = 40 / 2;
            harp_time(o) = 60;
            Harp_Move(o);
            break;
        case 0x02: Harp_Move(o); break;
        case 0x04: Harp_Wait(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $1B - LZ Water Surface
   disasm/_incObj/1B LZ Water Surface.asm
   --------------------------------------------------------------------------- */
#define surf_origX(o)  (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */
#define surf_freeze(o) (*(uint8_t *)((uint8_t *)(o) + 0x32)) /* objoff_32 */

static void Surf_Action(uint8_t *o) {
    /* Wrap the camera every $20px so the surface looks camera-independent. */
    int16_t d1 = (int16_t)((uint16_t)v_screenposx & 0xFFE0);
    d1 = (int16_t)(d1 + surf_origX(o));
    if (v_framebyte & 0x01) d1 = (int16_t)(d1 + 0x20);     /* flicker */
    obX(o) = d1;

    obY(o) = v_waterpos1;

    if (surf_freeze(o)) goto gamePaused;
    if (!(v_jpadpress1 & btnStart)) goto animate;
    obFrame(o) = (uint8_t)(obFrame(o) + 3);   /* seamless "paused" frames */
    surf_freeze(o) = 1;
    goto display;

gamePaused:
    if (f_pause) goto display;                             /* bne.s .display */
    surf_freeze(o) = 0;
    obFrame(o) = (uint8_t)(obFrame(o) - 3);

animate:
    obTimeFrame(o) = (uint8_t)(obTimeFrame(o) - 1);
    if ((int8_t)obTimeFrame(o) >= 0) goto display;         /* bpl.s .display */
    obTimeFrame(o) = 8 - 1;
    obFrame(o) = (uint8_t)(obFrame(o) + 1);
    if (obFrame(o) >= 3) obFrame(o) = 0;

display:
    DisplaySprite(o);
}

/* Object $1B dispatcher */
static void WaterSurface_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00:
            obRoutine(o) += 2;                             /* -> Surf_Action */
            obMap(o) = (uint32_t)(uintptr_t)Map_Surf;
            obGfx(o) = (uint16_t)(ArtTile_LZ_Water_Surface | Tile_Pal3 | Tile_Prio);
            obRender(o) = sprite_cam_field;
            obActWid(o) = 256 / 2;
            surf_origX(o) = obX(o);
            Surf_Action(o);
            break;
        case 0x02: Surf_Action(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $2C - Badnik - Jaws
   disasm/_incObj/2C Badnik - Jaws.asm
   --------------------------------------------------------------------------- */
#define jaws_turndelay_current(o) (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */
#define jaws_turndelay_base(o)    (*(int16_t *)((uint8_t *)(o) + 0x32)) /* objoff_32 */

static void Jaws_Swim(uint8_t *o) {
    jaws_turndelay_current(o) = (int16_t)(jaws_turndelay_current(o) - 1);
    if (jaws_turndelay_current(o) >= 0) goto animate;      /* bpl.s .animate */
    jaws_turndelay_current(o) = jaws_turndelay_base(o);
    obVelX(o) = (int16_t)-obVelX(o);
    obStatus(o) = (uint8_t)(obStatus(o) ^ 0x01);           /* bchg #0 */
    obPrevAni(o) = 1;              /* move.b #1,obPrevAni: force anim reload */

animate:
    AnimateSprite(o, Ani_Jaws);
    SpeedToPos(o);
    RememberState(o);
}

/* Object $2C dispatcher */
static void Jaws_ObjectMain(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00:
            obRoutine(o) += 2;                             /* -> Jaws_Swim */
            obMap(o) = (uint32_t)(uintptr_t)Map_Jaws;
            obGfx(o) = (uint16_t)(ArtTile_Jaws | Tile_Pal2);
            obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
            obColType(o) = (uint8_t)(col_32x24 | col_badnik);
            obPriority(o) = 4;
            /* FixBugs=0 uses 32/2 here instead of the corrected 48/2. */
            obActWid(o) = 32 / 2;

            uint16_t d0 = (uint16_t)((uint16_t)obSubtype(o) << 6); /* lsl.w #6 */
            d0 = (uint16_t)(d0 - 1);                       /* subq.w #1 */
            jaws_turndelay_current(o) = (int16_t)d0;
            jaws_turndelay_base(o) = (int16_t)d0;

            obVelX(o) = (int16_t)-0x40;
            if (obStatus(o) & 0x01) obVelX(o) = 0x40;
            Jaws_Swim(o);
            break;
        case 0x02: Jaws_Swim(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $2D - Badnik - Burrobot
   disasm/_incObj/2D Badnik - Burrobot.asm
   --------------------------------------------------------------------------- */
#define burro_timedelay(o) (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */
#define burro_checktype(o) (*(uint8_t *)((uint8_t *)(o) + 0x32)) /* objoff_32 */

static int Burro_CheckDistanceAndFaceSonic(uint8_t *o, int16_t d2,
                                           int16_t *out_d1);

/* Burro_Action_TurnAround — action 0 */
static void Burro_Action_TurnAround(uint8_t *o) {
    burro_timedelay(o) = (int16_t)(burro_timedelay(o) - 1);
    if (burro_timedelay(o) >= 0) return;                    /* bpl.s .return */

    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
    burro_timedelay(o) = 255;
    obVelX(o) = 0x80;
    obAnim(o) = 1;
    obStatus(o) = (uint8_t)(obStatus(o) ^ 0x01);           /* bchg #0 */
    if (obStatus(o) & 0x01) obVelX(o) = (int16_t)-0x80;    /* bne (new bit set) */
}

/* Burro_Action_Move_NextAction — shared by Move (ledge/timer) and the
   landing branch of Jump. */
static void Burro_Action_Move_NextAction(uint8_t *o) {
    /* btst #2,(v_vblank_byte).w / beq.s .jumpAgain : are we on frame 0-3 of the
       8-frame window? Then jump again, otherwise turn around. */
    if (!(v_vblank_byte & (1 << 2))) {                        /* .jumpAgain */
        ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);            /* -> Burro_Action_Jump */
        obVelY(o) = (int16_t)-0x400;                           /* launch upwards */
        obAnim(o) = 2;                                         /* drilling anim */
        return;
    }
    /* .turnAround */
    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) - 2);                /* -> TurnAround */
    burro_timedelay(o) = 60 - 1;
    obVelX(o) = 0;                                             /* stop moving */
    obAnim(o) = 0;                                             /* still anim */
}

/* Burro_Action_Move — action 2 */
static void Burro_Action_Move(uint8_t *o) {
    burro_timedelay(o) = (int16_t)(burro_timedelay(o) - 1);
    if (burro_timedelay(o) < 0) {                          /* bmi.s */
        Burro_Action_Move_NextAction(o);
        return;
    }

    SpeedToPos(o);

    /* bchg #0,burro_checktype : alternate floor align / ledge check */
    burro_checktype(o) = (uint8_t)(burro_checktype(o) ^ 0x01);
    if (burro_checktype(o) != 0) {                         /* bne.s .alignToFloor */
        int16_t d1 = 0, d3 = 0;
        ObjFloorDist(o, &d1, &d3);
        obY(o) = (int16_t)(obY(o) + d1);
        return;
    }

    /* .checkLedgeAhead */
    int16_t d3 = (int16_t)(obX(o) + 12);
    if (!(obStatus(o) & 0x01)) d3 = (int16_t)(d3 - 12 * 2);
    int16_t d1 = 0, angle = 0;
    ObjFloorDist2(o, d3, &d1, &angle);
    if (d1 >= 0x0C) {                                      /* cmpi.w #$C / bge */
        Burro_Action_Move_NextAction(o);
        return;
    }
}

/* Burro_Action_Jump — action 4 */
static void Burro_Action_Jump(uint8_t *o) {
    SpeedToPos(o);
    obVelY(o) = (int16_t)(obVelY(o) + 0x18);
    if (obVelY(o) < 0) return;                             /* bmi.s .return */
    obAnim(o) = 3;

    int16_t d1 = 0, d3 = 0;
    ObjFloorDist(o, &d1, &d3);
    if (d1 >= 0) return;                                   /* tst.w d1 / bpl */
    obY(o) = (int16_t)(obY(o) + d1);

    obVelY(o) = 0;
    obAnim(o) = 1;
    burro_timedelay(o) = 255;
    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) - 2);
    {
        int16_t d1b = 0;
        /* d2 is undefined here in the ASM and the cmp result (carry) is
           discarded by the caller, so the trigger-zone value cannot affect
           behaviour. d1 is likewise unused after this call. */
        (void)Burro_CheckDistanceAndFaceSonic(o, 0, &d1b);
    }
}

/* Burro_Action_ChkSonic — action 6 */
static void Burro_Action_ChkSonic(uint8_t *o) {
    int16_t d1 = 0;
    if (!Burro_CheckDistanceAndFaceSonic(o, 96, &d1)) return;      /* bhs */

    uint8_t *player = RAM_ADDR(v_player);
    int16_t d0 = (int16_t)(obY(player) - obY(o));
    /* sub.w obY(a0),d0 / bhs.s .return : no borrow, i.e. Sonic is not above */
    if ((uint16_t)obY(player) >= (uint16_t)obY(o)) return;
    if ((uint16_t)d0 < 0xFF80) return;                 /* cmpi.w #-128,d0 / blo */
    if (v_debuguse) return;                                 /* tst.w v_debuguse */

    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) - 2);
    obVelX(o) = d1;                    /* move.w d1,obVelX(a0) */
    obVelY(o) = (int16_t)-0x400;
}

/* Burro_CheckDistanceAndFaceSonic — disasm 2D 169-182. Face Sonic, then report
   whether he is inside the d2-sized trigger zone (returns the cmp's carry). */
static int Burro_CheckDistanceAndFaceSonic(uint8_t *o, int16_t d2,
                                           int16_t *out_d1) {
    int16_t d1 = 0x80;
    obStatus(o) = (uint8_t)(obStatus(o) | 0x01);            /* bset #0: face right */
    uint8_t *player = RAM_ADDR(v_player);
    int16_t d0 = (int16_t)(obX(player) - obX(o));
    /* sub.w obX(a0),d0 / bhs.s .checkDistance : no borrow, i.e. Sonic is left of
       the Burrobot, so d0 already is the absolute distance. */
    if ((uint16_t)obX(player) < (uint16_t)obX(o)) {
        d0 = (int16_t)-d0;                                  /* make X-difference positive */
        d1 = (int16_t)-d1;                                  /* move Burrobot to the left */
        obStatus(o) = (uint8_t)(obStatus(o) & ~0x01);       /* bclr #0: face left */
    }
    *out_d1 = d1;
    /* cmp.w d2,d0 : carry set when d0 < d2 (unsigned) */
    return (uint16_t)d0 < (uint16_t)d2;
}

/* Object $2D dispatcher */
static void Burrobot_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00:
            obRoutine(o) += 2;                             /* -> Burro_Action */
            obHeight(o) = 38 / 2;
            obWidth(o) = 16 / 2;
            obMap(o) = (uint32_t)(uintptr_t)Map_Burro;
            obGfx(o) = (uint16_t)ArtTile_Burrobot;
            obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
            obPriority(o) = 4;
            obColType(o) = (uint8_t)(col_24x36 | col_badnik);
            obActWid(o) = 24 / 2;
            ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 6); /* run ChkSonic first */
            obAnim(o) = 2;                                 /* drilling anim */
            /* fall through */
            __attribute__((fallthrough));
        case 0x02:
            switch (ob2ndRout(o)) {
                case 0x00: Burro_Action_TurnAround(o); break;
                case 0x02: Burro_Action_Move(o); break;
                case 0x04: Burro_Action_Jump(o); break;
                case 0x06: Burro_Action_ChkSonic(o); break;
            }
            AnimateSprite(o, Ani_Burro);
            RememberState(o);
            break;
    }
}

/* ---------------------------------------------------------------------------
   Object $62 - LZ Gargoyle
   disasm/_incObj/62 LZ Gargoyle.asm
   --------------------------------------------------------------------------- */

/* Gar_SpitRate — disasm 62 LZ Gargoyle.asm 19, indexed by subtype & $F. */
static const uint8_t Gar_SpitRate[8] = { 30, 60, 90, 120, 150, 180, 210, 240 };

static void Gar_Main(uint8_t *o) {
    obRoutine(o) += 2;                                     /* -> Gar_MakeFire */
    obMap(o) = (uint32_t)(uintptr_t)Map_Gar;
    obGfx(o) = (uint16_t)(ArtTile_LZ_Gargoyle | Tile_Pal3);
    obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
    obPriority(o) = 3;
    obActWid(o) = 32 / 2;

    uint8_t d0 = (uint8_t)(obSubtype(o) & 0x0F);
    if (d0 > 7) d0 = 7;                 /* ASM reads OOB; clamp for safety */
    obDelayAni(o) = Gar_SpitRate[d0];
    obTimeFrame(o) = obDelayAni(o);
    obSubtype(o) = (uint8_t)(obSubtype(o) & 0x0F);
}

static void Gar_MakeFire(uint8_t *o) {
    obTimeFrame(o) = (uint8_t)(obTimeFrame(o) - 1);
    if (obTimeFrame(o) != 0) return;                        /* bne.s .return */
    obTimeFrame(o) = obDelayAni(o);
    if (LZ_ChkObjectVisible(o)) return;                     /* bne.s .return */

    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (!a1) return;                                        /* bne.s .return */

    obID(a1) = id_Gargoyle;
    obRoutine(a1) = (uint8_t)(obRoutine(a1) + 4);   /* -> Gar_FireBall */
    obX(a1) = obX(o);
    obY(a1) = obY(o);
    obRender(a1) = obRender(o);
    obStatus(a1) = obStatus(o);
}

/* Gar_AniFire returns 1 when it tail-branched into DeleteObject. */
static int Gar_AniFire(uint8_t *o) {
    if ((v_framebyte & 0x07) == 0) {                        /* andi.b #7,d0 */
        obFrame(o) = (uint8_t)(obFrame(o) ^ 0x01);         /* bchg #0 */
    }

    SpeedToPos(o);

    /* btst #0,obStatus(a0) / bne.s .isRight : bit set (moving right) checks
       the right wall, bit clear (moving left) checks the left wall. */
    if (obStatus(o) & 0x01) {                               /* .isRight */
        if (PushB_ObjHitWallRight(o, 8) < 0) return 1;      /* tst.w d1 / bmi */
    } else {                                                /* .isLeft */
        if (PushB_ObjHitWallLeft(o, -8) < 0) return 1;
    }
    return 0;
}

/* Object $62 dispatcher */
static void Gargoyle_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int deleted = 0;

    switch (obRoutine(o)) {
        case 0x00: Gar_Main(o); break;
        case 0x02: Gar_MakeFire(o); break;
        case 0x04:                                     /* falls into Gar_AniFire */
            obRoutine(o) += 2;
            obHeight(o) = 16 / 2;
            obWidth(o) = 16 / 2;
            obMap(o) = (uint32_t)(uintptr_t)Map_Gar;
            obGfx(o) = (uint16_t)ArtTile_LZ_Gargoyle;
            obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
            obPriority(o) = 4;
            obColType(o) = (uint8_t)(col_8x8 | col_hurt);
            obActWid(o) = 16 / 2;
            obFrame(o) = 2;
            obY(o) = (int16_t)(obY(o) + 8);
            obVelX(o) = 0x200;
            if (!(obStatus(o) & 0x01)) obVelX(o) = (int16_t)-0x200;
            Sound_Queue(sfx_Fireball, false);
            deleted = Gar_AniFire(o);
            break;
        case 0x06: deleted = Gar_AniFire(o); break;
    }

    /* FixBugs=0: the `bmi.w DeleteObject` in Gar_AniFire branches into a
       routine that ends in rts, so control comes back here and would still
       fall into RememberState with a blanked slot.  Model it explicitly:
       the object is deleted and the display pass is skipped. */
    if (deleted) {
        DeleteObject(o);
        return;
    }
    RememberState(o);
}

/* ---------------------------------------------------------------------------
   Object $65 - LZ Waterfalls
   disasm/_incObj/65 LZ Waterfalls.asm
   --------------------------------------------------------------------------- */
static void WFall_Animate(uint8_t *o) {
    AnimateSprite(o, Ani_WFall);
}

static void WFall_Display(uint8_t *o) {
    RememberState(o);
}

static void WFall_OnWater(uint8_t *o) {
    int16_t d0 = (int16_t)(v_waterpos1 - 16);
    obY(o) = d0;
    WFall_Animate(o);                                       /* bra.s */
    WFall_Display(o);
}

static void WFall_Priority(uint8_t *o) {
    obGfx(o) = (uint16_t)(obGfx(o) & ~0x8000);  /* bclr #7 on the word */
    if (RAM_BYTE(v_lvllayout_fg + (layout_row * 2) + 6) != 7) {
        WFall_Animate(o);
        WFall_Display(o);
        return;
    }
    obGfx(o) = (uint16_t)(obGfx(o) | 0x8000);    /* bset #7 on the word */
    WFall_Animate(o);
    WFall_Display(o);
}

/* Object $65 dispatcher */
static void Waterfall_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {
        case 0x00:
            obRoutine(o) += 4;                         /* -> WFall_Display */
            obMap(o) = (uint32_t)(uintptr_t)Map_WFall;
            obGfx(o) = (uint16_t)(ArtTile_LZ_Splash | Tile_Pal3);
            obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
            obActWid(o) = 48 / 2;
            obPriority(o) = 1;

            uint8_t d0 = obSubtype(o);
            if (d0 & 0x80) obGfx(o) = (uint16_t)(obGfx(o) | 0x8000);

            d0 = (uint8_t)(d0 & 0x0F);
            obFrame(o) = d0;

            if (d0 != 9) { WFall_Display(o); break; }  /* cmpi.b #9 / bne */
            obPriority(o) = 0;
            obRoutine(o) = (uint8_t)(obRoutine(o) - 2); /* -> WFall_Animate */
            if (obSubtype(o) & (1 << 6)) obRoutine(o) = 6;
            if (obSubtype(o) & (1 << 5)) obRoutine(o) = 8;
            WFall_Animate(o);
            WFall_Display(o);
            break;
        case 0x02: WFall_Animate(o); WFall_Display(o); break;
        case 0x04: WFall_Display(o); break;
        case 0x06: WFall_OnWater(o); break;
        case 0x08: WFall_Priority(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $60 - Badnik - Orbinaut (LZ, SLZ, SBZ)
   disasm/_incObj/60 Badnik - Orbinaut.asm

   orb_circledir: equ objoff_36   circling direction for spikeballs (1 / -1)
   orb_balldata:  equ objoff_37   ball data array (0: number of not-fired
                                  spikeballs // 1-4: RAM indices for spikeballs)
   orb_parent:    equ objoff_3C   address of parent Orbinaut object
   --------------------------------------------------------------------------- */
#define orb_circledir(o) (*(uint8_t *)((uint8_t *)(o) + 0x36))  /* objoff_36 */
#define orb_ballcount(o) (*(uint8_t *)((uint8_t *)(o) + 0x37))  /* orb_balldata+0 */
#define orb_ballindex(o, i) (*(uint8_t *)((uint8_t *)(o) + 0x38 + (i))) /* orb_balldata+1..4 */
#define orb_parent(o)  (*(uint32_t *)((uint8_t *)(o) + 0x3C))  /* objoff_3C */

/* Orb_DisplayNoMove / .deleteWithSpikeballs — disasm 60 128-152 */
static void Orb_DisplayNoMove(uint8_t *o) {
    if (!OutOfRange(o, -1, 0)) {                       /* out_of_range.w .deleteWithSpikeballs */
        DisplaySprite(o);                        /* bra.w DisplaySprite */
        return;
    }

    /* .deleteWithSpikeballs: custom RememberState that also nukes the
       (non-fired) orbiting spikeballs. */
    uint8_t d0 = obRespawnNo(o);
    if (d0 != 0) {                               /* beq.s .deleteBalls */
        RAM_BYTE(v_objstate + 2 + d0) &= ~0x80;  /* bclr #7,2(a2,d0.w) */
    }

    /* .deleteBalls */
    uint8_t d2 = (uint8_t)(orb_ballcount(o) - 1); /* subq.w #1,d2 */
    if (!(d2 & 0x80)) {                          /* bcs.s .deleteParent (d2 = $FF) */
        /* .loopDeleteBalls */
        do {
            d0 = orb_ballindex(o, 0);             /* move.b (a2)+,d0 */
            uint8_t *a1 = (uint8_t *)Object_GetSlot(d0); /* lsl/addi -> v_objspace+d0*$40 */
            DeleteObject(a1);                    /* bsr.w DeleteChild (same as DeleteObject) */
            for (int i = 0; i < 3; i++) {        /* shift the (a2)+ cursor forward */
                orb_ballindex(o, i) = orb_ballindex(o, i + 1);
            }
        } while (--d2 != 0xFF);                   /* dbf d2,.loopDeleteBalls */
    }

    /* .deleteParent */
    DeleteObject(o);
}

/* Orb_Main — routine 0, disasm 60 23-95 */
static void Orb_Main(uint8_t *o) {
    obMap(o) = (uint32_t)(uintptr_t)Map_Orb;      /* set mappings */

    obGfx(o) = (uint16_t)ArtTile_SBZ_Orbinaut;   /* SBZ-specific art tile */
    if (v_zone != id_SBZ) {                      /* cmpi.b #id_SBZ,(v_zone).w */
        obGfx(o) = (uint16_t)(ArtTile_SLZ_Orbinaut | Tile_Pal2);
        if (v_zone == id_LZ) {                   /* cmpi.b #id_LZ,(v_zone).w */
            obGfx(o) = (uint16_t)ArtTile_LZ_Orbinaut;
        }
    }
    obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
    obPriority(o) = 4;
    obColType(o) = (uint8_t)(col_16x16 | col_badnik);
    obActWid(o) = 24 / 2;

    uint8_t d2 = 0;                              /* d2 = angle offsets, 0 degrees */
    uint8_t d1 = 4 - 1;                          /* moveq #4-1,d1: 4 spikeballs */
    orb_ballcount(o) = 0;

    /* .loopSpikeBalls */
    do {
        uint8_t *a1 = (uint8_t *)FindNextFreeObj(o); /* bsr.w FindNextFreeObj */
        if (a1 == NULL) break;                   /* bne.s .finishSpikeBalls (RAM full) */
        orb_ballcount(o) = (uint8_t)(orb_ballcount(o) + 1);
        orb_ballindex(o, orb_ballcount(o) - 1) = (uint8_t)Object_GetIndex(a1);

        obID(a1) = obID(o);
        obRoutine(a1) = 6;                       /* Orb_CircleSpikeball */
        obMap(a1) = obMap(o);
        obGfx(a1) = obGfx(o);
        obRender(a1) = (uint8_t)(obRender(a1) | sprite_cam_field);
        obPriority(a1) = 4;
        obActWid(a1) = 16 / 2;
        obFrame(a1) = 3;                         /* spikeball frame */
        obColType(a1) = (uint8_t)(col_8x8 | col_hurt);
        obAngle(a1) = d2;
        d2 = (uint8_t)(d2 + 0x40);               /* +90 degrees */
        orb_parent(a1) = (uint32_t)Object_GetIndex(o);
    } while (--d1 != 0xFF);                       /* dbf d1,.loopSpikeBalls */

    /* .finishSpikeBalls */
    int16_t d0 = 1;                              /* circle clockwise by default */
    if (obStatus(o) & 0x01) d0 = (int16_t)-1;     /* btst #0: facing left -> ccw */
    orb_circledir(o) = (uint8_t)d0;

    obRoutine(o) = obSubtype(o);                 /* use subtype as base routine */
    obRoutine(o) = (uint8_t)(obRoutine(o) + 2);  /* -> Orb_CheckSonic / Orb_DisplayAndMove */

    obVelX(o) = (int16_t)-0x40;                  /* move left */
    if (obStatus(o) & 0x01) obVelX(o) = 0x40;    /* btst #0: facing left -> right */
}

/* Orb_CheckSonic — routine 2, disasm 60 97-122 */
static void Orb_CheckSonic(uint8_t *o) {
    uint8_t *player = RAM_ADDR(v_player);
    int16_t d0 = (int16_t)(obX(player) - obX(o));
    if ((uint16_t)obX(player) < (uint16_t)obX(o)) d0 = (int16_t)-d0; /* bhs.s .chkX */
    if ((uint16_t)d0 >= 160) goto animate;       /* cmpi.w #160,d0 / bhs */

    d0 = (int16_t)(obY(player) - obY(o));
    if ((uint16_t)obY(player) < (uint16_t)obY(o)) d0 = (int16_t)-d0; /* bhs.s .chkY */
    if ((uint16_t)d0 >= 80) goto animate;        /* cmpi.w #80,d0 / bhs */

    if (v_debuguse) goto animate;                 /* tst.w (v_debuguse).w / bne */

    obAnim(o) = 1;                               /* "angry" animation */

animate:
    AnimateSprite(o, Ani_Orb);
    Orb_DisplayNoMove(o);                        /* bra.w Orb_DisplayNoMove */
}

/* Orb_DisplayAndMove / Orb_CircleSpikeball / Orb_FiredSpikeball */
static void Orb_DisplayAndMove(uint8_t *o) {
    SpeedToPos(o);
    Orb_DisplayNoMove(o);
}

static void Orb_CircleSpikeball(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)orb_parent(o)); /* movea.l orb_parent */

    if (obID(a1) != id_Orbinaut) {                /* has the parent been deleted? */
        DeleteObject(o);                          /* bne.w DeleteObject */
        return;
    }

    if (obFrame(a1) == 2 && obAngle(o) == 0x40) { /* angry? and directly underneath? */
        obRoutine(o) = (uint8_t)(obRoutine(o) + 2);      /* -> Orb_FiredSpikeball */
        orb_ballcount(a1) = (uint8_t)(orb_ballcount(a1) - 1);
        if (orb_ballcount(a1) == 0) {                     /* bne.s .fireSpikeball */
            obRoutine(a1) = (uint8_t)(obRoutine(a1) + 2); /* -> Orb_DisplayAndMove */
        }

        /* .fireSpikeball */
        obVelX(o) = (int16_t)-0x200;               /* shoot left */
        if (obStatus(a1) & 0x01) obVelX(o) = 0x200; /* btst #0: parent faces right */

        DisplaySprite(o);                         /* bra.w DisplaySprite */
        return;
    }

    /* .circleBalls */
    int16_t s0 = 0, s1 = 0;
    CalcSine(obAngle(o), &s0, &s1);
    s1 = (int16_t)(s1 >> 4);                      /* asr.w #4,d1: radius */
    obX(o) = (int16_t)(s1 + obX(a1));
    s0 = (int16_t)(s0 >> 4);                      /* asr.w #4,d0: radius */
    obY(o) = (int16_t)(s0 + obY(a1));

    obAngle(o) = (uint8_t)(obAngle(o) + orb_circledir(a1)); /* add.b d0,obAngle(a0) */
    DisplaySprite(o);
}

static void Orb_FiredSpikeball(uint8_t *o) {
    SpeedToPos(o);
    if (!(obRender(o) & 0x80)) {                  /* tst.b obRender(a0) / bpl */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
}

/* Object $60 dispatcher */
static void Orbinaut_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                      /* Orb_Index: 0/2/4/6/8 */
        case 0x00: Orb_Main(o); break;
        case 0x02: Orb_CheckSonic(o); break;
        case 0x04: Orb_DisplayAndMove(o); break;
        case 0x06: Orb_CircleSpikeball(o); break;
        case 0x08: Orb_FiredSpikeball(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $61 - multi-variant blocks (LZ)
   disasm/_incObj/61 LZ Blocks.asm

   lblk_origY:     equ objoff_30   original y-axis position
   lblk_origX:     equ objoff_34   original x-axis position
   lblk_time:      equ objoff_36   time delay for block movement
   lblk_untouched: equ objoff_38   flag block as untouched
   lblk_nudge:     equ objoff_3E   nudge Y-offset while Sonic is standing on block
   lblk_touchtype: equ objoff_3F   Sonic's touch response from SolidObject
   --------------------------------------------------------------------------- */
#define lblk_origY(o)     (*(int16_t *)((uint8_t *)(o) + 0x30))  /* objoff_30 */
#define lblk_origX(o)     (*(int16_t *)((uint8_t *)(o) + 0x34))  /* objoff_34 */
#define lblk_time(o)      (*(int16_t *)((uint8_t *)(o) + 0x36))  /* objoff_36 */
#define lblk_untouched(o) (*(uint8_t *)((uint8_t *)(o) + 0x38))  /* objoff_38 */
#define lblk_nudge(o)     (*(uint8_t *)((uint8_t *)(o) + 0x3E))  /* objoff_3E */
#define lblk_touchtype(o) (*(uint8_t *)((uint8_t *)(o) + 0x3F))  /* objoff_3F */

/* LBlk_Var: width, height (half-pixels) — disasm 61 23-28 */
static const uint8_t LBlk_Var[8] = { 32 / 2, 32 / 2,   /* block that sinks when stood on */
                                     64 / 2, 24 / 2,   /* platform that rises when stood on */
                                     32 / 2, 32 / 2,   /* cork block that floats on water */
                                     32 / 2, 32 / 2 }; /* generic solid block */

static void LBlk_Sink(uint8_t *o) {
    SpeedToPos(o);
    obVelY(o) = (int16_t)(obVelY(o) + 8);          /* make block fall faster */

    int16_t d1 = 0, d3 = 0;
    ObjFloorDist(o, &d1, &d3);
    if (!(d1 < 0)) return;                         /* tst.w d1 / bpl.w .return */
    d1 = (int16_t)(d1 + 1);                         /* manually adjust by 1px down */
    obY(o) = (int16_t)(obY(o) + d1);
    obVelY(o) = 0;
    obSubtype(o) = 0;                              /* -> LBlk_Stationary */
}

static void LBlk_Rise(uint8_t *o) {
    SpeedToPos(o);
    obVelY(o) = (int16_t)(obVelY(o) - 8);          /* make block rise faster */

    int16_t d1 = LZ_ObjHitCeiling(o);
    if (!(d1 < 0)) return;                         /* tst.w d1 / bpl.w .return */
    obY(o) = (int16_t)(obY(o) - d1);
    obVelY(o) = 0;
    obSubtype(o) = 0;                              /* -> LBlk_Stationary */
}

static void LBlk_CheckStand(uint8_t *o) {
    if (lblk_time(o) != 0) {                       /* tst.w lblk_time / bne.s */
        /* .delayAndAdvance */
        lblk_time(o) = (int16_t)(lblk_time(o) - 1);
        if (lblk_time(o) != 0) return;
        obSubtype(o) = (uint8_t)(obSubtype(o) + 1); /* -> LBlk_Sink or LBlk_Rise */
        lblk_untouched(o) = 0;
        return;
    }
    if (!(obStatus(o) & (1 << 3))) return;         /* btst #3: Sonic standing on it? */
    lblk_time(o) = 30;                             /* wait for half a second */
}

static void LBlk_SideSink(uint8_t *o) {
    if (lblk_touchtype(o) != 1) return;            /* cmpi.b #1,lblk_touchtype */
    obSubtype(o) = (uint8_t)(obSubtype(o) + 1);    /* -> LBlk_Sink */
    lblk_untouched(o) = 0;
}

/* LBlk_OnWater — cork block that follows the water surface. disasm 61 161-193
   sub.w obY(a0),d0 sets carry on borrow, so bcc (= no borrow, waterY >=u blockY,
   block at or above the water) is the test for bit 15 being CLEAR. */
static void LBlk_OnWater(uint8_t *o) {
    int16_t d0 = (int16_t)(v_waterpos1 - obY(o));
    if (d0 == 0) return;                           /* beq.s .return2 */
    if ((uint16_t)d0 < 0x8000) {                   /* bcc.s .corkSink (block above water) */
        /* .corkSink: at most 2px above the water level */
        if (d0 > 2) d0 = 2;                        /* cmpi.w #2,d0 / ble.s .sink */
        obY(o) = (int16_t)(obY(o) + d0);
        int16_t d1 = 0, d3 = 0;
        ObjFloorDist(o, &d1, &d3);
        if (!(d1 < 0)) return;                     /* bpl.w .return2 */
        obY(o) = (int16_t)(obY(o) + d1 + 1);
        return;
    }
    /* .corkRise: at most 2px below the water level */
    if (!(d0 >= -2)) d0 = -2;                      /* cmpi.w #-2,d0 / bge.s .rise */
    obY(o) = (int16_t)(obY(o) + d0);
    int16_t d1 = LZ_ObjHitCeiling(o);
    if (!(d1 < 0)) return;                         /* bpl.w .return */
    obY(o) = (int16_t)(obY(o) - d1);
}

/* LBlk_Nudge — disasm 61 195-225. Nudges the block down while Sonic stands on it. */
static void LBlk_Nudge(uint8_t *o) {
    if (!lblk_untouched(o)) return;                /* tst.b lblk_untouched / beq.s */
    if (!(obStatus(o) & (1 << 3))) {               /* btst #3 / bne.s .nudgeDown */
        if (lblk_nudge(o) == 0) return;            /* tst.b lblk_nudge / beq.s */
        lblk_nudge(o) = (uint8_t)(lblk_nudge(o) - 4);
    } else {
        /* .nudgeDown */
        if (lblk_nudge(o) == 0x40) return;         /* cmpi.b #$40 / beq.s */
        lblk_nudge(o) = (uint8_t)(lblk_nudge(o) + 4);
    }

    /* .updateYPosition */
    int16_t s0 = 0, s1 = 0;
    CalcSine(lblk_nudge(o), &s0, &s1);             /* d0 = sine */
    int32_t d0 = (int32_t)s0 * 0x400;              /* muls.w #$400,d0 */
    obY(o) = (int16_t)(lblk_origY(o) + (int16_t)(d0 >> 16)); /* swap d0 / add.w origY */
}

/* LBlk_Main — routine 0, disasm 61 30-57 */
static void LBlk_Main(uint8_t *o) {
    obRoutine(o) = (uint8_t)(obRoutine(o) + 2);    /* -> LBlk_Action */
    obMap(o) = (uint32_t)(uintptr_t)Map_LBlock;
    obGfx(o) = (uint16_t)(ArtTile_LZ_Blocks | Tile_Pal3);
    obRender(o) = (uint8_t)sprite_cam_field;        /* move.b (not ori.b) */
    obPriority(o) = 3;

    uint16_t d0 = (uint16_t)((uint16_t)obSubtype(o) >> 3); /* lsr.w #3,d0 */
    d0 &= 0xE;                                     /* andi.w #$E,d0 */
    obActWid(o) = LBlk_Var[d0];                    /* width for block */
    obHeight(o) = LBlk_Var[d0 + 1];               /* height for block */
    obFrame(o) = (uint8_t)(d0 >> 1);               /* frame ID (0-3) */

    lblk_origX(o) = obX(o);
    lblk_origY(o) = obY(o);

    uint8_t d0b = (uint8_t)(obSubtype(o) & 0x0F);  /* read only the lower digit */
    if (d0b == 0) return;                          /* beq.s LBlk_Action */
    if (d0b == 7) return;                          /* cmpi.b #7 / beq.s (LZ1 secret ptfm) */
    lblk_untouched(o) = 1;
}

/* LBlk_Action — routine 2, disasm 61 59-91 */
static void LBlk_Action(uint8_t *o) {
    int16_t prev_x = obX(o);                       /* move.w obX(a0),-(sp) */

    switch (obSubtype(o) & 0x0F) {                 /* LBlk_Types jump table */
        case 0x0: break;                           /* LBlk_Stationary */
        case 1: case 3: LBlk_CheckStand(o); break;
        case 2: case 6: LBlk_Sink(o);      break;
        case 4:         LBlk_Rise(o);      break;
        case 5:         LBlk_SideSink(o);  break;
        case 7:         LBlk_OnWater(o);   break;
    }

    if (obRender(o) & 0x80) {                      /* tst.b obRender(a0) / bpl.s .chkdel */
        int16_t d1 = (int16_t)(obActWid(o) + sonic_solid_width);
        int16_t d2 = (int16_t)obHeight(o);
        int16_t d3 = (int16_t)(d2 + 1);            /* +1px for the stood-on check */
        lblk_touchtype(o) = (uint8_t)SolidObject(o, d1, d2, d3, prev_x, NULL, NULL);
        LBlk_Nudge(o);
    }

    /* .chkdel */
    if (OutOfRange(o, lblk_origX(o), 0)) {               /* out_of_range.w DeleteObject */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
}

/* Object $61 dispatcher */
static void LabyrinthBlock_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                        /* LBlk_Index: 0/2 */
        case 0x00: LBlk_Main(o); break;
        case 0x02: LBlk_Action(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $63 - platforms on a conveyor belt (LZ)
   disasm/_incObj/63 LZ Conveyor.asm

   lcon_groupid:  equ objoff_2F   copy of obSubtype from the initial group spawner
   lcon_baseX:    equ objoff_30   base X-position for entire group
   lcon_nextX:    equ objoff_34   next target X-position for platform
   lcon_nextY:    equ objoff_36   next target Y-position for platform
   lcon_posindex: equ objoff_38   current index in target positioning data
   lcon_count:    equ objoff_39   number of entries in group (multiplied by 4)
   lcon_increment:equ objoff_3A   value to increment to next entry (+4 or -4)
   lcon_reversed: equ objoff_3B   flag set if conveyor direction is currently reversed
   lcon_targetdata:equ objoff_3C  pointer to corner data for group
   --------------------------------------------------------------------------- */
#define lcon_groupid(o)   (*(uint8_t *)((uint8_t *)(o) + 0x2F))  /* objoff_2F */
#define lcon_baseX(o)     (*(int16_t *)((uint8_t *)(o) + 0x30))  /* objoff_30 */
#define lcon_nextX(o)     (*(int16_t *)((uint8_t *)(o) + 0x34))  /* objoff_34 */
#define lcon_nextY(o)     (*(int16_t *)((uint8_t *)(o) + 0x36))  /* objoff_36 */
#define lcon_posindex(o)  (*(uint8_t *)((uint8_t *)(o) + 0x38))  /* objoff_38 */
#define lcon_count(o)     (*(uint8_t *)((uint8_t *)(o) + 0x39))  /* objoff_39 */
#define lcon_increment(o) (*(uint8_t *)((uint8_t *)(o) + 0x3A))  /* objoff_3A */
#define lcon_reversed(o)  (*(uint8_t *)((uint8_t *)(o) + 0x3B))  /* objoff_3B */
#define lcon_targetdata(o) (*(uint32_t *)((uint8_t *)(o) + 0x3C)) /* objoff_3C */

/* LCon_Data: conveyor belt corner target coordinate definitions.
   Each group corresponds to the upper nybble of the platform subtype.
   Format: dc.w number of entries times 4 / dc.w base X position / dc.w entries...
   Word 0 = entry count * 4, word 1 = base X, then (X, Y) pairs. */
static const int16_t LCon_CornerData[6][14] = {
    /* .group0 : baseX $1070, baseY $2F0, 6 entries */
    { 6 * 4, 0x1070,
      0x1078, 0x021A, 0x10BE, 0x0260, 0x10BE, 0x0393,
      0x108C, 0x03C5, 0x1022, 0x0390, 0x1022, 0x0244 },
    /* .group1 : baseX $1280, baseY $377, 5 entries */
    { 5 * 4, 0x1280,
      0x127E, 0x0280, 0x12CE, 0x02D0, 0x12CE, 0x046E,
      0x1232, 0x0420, 0x1232, 0x02CC },
    /* .group2 : baseX $0D68, baseY $530, 4 entries */
    { 4 * 4, 0x0D68,
      0x0D22, 0x0482, 0x0D22, 0x05DE, 0x0DAE, 0x05DE,
      0x0DAE, 0x0482 },
    /* .group3 : baseX $0DA0, baseY $440, 4 entries */
    { 4 * 4, 0x0DA0,
      0x0D62, 0x03A2, 0x0DEE, 0x03A2, 0x0DEE, 0x04DE,
      0x0D62, 0x04DE },
    /* .group4 : baseX $0D00, baseY $310, 5 entries */
    { 5 * 4, 0x0D00,
      0x0CAC, 0x0242, 0x0DDE, 0x0242, 0x0DDE, 0x03DE,
      0x0C52, 0x03DE, 0x0C52, 0x029C },
    /* .group5 : baseX $1300, baseY $264, 4 entries */
    { 4 * 4, 0x1300,
      0x1252, 0x020A, 0x13DE, 0x020A, 0x13DE, 0x02BE,
      0x1252, 0x02BE },
};

/* ObjPosLZPlatform_Index (offset 0x38 in the ASM's ObjPos_Index table) maps
   group IDs 0-5 to the custom platform positioning data in objpos/platforms/. */
/* ObjPosLZPlatform_Index (offset 0x38 in the ASM's ObjPos_Index table) maps
   group IDs 0-5 to the custom platform positioning data in objpos/platforms/. */
static const uint8_t *LZ_ConveyorPlatformData(uint8_t group) {
    const uint8_t *p = NULL;
    switch (group) {
        case 0: p = ObjPos_LZ1pf1; break;
        case 1: p = ObjPos_LZ1pf2; break;
        case 2: p = ObjPos_LZ2pf1; break;
        case 3: p = ObjPos_LZ2pf2; break;
        case 4: p = ObjPos_LZ3pf1; break;
        case 5: p = ObjPos_LZ3pf2; break;
        default: p = NULL; break;
    }
    /* [DBG] */
    fprintf(stderr, "[LZ_ConvData] group=%d -> ptr=%p\n",
            group, (const void *)p);
    if (p) {
        fprintf(stderr, "  bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        /* BE16S/16 sobre los bytes crudos */
        uint16_t w0 = ((uint16_t)p[0] << 8) | p[1];
        uint16_t w1 = ((uint16_t)p[2] << 8) | p[3];
        uint16_t w2 = ((uint16_t)p[4] << 8) | p[5];
        uint16_t w3 = ((uint16_t)p[6] << 8) | p[7];
        fprintf(stderr, "  BE16: w0(count)=$%04X w1(X0)=$%04X w2(Y0)=$%04X w3(sub0)=$%04X\n",
                w0, w1, w2, w3);
        /* LE para comparar */
        uint16_t l0 = ((uint16_t)p[1] << 8) | p[0];
        uint16_t l1 = ((uint16_t)p[3] << 8) | p[2];
        fprintf(stderr, "  LE16: w0=$%04X w1=$%04X\n", l0, l1);
    }
    /* [/DBG] */
    return p;
}

/* Corner entry lookup: the ASM indexes (a1, d1.w) with a1 = first entry and
   d1 = byte offset into LCon_Data, so word 2 + d1/2 of the group holds X. */
static void LCon_TargetPos(uint8_t *o, uint8_t index, int16_t *out_x, int16_t *out_y) {
    const int16_t *grp = LCon_CornerData[lcon_targetdata(o) & 0xFF];
    *out_x = grp[2 + index / 2];
    *out_y = grp[3 + index / 2];
}

/* divs.w: signed 32/16 division with 68000 semantics (truncates toward zero and
   the remainder takes the dividend's sign, which is what C99 / and % already do).
   On overflow the M68000 raises the V flag and leaves $8000 in the quotient with
   the dividend in the remainder; that is reproduced here. The LZ corner data keeps
   both axis deltas in the same ballpark, so the overflow never triggers in play. */
static void LZ_DivSW(int32_t dividend, int16_t divisor,
                     int16_t *out_quot, int16_t *out_rem) {
    int32_t q = dividend / (int32_t)divisor;
    if (q > 32767 || q < -32768) {                 /* overflow flag set */
        *out_quot = (int16_t)0x8000;
        *out_rem = (int16_t)dividend;
        return;
    }
    *out_quot = (int16_t)q;
    *out_rem = (int16_t)(dividend % (int32_t)divisor);
}

/* LCon_ChangeDir — disasm 63 269-333. Also used by object 63 (SBZ). */
static void LCon_ChangeDir(uint8_t *o) {
    int16_t d0 = (int16_t)(obX(o) - lcon_nextX(o));
    int16_t d2 = (int16_t)-0x100;                  /* move left */
    if ((uint16_t)obX(o) < (uint16_t)lcon_nextX(o)) { /* bcc.s .calcYDirection */
        d0 = (int16_t)-d0;                          /* keep X-difference positive */
        d2 = (int16_t)-d2;                          /* move right instead */
    }

    int16_t d1 = (int16_t)(obY(o) - lcon_nextY(o));
    int16_t d3 = (int16_t)-0x100;                  /* move up */
    if ((uint16_t)obY(o) < (uint16_t)lcon_nextY(o)) { /* bcc.s .checkGreaterDistance */
        d1 = (int16_t)-d1;                          /* keep Y-difference positive */
        d3 = (int16_t)-d3;                          /* move down instead */
    }

    int16_t quot = 0, rem = 0;
    if ((uint16_t)d1 < (uint16_t)d0) {              /* cmp.w d0,d1 / bcs.s .moveVertical */
        /* .moveVertical */
        d1 = (int16_t)(obY(o) - lcon_nextY(o));      /* signed Y-difference */
        if (d1 != 0) {                              /* beq.s .setSpeedsVertical */
            LZ_DivSW((int32_t)d1 * 0x100, d0, &quot, &rem); /* ext.l / asl.l #8 */
            quot = (int16_t)-quot;                   /* neg.w: move towards the target */
        }
        obVelY(o) = quot;
        obVelX(o) = d2;
        obSubpixelY(o) = rem;                       /* swap d1 -> remainder */
        obSubpixelX(o) = 0;                         /* clr.w obSubpixelY(a0) */
        return;
    }

    /* horizontal movement */
    d0 = (int16_t)(obX(o) - lcon_nextX(o));         /* signed X-difference */
    if (d0 != 0) {                                  /* beq.s .setSpeedsHorizontal */
        LZ_DivSW((int32_t)d0 * 0x100, d1, &quot, &rem);
        quot = (int16_t)-quot;
    }
    obVelX(o) = quot;
    obVelY(o) = d3;
    obSubpixelX(o) = rem;                           /* swap d0 -> remainder */
    obSubpixelY(o) = 0;                             /* clr.w obSubpixelY(a0) */
}

/* LCon_Platform_Update — disasm 63 224-268 */
static void LCon_Platform_Update(uint8_t *o) {
    if (RAM_BYTE(f_switch + 0x0E) != 0) {          /* tst.b (f_switch+$E).w */
        if (!lcon_reversed(o)) {                    /* tst.b lcon_reversed / bne.s */
            lcon_reversed(o) = 1;
            f_conveyrev = 1;                        /* (f_conveyrev).w */
            lcon_increment(o) = (uint8_t)(0u - lcon_increment(o)); /* neg.b */
            goto nextCorner;
        }
    }

    /* .checkAtCorner */
    if (obX(o) != lcon_nextX(o)) goto updatePos;     /* cmp.w lcon_nextX,d0 / bne.s */
    if (obY(o) != lcon_nextY(o)) goto updatePos;

nextCorner:
    {
        uint8_t d1 = (uint8_t)(lcon_posindex(o) + lcon_increment(o));
        if ((uint8_t)d1 >= lcon_count(o)) {         /* cmp.b lcon_count,d1 / bcs.s */
            uint8_t d0 = d1;                        /* backup for reversal check */
            d1 = 0;
            if (!(d0 & 0x80)) goto setTargetIndex;  /* tst.b d0 / bpl.s (not reversed) */
            d1 = (uint8_t)(lcon_count(o) - 4);      /* last entry, indices are 0-based */
        }
    setTargetIndex:
        lcon_posindex(o) = d1;
        LCon_TargetPos(o, d1, &lcon_nextX(o), &lcon_nextY(o));
        LCon_ChangeDir(o);
    }

updatePos:
    SpeedToPos(o);
}

/* LCon_Main_Platform — disasm 63 71-127 */
static void LCon_Platform(uint8_t *o);
static void LCon_Main_Platform(uint8_t *o) {
    obFrame(o) = 4;                                 /* "platform" frame ID */

    uint16_t d0 = (uint16_t)obSubtype(o);
    uint16_t d1 = d0;
    d0 = (uint16_t)(d0 >> 3);                      /* upper digit, x2 */
    d0 &= 0x1E;                                     /* limit to sane values */
    uint8_t group = (uint8_t)(d0 >> 1);
    if (group > 5) return;                          /* ASM would read stray data */

    lcon_targetdata(o) = group;
    /* `move.w (a2)+,lcon_posindex(a0)` is a big-endian word store, so the
       header word $XX00 leaves lcon_posindex ($38) = 0 and lcon_count ($39) = XX. */
    lcon_count(o) = (uint8_t)LCon_CornerData[group][0]; /* -> $39 */
    lcon_baseX(o) = LCon_CornerData[group][1];

    d1 = (uint16_t)(d1 & 0xF);
    d1 = (uint16_t)(d1 << 2);                       /* 4 bytes per entry */
    lcon_posindex(o) = (uint8_t)d1;                 /* initial target index */
    lcon_increment(o) = 4;                          /* increment forwards */

    if (f_conveyrev) {                              /* tst.b (f_conveyrev).w */
        lcon_reversed(o) = 1;
        lcon_increment(o) = (uint8_t)(0u - lcon_increment(o)); /* increment backwards */
        uint8_t d1b = (uint8_t)(lcon_posindex(o) + lcon_increment(o));
        if ((uint8_t)d1b >= lcon_count(o)) {         /* cmp.b lcon_count,d1 / bcs.s */
            uint8_t d0b = d1b;
            d1b = 0;
            if (!(d0b & 0x80)) {                     /* tst.b d0 / bpl.s */
                lcon_posindex(o) = d1b;
                goto finishPlatform;
            }
            d1b = (uint8_t)(lcon_count(o) - 4);
            lcon_posindex(o) = d1b;
        } else {
            lcon_posindex(o) = d1b;
        }
    }

finishPlatform:
    LCon_TargetPos(o, lcon_posindex(o), &lcon_nextX(o), &lcon_nextY(o));
    LCon_ChangeDir(o);
    LCon_Platform(o);                               /* bra.w LCon_Platform */
}

/* LCon_Main_Spawner — disasm 63 129-186. Returns 1 when the caller must skip
   its own out_of_range/display tail (the ASM's `addq.l #4,sp`). */
/* LCon_Main_Spawner — disasm 63 129-186. Returns 1 when the caller must skip
   its own out_of_range/display tail (the ASM's `addq.l #4,sp`). */
static int LCon_Main_Spawner(uint8_t *o, uint8_t subtype) {
    uint8_t group = (uint8_t)(subtype & 0x7F);

    /* [DBG] */
    fprintf(stderr, "[LCon_Spawner] slot=%d subtype=$%02X group=%d\n",
            Object_GetIndex(o), subtype, group);
    /* [/DBG] */

    lcon_groupid(o) = subtype;                      /* remember parent group ID */

    uint8_t flag = RAM_BYTE(v_obj63 + group);

    /* [DBG] */
    fprintf(stderr, "  v_obj63[%d] before = $%02X\n", group, flag);
    /* [/DBG] */

    RAM_BYTE(v_obj63 + group) = (uint8_t)(flag | 0x01); /* bset #0 */
    if (flag & 0x01) {                              /* bne.w DeleteObject */
        /* [DBG] */
        fprintf(stderr, "  flag already set -> abort (no platforms)\n");
        /* [/DBG] */
        DeleteObject(o);
        return 0;
    }

    /* .spawn: read the custom platform positioning data for this group. */
    const uint8_t *a2 = LZ_ConveyorPlatformData(group);
    if (a2 == NULL) {
        /* [DBG] */
        fprintf(stderr, "  platform data NULL -> abort (no platforms)\n");
        /* [/DBG] */
        return 1;
    }
    int16_t d1 = BE16S(a2);
    a2 += 2;

    /* [DBG] */
    fprintf(stderr, "  count(d1)=%d, first entry: X=$%04X Y=$%04X sub=$%02X\n",
            d1, BE16S(a2), BE16S(a2 + 2), (uint8_t)BE16(a2 + 4));
    /* [/DBG] */

    uint8_t *a1 = o;                                /* first platform reuses this slot */
    int it = 0;
    for (;;) {
        /* [DBG] */
        fprintf(stderr, "  iter %d: a1 slot=%d\n",
                it, a1 ? Object_GetIndex(a1) : -1);
        /* [/DBG] */
        if (a1 != NULL) {                           /* .makePlatform */
            obID(a1) = id_LabyrinthConvey;           /* obRoutine left at 0 on purpose */
            obX(a1) = BE16S(a2);  a2 += 2;
            obY(a1) = BE16S(a2);  a2 += 2;
            obSubtype(a1) = (uint8_t)BE16(a2); a2 += 2;
            /* [DBG] */
            fprintf(stderr, "    created @ slot %d: X=$%04X Y=$%04X sub=$%02X\n",
                    Object_GetIndex(a1), obX(a1), obY(a1), obSubtype(a1));
            /* [/DBG] */
        }
        if (d1 == 0) break;                         /* dbf d1,.loopMakePlatforms */
        d1--;
        a1 = (uint8_t *)FindFreeObj();               /* FixBugs=0: any free slot */
        /* [DBG] */
        if (!a1) fprintf(stderr, "    FindFreeObj FAILED on iter %d\n", it + 1);
        /* [/DBG] */
        it++;
    }
    /* [DBG] */
    fprintf(stderr, "  spawner done, %d platforms created\n", it + 1);
    /* [/DBG] */
    return 1;                                       /* addq.l #4,sp / rts */
}

/* LCon_Wheel — routine 6 (decorative wheel). Returns 1 (skips the tail). */
static int LCon_Wheel(uint8_t *o) {
    if ((v_framecount & 3) == 0) {                  /* andi.w #3,d0 / bne.s .display */
        int8_t d1 = 1;
        if (f_conveyrev) d1 = (int8_t)-d1;          /* go to previous frame instead */
        obFrame(o) = (uint8_t)(obFrame(o) + d1);
        obFrame(o) = (uint8_t)(obFrame(o) & 3);      /* andi.b #3,obFrame(a0) */
    }
    /* .display: addq.l #4,sp ; bra.w RememberState */
    RememberState(o);
    return 1;
}

/* LCon_Platform — routine 2, disasm 63 189-196 */
static void LCon_Platform(uint8_t *o) {
    PlatformObject(o, (int16_t)obActWid(o));
    LCon_Platform_Update(o);
}

/* LCon_OnPlatform — routine 4, disasm 63 198-207 */
static void LCon_OnPlatform(uint8_t *o) {
    int16_t dummy = 0;
    ExitPlatform(o, (int16_t)obActWid(o), &dummy);
    int16_t prev_x = obX(o);
    LCon_Platform_Update(o);
    MvSonicOnPtfm2(o, prev_x);
}

/* LCon_Main — routine 0, disasm 63 52-68. Returns 1 to skip the caller's tail. */
static int LCon_Main(uint8_t *o) {
    uint8_t d0 = obSubtype(o);

    /* [DBG] */
    fprintf(stderr, "[LCon_Main] slot=%d subtype=$%02X routine=%d\n",
            Object_GetIndex(o), d0, obRoutine(o));
    /* [/DBG] */

    if (d0 & 0x80) return LCon_Main_Spawner(o, d0); /* bmi.w LCon_Main_Spawner */

    obRoutine(o) = (uint8_t)(obRoutine(o) + 2);    /* -> LCon_Platform */
    obMap(o) = (uint32_t)(uintptr_t)Map_LConv;
    obGfx(o) = (uint16_t)(ArtTile_LZ_Conveyor_Belt | Tile_Pal3);
    obRender(o) = (uint8_t)(obRender(o) | sprite_cam_field);
    obActWid(o) = 32 / 2;
    obPriority(o) = 4;

    if (obSubtype(o) != 0x7F) {                     /* cmpi.b #$7F / bne.s */
        LCon_Main_Platform(o);
        return 0;                                   /* returns to LabyrinthConvey */
    }
    obRoutine(o) = (uint8_t)(obRoutine(o) + 4);    /* -> LCon_Wheel */
    obGfx(o) = (uint16_t)ArtTile_LZ_Conveyor_Belt; /* palette line 1 */
    obPriority(o) = 1;
    return LCon_Wheel(o);
}

/* Out-of-range distance value, the raw d0 left behind by the out_of_range
   macro (Macros.asm 278-295). $63 needs it for its act-3 special case. */
static int16_t LZ_OutOfRangeValue(int16_t pos) {
    uint16_t d0 = (uint16_t)pos & 0xFF80;
    uint16_t d1 = ((uint16_t)RAM_WORD(0xF700) - 128) & 0xFF80;
    return (int16_t)(d0 - d1);
}

/* Object $63 dispatcher — disasm 63 6-34 */
/* Object $63 dispatcher — disasm 63 6-34 */
static void LabyrinthConvey_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;
    int skip_tail = 0;

    /* [DBG] */
    fprintf(stderr, "[LCon] dispatch slot=%d routine=%d subtype=$%02X\n",
            Object_GetIndex(o), obRoutine(o), obSubtype(o));
    /* [/DBG] */

    switch (obRoutine(o)) {                         /* LCon_Index: 0/2/4/6 */
        case 0x00: skip_tail = LCon_Main(o); break;
        case 0x02: LCon_Platform(o); break;
        case 0x04: LCon_OnPlatform(o); break;
        case 0x06: skip_tail = LCon_Wheel(o); break;
    }
    if (skip_tail) return;

    int16_t d0 = LZ_OutOfRangeValue(lcon_baseX(o));  /* out_of_range.s .outOfRange */
    if ((uint16_t)d0 <= 640) {                      /* cmpi.w #128+320+192 / bhi */
        DisplaySprite(o);
        return;
    }

    /* .outOfRange: keeps LZ3's wide platform group 4 from popping in */
    if (v_act == act3) {                            /* cmpi.b #act3,(v_act).w */
        if ((uint16_t)d0 >= 0xFF80) {               /* cmpi.w #$FF80,d0 / bhs.s */
            DisplaySprite(o);
            return;
        }
    }

    /* .delete */
    uint8_t gid = lcon_groupid(o);

    /* [DBG] */
    fprintf(stderr, "[LCon] OUT OF RANGE slot=%d gid=$%02X d0=$%04X\n",
            Object_GetIndex(o), gid, (uint16_t)d0);
    /* [/DBG] */

    if (!(gid & 0x80)) {                            /* bpl.w DeleteObject */
        DeleteObject(o);
        return;
    }
    gid &= 0x7F;
    RAM_BYTE(v_obj63 + gid) &= (uint8_t)~0x01;      /* bclr #0,(a2,d0.w) */
    DeleteObject(o);
}

/* ---------------------------------------------------------------------------
   Object $64 - air bubbles (LZ)
   disasm/_incObj/64 LZ Air Bubbles.asm

   bub_inhalable:  equ objoff_2E  flag set when bubble is collectable
   bub_origX:      equ objoff_30  original x-axis position
   bub_time:       equ objoff_32  time until next large bubble spawn
   bub_timebase:   equ objoff_33  base time to reset bub_time to after spawn
   bub_minicount:  equ objoff_34  number of smaller bubbles to spawn
   bub_bubbleflag: equ objoff_36  1 = spawning; +$4000 = large spawned; +$8000 = allow
   bub_randomtime: equ objoff_38  randomised time between mini bubble spawns
   bub_typelist:   equ objoff_3C  address of bubble type list
   --------------------------------------------------------------------------- */
#define bub_inhalable(o)  (*(uint8_t *)((uint8_t *)(o) + 0x2E))  /* objoff_2E */
#define bub_origX(o)      (*(int16_t *)((uint8_t *)(o) + 0x30)) /* objoff_30 */
/* bub_time is a BYTE at 0x32 and bub_timebase is the very next BYTE at 0x33:
   the ASM only ever does subq.b/move.b on it, so it must not be a word here
   or every write would clobber bub_timebase. */
#define bub_time(o)       (*(uint8_t *)((uint8_t *)(o) + 0x32))  /* objoff_32 */
#define bub_timebase(o)   (*(uint8_t *)((uint8_t *)(o) + 0x33)) /* objoff_33 */
#define bub_minicount(o)  (*(uint8_t *)((uint8_t *)(o) + 0x34)) /* objoff_34 */
#define bub_bubbleflag(o) (*(uint16_t *)((uint8_t *)(o) + 0x36))/* objoff_36 */
#define bub_randomtime(o) (*(int16_t *)((uint8_t *)(o) + 0x38)) /* objoff_38 */
#define bub_typelist(o)   (*(uint32_t *)((uint8_t *)(o) + 0x3C))/* objoff_3C */

/* Bub_BblTypes: non-inhalable bubbles production sequence (0 = small, 1 = medium).
   The ASM stores a pointer into this list; here the offset is kept instead. */
static const uint8_t Bub_BblTypes[18] = {
    0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0
};

/* Bub_ChkSonic — disasm 64 245-277. Returns 1 when Sonic is in range. */
static int Bub_ChkSonic(uint8_t *o) {
    if ((int8_t)f_playerctrl < 0) return 0;         /* tst.b (f_playerctrl).w / bmi.s */

    uint8_t *a1 = RAM_ADDR(v_player);
    int16_t d0 = obX(a1);
    int16_t d1 = (int16_t)(obX(o) - 16);            /* check left */
    if (!((uint16_t)d1 < (uint16_t)d0)) return 0;   /* cmp.w d0,d1 / bhs.s */
    d1 = (int16_t)(d1 + 16 * 2);                    /* check right */
    if ((uint16_t)d1 < (uint16_t)d0) return 0;      /* cmp.w d0,d1 / blo.s */

    d0 = obY(a1);
    d1 = obY(o);
    if (!((uint16_t)d1 < (uint16_t)d0)) return 0;   /* cmp.w d0,d1 / bhs.s */
    d1 = (int16_t)(d1 + 16);                        /* check bottom */
    if ((uint16_t)d1 < (uint16_t)d0) return 0;      /* cmp.w d0,d1 / blo.s */

    return 1;                                       /* .collectBubble */
}

/* Bub_Bursting — routine 6, disasm 64 143-153 */
static void Bub_Bursting(uint8_t *o) {
    AnimateSprite(o, Ani_Bub);
    if (!(obRender(o) & 0x80)) {                    /* tst.b obRender(a0) / bpl.s */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
}

/* Bub_ChkWater — routine 4, disasm 64 71-140 */
static void Bub_ChkWater(uint8_t *o) {
    int16_t d0 = (int16_t)v_waterpos1;
    if (!((uint16_t)d0 < (uint16_t)obY(o))) {        /* cmp.w obY(a0),d0 / blo.s .wobble */
        /* .burst: still under the surface's sway, so pop the bubble */
        obRoutine(o) = 6;                           /* -> Bub_Bursting */
        obAnim(o) = (uint8_t)(obAnim(o) + 3);
        Bub_Bursting(o);
        return;
    }

    /* .wobble */
    uint8_t angle = obAngle(o);
    obAngle(o) = (uint8_t)(obAngle(o) + 1);          /* addq.b #1,obAngle(a0) */
    int16_t xoff = (int16_t)Drown_WobbleData[angle & 0x7F];
    obX(o) = (int16_t)(bub_origX(o) + xoff);

    if (bub_inhalable(o) && Bub_ChkSonic(o)) {
        /* FixBugs=0: the `tst.w (v_debuguse).w` guard sits inside `if FixBugs`. */
        ResumeMusic();                              /* cancel countdown music */
        Sound_Queue(sfx_Bubble, false);

        uint8_t *a1 = RAM_ADDR(v_player);
        obVelX(a1) = 0;
        obVelY(a1) = 0;
        obInertia(a1) = 0;
        obAnim(a1) = id_SonGetAir;
        locktime(a1) = 35;                          /* disable D-Pad input */
        jumping(a1) = 0;
        obStatus(a1) &= ~(1 << 5);                  /* bclr #5: pushing flag */
        obStatus(a1) &= ~(1 << 4);                  /* bclr #4: roll-jump flag */

        if (obStatus(a1) & (1 << 2)) {              /* btst #2: was Sonic rolling? */
            obStatus(a1) &= ~(1 << 2);              /* clear rolling flag */
            obHeight(a1) = sonic_height;
            obWidth(a1) = sonic_width;
            obY(a1) = (int16_t)(obY(a1) - (sonic_height - sonic_roll_height));
        }
        /* .burst */
        obRoutine(o) = 6;
        obAnim(o) = (uint8_t)(obAnim(o) + 3);
        Bub_Bursting(o);
        return;
    }

    /* .display */
    SpeedToPos(o);
    if (!(obRender(o) & 0x80)) {                    /* tst.b obRender(a0) / bpl.s */
        DeleteObject(o);
        return;
    }
    DisplaySprite(o);
}

/* Bub_BubbleMaker — routine $A, disasm 64 158-243 */
static void Bub_BubbleMaker(uint8_t *o) {
    if (bub_bubbleflag(o) == 0) {                   /* tst.w bub_bubbleflag / bne.s */
        int16_t d0 = (int16_t)v_waterpos1;
        if ((uint16_t)d0 >= (uint16_t)obY(o)) goto display; /* bhs.w .display */
        if (!(obRender(o) & 0x80)) goto display;    /* tst.b obRender / bpl.w .display */

        bub_randomtime(o) = (int16_t)(bub_randomtime(o) - 1);
        if (bub_randomtime(o) >= 0) goto animate;   /* bpl.w .animate */
        bub_bubbleflag(o) = 1;                      /* enter bubble spawning mode */

        /* .tryAgain */
        uint16_t r;
        do {
            r = RandomNumber();
        } while ((r & 7) >= 6);                     /* cmpi.w #6,d0 / bhs.s .tryAgain */
        bub_minicount(o) = (uint8_t)(r & 7);        /* 0-5 small bubbles */
        bub_typelist(o) = r & 0xC;                  /* multiple of 4, up to 12 */

        bub_time(o) = (uint8_t)(bub_time(o) - 1);             /* subq.b #1,bub_time(a0) */
        if ((int8_t)bub_time(o) < 0) {                        /* bpl.s .goSpawn */
            bub_time(o) = bub_timebase(o);                    /* move.b bub_timebase */
            bub_bubbleflag(o) = (uint16_t)(bub_bubbleflag(o) | 0x8000);  /* bset #7 */
        }
        /* .goSpawn / .spawnBubble */
    } else {
        /* .waitForBubble */
        bub_randomtime(o) = (int16_t)(bub_randomtime(o) - 1);
        if (bub_randomtime(o) >= 0) goto animate;   /* bpl.w .animate */
    }

    /* .spawnBubble */
    bub_randomtime(o) = (int16_t)(RandomNumber() & 0x1F);

    uint8_t *a1 = (uint8_t *)FindFreeObj();
    if (a1 == NULL) goto chkReset;                  /* bne.s .chkReset */

    obID(a1) = id_Bubble;
    obX(a1) = obX(o);
    obX(a1) = (int16_t)(obX(a1) + (int16_t)((RandomNumber() & 0xF) - 8));
    obY(a1) = obY(o);
    obSubtype(a1) = Bub_BblTypes[bub_typelist(o) + bub_minicount(o)];

    if (bub_bubbleflag(o) & 0x8000) {               /* btst #7 / beq.s .chkReset */
        if ((RandomNumber() & 3) == 0) {            /* 1/4 chance for a large bubble */
            uint16_t was = bub_bubbleflag(o);
            bub_bubbleflag(o) = (uint16_t)(was | 0x4000);
            if (was & 0x4000) goto chkReset;        /* bne.s .chkReset: flag was already set */
            obSubtype(a1) = 2;                      /* large / inhalable, then fall through */
        }
        /* .chkFallback — reachable only when bit 7 is set (or the 1/4 roll
           succeeded); `beq.s .chkReset` skips it entirely otherwise. */
        if (bub_minicount(o) == 0) {                /* tst.b bub_minicount / bne.s */
            uint16_t was = bub_bubbleflag(o);
            bub_bubbleflag(o) = (uint16_t)(was | 0x4000);
            if (!(was & 0x4000)) {
                obSubtype(a1) = 2;
            }
        }
    }

chkReset:
    bub_minicount(o) = (uint8_t)(bub_minicount(o) - 1);
    if (!(bub_minicount(o) & 0x80)) goto animate;   /* bpl.s .animate */
    bub_randomtime(o) = (int16_t)(bub_randomtime(o) + (int16_t)((RandomNumber() & 0x7F) + 0x80));
    bub_bubbleflag(o) = 0;

animate:
    AnimateSprite(o, Ani_Bub);

display:
    if (OutOfRange(o, -1, 0)) {                           /* out_of_range.w DeleteObject */
        DeleteObject(o);
        return;
    }
    {
        int16_t d0 = (int16_t)v_waterpos1;
        if ((uint16_t)d0 < (uint16_t)obY(o)) {            /* blo.w DisplaySprite */
            DisplaySprite(o);
        }
    }
}

/* Bub_Main / Bub_Inflate — disasm 64 30-69 */
static void Bub_Main(uint8_t *o) {
    obRoutine(o) = (uint8_t)(obRoutine(o) + 2);    /* -> Bub_Inflate */
    obMap(o) = (uint32_t)(uintptr_t)Map_Bub;
    obGfx(o) = (uint16_t)(ArtTile_LZ_Bubbles | Tile_Prio);
    obRender(o) = (uint8_t)(sprite_rendered | sprite_cam_field);  /* move.b (not ori.b) */
    obActWid(o) = 32 / 2;
    obPriority(o) = 1;

    uint8_t d0 = obSubtype(o);
    if (!(d0 & 0x80)) {                             /* bpl.s .bubble */
        /* .bubble */
        obAnim(o) = d0;                             /* bubble size from subtype */
        bub_origX(o) = obX(o);
        obVelY(o) = (int16_t)-0x88;                  /* float upwards */
        obAngle(o) = (uint8_t)RandomNumber();       /* random start wobble angle */
        /* fall through to Bub_Inflate */
        AnimateSprite(o, Ani_Bub);
        if (obFrame(o) != 6) {                      /* cmpi.b #6,obFrame / bne.s */
            Bub_ChkWater(o);
            return;
        }
        bub_inhalable(o) = 1;
        Bub_ChkWater(o);
        return;
    }

    /* bubble maker (subtype $80 and above) */
    obRoutine(o) = (uint8_t)(obRoutine(o) + 8);    /* -> Bub_BubbleMaker */
    d0 = (uint8_t)(d0 & 0x7F);
    bub_time(o) = d0;                                         /* move.b d0,bub_time(a0) */
    bub_timebase(o) = d0;                                     /* move.b d0,bub_timebase(a0) */
    obAnim(o) = 6;
    Bub_BubbleMaker(o);
}

/* Object $64 dispatcher */
static void Bubble_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                         /* Bub_Index: 0/2/4/6/8/A */
        case 0x00: Bub_Main(o); break;
        case 0x02:                                      /* falls into Bub_ChkWater */
            AnimateSprite(o, Ani_Bub);
            if (obFrame(o) == 6) bub_inhalable(o) = 1;
            Bub_ChkWater(o);
            break;
        case 0x04: Bub_ChkWater(o); break;
        case 0x06: Bub_Bursting(o); break;
        case 0x08: DeleteObject(o); break;              /* Bub_BurstDelete */
        case 0x0A: Bub_BubbleMaker(o); break;
    }
}

/* ---------------------------------------------------------------------------
   Object $77 - Eggman (LZ)
   disasm/_incObj/77 Boss - LZ Main.asm

   BossLabyrinth_ParentObj:      equ objoff_34  pointer to main boss controller
   BossLabyrinth_GenericTimer:   equ objoff_3C  timer for the end-of-fight anim
   BossLabyrinth_EarlyDefeatFlag:equ objoff_3D  set if boss defeated while rising
   BossLabyrinth_SineCounter:    equ objoff_3F  sine counter for bobbing motion
   --------------------------------------------------------------------------- */
#define blz_parentobj(o)   (*(uint32_t *)((uint8_t *)(o) + 0x34)) /* objoff_34 */
#define blz_timer(o)       (*(uint8_t *)((uint8_t *)(o) + 0x3C)) /* objoff_3C */
#define blz_earlydefeat(o) (*(uint8_t *)((uint8_t *)(o) + 0x3D)) /* objoff_3D */
#define blz_sinecnt(o)     (*(uint8_t *)((uint8_t *)(o) + 0x3F)) /* objoff_3F */

/* BossLabyrinth_ObjData: routine number, animation */
static const uint8_t BossLabyrinth_ObjData[6] = {
    2, 0,   /* ship body */
    4, 1,   /* face */
    6, 7,   /* thruster */
};

/* Copy the logical X/Y flip bits from obStatus into the render flags. */
static void BLZ_SyncFlip(uint8_t *o) {
    uint8_t d0 = (uint8_t)((sprite_xflip | sprite_yflip) & obStatus(o));
    obRender(o) = (uint8_t)((obRender(o) & ~(sprite_xflip | sprite_yflip)) | d0);
}

/* BossLabyrinth_Display — shared by the face and thruster children. */
static void BLZ_Child_Display(uint8_t *o) {
    AnimateSprite(o, Ani_Eggman);
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)blz_parentobj(o));
    obX(o) = obX(a1);
    obY(o) = obY(a1);
    obStatus(o) = obStatus(a1);
    BLZ_SyncFlip(o);
    DisplaySprite(o);
}

/* BLZ_ShipUpdate — hit flash / defeat bookkeeping. disasm 77 120-149 */
static void BLZ_ShipUpdate(uint8_t *o) {
    if (blz_earlydefeat(o)) {                      /* tst.b / bne.s .defeated */
        BossDefeated(o);
        return;
    }
    if (obStatus(o) & 0x80) {                      /* tst.b obStatus / bmi.s */
        AddPoints(100);                            /* .setDefeatFlag */
        blz_earlydefeat(o) = 0xFF;
        return;
    }
    if (obColType(o) != 0) return;                 /* tst.b obColType / bne.s .exit */
    if (obBossFlash(o) == 0) {                     /* tst.b obBossFlash / bne.s */
        obBossFlash(o) = 0x20;                     /* number of frames to flash */
        Sound_Queue(sfx_HitBoss, false);
    }

    /* .flash: toggle the 2nd entry of palette line 2 between black and white.
       `lea (v_palette+$22).w,a1` then `tst.w (a1)` — the read is a *word*. */
    uint16_t *pal = (uint16_t *)RAM_ADDR(v_palette + 0x22);
    int16_t d0 = cBlack;                           /* moveq #0,d0 (black) */
    if (*pal != 0) {                               /* tst.w (a1) / bne.s .writeColor */
        d0 = cBlack;                               /* not black -> already white, keep 0 */
    } else {
        d0 = cWhite;                               /* move.w #cWhite,d0 */
    }
    *pal = (uint16_t)d0;                           /* move.w d0,(a1) */
    obBossFlash(o) = (uint8_t)(obBossFlash(o) - 1);
    if (obBossFlash(o) != 0) return;              /* bne.s .exit: keep flashing */
    obColType(o) = (uint8_t)(col_48x48 | col_boss); /* restore collision */
}

/* obBossX/obBossY are 16.16 fixed point. The ASM only ever touches them with
   `move.w`/`cmpi.w`, which address the *high* word, so that word holds the pixel
   position and the low word the 1/65536px remainder (BossMove adds obVel << 8 to
   the whole longword, i.e. one velocity unit is 1/256px). These helpers keep the
   remainder intact on writes, exactly like `move.w`. */
static int16_t blz_bossX(uint8_t *o) { return (int16_t)(obBossX(o) >> 16); }
static int16_t blz_bossY(uint8_t *o) { return (int16_t)(obBossY(o) >> 16); }
static void blz_setBossX(uint8_t *o, int16_t x) {
    obBossX(o) = (int32_t)((uint32_t)obBossX(o) & 0x0000FFFFu) | ((int32_t)(uint16_t)x << 16);
}
static void blz_setBossY(uint8_t *o, int16_t y) {
    obBossY(o) = (int32_t)((uint32_t)obBossY(o) & 0x0000FFFFu) | ((int32_t)(uint16_t)y << 16);
}

/* BLZ_MoveBoss — disasm 77 108-111 */
static void BLZ_MoveBoss(uint8_t *o) {
    BossMove(o);
    obY(o) = blz_bossY(o);                         /* move.w obBossY(a0),obY(a0) */
    obX(o) = blz_bossX(o);                         /* move.w obBossX(a0),obX(a0) */
    BLZ_ShipUpdate(o);
}

/* BossLabyrinth_ShipIndex handlers (disasm 77 92-478) */
static void BLZ_ShipStart(uint8_t *o, uint8_t *a1) {
    /* cmpi.w #boss_lz_x-$40,d0 / blo.s BLZ_MoveBoss */
    if ((uint16_t)obX(a1) >= 0x1DA0) {
        obVelY(o) = (int16_t)-0x180;
        obVelX(o) = 0x60;
        ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
    }
    BLZ_MoveBoss(o);
}

static void BLZ_ShipMove1(uint8_t *o) {
    int16_t d0 = -2;                              /* moveq #-2,d0: condition counter */
    if (blz_bossX(o) >= 0x1E48) {                 /* cmpi.w #boss_lz_x+$68 / blo.s */
        blz_setBossX(o, 0x1E48);                  /* set X to target */
        obVelX(o) = 0;                            /* stop moving horizontally */
        d0++;
    }
    if (blz_bossY(o) <= 0x500) {                  /* cmpi.w #boss_lz_y+$440 / bgt.s */
        blz_setBossY(o, 0x500);                   /* set Y to target */
        obVelY(o) = 0;                            /* stop moving vertically */
        d0++;
    }
    if (d0 != 0) {                                /* bne.s .moveBoss */
        BLZ_MoveBoss(o);
        return;
    }
    /* both conditions met: set the speeds for the next phase */
    obVelX(o) = 0x140;
    obVelY(o) = (int16_t)-0x200;
    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
    BLZ_MoveBoss(o);
}

static void BLZ_ShipMove2(uint8_t *o) {
    int16_t d0 = -2;
    if (blz_bossX(o) >= 0x1E70) {                 /* cmpi.w #boss_lz_x+$90 / blo.s */
        blz_setBossX(o, 0x1E70);
        obVelX(o) = 0;
        d0++;
    }
    if (blz_bossY(o) <= 0x4C0) {                  /* cmpi.w #boss_lz_y+$400 / bgt.s */
        blz_setBossY(o, 0x4C0);
        obVelY(o) = 0;
        d0++;
    }
    if (d0 != 0) {
        BLZ_MoveBoss(o);
        return;
    }
    obVelY(o) = (int16_t)-0x180;                  /* Y velocity only for next phase */
    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
    blz_sinecnt(o) = 0;                           /* clear vertical bob */
    BLZ_MoveBoss(o);
}

static void BLZ_ShipMove3(uint8_t *o) {
    if (blz_bossY(o) <= 0x100) {                  /* cmpi.w #boss_lz_y+$40 / bgt.s */
        blz_setBossY(o, 0x100);
        obVelX(o) = 0x140;
        obVelY(o) = (int16_t)-0x80;
        if (blz_earlydefeat(o)) {                 /* tst.b / beq.s .moveBoss */
            obVelX(o) = (int16_t)(obVelX(o) * 2);  /* asl.w obVelX(a0) */
            obVelY(o) = (int16_t)(obVelY(o) * 2);  /* asl.w obVelY(a0) */
        }
        /* .moveBoss */
        ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
        BLZ_MoveBoss(o);
        return;
    }

    /* .setDirection */
    obStatus(o) |= 0x01;                           /* bset #0: face to the right */
    blz_sinecnt(o) = (uint8_t)(blz_sinecnt(o) + 2);
    int16_t s0 = 0, s1 = 0;
    CalcSine(blz_sinecnt(o), &s0, &s1);
    if (s1 < 0) obStatus(o) &= (uint8_t)~0x01;    /* tst.w d1 / bpl.s .trackDistance */

    /* .trackDistance: asr.w #4 / swap / clr.w / add.l obBossX / swap / move.w */
    int32_t d0 = (int32_t)(((uint32_t)(uint16_t)(int16_t)(s0 >> 4)) << 16);
    d0 += obBossX(o);
    d0 = (int32_t)(((uint32_t)d0 >> 16) | ((uint32_t)d0 << 16)); /* swap d0 */
    obX(o) = (int16_t)d0;

    /* Slow down while Sonic approaches, so he can be hit. disasm 77 246-262 */
    int16_t d0w = obVelY(o);
    int16_t d1 = (int16_t)(obY(RAM_ADDR(v_player)) - obY(o));
    if (!(d1 < 0)) {                              /* bcs.s .calcSpeed */
        d1 = (int16_t)(d1 - 72);                  /* subi.w #72,d1 */
        if (!(d1 < 0)) {                          /* bcs.s .calcSpeed */
            d0w = (int16_t)(d0w >> 1);            /* asr.w #1,d0 */
            d1 = (int16_t)(d1 - 40);              /* subi.w #40,d1 */
            if (!(d1 < 0)) {                      /* bcs.s .calcSpeed */
                d0w = (int16_t)(d0w >> 1);
                d1 = (int16_t)(d1 - 40);
                if (!(d1 < 0)) d0w = 0;           /* moveq #0,d0 */
            }
        }
    }
    /* .calcSpeed: ext.l d0 / asl.l #8,d0 */
    int32_t dv = (int32_t)d0w * 0x100;
    if (blz_earlydefeat(o)) dv = dv * 2;          /* add.l d0,d0 */
    /* .setSpeed */
    obBossY(o) += dv;
    obY(o) = blz_bossY(o);                         /* move.w obBossY(a0),obY(a0) */
    BLZ_ShipUpdate(o);
}

static void BLZ_ShipAtTop(uint8_t *o) {
    int16_t d0 = -2;
    if (blz_bossX(o) >= 0x1F4C) {                 /* cmpi.w #boss_lz_x+$16C / blo.s */
        blz_setBossX(o, 0x1F4C);
        obVelX(o) = 0;
        d0++;
    }
    if (blz_bossY(o) <= (int16_t)boss_lz_y) {     /* cmpi.w #boss_lz_y / bgt.s */
        blz_setBossY(o, (int16_t)boss_lz_y);
        obVelY(o) = 0;
        d0++;
    }
    if (d0 != 0) {                                /* bne.s .moveBoss */
        BLZ_MoveBoss(o);
        return;
    }
    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
    obStatus(o) &= (uint8_t)~0x01;                /* bclr #0: face to the left */
    BLZ_MoveBoss(o);
}

static void BLZ_ShipWait(uint8_t *o, uint8_t *a1) {
    if (blz_earlydefeat(o)) goto startEscape;     /* tst.b / bne.s .startEscape */
    if (obX(a1) < 0x1EC8) goto skip;              /* cmpi.w #boss_lz_x+$E8 / blt.s */
    if (obY(a1) > 0xF0) goto skip;                /* cmpi.w #boss_lz_y+$30 / bgt.s */
    blz_timer(o) = 50;                            /* set a timer for 50 frames */
    /* falls through into .startEscape */
startEscape:
    Sound_Queue(bgm_LZ, true);                     /* play LZ music */
    f_lockscreen = 0;                              /* Revision<>0 */
    obStatus(o) |= 0x01;                           /* bset #0: face to the right */
    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
skip:
    BLZ_MoveBoss(o);
}

static void BLZ_Escape1(uint8_t *o) {
    if (!blz_earlydefeat(o)) {                     /* tst.b / bne.s .escape */
        blz_timer(o) = (uint8_t)(blz_timer(o) - 1);
        if (blz_timer(o) != 0) {                   /* bne.s .exit */
            BLZ_MoveBoss(o);
            return;
        }
    }
    /* .escape */
    blz_timer(o) = 0;
    obVelX(o) = 0x400;
    obVelY(o) = (int16_t)-0x40;
    blz_earlydefeat(o) = 0;
    ob2ndRout(o) = (uint8_t)(ob2ndRout(o) + 2);
    BLZ_MoveBoss(o);
}

static void BLZ_Escape2(uint8_t *o) {
    if ((uint16_t)v_limitright2 >= (uint16_t)boss_lz_end) { /* cmpi.w / bhs.s */
        /* .checkOffscreen: has Eggman left the screen? (bit 7 of obRender) */
        if (!(obRender(o) & 0x80)) {
            /* BossLabyrinth_ShipDel. FixBugs=0: `jmp DeleteObject` from inside
               the routine, so control comes back to BossLabyrinth_ShipMain and
               the animation + display pass still runs on the cleared slot. */
            DeleteObject(o);
            return;
        }
        BLZ_MoveBoss(o);                           /* .flee */
        return;
    }
    v_limitright2 = (uint16_t)(v_limitright2 + 2); /* unlock 2 more pixels */
    BLZ_MoveBoss(o);                               /* .flee */
}

/* BossLabyrinth_ShipMain — routine 2 */
static void BossLabyrinth_ShipMain(uint8_t *o) {
    uint8_t *a1 = RAM_ADDR(v_player);

    switch (ob2ndRout(o)) {                        /* BossLabyrinth_ShipIndex */
        case 0x00: BLZ_ShipStart(o, a1); break;
        case 0x02: BLZ_ShipMove1(o);   break;
        case 0x04: BLZ_ShipMove2(o);   break;
        case 0x06: BLZ_ShipMove3(o);   break;
        case 0x08: BLZ_ShipAtTop(o);   break;
        case 0x0A: BLZ_ShipWait(o, a1); break;
        case 0x0C: BLZ_Escape1(o);     break;
        case 0x0E: BLZ_Escape2(o);     break;
    }

    AnimateSprite(o, Ani_Eggman);
    BLZ_SyncFlip(o);
    DisplaySprite(o);
}

/* BossLabyrinth_FaceMain — routine 4, disasm 77 400-448 */
static void BossLabyrinth_FaceMain(uint8_t *o) {
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)blz_parentobj(o));
    if (obID(a1) != obID(o)) {                     /* boss deleted offscreen? */
        DeleteObject(o);
        return;
    }
    uint8_t d0 = ob2ndRout(a1);
    uint8_t d1 = 1;
    /* FixBugs=0: the test reads the FACE's own flag, which is always 0, so the
       defeated animation is unreachable here. */
    if (blz_earlydefeat(o)) {                      /* tst.b BossLabyrinth_EarlyDefeatFlag(a0) */
        d1 = 0x0A;                                 /* defeated */
    } else if (obColType(a1) == 0) {               /* .checkHitState */
        d1 = 5;                                    /* facehit */
    } else if (obRoutine(RAM_ADDR(v_player)) >= 4) { /* .checkSonicState */
        d1 = 4;                                    /* facelaugh */
    }
    /* .writeAnim */
    obAnim(o) = d1;
    if (d0 == 0x0E) {                              /* cmpi.b #$E,d0 / bne.s .skip */
        obAnim(o) = 6;                             /* facepanic */
        if (!(obRender(o) & 0x80)) {               /* tst.b obRender / bpl.s */
            DeleteObject(o);
            return;
        }
    }
    BLZ_Child_Display(o);
}

/* BossLabyrinth_FlameMain — routine 6, disasm 77 450-478 */
static void BossLabyrinth_FlameMain(uint8_t *o) {
    obAnim(o) = 7;                                 /* default invisible state */
    uint8_t *a1 = (uint8_t *)Object_GetSlot((int)blz_parentobj(o));
    if (obID(a1) != obID(o)) {                     /* boss deleted offscreen? */
        DeleteObject(o);
        return;
    }
    /* FixBugs=0: the `bne.s` has no label, so it jumps straight to .skip and the
       .checkMove block (the animated thruster for horizontal travel) is dead
       code. The thruster therefore only shows up in Escape2. */
    if (ob2ndRout(a1) == 0x0E) {                   /* cmpi.b #$E,ob2ndRout(a1) */
        obAnim(o) = 0x0B;                          /* thruster: taking off */
        if (!(obRender(o) & 0x80)) {               /* tst.b obRender / bpl.s */
            DeleteObject(o);
            return;
        }
    }
    BLZ_Child_Display(o);
}

/* BossLabyrinth_Main — routine 0, disasm 77 31-56 */
static void BossLabyrinth_Main(uint8_t *o) {
    obX(o) = (int16_t)(boss_lz_x + 0x30);
    obY(o) = (int16_t)(boss_lz_y + 0x500);
    /* `move.w obX(a0),obBossX(a0)` writes the high word of the 16.16 field; the
       low word keeps the 0 it had in the freshly cleared object slot. */
    obBossX(o) = ((int32_t)(int16_t)obX(o)) << 16;
    obBossY(o) = ((int32_t)(int16_t)obY(o)) << 16;
    obColType(o) = (uint8_t)(col_48x48 | col_boss);
    obBossHits(o) = 8;
    obPriority(o) = 4;

    const uint8_t *a2 = BossLabyrinth_ObjData;
    uint8_t *a1 = o;                               /* first child reuses this slot */
    for (int i = 0; i < 3; i++) {                  /* moveq #2,d1 -> dbf, 3 passes */
        if (i > 0) {
            a1 = (uint8_t *)FindNextFreeObj(a1);
            if (!a1) break;                        /* bne.s BossLabyrinth_ShipMain */
            obID(a1) = id_BossLabyrinth;
            obX(a1) = obX(o);
            obY(a1) = obY(o);
        }
        obStatus(o) &= (uint8_t)~0x01;             /* bclr #0,obStatus(a0) */
        ob2ndRout(a1) = 0;
        obRoutine(a1) = *a2++;
        obAnim(a1) = *a2++;
        obPriority(a1) = obPriority(o);
        obMap(a1) = (uint32_t)(uintptr_t)Map_Eggman;
        obGfx(a1) = (uint16_t)ArtTile_Eggman;
        obRender(a1) = sprite_cam_field;
        obActWid(a1) = 64 / 2;
        blz_parentobj(a1) = (uint32_t)Object_GetIndex(o); /* slot index, not a pointer */
    }
    /* BossLabyrinth_ShipMain is reached on the same pass as the last child. */
    BossLabyrinth_ShipMain(o);
}

/* Object $77 dispatcher */
static void BossLabyrinth_ObjectMain(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    switch (obRoutine(o)) {                       /* BossLabyrinth_Index: 0/2/4/6 */
        case 0x00: BossLabyrinth_Main(o); break;
        case 0x02: BossLabyrinth_ShipMain(o); break;
        case 0x04: BossLabyrinth_FaceMain(o); break;
        case 0x06: BossLabyrinth_FlameMain(o); break;
    }
}
