/* ===========================================================================
   Debug Mode — port of _incObj/DebugMode.asm
   Target: REV01, FixBugs=0 (same as the rest of the port).

   Called from SonicPlayer_Main (objects.c) whenever RAM_WORD(v_debuguse)
   is non-zero. The debug object shown on screen is Sonic's own object slot
   (a0/v_player), temporarily given the selected debug item's mappings.
   =========================================================================== */

#include "ram.h"
#include "constants.h"
#include "objects.h"
#include "data.h"
#include "debugmode.h"

#include <stdint.h>

/* GFX and animation constants (objects.c / _incObj/01 Sonic.asm) */
#define id_Walk        0x00
#define id_Roll        0x02
#define fr_Null        0x00
#define id_EndZ        6

/* Game mode constants (see main.c GM_Special = $10) */
#define GM_Special     0x10

/* ---------------------------------------------------------------------------
   Debug mode calibrations (DebugMode.asm lines 5-6)
   --------------------------------------------------------------------------- */
#define debug_movedelay  12   /* frames to wait when holding D-Pad before moving */
#define debug_startspeed 15   /* initial movement speed when first holding D-Pad */

/* Absolute bottom bound for the debug object (FixBugs=0: $7FF) */
#define DEBUG_MAX_Y   (0x7FFu << 16)

/* ---------------------------------------------------------------------------
   Debug list entries. The ASM packs the object ID into the high byte of the
   mappings longword (dbug: dc.l map+(object<<24)); the map pointer itself is
   kept separately because the port's Map_* symbols are runtime variables.
   --------------------------------------------------------------------------- */
typedef struct {
    uint8_t        id;        /* object ID            */
    uint8_t        subtype;   /* object subtype       */
    uint8_t        frame;     /* frame ID             */
    uint16_t       vram;      /* VRAM setting         */
} DebugItemEntry;

#define DBUG(o, st, fr, vr) { (o), (st), (fr), (vr) }

/* ---------------------------------------------------------------------------
   Lookup the mappings pointer for a debug object ID (the ASM stores it in
   the same list entry; here it comes from the object-ID field).
   --------------------------------------------------------------------------- */
static const uint8_t *Debug_MapForId(uint8_t id) {
    switch (id) {
        case id_Rings:              return Map_Ring;
        case id_Monitor:            return Map_Monitor;
        case id_Crabmeat:           return Map_Crab;
        case id_BuzzBomber:         return Map_Buzz;
        case id_Chopper:            return Map_Chop;
        case id_Spikes:             return Map_Spike;
        case id_BasicPlatform:      return Map_Plat_GHZ;
        case id_PurpleRock:         return Map_PRock;
        case id_MotoBug:            return Map_Moto;
        case id_Springs:            return Map_Spring;
        case id_Newtron:            return Map_Newt;
        case id_EdgeWalls:          return Map_Edge;
        case id_Obj19:              return Map_GBall;
        case id_Lamppost:           return Map_Lamp;
        case id_GiantRing:          return Map_GRing;
        case id_HiddenBonus:        return Map_Bonus;
        case id_Jaws:               return Map_Jaws;
        case id_Burrobot:           return Map_Burro;
        case id_Harpoon:            return Map_Harp;
        case id_PushBlock:          return Map_Push;
        case id_Button:             return Map_But;
        case id_MovingBlock:        return Map_MBlockLZ;
        case id_LabyrinthBlock:     return Map_LBlock;
        case id_Gargoyle:           return Map_Gar;
        case id_LabyrinthConvey:    return Map_LConv;
        case id_Orbinaut:           return Map_Orb;
        case id_Bubble:             return Map_Bub;
        case id_Waterfall:          return Map_WFall;
        case id_Pole:               return Map_Pole;
        case id_FlapDoor:           return Map_Flap;
        case id_LavaMaker:          return Map_Fire;
        case id_MarbleBrick:        return Map_Brick;
        case id_GeyserMaker:        return Map_Geyser;
        case id_LavaWall:           return Map_LWall;
        case id_Yadrin:             return Map_Yad;
        case id_SmashBlock:         return Map_Smab;
        case id_CollapseFloor:      return Map_CFlo;
        case id_LavaTag:            return Map_LTag;
        case id_Basaran:            return Map_Bas;
        case id_Caterkiller:        return Map_Cat;
        case id_Elevator:           return Map_Elev;
        case id_CirclingPlatform:   return Map_Circ;
        case id_Staircase:          return Map_Stair;
        case id_Fan:                return Map_Fan;
        case id_Seesaw:             return Map_Seesaw;
        case id_Scenery:            return Map_Scen;
        case id_Bomb:               return Map_Bomb;
        case id_Roller:             return Map_Roll;
        case id_SpinningLight:      return Map_Light;
        case id_Bumper:             return Map_Bump;
        case id_FloatingBlock:      return Map_FBlock;
        case id_SwingingPlatform:   return Map_BBall;
        case id_RunningDisc:        return Map_Disc;
        case id_SpinPlatform:       return Map_Spin;
        case id_Saws:               return Map_Saw;
        case id_ScrapStomp:         return Map_Stomp;
        case id_AutoDoor:           return Map_ADoor;
        case id_VanishPlatform:     return Map_VanP;
        case id_Flamethrower:       return Map_Flame;
        case id_Electro:            return Map_Elec;
        case id_Girder:             return Map_Gird;
        case id_Invisibarrier:      return Map_Invis;
        case id_BallHog:            return Map_Hog;
        default:                    return NULL;    /* unmatched asset (unstaged) */
    }
}

/* ---------------------------------------------------------------------------
   Per-zone debug item lists (DebugMode.asm DebugList, lines 394-587).
   NonFixBugs branches are used throughout (Gargoyle VRAM, MZ palettes,
   REV01 ending/special-stage list).
   --------------------------------------------------------------------------- */

static const DebugItemEntry GHZDebugList[] = {
    DBUG(id_Rings,         0,    0,  ArtTile_Ring|Tile_Pal2),
    DBUG(id_Monitor,       0,    0,  ArtTile_Monitor),
    DBUG(id_Crabmeat,      0,    0,  ArtTile_Crabmeat),
    DBUG(id_BuzzBomber,    0,    0,  ArtTile_Buzz_Bomber),
    DBUG(id_Chopper,       0,    0,  ArtTile_Chopper),
    DBUG(id_Spikes,        0,    0,  ArtTile_Spikes),
    DBUG(id_BasicPlatform, 0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_PurpleRock,    0,    0,  ArtTile_GHZ_Purple_Rock|Tile_Pal4),
    DBUG(id_MotoBug,       0,    0,  ArtTile_Moto_Bug),
    DBUG(id_Springs,       0,    0,  ArtTile_Spring_Horizontal),
    DBUG(id_Newtron,       0,    0,  ArtTile_Newtron|Tile_Pal2),
    DBUG(id_EdgeWalls,     0,    0,  ArtTile_GHZ_Edge_Wall|Tile_Pal3),
    DBUG(id_Obj19,         0,    0,  ArtTile_GHZ_Giant_Ball|Tile_Pal3),
    DBUG(id_Lamppost,      1,    0,  ArtTile_Lamppost),
    DBUG(id_GiantRing,     0,    0,  ArtTile_Giant_Ring|Tile_Pal2),
    DBUG(id_HiddenBonus,   1,    1,  ArtTile_Hidden_Points|Tile_Prio),
};

static const DebugItemEntry LZDebugList[] = {
    DBUG(id_Rings,            0,    0,    ArtTile_Ring|Tile_Pal2),
    DBUG(id_Monitor,          0,    0,    ArtTile_Monitor),
    DBUG(id_Springs,          0,    0,    ArtTile_Spring_Horizontal),
    DBUG(id_Jaws,             8,    0,    ArtTile_Jaws|Tile_Pal2),
    DBUG(id_Burrobot,         0,    2,    ArtTile_Burrobot|Tile_Prio),
    DBUG(id_Harpoon,          0,    0,    ArtTile_LZ_Harpoon),
    DBUG(id_Harpoon,          2,    3,    ArtTile_LZ_Harpoon),
    DBUG(id_PushBlock,        0,    0,    ArtTile_LZ_Push_Block|Tile_Pal3),
    DBUG(id_Button,           0,    0,    ArtTile_Button_Main),
    DBUG(id_Spikes,           0,    0,    ArtTile_Spikes),
    DBUG(id_MovingBlock,      4,    0,    ArtTile_LZ_Moving_Block|Tile_Pal3),
    DBUG(id_LabyrinthBlock,   1,    0,    ArtTile_LZ_Blocks|Tile_Pal3),
    DBUG(id_LabyrinthBlock,   0x13, 1,    ArtTile_LZ_Blocks|Tile_Pal3),
    DBUG(id_LabyrinthBlock,   5,    0,    ArtTile_LZ_Blocks|Tile_Pal3),
    /* NonFixBugs: Gargoyle uses (ArtTile_LZ_UnusedFace-2) */
    DBUG(id_Gargoyle,         0,    0,    (ArtTile_LZ_UnusedFace-2)|Tile_Pal3),
    DBUG(id_LabyrinthBlock,   0x27, 2,    ArtTile_LZ_Blocks|Tile_Pal3),
    DBUG(id_LabyrinthBlock,   0x30, 3,    ArtTile_LZ_Blocks|Tile_Pal3),
    DBUG(id_LabyrinthConvey,  0x7F, 0,    ArtTile_LZ_Conveyor_Belt),
    DBUG(id_Orbinaut,         0,    0,    ArtTile_LZ_Orbinaut),
    DBUG(id_Bubble,           0x84, 0x13, ArtTile_LZ_Bubbles|Tile_Prio),
    DBUG(id_Waterfall,        2,    2,    ArtTile_LZ_Splash|Tile_Pal3|Tile_Prio),
    DBUG(id_Waterfall,        9,    9,    ArtTile_LZ_Splash|Tile_Pal3|Tile_Prio),
    DBUG(id_Pole,             0,    0,    ArtTile_LZ_Pole|Tile_Pal3),
    DBUG(id_FlapDoor,         2,    0,    ArtTile_LZ_Flapping_Door|Tile_Pal3),
    DBUG(id_Lamppost,         1,    0,    ArtTile_Lamppost),
};

static const DebugItemEntry MZDebugList[] = {
    DBUG(id_Rings,         0,    0,  ArtTile_Ring|Tile_Pal2),
    DBUG(id_Monitor,       0,    0,  ArtTile_Monitor),
    DBUG(id_BuzzBomber,    0,    0,  ArtTile_Buzz_Bomber),
    DBUG(id_Spikes,        0,    0,  ArtTile_Spikes),
    DBUG(id_Springs,       0,    0,  ArtTile_Spring_Horizontal),
    DBUG(id_LavaMaker,     0,    0,  ArtTile_MZ_Fireball),
    DBUG(id_MarbleBrick,   0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_GeyserMaker,   0,    0,  ArtTile_MZ_Lava|Tile_Pal4),
    DBUG(id_LavaWall,      0,    0,  ArtTile_MZ_Lava|Tile_Pal4),
    DBUG(id_PushBlock,     0,    0,  ArtTile_MZ_Block|Tile_Pal3),
    DBUG(id_Yadrin,        0,    0,  ArtTile_Yadrin|Tile_Pal2),
    DBUG(id_SmashBlock,    0,    0,  ArtTile_MZ_Block|Tile_Pal3),
    /* NonFixBugs: incorrect palette lines on MZ moving block/collapsing floor */
    DBUG(id_MovingBlock,   0,    0,  ArtTile_MZ_Block),
    DBUG(id_CollapseFloor, 0,    0,  ArtTile_MZ_Block|Tile_Pal4),
    DBUG(id_LavaTag,       0,    0,  ArtTile_Monitor|Tile_Prio),
    DBUG(id_Basaran,       0,    0,  ArtTile_Basaran),
    DBUG(id_Caterkiller,   0,    0,  ArtTile_MZ_SYZ_Caterkiller|Tile_Pal2),
    DBUG(id_Lamppost,      1,    0,  ArtTile_Lamppost),
};

static const DebugItemEntry SLZDebugList[] = {
    DBUG(id_Rings,             0,    0,  ArtTile_Ring|Tile_Pal2),
    DBUG(id_Monitor,           0,    0,  ArtTile_Monitor),
    DBUG(id_Elevator,          0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_CollapseFloor,     0,    2,  ArtTile_SLZ_Collapsing_Floor|Tile_Pal3),
    DBUG(id_BasicPlatform,     0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_CirclingPlatform,  0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_Staircase,         0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_Fan,               0,    0,  ArtTile_SLZ_Fan|Tile_Pal3),
    DBUG(id_Seesaw,            0,    0,  ArtTile_SLZ_Seesaw),
    DBUG(id_Springs,           0,    0,  ArtTile_Spring_Horizontal),
    DBUG(id_LavaMaker,         0,    0,  ArtTile_SLZ_Fireball),
    DBUG(id_Scenery,           0,    0,  ArtTile_SLZ_Fireball_Launcher|Tile_Pal3),
    DBUG(id_Bomb,              0,    0,  ArtTile_Bomb),
    DBUG(id_Orbinaut,          0,    0,  ArtTile_SLZ_Orbinaut|Tile_Pal2),
    DBUG(id_Lamppost,          1,    0,  ArtTile_Lamppost),
};

static const DebugItemEntry SYZDebugList[] = {
    DBUG(id_Rings,          0,    0,  ArtTile_Ring|Tile_Pal2),
    DBUG(id_Monitor,        0,    0,  ArtTile_Monitor),
    DBUG(id_Spikes,         0,    0,  ArtTile_Spikes),
    DBUG(id_Springs,        0,    0,  ArtTile_Spring_Horizontal),
    DBUG(id_Roller,         0,    0,  ArtTile_Roller),
    DBUG(id_SpinningLight,  0,    0,  ArtTile_Level),
    DBUG(id_Bumper,         0,    0,  ArtTile_SYZ_Bumper),
    DBUG(id_Crabmeat,       0,    0,  ArtTile_Crabmeat),
    DBUG(id_BuzzBomber,     0,    0,  ArtTile_Buzz_Bomber),
    DBUG(id_Yadrin,         0,    0,  ArtTile_Yadrin|Tile_Pal2),
    DBUG(id_BasicPlatform,  0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_FloatingBlock,  0,    0,  ArtTile_Level|Tile_Pal3),
    DBUG(id_Button,         0,    0,  ArtTile_Button_Main),
    DBUG(id_Caterkiller,    0,    0,  ArtTile_MZ_SYZ_Caterkiller|Tile_Pal2),
    DBUG(id_Lamppost,       1,    0,  ArtTile_Lamppost),
};

static const DebugItemEntry SBZDebugList[] = {
    DBUG(id_Rings,             0,    0,    ArtTile_Ring|Tile_Pal2),
    DBUG(id_Monitor,           0,    0,    ArtTile_Monitor),
    DBUG(id_Bomb,              0,    0,    ArtTile_Bomb),
    DBUG(id_Orbinaut,          0,    0,    ArtTile_SBZ_Orbinaut),
    DBUG(id_Caterkiller,       0,    0,    ArtTile_SBZ_Caterkiller|Tile_Pal2),
    DBUG(id_SwingingPlatform,  7,    2,    ArtTile_SBZ_Swing|Tile_Pal3),
    DBUG(id_RunningDisc,       0xE0, 0,    ArtTile_SBZ_Disc|Tile_Pal3|Tile_Prio),
    DBUG(id_MovingBlock,       0x28, 2,    ArtTile_SBZ_Moving_Block_Short|Tile_Pal2),
    DBUG(id_Button,            0,    0,    ArtTile_Button_Main),
    DBUG(id_SpinPlatform,      3,    0,    ArtTile_SBZ_Trap_Door|Tile_Pal3),
    DBUG(id_SpinPlatform,      0x83, 0,    ArtTile_SBZ_Spinning_Platform),
    DBUG(id_Saws,              2,    0,    ArtTile_SBZ_Saw|Tile_Pal3),
    DBUG(id_CollapseFloor,     0,    0,    ArtTile_SBZ_Collapsing_Floor|Tile_Pal3),
    DBUG(id_MovingBlock,       0x39, 3,    ArtTile_SBZ_Moving_Block_Long|Tile_Pal3),
    DBUG(id_ScrapStomp,        0,    0,    ArtTile_SBZ_Moving_Block_Short|Tile_Pal2),
    DBUG(id_AutoDoor,          0,    0,    ArtTile_SBZ_Door|Tile_Pal3),
    DBUG(id_ScrapStomp,        0x13, 1,    ArtTile_SBZ_Moving_Block_Short|Tile_Pal2),
    DBUG(id_Saws,              1,    0,    ArtTile_SBZ_Saw|Tile_Pal3),
    DBUG(id_ScrapStomp,        0x24, 1,    ArtTile_SBZ_Moving_Block_Short|Tile_Pal2),
    DBUG(id_Saws,              4,    2,    ArtTile_SBZ_Saw|Tile_Pal3),
    DBUG(id_ScrapStomp,        0x34, 1,    ArtTile_SBZ_Moving_Block_Short|Tile_Pal2),
    DBUG(id_VanishPlatform,    0,    0,    ArtTile_SBZ_Vanishing_Block|Tile_Pal3),
    DBUG(id_Flamethrower,      0x64, 0,    ArtTile_SBZ_Flamethrower|Tile_Prio),
    DBUG(id_Flamethrower,      0x64, 0xB,  ArtTile_SBZ_Flamethrower|Tile_Prio),
    DBUG(id_Electro,           4,    0,    ArtTile_SBZ_Electric_Orb),
    DBUG(id_Girder,            0,    0,    ArtTile_SBZ_Girder|Tile_Pal3),
    DBUG(id_Invisibarrier,     0x11, 0,    ArtTile_Monitor|Tile_Prio),
    DBUG(id_BallHog,           4,    0,    ArtTile_Ball_Hog|Tile_Pal2),
    DBUG(id_Lamppost,          1,    0,    ArtTile_Lamppost),
};

/* Ending sequence + Special Stages list (REV01: two rings, second is blank) */
static const DebugItemEntry EndingSSDebugList[] = {
    DBUG(id_Rings,  0, 0,  ArtTile_Ring|Tile_Pal2),
    DBUG(id_Rings,  0, 8,  ArtTile_Ring|Tile_Pal2),
};

static const DebugItemEntry *const DebugLists[] = {
    GHZDebugList,          /* 0 */
    LZDebugList,           /* 1 */
    MZDebugList,           /* 2 */
    SLZDebugList,          /* 3 */
    SYZDebugList,          /* 4 */
    SBZDebugList,          /* 5 */
    EndingSSDebugList,     /* 6 */
};

/* Entry counts for each list (counts are word-stored before each list in the
   ASM; derived here from the table sizes). */
static const uint8_t DebugListCounts[] = {
    (uint8_t)(sizeof(GHZDebugList)      / sizeof(GHZDebugList[0])),
    (uint8_t)(sizeof(LZDebugList)       / sizeof(LZDebugList[0])),
    (uint8_t)(sizeof(MZDebugList)       / sizeof(MZDebugList[0])),
    (uint8_t)(sizeof(SLZDebugList)      / sizeof(SLZDebugList[0])),
    (uint8_t)(sizeof(SYZDebugList)      / sizeof(SYZDebugList[0])),
    (uint8_t)(sizeof(SBZDebugList)      / sizeof(SBZDebugList[0])),
    (uint8_t)(sizeof(EndingSSDebugList) / sizeof(EndingSSDebugList[0])),
};

/* ---------------------------------------------------------------------------
   Debug_ShowItem — set mappings, VRAM setting and frame for the displayed
   debug object (DebugMode.asm lines 356-364).
   --------------------------------------------------------------------------- */
static void Debug_ShowItem(uint8_t *o, const DebugItemEntry *list) {
    const DebugItemEntry *e = &list[v_debugitem];

    obMap(o)   = (uint32_t)(uintptr_t)Debug_MapForId(e->id);   /* move.l (a2,d0.w),obMap(a0) */
    obGfx(o)   = e->vram;                                       /* move.w 6(a2,d0.w),obGfx(a0) */
    obFrame(o) = e->frame;                                      /* move.b 5(a2,d0.w),obFrame(a0) */
}

/* ---------------------------------------------------------------------------
   Debug_Move — D-Pad movement (DebugMode.asm Debug_Move, lines 173-255).
   The longword position is (Y<<16)|subpixel, matching 'move.l obY(a0),d2'.
   NonFixBugs bounds: absolute 0 top/left, $7FF bottom, no right bounds.
   --------------------------------------------------------------------------- */
static void Debug_Move(uint8_t *o, uint8_t d4) {
    /* moveq #0,d1; move.b (v_debugspeed).w,d1; addq.w #1,d1; swap; asr.l #4 */
    uint32_t d1 = (uint32_t)(uint16_t)(v_debugspeed + 1) << 12;

    uint32_t d2 = ((uint32_t)(uint16_t)obY(o) << 16) | (uint16_t)obSubpixelY(o);
    uint32_t d3 = ((uint32_t)(uint16_t)obX(o) << 16) | (uint16_t)obSubpixelX(o);

    if (d4 & btnUp) {                            /* .chkUp */
        if (d2 < d1)                             /* bcc.s .chkDown */
            d2 = 0;                              /* underflow: clamp to top */
        else
            d2 -= d1;
    }

    if (d4 & btnDn) {                            /* .chkDown */
        d2 += d1;
        if (d2 >= DEBUG_MAX_Y)                   /* cmpi.l #$7FF<<16,d2 / blo.s */
            d2 = DEBUG_MAX_Y;
    }

    if (d4 & btnL) {                             /* .chkLeft */
        if (d3 < d1)                             /* bcc.s .chkRight */
            d3 = 0;                              /* underflow: clamp to left */
        else
            d3 -= d1;
    }

    if (d4 & btnR) {                             /* .chkRight (no bounds) */
        d3 += d1;
    }

    /* .setNewDebugPosition */
    obY(o)         = (int16_t)(d2 >> 16);
    obSubpixelY(o) = (int16_t)(uint16_t)d2;
    obX(o)         = (int16_t)(d3 >> 16);
    obSubpixelX(o) = (int16_t)(uint16_t)d3;
}

/* ---------------------------------------------------------------------------
   Debug_ChgItem — cycle items and spawn them (DebugMode.asm lines 262-310).
   --------------------------------------------------------------------------- */
static void Debug_ExitDebugMode(uint8_t *o);

static void Debug_ChgItem(uint8_t *o, const DebugItemEntry *list, uint8_t d6) {
    /* A held + C pressed: cycle back one item */
    if ((v_jpadhold1 & btnA) && (v_jpadpress1 & btnC)) {
        uint8_t previous = v_debugitem;
        v_debugitem = (uint8_t)(v_debugitem - 1);   /* subq.b #1 */
        if (previous == 0) {                        /* bcc.s .display (borrow) */
            v_debugitem = (uint8_t)(v_debugitem + d6);  /* wrap to last entry */
        }
        Debug_ShowItem(o, list);
        return;
    }

    /* A pressed: cycle forwards one item */
    if (v_jpadpress1 & btnA) {
        v_debugitem = (uint8_t)(v_debugitem + 1);   /* addq.b #1 */
        if (d6 <= v_debugitem)                      /* cmp.b (v_debugitem).w,d6 / bhi.s */
            v_debugitem = 0;                        /* wrap to start */
        Debug_ShowItem(o, list);
        return;
    }

    /* C pressed: spawn a new object at the debug object's position */
    if (v_jpadpress1 & btnC) {
        uint8_t *a1 = FindFreeObj();                /* jsr (FindFreeObj) */
        if (a1 == NULL) {                           /* bne.s Debug_ExitDebugMode */
            Debug_ExitDebugMode(o);
            return;
        }
        obX(a1)   = obX(o);                         /* move.w obX(a0),obX(a1) */
        obY(a1)   = obY(o);                         /* move.w obY(a0),obY(a1) */
        obID(a1)     = list[v_debugitem].id;        /* _move.b obMap(a0),obID(a1) */
        obRender(a1) = obRender(o);                 /* move.b obRender(a0),obRender(a1) */
        obStatus(a1) = obRender(o) & 0x7F;          /* obRender->status, bit 7 cleared */
        obSubtype(a1)= list[v_debugitem].subtype;   /* move.b 4(a2,d0.w),obSubtype(a1) */
        return;                                     /* rts */
    }

    Debug_ExitDebugMode(o);
}

/* ---------------------------------------------------------------------------
   Debug_ExitDebugMode — exit debug mode, restore Sonic (lines 316-342).
   --------------------------------------------------------------------------- */
static void Debug_ExitDebugMode(uint8_t *o) {
    if (!(v_jpadpress1 & btnB))                     /* btst #bitB / beq.s .return */
        return;

    v_debuguse = 0;                                 /* move.w d0,(v_debuguse).w */
    obMap(o) = (uint32_t)(uintptr_t)Map_Sonic;      /* reset Sonic's mappings */
    obGfx(o) = ArtTile_Sonic;                       /* reset Sonic's art tile */
    obAnim(o) = id_Walk;                            /* reset animation to walking */
    obSubpixelX(o) = 0;                             /* clear X subpixel */
    obSubpixelY(o) = 0;                             /* clear Y subpixel */

    v_limittop2 = v_limittopdb;                     /* restore top boundary */
    v_limitbtm1 = v_limitbtmdb;                     /* restore bottom boundary */

    if (v_gamemode == GM_Special) {
        v_ssangle = 0;                              /* make Special Stage upright */
        v_ssrotate = ss_rotatespeed;                /* restart maze rotation */
        obMap(o) = (uint32_t)(uintptr_t)Map_Sonic;  /* redundant, already done */
        obGfx(o) = ArtTile_Sonic;                   /* redundant, already done */
        obAnim(o) = id_Roll;                        /* reset animation to rolling */
        obStatus(o) |= (1 << 2) | (1 << 1);         /* force rolling + in-air state */
    }
}

/* ---------------------------------------------------------------------------
   Debug_Control — movement, item cycling and object spawning
   (DebugMode.asm lines 141-343).
   --------------------------------------------------------------------------- */
static void Debug_Control(uint8_t *o, const DebugItemEntry *list, uint8_t d6) {
    uint8_t d4;

    if (v_jpadpress1 & btnDir) {
        /* D-Pad pressed this frame: move immediately */
        d4 = v_jpadhold1;                        /* Debug_Move_GetDirections */
        Debug_Move(o, d4);
    } else if (v_jpadhold1 & btnDir) {
        /* D-Pad held: wait debug_movedelay frames, then accelerate */
        v_debugspeedtimer = (uint8_t)(v_debugspeedtimer - 1);  /* subq.b #1 */
        if (v_debugspeedtimer != 0) {            /* bne.s Debug_Move */
            d4 = v_jpadhold1;
            Debug_Move(o, d4);                   /* move with current speed */
        } else {
            v_debugspeedtimer = 1;               /* keep triggering every frame */
            v_debugspeed = (uint8_t)(v_debugspeed + 1);  /* addq.b #1 */
            if (v_debugspeed == 0)               /* bne.s Debug_Move_GetDirections */
                v_debugspeed = 0xFF;             /* cap speed at max */
            d4 = v_jpadhold1;
            Debug_Move(o, d4);
        }
    } else {
        /* No D-Pad buttons pressed: reset delay and speed, skip movement */
        v_debugspeedtimer = debug_movedelay;
        v_debugspeed = debug_startspeed;
    }

    Debug_ChgItem(o, list, d6);
}

/* ---------------------------------------------------------------------------
   Debug_Init — Routine 0 (DebugMode.asm lines 20-114).
   --------------------------------------------------------------------------- */
static void Debug_Init(uint8_t *o) {
    /* addq.b #2,(v_debuguse).w. The PC RAM is little-endian, so the initial
       word write of #1 in Sonic_Control leaves byte v_debuguse == 1; store
       the value the jump table keys on (2) directly with the 68k invariant. */
    RAM_BYTE(0xFE08) = 2;

    v_limittopdb = v_limittop2;                     /* buffer x-boundary */
    v_limitbtmdb = v_limitbtm1;                     /* buffer y-boundary */

    /* Unlock boundaries (FixBugs=0: always applied, no Special Stage skip) */
    v_limittop2 = 0;                                /* unlock top screen boundary */
    v_limitbtm1 = 0x800 - 224;                      /* unlock bottom - screen height */

    /* Vertical wrapping in LZ3/SBZ2-style levels */
    obY(o)                = obY(o) & 0x7FF;         /* andi.w #$7FF,obY(a0) */
    RAM_WORD(0xF704)     &= 0x7FF;                  /* andi.w #$7FF,(v_screenposy).w */
    RAM_WORD(0xF70C)     &= 0x3FF;                  /* andi.w #$3FF,(v_bgscreenposy).w */

    obFrame(o) = fr_Null;                           /* set frame to null (blank) */
    obAnim(o)  = id_Walk;                           /* set animation to walk */

    /* Select debug list by Zone ID (Special Stage uses the ending list) */
    uint8_t d0 = id_EndZ;
    if (v_gamemode != GM_Special)
        d0 = v_zone;
    const DebugItemEntry *list = DebugLists[d0];
    uint8_t d6 = DebugListCounts[d0];

    if (d6 <= v_debugitem)                          /* cmp.b (v_debugitem).w,d6 / bhi.s */
        v_debugitem = 0;                            /* reset to start of list */

    Debug_ShowItem(o, list);                        /* load selected item graphics */
    v_debugspeedtimer = debug_movedelay;            /* initial move delay (12) */
    v_debugspeed = 1;                               /* NonFixBugs: initial speed is 1 */
}

/* ---------------------------------------------------------------------------
   Debug_Action — Routine 2 (DebugMode.asm lines 117-132).
   --------------------------------------------------------------------------- */
static void Debug_Action(uint8_t *o) {
    /* Select debug list by Zone ID (Special Stage uses the ending list) */
    uint8_t d0 = id_EndZ;
    if (v_gamemode != GM_Special)
        d0 = v_zone;
    const DebugItemEntry *list = DebugLists[d0];
    uint8_t d6 = DebugListCounts[d0];

    Debug_Control(o, list, d6);
    DisplaySprite(o);                               /* jmp DisplaySprite */
}

/* ---------------------------------------------------------------------------
   DebugMode — dispatch (DebugMode.asm lines 9-17).
   Byte v_debuguse == 2 selects Debug_Action; any other value (0 after a
   word-clear, 1 on the frame Sonic_Control entered debug mode) inits.
   --------------------------------------------------------------------------- */
void DebugMode_Main(void *obj) {
    uint8_t *o = (uint8_t *)obj;

    if (RAM_BYTE(0xFE08) == 2)   /* (v_debuguse byte; jump table keys on 2) */
        Debug_Action(o);
    else
        Debug_Init(o);
}