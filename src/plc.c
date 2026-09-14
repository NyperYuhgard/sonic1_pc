#include "plc.h"
#include "types.h"
#include "constants.h"
#include "ram.h"
#include "data.h"
#include "decomp.h"
#include <string.h>

/* ===================================================================
   Pattern Load Cue (PLC) system
   Ported from _inc/Pattern Load Cues.asm (queue) and the AddPLC/RunPLC
   subroutines in sonic.asm.

   A PLC slot is 6 bytes: 4-byte Nemesis source pointer + 2-byte art
   tile destination (matching the ASM `plcm gfx,vram` = dc.l gfx,
   dc.w (vram)*tile_size).  The port's NemDecToVRAM() decompresses a
   full NEM in one call, so RunPLC() just processes one slot per frame.
   =================================================================== */

typedef struct {
    const uint8_t **src;                      /* -> runtime-loaded asset pointer */
    uint16_t  dest;                     /* destination art tile number */
} plc_entry;

#define plc_decl(gfx, vram) { &(gfx), (uint16_t)(vram) }

/* PLC_Main (plcid_Main) — _inc/Pattern Load Cues.asm
   The ASM list also contains Nem_Lamp (lamppost) and Nem_Points (points
   from enemy), neither of which is ported yet. */
static const plc_entry plc_Main[] = {
    plc_decl(Nem_Lamp,  ArtTile_Lamppost),
    plc_decl(Nem_Hud,   ArtTile_HUD),
    plc_decl(Nem_Lives, ArtTile_Lives_Counter),
    plc_decl(Nem_Ring,  ArtTile_Ring),
    plc_decl(Nem_Points, ArtTile_Points),
};

/* PLC_Main2 (plcid_Main2) — _inc/Pattern Load Cues.asm lines 84-88 */
static const plc_entry plc_Main2[] = {
    plc_decl(Nem_Monitors, ArtTile_Monitor),
    plc_decl(Nem_Shield,   ArtTile_Shield),
    plc_decl(Nem_Stars,    ArtTile_Invincibility),
};

/* PLC_Signpost (plcid_Signpost) — _inc/Pattern Load Cues.asm lines 296-300 */
static const plc_entry plc_Signpost[] = {
    plc_decl(Nem_SignPost, ArtTile_Signpost),
    plc_decl(Nem_Bonus,    ArtTile_Hidden_Points),
    plc_decl(Nem_BigFlash, ArtTile_Giant_Ring_Flash),
};

/* PLC_GHZ (plcid_GHZ) — _inc/Pattern Load Cues.asm lines 107-120 */
static const plc_entry plc_GHZ[] = {
    plc_decl(Nem_GHZ_1st, ArtTile_Level),
    plc_decl(Nem_GHZ_2nd, ArtTile_Level + 0x1CD),
    plc_decl(Nem_Stalk,   ArtTile_GHZ_Flower_Stalk),
    plc_decl(Nem_PplRock, ArtTile_GHZ_Purple_Rock),
    plc_decl(Nem_Crabmeat, ArtTile_Crabmeat),
    plc_decl(Nem_Buzz,    ArtTile_Buzz_Bomber),
    plc_decl(Nem_Chopper, ArtTile_Chopper),
    plc_decl(Nem_Newtron, ArtTile_Newtron),
    plc_decl(Nem_Motobug, ArtTile_Moto_Bug),
    plc_decl(Nem_Spikes,  ArtTile_Spikes),
    plc_decl(Nem_HSpring, ArtTile_Spring_Horizontal),
    plc_decl(Nem_VSpring, ArtTile_Spring_Vertical),
};

/* PLC_GHZ2 (plcid_GHZ2) — _inc/Pattern Load Cues.asm lines 122-129 */
static const plc_entry plc_GHZ2[] = {
    plc_decl(Nem_Swing,     ArtTile_GHZ_MZ_Swing),
    plc_decl(Nem_Bridge,    ArtTile_GHZ_Bridge),
    plc_decl(Nem_SpikePole, ArtTile_GHZ_Spike_Pole),
    plc_decl(Nem_Ball,      ArtTile_GHZ_Giant_Ball),
    plc_decl(Nem_GhzWall1,  ArtTile_GHZ_SLZ_Smashable_Wall),
    plc_decl(Nem_GhzWall2,  ArtTile_GHZ_Edge_Wall),
};

static const plc_entry plc_Explode[] = {
    plc_decl(Nem_Explode, ArtTile_Explosion),
};

/* PLC_GHZAnimals (plcid_GHZAnimals) — _inc/Pattern Load Cues.asm lines 343-346 */
static const plc_entry plc_GHZAnimals[] = {
    plc_decl(Nem_Rabbit,  ArtTile_Animal_1),
    plc_decl(Nem_Flicky,  ArtTile_Animal_2),
};

/* PLC_LZAnimals (plcid_LZAnimals) — _inc/Pattern Load Cues.asm lines 351-354 */
static const plc_entry plc_LZAnimals[] = {
    plc_decl(Nem_Penguin, ArtTile_Animal_1),
    plc_decl(Nem_Seal,    ArtTile_Animal_2),
};

/* PLC_MZAnimals (plcid_MZAnimals) — _inc/Pattern Load Cues.asm lines 359-362 */
static const plc_entry plc_MZAnimals[] = {
    plc_decl(Nem_Squirrel, ArtTile_Animal_1),
    plc_decl(Nem_Seal,     ArtTile_Animal_2),
};

/* PLC_SLZAnimals (plcid_SLZAnimals) — _inc/Pattern Load Cues.asm lines 367-370 */
static const plc_entry plc_SLZAnimals[] = {
    plc_decl(Nem_Pig,    ArtTile_Animal_1),
    plc_decl(Nem_Flicky, ArtTile_Animal_2),
};

/* PLC_SYZAnimals (plcid_SYZAnimals) — _inc/Pattern Load Cues.asm lines 375-378 */
static const plc_entry plc_SYZAnimals[] = {
    plc_decl(Nem_Pig,     ArtTile_Animal_1),
    plc_decl(Nem_Chicken, ArtTile_Animal_2),
};

/* PLC_SBZAnimals (plcid_SBZAnimals) — _inc/Pattern Load Cues.asm lines 383-386 */
static const plc_entry plc_SBZAnimals[] = {
    plc_decl(Nem_Rabbit,  ArtTile_Animal_1),
    plc_decl(Nem_Chicken, ArtTile_Animal_2),
};

/* PLC_TitleCard (plcid_TitleCard) — _inc/Pattern Load Cues.asm lines 277-279 */
static const plc_entry plc_TitleCard[] = {
    plc_decl(Nem_TitleCard, ArtTile_Title_Card),
};

/* plc_GameOver (plcid_GameOver) — _inc/Pattern Load Cues.asm lines 100-102 */
static const plc_entry plc_GameOver[] = {
    plc_decl(Nem_GameOver, ArtTile_Game_Over),
};

/* plc_LZ (plcid_LZ) — _inc/Pattern Load Cues.asm lines 134-147 */
static const plc_entry plc_LZ[] = {
    plc_decl(Nem_LZ         , ArtTile_Level),
    plc_decl(Nem_LzBlock1   , ArtTile_LZ_Block_1),
    plc_decl(Nem_LzBlock2   , ArtTile_LZ_Block_2),
    plc_decl(Nem_Splash     , ArtTile_LZ_Splash),
    plc_decl(Nem_Water      , ArtTile_LZ_Water_Surface),
    plc_decl(Nem_LzSpikeBall, ArtTile_LZ_Spikeball_Chain),
    plc_decl(Nem_FlapDoor   , ArtTile_LZ_Flapping_Door),
    plc_decl(Nem_Bubbles    , ArtTile_LZ_Bubbles),
    plc_decl(Nem_LzBlock3   , ArtTile_LZ_Moving_Block),
    plc_decl(Nem_LzDoor1    , ArtTile_LZ_Door),
    plc_decl(Nem_Harpoon    , ArtTile_LZ_Harpoon),
    plc_decl(Nem_Burrobot   , ArtTile_Burrobot),
};

/* plc_LZ2 (plcid_LZ2) — _inc/Pattern Load Cues.asm lines 149-165 (REV01) */
static const plc_entry plc_LZ2[] = {
    plc_decl(Nem_LzPole  , ArtTile_LZ_Pole),
    plc_decl(Nem_LzDoor2 , ArtTile_LZ_Blocks),
    plc_decl(Nem_LzWheel , ArtTile_LZ_Conveyor_Belt),
    plc_decl(Nem_Gargoyle, ArtTile_LZ_Gargoyle),
    plc_decl(Nem_LzPlatfm, ArtTile_LZ_Rising_Platform),
    plc_decl(Nem_Orbinaut, ArtTile_LZ_Orbinaut),
    plc_decl(Nem_Jaws    , ArtTile_Jaws),
    plc_decl(Nem_LzSwitch, ArtTile_Button),
    plc_decl(Nem_Cork    , ArtTile_LZ_Cork),
    plc_decl(Nem_Spikes  , ArtTile_Spikes),
    plc_decl(Nem_HSpring , ArtTile_Spring_Horizontal),
    plc_decl(Nem_VSpring , ArtTile_Spring_Vertical),
};

/* plc_MZ (plcid_MZ) — _inc/Pattern Load Cues.asm lines 170-181 */
static const plc_entry plc_MZ[] = {
    plc_decl(Nem_MZ     , ArtTile_Level),
    plc_decl(Nem_MzMetal, ArtTile_MZ_Spike_Stomper),
    plc_decl(Nem_MzFire , ArtTile_MZ_Fireball),
    plc_decl(Nem_Swing  , ArtTile_GHZ_MZ_Swing),
    plc_decl(Nem_MzGlass, ArtTile_MZ_Glass_Pillar),
    plc_decl(Nem_Lava   , ArtTile_MZ_Lava),
    plc_decl(Nem_Buzz   , ArtTile_Buzz_Bomber),
    plc_decl(Nem_Yadrin , ArtTile_Yadrin),
    plc_decl(Nem_Basaran, ArtTile_Basaran),
    plc_decl(Nem_Cater  , ArtTile_MZ_SYZ_Caterkiller),
};

/* plc_MZ2 (plcid_MZ2) — _inc/Pattern Load Cues.asm lines 183-189 */
static const plc_entry plc_MZ2[] = {
    plc_decl(Nem_MzSwitch, ArtTile_Button_Main),
    plc_decl(Nem_Spikes  , ArtTile_Spikes),
    plc_decl(Nem_HSpring , ArtTile_Spring_Horizontal),
    plc_decl(Nem_VSpring , ArtTile_Spring_Vertical),
    plc_decl(Nem_MzBlock , ArtTile_MZ_Block),
};

/* plc_SLZ (plcid_SLZ) — _inc/Pattern Load Cues.asm lines 194-204 */
static const plc_entry plc_SLZ[] = {
    plc_decl(Nem_SLZ     , ArtTile_Level),
    plc_decl(Nem_Bomb    , ArtTile_Bomb),
    plc_decl(Nem_Orbinaut, ArtTile_SLZ_Orbinaut),
    plc_decl(Nem_MzFire  , ArtTile_SLZ_Fireball),
    plc_decl(Nem_SlzBlock, ArtTile_SLZ_Collapsing_Floor),
    plc_decl(Nem_SlzWall , ArtTile_GHZ_SLZ_Smashable_Wall+4),
    plc_decl(Nem_Spikes  , ArtTile_Spikes),
    plc_decl(Nem_HSpring , ArtTile_Spring_Horizontal),
    plc_decl(Nem_VSpring , ArtTile_Spring_Vertical),
};

/* plc_SLZ2 (plcid_SLZ2) — _inc/Pattern Load Cues.asm lines 206-213 */
static const plc_entry plc_SLZ2[] = {
    plc_decl(Nem_Seesaw   , ArtTile_SLZ_Seesaw),
    plc_decl(Nem_Fan      , ArtTile_SLZ_Fan),
    plc_decl(Nem_Pylon    , ArtTile_SLZ_Pylon),
    plc_decl(Nem_SlzSwing , ArtTile_SLZ_Swing),
    plc_decl(Nem_SlzCannon, ArtTile_SLZ_Fireball_Launcher),
    plc_decl(Nem_SlzSpike , ArtTile_SLZ_Spikeball),
};

/* plc_SYZ (plcid_SYZ) — _inc/Pattern Load Cues.asm lines 218-224 */
static const plc_entry plc_SYZ[] = {
    plc_decl(Nem_SYZ     , ArtTile_Level),
    plc_decl(Nem_Crabmeat, ArtTile_Crabmeat),
    plc_decl(Nem_Buzz    , ArtTile_Buzz_Bomber),
    plc_decl(Nem_Yadrin  , ArtTile_Yadrin),
    plc_decl(Nem_Roller  , ArtTile_Roller),
};

/* plc_SYZ2 (plcid_SYZ2) — _inc/Pattern Load Cues.asm lines 226-238 (FixBugs=0) */
static const plc_entry plc_SYZ2[] = {
    plc_decl(Nem_Bumper   , ArtTile_SYZ_Bumper),
    plc_decl(Nem_SyzSpike1, ArtTile_SYZ_Big_Spikeball),
    plc_decl(Nem_SyzSpike2, ArtTile_SYZ_Spikeball_Chain),
    plc_decl(Nem_Cater    , ArtTile_MZ_SYZ_Caterkiller),
    plc_decl(Nem_LzSwitch , ArtTile_Button),
    plc_decl(Nem_Spikes   , ArtTile_Spikes),
    plc_decl(Nem_HSpring  , ArtTile_Spring_Horizontal),
    plc_decl(Nem_VSpring  , ArtTile_Spring_Vertical),
};

/* plc_SBZ (plcid_SBZ) — _inc/Pattern Load Cues.asm lines 243-256 */
static const plc_entry plc_SBZ[] = {
    plc_decl(Nem_SBZ      , ArtTile_Level),
    plc_decl(Nem_Stomper  , ArtTile_SBZ_Moving_Block_Short),
    plc_decl(Nem_SbzDoor1 , ArtTile_SBZ_Door),
    plc_decl(Nem_Girder   , ArtTile_SBZ_Girder),
    plc_decl(Nem_BallHog  , ArtTile_Ball_Hog),
    plc_decl(Nem_SbzWheel1, ArtTile_SBZ_Disc),
    plc_decl(Nem_SbzWheel2, ArtTile_SBZ_Junction),
    plc_decl(Nem_SyzSpike1, ArtTile_SBZ_Swing),
    plc_decl(Nem_Cutter   , ArtTile_SBZ_Saw),
    plc_decl(Nem_FlamePipe, ArtTile_SBZ_Flamethrower),
    plc_decl(Nem_SbzFloor , ArtTile_SBZ_Collapsing_Floor),
    plc_decl(Nem_SbzBlock , ArtTile_SBZ_Vanishing_Block),
};

/* plc_SBZ2 (plcid_SBZ2) — _inc/Pattern Load Cues.asm lines 258-272 */
static const plc_entry plc_SBZ2[] = {
    plc_decl(Nem_Cater     , ArtTile_SBZ_Caterkiller),
    plc_decl(Nem_Bomb      , ArtTile_Bomb),
    plc_decl(Nem_Orbinaut  , ArtTile_SBZ_Orbinaut),
    plc_decl(Nem_SlideFloor, ArtTile_SBZ_Moving_Block_Long),
    plc_decl(Nem_SbzDoor2  , ArtTile_SBZ_Horizontal_Door),
    plc_decl(Nem_Electric  , ArtTile_SBZ_Electric_Orb),
    plc_decl(Nem_TrapDoor  , ArtTile_SBZ_Trap_Door),
    plc_decl(Nem_SbzFloor  , ArtTile_SBZ_Collapsing_Floor+4),
    plc_decl(Nem_SpinPform , ArtTile_SBZ_Spinning_Platform),
    plc_decl(Nem_LzSwitch  , ArtTile_Button),
    plc_decl(Nem_Spikes    , ArtTile_Spikes),
    plc_decl(Nem_HSpring   , ArtTile_Spring_Horizontal),
    plc_decl(Nem_VSpring   , ArtTile_Spring_Vertical),
};

/* plc_Boss (plcid_Boss) — _inc/Pattern Load Cues.asm lines 284-291 */
static const plc_entry plc_Boss[] = {
    plc_decl(Nem_Eggman  , ArtTile_Eggman),
    plc_decl(Nem_Weapons , ArtTile_Eggman_Weapons),
    plc_decl(Nem_Prison  , ArtTile_Prison_Capsule),
    plc_decl(Nem_Bomb    , ArtTile_Eggman_Spikeball),
    plc_decl(Nem_SlzSpike, ArtTile_Eggman_Spikeball),
    plc_decl(Nem_Exhaust , ArtTile_Eggman_Exhaust),
};

/* plc_SpecialStage (plcid_SpecialStage) — _inc/Pattern Load Cues.asm lines 315-333 */
static const plc_entry plc_SpecialStage[] = {
    plc_decl(Nem_SSBgCloud , ArtTile_SS_Background_Clouds),
    plc_decl(Nem_SSBgFish  , ArtTile_SS_Background_Fish),
    plc_decl(Nem_SSWalls   , ArtTile_SS_Wall),
    plc_decl(Nem_Bumper    , ArtTile_SS_Bumper),
    plc_decl(Nem_SSGOAL    , ArtTile_SS_Goal),
    plc_decl(Nem_SSUpDown  , ArtTile_SS_Up_Down),
    plc_decl(Nem_SSRBlock  , ArtTile_SS_R_Block),
    plc_decl(Nem_SS1UpBlock, ArtTile_SS_Extra_Life),
    plc_decl(Nem_SSEmStars , ArtTile_SS_Emerald_Sparkle),
    plc_decl(Nem_SSRedWhite, ArtTile_SS_Red_White_Block),
    plc_decl(Nem_SSGhost   , ArtTile_SS_Ghost_Block),
    plc_decl(Nem_SSWBlock  , ArtTile_SS_W_Block),
    plc_decl(Nem_SSGlass   , ArtTile_SS_Glass),
    plc_decl(Nem_SSEmerald , ArtTile_SS_Emerald),
    plc_decl(Nem_SSZone1   , ArtTile_SS_Zone_1),
    plc_decl(Nem_SSZone2   , ArtTile_SS_Zone_2),
    plc_decl(Nem_SSZone3   , ArtTile_SS_Zone_3),
};

/* plc_SSResult (plcid_SSResult) — _inc/Pattern Load Cues.asm lines 391-394 */
static const plc_entry plc_SSResult[] = {
    plc_decl(Nem_ResultEm , ArtTile_SS_Results_Emeralds),
    plc_decl(Nem_MiniSonic, ArtTile_Mini_Sonic),
};

/* plc_Ending (plcid_Ending) — _inc/Pattern Load Cues.asm lines 399-417 (REV01) */
static const plc_entry plc_Ending[] = {
    plc_decl(Nem_GHZ_1st  , ArtTile_Level),
    plc_decl(Nem_GHZ_2nd  , ArtTile_Level+0x1CD),
    plc_decl(Nem_Stalk    , ArtTile_GHZ_Flower_Stalk),
    plc_decl(Nem_EndFlower, ArtTile_Ending_Flowers),
    plc_decl(Nem_EndEm    , ArtTile_Ending_Emeralds),
    plc_decl(Nem_EndSonic , ArtTile_Ending_Sonic),
    plc_decl(Nem_Rabbit   , ArtTile_Ending_Rabbit),
    plc_decl(Nem_Chicken  , ArtTile_Ending_Chicken),
    plc_decl(Nem_Penguin  , ArtTile_Ending_Penguin),
    plc_decl(Nem_Seal     , ArtTile_Ending_Seal),
    plc_decl(Nem_Pig      , ArtTile_Ending_Pig),
    plc_decl(Nem_Flicky   , ArtTile_Ending_Flicky),
    plc_decl(Nem_Squirrel , ArtTile_Ending_Squirrel),
    plc_decl(Nem_EndStH   , ArtTile_Ending_STH),
};

/* plc_TryAgain (plcid_TryAgain) — _inc/Pattern Load Cues.asm lines 422-426 */
static const plc_entry plc_TryAgain[] = {
    plc_decl(Nem_EndEm     , ArtTile_Try_Again_Emeralds),
    plc_decl(Nem_TryAgain  , ArtTile_Try_Again_Eggman),
    plc_decl(Nem_CreditText, ArtTile_Credits_Font),
};

/* plc_EggmanSBZ2 (plcid_EggmanSBZ2) — _inc/Pattern Load Cues.asm lines 431-435 */
static const plc_entry plc_EggmanSBZ2[] = {
    plc_decl(Nem_SbzBlock  , ArtTile_Eggman_Trap_Floor),
    plc_decl(Nem_Sbz2Eggman, ArtTile_Eggman),
    plc_decl(Nem_LzSwitch  , ArtTile_Eggman_Button-4),
};

/* plc_FZBoss (plcid_FZBoss) — _inc/Pattern Load Cues.asm lines 440-446 */
static const plc_entry plc_FZBoss[] = {
    plc_decl(Nem_FzEggman  , ArtTile_FZ_Eggman_Fleeing),
    plc_decl(Nem_FzBoss    , ArtTile_FZ_Boss),
    plc_decl(Nem_Eggman    , ArtTile_Eggman),
    plc_decl(Nem_Sbz2Eggman, ArtTile_FZ_Eggman_No_Vehicle),
    plc_decl(Nem_Exhaust   , ArtTile_Eggman_Exhaust),
};



/* Static asset list sentinels (the value of a pointer is not a constant) */
static const plc_entry plc_Empty[] = { { 0, 0 } };

/* ------------------------------------------------------------------
   The ASM queue stores a 32-bit dc.l source address; host pointers are
   64-bit, so the RAM 6-byte slot can only hold the dest.  plc_src[]
   mirrors the RAM queue (same slot index) with the real pointers.
   A NULL entry means the slot is free, matching the ASM tst.l check.
   ------------------------------------------------------------------ */
static const uint8_t *plc_src[plc_slot_count];

/* ArtLoadCues index — must stay in the same order as plcid_* in
   constants.h (see _inc/Pattern Load Cues.asm lines 27-68).  Counts
   mirror the plcheader value ((size)/6)-1; -1 = no list. */
typedef struct {
    const plc_entry *entries;
    int              count;
} plc_list;

#define PLC_NONE { plc_Empty, -1 }

static const plc_list plc_index[] = {
    /* 0: */           { plc_Main,         (int)(sizeof(plc_Main) / sizeof(plc_Main[0])) - 1 },
    /* 1: */           { plc_Main2,        (int)(sizeof(plc_Main2) / sizeof(plc_Main2[0])) - 1 },
    /* 2: */           { plc_Explode,      (int)(sizeof(plc_Explode) / sizeof(plc_Explode[0])) - 1 },
    /* 3: */           { plc_GameOver,     (int)(sizeof(plc_GameOver) / sizeof(plc_GameOver[0])) - 1 },
    /* 4: */           { plc_GHZ,          (int)(sizeof(plc_GHZ) / sizeof(plc_GHZ[0])) - 1 },
    /* 5: */           { plc_GHZ2,         (int)(sizeof(plc_GHZ2) / sizeof(plc_GHZ2[0])) - 1 },
    /* 6: */           { plc_LZ,           (int)(sizeof(plc_LZ) / sizeof(plc_LZ[0])) - 1 },
    /* 7: */           { plc_LZ2,          (int)(sizeof(plc_LZ2) / sizeof(plc_LZ2[0])) - 1 },
    /* 8: */           { plc_MZ,           (int)(sizeof(plc_MZ) / sizeof(plc_MZ[0])) - 1 },
    /* 9: */           { plc_MZ2,          (int)(sizeof(plc_MZ2) / sizeof(plc_MZ2[0])) - 1 },
    /* 10: */          { plc_SLZ,          (int)(sizeof(plc_SLZ) / sizeof(plc_SLZ[0])) - 1 },
    /* 11: */          { plc_SLZ2,         (int)(sizeof(plc_SLZ2) / sizeof(plc_SLZ2[0])) - 1 },
    /* 12: */          { plc_SYZ,          (int)(sizeof(plc_SYZ) / sizeof(plc_SYZ[0])) - 1 },
    /* 13: */          { plc_SYZ2,         (int)(sizeof(plc_SYZ2) / sizeof(plc_SYZ2[0])) - 1 },
    /* 14: */          { plc_SBZ,          (int)(sizeof(plc_SBZ) / sizeof(plc_SBZ[0])) - 1 },
    /* 15: */          { plc_SBZ2,         (int)(sizeof(plc_SBZ2) / sizeof(plc_SBZ2[0])) - 1 },
    /* 16: */          { plc_TitleCard,    (int)(sizeof(plc_TitleCard) / sizeof(plc_TitleCard[0])) - 1 },
    /* 17: */          { plc_Boss,         (int)(sizeof(plc_Boss) / sizeof(plc_Boss[0])) - 1 },
    /* 18: */          { plc_Signpost,     (int)(sizeof(plc_Signpost) / sizeof(plc_Signpost[0])) - 1 },
    /* 19: */          { plc_SpecialStage, (int)(sizeof(plc_SpecialStage) / sizeof(plc_SpecialStage[0])) - 1 }, /* REV01: PLC_Warp aliases PLC_SpecialStage */
    /* 20: */          { plc_SpecialStage, (int)(sizeof(plc_SpecialStage) / sizeof(plc_SpecialStage[0])) - 1 },
    /* 21: */          { plc_GHZAnimals,   (int)(sizeof(plc_GHZAnimals) / sizeof(plc_GHZAnimals[0])) - 1 },
    /* 22: */          { plc_LZAnimals,    (int)(sizeof(plc_LZAnimals) / sizeof(plc_LZAnimals[0])) - 1 },
    /* 23: */          { plc_MZAnimals,    (int)(sizeof(plc_MZAnimals) / sizeof(plc_MZAnimals[0])) - 1 },
    /* 24: */          { plc_SLZAnimals,   (int)(sizeof(plc_SLZAnimals) / sizeof(plc_SLZAnimals[0])) - 1 },
    /* 25: */          { plc_SYZAnimals,   (int)(sizeof(plc_SYZAnimals) / sizeof(plc_SYZAnimals[0])) - 1 },
    /* 26: */          { plc_SBZAnimals,   (int)(sizeof(plc_SBZAnimals) / sizeof(plc_SBZAnimals[0])) - 1 },
    /* 27: */          { plc_SSResult,     (int)(sizeof(plc_SSResult) / sizeof(plc_SSResult[0])) - 1 },
    /* 28: */          { plc_Ending,       (int)(sizeof(plc_Ending) / sizeof(plc_Ending[0])) - 1 },
    /* 29: */          { plc_TryAgain,     (int)(sizeof(plc_TryAgain) / sizeof(plc_TryAgain[0])) - 1 },
    /* 30: */          { plc_EggmanSBZ2,   (int)(sizeof(plc_EggmanSBZ2) / sizeof(plc_EggmanSBZ2[0])) - 1 },
    /* 31: */          { plc_FZBoss,       (int)(sizeof(plc_FZBoss) / sizeof(plc_FZBoss[0])) - 1 },
};

/* Clear all pending PLC entries (from sonic.asm ClearPLC) */
void ClearPLC(void) {
    memset(RAM_ADDR(v_plc_buffer), 0, plc_slot_size * plc_slot_count);
    memset(plc_src, 0, sizeof(plc_src));
}

/* Add a PLC list to the queue (sonic.asm AddPLC)
   Copies the src/dest pairs of the list pointed to by `id` (from the
   ArtLoadCues index into the first free v_plc_buffer slot. */
void AddPLC(int id) {
    const plc_list *list;
    int i, slot;

    if (id < 0 || id >= (int)(sizeof(plc_index) / sizeof(plc_index[0]))) {
        return;
    }
    list = &plc_index[id];
    if (list->count < 0) {
        return;                          /* no list: bmi.s .return */
    }

    /* findspace: first free slot is signalled by a NULL source pointer */
    for (slot = 0; slot < plc_slot_count && plc_src[slot] != NULL; slot++) {
        /* advance to next slot */
    }
    if (slot >= plc_slot_count) {
        return;                          /* queue full */
    }

    /* copytoRAM: entries in list order (first entry first), matching the
       ASM (a1)+ copy loop; asset-failed (NULL) entries are skipped */
    for (i = 0; i <= list->count; i++) {
        const uint8_t *p = *list->entries[i].src;
        if (p == NULL) {
            continue;
        }
        if (slot >= plc_slot_count) {
            return;                      /* queue full */
        }
        plc_src[slot] = p;
        *(uint16_t *)(RAM_ADDR(v_plc_buffer) + slot * plc_slot_size + 4) =
            list->entries[i].dest;
        slot++;
    }
}

/* Start a new PLC (clear + add) */
void NewPLC(int id) {
    ClearPLC();
    AddPLC(id);
}

/* Non-zero when the PLC queue is empty (ASM: tst.l (v_plc_buffer).w) */
int PLC_IsEmpty(void) {
    return plc_src[0] == NULL;
}

/* Process one pending PLC entry per frame (sonic.asm RunPLC)
   The ASM runs a fine-grained Nemesis state machine across VBlank
   (ProcessPLC_9Tiles); the port's NemDecToVRAM() is synchronous, so
   decompression happens here and the entry is dropped from the queue. */
void RunPLC(void) {
    uint8_t *slot;
    int i;

    if (plc_src[0] == NULL) {
        return;                          /* nothing queued */
    }

    slot = RAM_ADDR(v_plc_buffer);
    NemDecToVRAM(plc_src[0],
                 (uint32_t)*(uint16_t *)(slot + 4) * tile_size);

    /* drop the processed entry: shift the queue down one slot */
    for (i = 1; i < plc_slot_count; i++) {
        plc_src[i - 1] = plc_src[i];
    }
    plc_src[plc_slot_count - 1] = NULL;

    memmove(slot, slot + plc_slot_size,
            (size_t)(RAM_ADDR(v_plc_buffer_only_end) - slot) - plc_slot_size);
    memset(RAM_ADDR(v_plc_buffer_only_end) - plc_slot_size, 0, plc_slot_size);
}
