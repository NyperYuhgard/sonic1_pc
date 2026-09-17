#include "data.h"
#include "assets.h"
#include "palette.h"
#include "constants.h"
#include "level.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>

/* Object mapping pointers are stored in 32-bit fields (obMap), so mapping
   data must live below 4 GB. MAP_32BIT asks the kernel for such an address.
   A size_t header is kept before the data so the mapping can be munmap'd
   later (the buffer is not a malloc chunk and cannot be free()'d). */
static const uint8_t *alloc_32bit(size_t n) {
    size_t hdr = sizeof(size_t);
    void *base = mmap(NULL, n + hdr, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (base == MAP_FAILED) return NULL;
    ((size_t *)base)[0] = n;
    return (const uint8_t *)base + hdr;
}

static void free_32bit(const uint8_t *p) {
    if (!p) return;
    size_t n = ((const size_t *)p)[-1];
    munmap((void *)(p - sizeof(size_t)), n + sizeof(size_t));
}

/* ============================================================================
   Asset pointers
   ============================================================================ */
const uint8_t *Pal_SegaBG = NULL;
size_t   Pal_SegaBG_len = 0;

const uint8_t *Pal_Sega1 = NULL;
size_t   Pal_Sega1_len = 0;

const uint8_t *Pal_Sega2 = NULL;
size_t   Pal_Sega2_len = 0;

const uint8_t *Nem_SegaLogo = NULL;
size_t   Nem_SegaLogo_len = 0;

const uint8_t *Eni_SegaLogo = NULL;
size_t   Eni_SegaLogo_len = 0;

const uint8_t *Pal_Title = NULL;
size_t   Pal_Title_len = 0;

const uint8_t *Pal_TitleCycWater = NULL;
size_t   Pal_TitleCycWater_len = 0;

const uint8_t *Pal_GHZCycWater = NULL;
size_t   Pal_GHZCycWater_len = 0;

const uint8_t *Pal_LevelSel = NULL;
size_t   Pal_LevelSel_len = 0;

const uint8_t *Pal_Sonic = NULL;
size_t   Pal_Sonic_len = 0;

const uint8_t *Pal_GHZ = NULL;
size_t   Pal_GHZ_len = 0;

const uint8_t *Pal_LZ = NULL;
size_t   Pal_LZ_len = 0;
const uint8_t *Pal_LZWater = NULL;
size_t   Pal_LZWater_len = 0;
const uint8_t *Pal_LZSonWater = NULL;
size_t   Pal_LZSonWater_len = 0;
const uint8_t *Pal_MZ = NULL;
size_t   Pal_MZ_len = 0;
const uint8_t *Pal_SLZ = NULL;
size_t   Pal_SLZ_len = 0;
const uint8_t *Pal_SYZ = NULL;
size_t   Pal_SYZ_len = 0;
const uint8_t *Pal_SBZ1 = NULL;
size_t   Pal_SBZ1_len = 0;
const uint8_t *Pal_SBZ2 = NULL;
size_t   Pal_SBZ2_len = 0;
const uint8_t *Pal_SBZ3 = NULL;
size_t   Pal_SBZ3_len = 0;
const uint8_t *Pal_SBZ3Water = NULL;
size_t   Pal_SBZ3Water_len = 0;
const uint8_t *Pal_SBZ3SonWat = NULL;
size_t   Pal_SBZ3SonWat_len = 0;
const uint8_t *Pal_Special = NULL;
size_t   Pal_Special_len = 0;
const uint8_t *Pal_SSResult = NULL;
size_t   Pal_SSResult_len = 0;
const uint8_t *Pal_Continue = NULL;
size_t   Pal_Continue_len = 0;
const uint8_t *Pal_Ending = NULL;
size_t   Pal_Ending_len = 0;

const uint8_t *Nem_JapNames = NULL;
size_t   Nem_JapNames_len = 0;

const uint8_t *Eni_JapNames = NULL;
size_t   Eni_JapNames_len = 0;

const uint8_t *Nem_CreditText = NULL;
size_t   Nem_CreditText_len = 0;

const uint8_t *Nem_TitleFg = NULL;
size_t   Nem_TitleFg_len = 0;

const uint8_t *Nem_TitleSonic = NULL;
size_t   Nem_TitleSonic_len = 0;

const uint8_t *Nem_TitleTM = NULL;
size_t   Nem_TitleTM_len = 0;

const uint8_t *Art_Text = NULL;
size_t   Art_Text_len = 0;

const uint8_t *Blk16_GHZ = NULL;
size_t   Blk16_GHZ_len = 0;

const uint8_t *Blk256_GHZ = NULL;
size_t   Blk256_GHZ_len = 0;

const uint8_t *Blk16_LZ = NULL;
size_t   Blk16_LZ_len = 0;

const uint8_t *Blk256_LZ = NULL;
size_t   Blk256_LZ_len = 0;

const uint8_t *Blk16_MZ = NULL;
size_t   Blk16_MZ_len = 0;

const uint8_t *Blk256_MZ = NULL;
size_t   Blk256_MZ_len = 0;

const uint8_t *Blk16_SLZ = NULL;
size_t   Blk16_SLZ_len = 0;

const uint8_t *Blk256_SLZ = NULL;
size_t   Blk256_SLZ_len = 0;

const uint8_t *Blk16_SYZ = NULL;
size_t   Blk16_SYZ_len = 0;

const uint8_t *Blk256_SYZ = NULL;
size_t   Blk256_SYZ_len = 0;

const uint8_t *Blk16_SBZ = NULL;
size_t   Blk16_SBZ_len = 0;

const uint8_t *Blk256_SBZ = NULL;
size_t   Blk256_SBZ_len = 0;

const uint8_t *Eni_Title = NULL;
size_t   Eni_Title_len = 0;

const uint8_t *Nem_GHZ_1st = NULL;
size_t   Nem_GHZ_1st_len = 0;

/* Animal art (artnem/Animal *.nem) — used by PLC_*Animals */
const uint8_t *Nem_Rabbit = NULL;
size_t   Nem_Rabbit_len = 0;
const uint8_t *Nem_Chicken = NULL;
size_t   Nem_Chicken_len = 0;
const uint8_t *Nem_Penguin = NULL;
size_t   Nem_Penguin_len = 0;
const uint8_t *Nem_Seal = NULL;
size_t   Nem_Seal_len = 0;
const uint8_t *Nem_Pig = NULL;
size_t   Nem_Pig_len = 0;
const uint8_t *Nem_Flicky = NULL;
size_t   Nem_Flicky_len = 0;
const uint8_t *Nem_Squirrel = NULL;
size_t   Nem_Squirrel_len = 0;

// Remaining PLC art (artnem/*.nem verbatim names)
const uint8_t *Nem_BallHog = NULL;
size_t   Nem_BallHog_len = 0;
const uint8_t *Nem_Basaran = NULL;
size_t   Nem_Basaran_len = 0;
const uint8_t *Nem_Bomb = NULL;
size_t   Nem_Bomb_len = 0;
const uint8_t *Nem_Bubbles = NULL;
size_t   Nem_Bubbles_len = 0;
const uint8_t *Nem_Bumper = NULL;
size_t   Nem_Bumper_len = 0;
const uint8_t *Nem_Burrobot = NULL;
size_t   Nem_Burrobot_len = 0;
const uint8_t *Nem_Cater = NULL;
size_t   Nem_Cater_len = 0;
const uint8_t *Nem_Cork = NULL;
size_t   Nem_Cork_len = 0;
const uint8_t *Nem_Cutter = NULL;
size_t   Nem_Cutter_len = 0;
const uint8_t *Nem_Eggman = NULL;
size_t   Nem_Eggman_len = 0;
const uint8_t *Nem_Electric = NULL;
size_t   Nem_Electric_len = 0;
const uint8_t *Nem_EndEm = NULL;
size_t   Nem_EndEm_len = 0;
const uint8_t *Nem_EndFlower = NULL;
size_t   Nem_EndFlower_len = 0;
const uint8_t *Nem_EndSonic = NULL;
size_t   Nem_EndSonic_len = 0;
const uint8_t *Nem_EndStH = NULL;
size_t   Nem_EndStH_len = 0;
const uint8_t *Nem_Exhaust = NULL;
size_t   Nem_Exhaust_len = 0;
const uint8_t *Nem_Fan = NULL;
size_t   Nem_Fan_len = 0;
const uint8_t *Nem_FlamePipe = NULL;
size_t   Nem_FlamePipe_len = 0;
const uint8_t *Nem_FlapDoor = NULL;
size_t   Nem_FlapDoor_len = 0;
const uint8_t *Nem_FzBoss = NULL;
size_t   Nem_FzBoss_len = 0;
const uint8_t *Nem_FzEggman = NULL;
size_t   Nem_FzEggman_len = 0;
const uint8_t *Nem_GameOver = NULL;
size_t   Nem_GameOver_len = 0;
const uint8_t *Nem_Gargoyle = NULL;
size_t   Nem_Gargoyle_len = 0;
const uint8_t *Nem_Girder = NULL;
size_t   Nem_Girder_len = 0;
const uint8_t *Nem_Harpoon = NULL;
size_t   Nem_Harpoon_len = 0;
const uint8_t *Nem_Jaws = NULL;
size_t   Nem_Jaws_len = 0;
const uint8_t *Nem_LZ = NULL;
size_t   Nem_LZ_len = 0;
const uint8_t *Nem_Lamp = NULL;
size_t   Nem_Lamp_len = 0;
const uint8_t *Nem_Lava = NULL;
size_t   Nem_Lava_len = 0;
const uint8_t *Nem_LzBlock1 = NULL;
size_t   Nem_LzBlock1_len = 0;
const uint8_t *Nem_LzBlock2 = NULL;
size_t   Nem_LzBlock2_len = 0;
const uint8_t *Nem_LzBlock3 = NULL;
size_t   Nem_LzBlock3_len = 0;
const uint8_t *Nem_LzDoor1 = NULL;
size_t   Nem_LzDoor1_len = 0;
const uint8_t *Nem_LzDoor2 = NULL;
size_t   Nem_LzDoor2_len = 0;
const uint8_t *Nem_LzPlatfm = NULL;
size_t   Nem_LzPlatfm_len = 0;
const uint8_t *Nem_LzPole = NULL;
size_t   Nem_LzPole_len = 0;
const uint8_t *Nem_LzSpikeBall = NULL;
size_t   Nem_LzSpikeBall_len = 0;
const uint8_t *Nem_LzSwitch = NULL;
size_t   Nem_LzSwitch_len = 0;
const uint8_t *Nem_LzWheel = NULL;
size_t   Nem_LzWheel_len = 0;
const uint8_t *Nem_MZ = NULL;
size_t   Nem_MZ_len = 0;
const uint8_t *Nem_MiniSonic = NULL;
size_t   Nem_MiniSonic_len = 0;
const uint8_t *Nem_MzBlock = NULL;
size_t   Nem_MzBlock_len = 0;
const uint8_t *Nem_MzFire = NULL;
size_t   Nem_MzFire_len = 0;
const uint8_t *Nem_MzGlass = NULL;
size_t   Nem_MzGlass_len = 0;
const uint8_t *Nem_MzMetal = NULL;
size_t   Nem_MzMetal_len = 0;
const uint8_t *Nem_MzSwitch = NULL;
size_t   Nem_MzSwitch_len = 0;
const uint8_t *Nem_Orbinaut = NULL;
size_t   Nem_Orbinaut_len = 0;
const uint8_t *Nem_Points = NULL;
size_t   Nem_Points_len = 0;
const uint8_t *Nem_Prison = NULL;
size_t   Nem_Prison_len = 0;
const uint8_t *Nem_Pylon = NULL;
size_t   Nem_Pylon_len = 0;
const uint8_t *Nem_ResultEm = NULL;
size_t   Nem_ResultEm_len = 0;
const uint8_t *Nem_Roller = NULL;
size_t   Nem_Roller_len = 0;
const uint8_t *Nem_SBZ = NULL;
size_t   Nem_SBZ_len = 0;
const uint8_t *Nem_SLZ = NULL;
size_t   Nem_SLZ_len = 0;
const uint8_t *Nem_SS1UpBlock = NULL;
size_t   Nem_SS1UpBlock_len = 0;
const uint8_t *Nem_SSBgCloud = NULL;
size_t   Nem_SSBgCloud_len = 0;
const uint8_t *Nem_SSBgFish = NULL;
size_t   Nem_SSBgFish_len = 0;
const uint8_t *Nem_SSEmStars = NULL;
size_t   Nem_SSEmStars_len = 0;
const uint8_t *Nem_SSEmerald = NULL;
size_t   Nem_SSEmerald_len = 0;
const uint8_t *Nem_SSGOAL = NULL;
size_t   Nem_SSGOAL_len = 0;
const uint8_t *Nem_SSGhost = NULL;
size_t   Nem_SSGhost_len = 0;
const uint8_t *Nem_SSGlass = NULL;
size_t   Nem_SSGlass_len = 0;
const uint8_t *Nem_SSRBlock = NULL;
size_t   Nem_SSRBlock_len = 0;
const uint8_t *Nem_SSRedWhite = NULL;
size_t   Nem_SSRedWhite_len = 0;
const uint8_t *Nem_SSUpDown = NULL;
size_t   Nem_SSUpDown_len = 0;
const uint8_t *Nem_SSWBlock = NULL;
size_t   Nem_SSWBlock_len = 0;
const uint8_t *Nem_SSWalls = NULL;
size_t   Nem_SSWalls_len = 0;
const uint8_t *Nem_SSZone1 = NULL;
size_t   Nem_SSZone1_len = 0;
const uint8_t *Nem_SSZone2 = NULL;
size_t   Nem_SSZone2_len = 0;
const uint8_t *Nem_SSZone3 = NULL;
size_t   Nem_SSZone3_len = 0;
const uint8_t *Nem_SSZone4 = NULL;
size_t   Nem_SSZone4_len = 0;
const uint8_t *Nem_SSZone5 = NULL;
size_t   Nem_SSZone5_len = 0;
const uint8_t *Nem_SSZone6 = NULL;
size_t   Nem_SSZone6_len = 0;
const uint8_t *Nem_SYZ = NULL;
size_t   Nem_SYZ_len = 0;
const uint8_t *Nem_Sbz2Eggman = NULL;
size_t   Nem_Sbz2Eggman_len = 0;
const uint8_t *Nem_SbzBlock = NULL;
size_t   Nem_SbzBlock_len = 0;
const uint8_t *Nem_SbzDoor1 = NULL;
size_t   Nem_SbzDoor1_len = 0;
const uint8_t *Nem_SbzDoor2 = NULL;
size_t   Nem_SbzDoor2_len = 0;
const uint8_t *Nem_SbzFloor = NULL;
size_t   Nem_SbzFloor_len = 0;
const uint8_t *Nem_SbzWheel1 = NULL;
size_t   Nem_SbzWheel1_len = 0;
const uint8_t *Nem_SbzWheel2 = NULL;
size_t   Nem_SbzWheel2_len = 0;
const uint8_t *Nem_Seesaw = NULL;
size_t   Nem_Seesaw_len = 0;
const uint8_t *Nem_SlideFloor = NULL;
size_t   Nem_SlideFloor_len = 0;
const uint8_t *Nem_SlzBlock = NULL;
size_t   Nem_SlzBlock_len = 0;
const uint8_t *Nem_SlzCannon = NULL;
size_t   Nem_SlzCannon_len = 0;
const uint8_t *Nem_SlzSpike = NULL;
size_t   Nem_SlzSpike_len = 0;
const uint8_t *Nem_SlzSwing = NULL;
size_t   Nem_SlzSwing_len = 0;
const uint8_t *Nem_SlzWall = NULL;
size_t   Nem_SlzWall_len = 0;
const uint8_t *Nem_SpinPform = NULL;
size_t   Nem_SpinPform_len = 0;
const uint8_t *Nem_Splash = NULL;
size_t   Nem_Splash_len = 0;
const uint8_t *Nem_Stomper = NULL;
size_t   Nem_Stomper_len = 0;
const uint8_t *Nem_SyzSpike1 = NULL;
size_t   Nem_SyzSpike1_len = 0;
const uint8_t *Nem_SyzSpike2 = NULL;
size_t   Nem_SyzSpike2_len = 0;
const uint8_t *Nem_TrapDoor = NULL;
size_t   Nem_TrapDoor_len = 0;
const uint8_t *Nem_TryAgain = NULL;
size_t   Nem_TryAgain_len = 0;
const uint8_t *Nem_Water = NULL;
size_t   Nem_Water_len = 0;
const uint8_t *Nem_Weapons = NULL;
size_t   Nem_Weapons_len = 0;
const uint8_t *Nem_Yadrin = NULL;
size_t   Nem_Yadrin_len = 0;

const uint8_t *Level_GHZ1 = NULL;
size_t   Level_GHZ1_len = 0;

const uint8_t *Level_GHZbg = NULL;
size_t   Level_GHZbg_len = 0;

const uint8_t *Level_GHZ2 = NULL;
size_t   Level_GHZ2_len = 0;
const uint8_t *Level_GHZ3 = NULL;
size_t   Level_GHZ3_len = 0;
const uint8_t *Level_LZ1 = NULL;
size_t   Level_LZ1_len = 0;
const uint8_t *Level_LZ2 = NULL;
size_t   Level_LZ2_len = 0;
const uint8_t *Level_LZ3 = NULL;
size_t   Level_LZ3_len = 0;
const uint8_t *Level_LZbg = NULL;
size_t   Level_LZbg_len = 0;
const uint8_t *Level_SBZ3 = NULL;
size_t   Level_SBZ3_len = 0;
const uint8_t *Level_MZ1 = NULL;
size_t   Level_MZ1_len = 0;
const uint8_t *Level_MZ1bg = NULL;
size_t   Level_MZ1bg_len = 0;
const uint8_t *Level_MZ2 = NULL;
size_t   Level_MZ2_len = 0;
const uint8_t *Level_MZ2bg = NULL;
size_t   Level_MZ2bg_len = 0;
const uint8_t *Level_MZ3 = NULL;
size_t   Level_MZ3_len = 0;
const uint8_t *Level_MZ3bg = NULL;
size_t   Level_MZ3bg_len = 0;
const uint8_t *Level_SLZ1 = NULL;
size_t   Level_SLZ1_len = 0;
const uint8_t *Level_SLZ2 = NULL;
size_t   Level_SLZ2_len = 0;
const uint8_t *Level_SLZ3 = NULL;
size_t   Level_SLZ3_len = 0;
const uint8_t *Level_SLZbg = NULL;
size_t   Level_SLZbg_len = 0;
const uint8_t *Level_SYZ1 = NULL;
size_t   Level_SYZ1_len = 0;
const uint8_t *Level_SYZ2 = NULL;
size_t   Level_SYZ2_len = 0;
const uint8_t *Level_SYZ3 = NULL;
size_t   Level_SYZ3_len = 0;
const uint8_t *Level_SYZbg = NULL;
size_t   Level_SYZbg_len = 0;
const uint8_t *Level_SBZ1 = NULL;
size_t   Level_SBZ1_len = 0;
const uint8_t *Level_SBZ1bg = NULL;
size_t   Level_SBZ1bg_len = 0;
const uint8_t *Level_SBZ2 = NULL;
size_t   Level_SBZ2_len = 0;
const uint8_t *Level_SBZ2bg = NULL;
size_t   Level_SBZ2bg_len = 0;
const uint8_t *Level_End = NULL;
size_t   Level_End_len = 0;

/* Level PLC graphics (PLC_GHZ + PLC_Main2). Only Nem_GHZ_1st is staged so
   far; these stay NULL (AddPLC skips them) until their assets land. */
const uint8_t *Nem_GHZ_2nd = NULL;
size_t   Nem_GHZ_2nd_len = 0;
const uint8_t *Nem_Stalk = NULL;
size_t   Nem_Stalk_len = 0;
const uint8_t *Nem_PplRock = NULL;
size_t   Nem_PplRock_len = 0;
const uint8_t *Nem_Crabmeat = NULL;
size_t   Nem_Crabmeat_len = 0;
const uint8_t *Nem_Buzz = NULL;
size_t   Nem_Buzz_len = 0;
const uint8_t *Nem_Chopper = NULL;
size_t   Nem_Chopper_len = 0;
const uint8_t *Nem_Newtron = NULL;
size_t   Nem_Newtron_len = 0;
const uint8_t *Nem_Motobug = NULL;
size_t   Nem_Motobug_len = 0;
const uint8_t *Nem_Spikes = NULL;
size_t   Nem_Spikes_len = 0;
const uint8_t *Nem_HSpring = NULL;
size_t   Nem_HSpring_len = 0;
const uint8_t *Nem_VSpring = NULL;
size_t   Nem_VSpring_len = 0;
const uint8_t *Nem_Monitors = NULL;
size_t   Nem_Monitors_len = 0;
const uint8_t *Nem_Shield = NULL;
size_t   Nem_Shield_len = 0;
const uint8_t *Nem_Stars = NULL;
size_t   Nem_Stars_len = 0;

const uint8_t *Nem_SignPost = NULL;
size_t   Nem_SignPost_len = 0;
const uint8_t *Nem_Bonus = NULL;
size_t   Nem_Bonus_len = 0;
const uint8_t *Nem_BigFlash = NULL;
size_t   Nem_BigFlash_len = 0;

const uint8_t *Nem_Hud = NULL;
size_t   Nem_Hud_len = 0;

const uint8_t *Nem_Lives = NULL;
size_t   Nem_Lives_len = 0;

const uint8_t *Art_Hud = NULL;
size_t   Art_Hud_len = 0;

const uint8_t *Art_LivesNums = NULL;
size_t   Art_LivesNums_len = 0;

const uint8_t *Map_HUD = NULL;
size_t   Map_HUD_len = 0;

const uint8_t *Map_Shield = NULL;
size_t   Map_Shield_len = 0;

const uint8_t *Map_Smash = NULL;
size_t   Map_Smash_len = 0;

const uint8_t *Map_Hel = NULL;
size_t   Map_Hel_len = 0;

const uint8_t *Map_Swing_SLZ = NULL;
size_t  Map_Swing_SLZ_len = 0;

const uint8_t *Map_Swing_GHZ = NULL;
size_t  Map_Swing_GHZ_len = 0;

const uint8_t *Map_Eggman = NULL;
size_t  Map_Eggman_len = 0;

const uint8_t *Map_BossItems = NULL;
size_t  Map_BossItems_len = 0;

const uint8_t *Map_Sonic = NULL;
size_t   Map_Sonic_len = 0;

const uint8_t *Art_Sonic = NULL;
size_t   Art_Sonic_len = 0;
const uint8_t *SonicDynPLC = NULL;
size_t   SonicDynPLC_len = 0;
const uint8_t *Ani_Sonic = NULL;
size_t   Ani_Sonic_len = 0;

const uint8_t *Ani_Chop = NULL;
size_t  Ani_Chop_len = 0;

const uint8_t *Ani_Eggman = NULL;
size_t  Ani_Eggman_len = 0;

const uint8_t *Ani_Monitor = NULL;
size_t   Ani_Monitor_len = 0;

const uint8_t *Ani_Shield = NULL;
size_t Ani_Shield_len = 0;

const uint8_t *Ani_Spring = NULL;
size_t   Ani_Spring_len = 0;

const uint8_t *ObjPos_GHZ1 = NULL;
size_t   ObjPos_GHZ1_len = 0;
const uint8_t *ObjPos_GHZ2 = NULL;
size_t   ObjPos_GHZ2_len = 0;
const uint8_t *ObjPos_GHZ3 = NULL;
size_t   ObjPos_GHZ3_len = 0;
const uint8_t *ObjPos_LZ1 = NULL;
size_t   ObjPos_LZ1_len = 0;
const uint8_t *ObjPos_LZ2 = NULL;
size_t   ObjPos_LZ2_len = 0;
const uint8_t *ObjPos_LZ3 = NULL;
size_t   ObjPos_LZ3_len = 0;
const uint8_t *ObjPos_SBZ3 = NULL;
size_t   ObjPos_SBZ3_len = 0;
const uint8_t *ObjPos_MZ1 = NULL;
size_t   ObjPos_MZ1_len = 0;
const uint8_t *ObjPos_MZ2 = NULL;
size_t   ObjPos_MZ2_len = 0;
const uint8_t *ObjPos_MZ3 = NULL;
size_t   ObjPos_MZ3_len = 0;
const uint8_t *ObjPos_SLZ1 = NULL;
size_t   ObjPos_SLZ1_len = 0;
const uint8_t *ObjPos_SLZ2 = NULL;
size_t   ObjPos_SLZ2_len = 0;
const uint8_t *ObjPos_SLZ3 = NULL;
size_t   ObjPos_SLZ3_len = 0;
const uint8_t *ObjPos_SYZ1 = NULL;
size_t   ObjPos_SYZ1_len = 0;
const uint8_t *ObjPos_SYZ2 = NULL;
size_t   ObjPos_SYZ2_len = 0;
const uint8_t *ObjPos_SYZ3 = NULL;
size_t   ObjPos_SYZ3_len = 0;
const uint8_t *ObjPos_SBZ1 = NULL;
size_t   ObjPos_SBZ1_len = 0;
const uint8_t *ObjPos_SBZ2 = NULL;
size_t   ObjPos_SBZ2_len = 0;
const uint8_t *ObjPos_FZ = NULL;
size_t   ObjPos_FZ_len = 0;
const uint8_t *ObjPos_End = NULL;
size_t   ObjPos_End_len = 0;

const uint8_t *Nem_Ring = NULL;
size_t   Nem_Ring_len = 0;

const uint8_t *Map_Ring = NULL;
size_t   Map_Ring_len = 0;

const uint8_t *Ani_Ring = NULL;
size_t   Ani_Ring_len = 0;

const uint8_t *Map_Sign = NULL;
size_t   Map_Sign_len = 0;

const uint8_t *Ani_Sign = NULL;
size_t   Ani_Sign_len = 0;

/* Crabmeat mappings and animation scripts */
const uint8_t *Map_Crab = NULL;
size_t   Map_Crab_len = 0;

const uint8_t *Ani_Crab = NULL;
size_t   Ani_Crab_len = 0;

/* Motobug mappings and animation scripts */
const uint8_t *Map_Moto = NULL;
size_t   Map_Moto_len = 0;

const uint8_t *Ani_Moto = NULL;
size_t   Ani_Moto_len = 0;

/* Buzz Bomber mappings and animation scripts */
const uint8_t *Map_Buzz = NULL;
size_t   Map_Buzz_len = 0;

const uint8_t *Ani_Buzz = NULL;
size_t   Ani_Buzz_len = 0;

const uint8_t *Map_Missile = NULL;
size_t   Map_Missile_len = 0;

const uint8_t *Ani_Missile = NULL;
size_t   Ani_Missile_len = 0;

/* GHZ bridge (id_Bridge) mappings */
const uint8_t *Map_Bri = NULL;
size_t   Map_Bri_len = 0;

/* GHZ purple rock (id_PurpleRock) mappings */
static size_t Map_PRock_len = 0;
/* GHZ edge walls (id_EdgeWalls)  */
const uint8_t *Nem_GhzWall2 = NULL;
size_t   Nem_GhzWall2_len = 0;

/* GHZ2 PLC graphics (PLC_GHZ2) — swinging platform, bridge, spiked log,
   giant ball and breakable wall */
const uint8_t *Nem_Swing = NULL;
size_t   Nem_Swing_len = 0;
const uint8_t *Nem_Bridge = NULL;
size_t   Nem_Bridge_len = 0;
const uint8_t *Nem_SpikePole = NULL;
size_t   Nem_SpikePole_len = 0;
const uint8_t *Nem_Ball = NULL;
size_t   Nem_Ball_len = 0;
const uint8_t *Nem_GhzWall1 = NULL;
size_t   Nem_GhzWall1_len = 0;

/* Object mappings referenced by the DebugMode item lists.
   Most are unstaged (NULL) until their maps ".asm" assets are ported. */
const uint8_t *Map_Monitor = NULL;
size_t   Map_Monitor_len = 0;
const uint8_t *Map_Chop = NULL;
size_t   Map_Chop_len = 0;
const uint8_t *Map_Spike = NULL;
size_t   Map_Spike_len = 0;
const uint8_t *Map_Plat_GHZ = NULL;
size_t   Map_Plat_GHZ_len = 0;
const uint8_t *Map_PRock = NULL;
const uint8_t *Map_Spring = NULL;
size_t   Map_Spring_len = 0;
const uint8_t *Map_Newt = NULL;
/* GHZ edge walls (id_EdgeWalls) mappings */
static size_t Map_Edge_len = 0;
const uint8_t *Map_Edge = NULL;
const uint8_t *Map_GBall = NULL;
static size_t Map_GBall_len = 0;
const uint8_t *Map_Lamp = NULL;
const uint8_t *Map_GRing = NULL;
const uint8_t *Map_Bonus = NULL;
const uint8_t *Map_Jaws = NULL;
const uint8_t *Map_Burro = NULL;
const uint8_t *Map_Harp = NULL;
const uint8_t *Map_Push = NULL;
const uint8_t *Map_But = NULL;
const uint8_t *Map_MBlockLZ = NULL;
const uint8_t *Map_LBlock = NULL;
const uint8_t *Map_Gar = NULL;
const uint8_t *Map_LConv = NULL;
const uint8_t *Map_Orb = NULL;
const uint8_t *Map_Bub = NULL;
const uint8_t *Map_WFall = NULL;
const uint8_t *Map_Pole = NULL;
const uint8_t *Map_Flap = NULL;
const uint8_t *Map_Fire = NULL;
const uint8_t *Map_Brick = NULL;
const uint8_t *Map_Geyser = NULL;
const uint8_t *Map_LWall = NULL;
const uint8_t *Map_Yad = NULL;
const uint8_t *Map_Smab = NULL;
const uint8_t *Map_MBlock = NULL;
const uint8_t *Map_CFlo = NULL;
size_t   Map_CFlo_len = 0;
const uint8_t *Map_LTag = NULL;
const uint8_t *Map_Bas = NULL;
const uint8_t *Map_Cat = NULL;
const uint8_t *Map_Elev = NULL;
const uint8_t *Map_Plat_SLZ = NULL;
const uint8_t *Map_Circ = NULL;
const uint8_t *Map_Stair = NULL;
const uint8_t *Map_Fan = NULL;
const uint8_t *Map_Seesaw = NULL;
const uint8_t *Map_Scen = NULL;
size_t   Map_Scen_len = 0;
const uint8_t *Map_Bomb = NULL;
const uint8_t *Map_Roll = NULL;
const uint8_t *Map_Light = NULL;
const uint8_t *Map_Bump = NULL;
const uint8_t *Map_Plat_SYZ = NULL;
const uint8_t *Map_FBlock = NULL;
const uint8_t *Map_BBall = NULL;
const uint8_t *Map_Disc = NULL;
const uint8_t *Map_Trap = NULL;
const uint8_t *Map_Spin = NULL;
const uint8_t *Map_Saw = NULL;
const uint8_t *Map_Stomp = NULL;
const uint8_t *Map_ADoor = NULL;
const uint8_t *Map_VanP = NULL;
const uint8_t *Map_Flame = NULL;
const uint8_t *Map_Elec = NULL;
const uint8_t *Map_Gird = NULL;
const uint8_t *Map_Invis = NULL;
const uint8_t *Map_Hog = NULL;
const uint8_t *Map_Animal1 = NULL;
size_t   Map_Animal1_len = 0;
const uint8_t *Map_Animal2 = NULL;
size_t   Map_Animal2_len = 0;
const uint8_t *Map_Animal3 = NULL;
size_t   Map_Animal3_len = 0;
const uint8_t *Map_Ledge = NULL;
size_t   Map_Ledge_len = 0;

/* Points object mappings (28, 29 Animals and Points.asm) */
const uint8_t *Map_Points = NULL;
size_t   Map_Points_len = 0;

/* Uncompressed level art for AnimateLevelAct (AnimateLevelGfx.asm).
   Art_MzLava1/2 and Art_MzTorch are for MZ, Art_SbzSmoke for SBZ,
   Art_BigRing for the giant ring object; only the GHZ art + BigRing
   are reachable while GHZ is the playable zone. */
const uint8_t *Art_GhzWater = NULL;
size_t   Art_GhzWater_len = 0;
const uint8_t *Art_GhzFlower1 = NULL;
size_t   Art_GhzFlower1_len = 0;
const uint8_t *Art_GhzFlower2 = NULL;
size_t   Art_GhzFlower2_len = 0;
const uint8_t *Art_MzLava1 = NULL;
size_t   Art_MzLava1_len = 0;
const uint8_t *Art_MzLava2 = NULL;
size_t   Art_MzLava2_len = 0;
const uint8_t *Art_MzTorch = NULL;
size_t   Art_MzTorch_len = 0;
const uint8_t *Art_SbzSmoke = NULL;
size_t   Art_SbzSmoke_len = 0;
const uint8_t *Art_BigRing = NULL;
size_t   Art_BigRing_len = 0;

/* Collision index tables (AngleMap, CollArray1, CollArray2) */
const uint8_t *Col_AngleMap = NULL;
size_t   Col_AngleMap_len = 0;
const uint8_t *Col_CollArray1 = NULL;
size_t   Col_CollArray1_len = 0;
const uint8_t *Col_CollArray2 = NULL;
size_t   Col_CollArray2_len = 0;

/* Per-zone collision indexes (ColPointers, sonic.asm:3116-3121).
   Only GHZ is staged so far; the rest stay NULL (ColIndexLoad picks a
   NULL pointer) until their collide ".bin" lands. */
const uint8_t *Col_GHZ = NULL;
size_t   Col_GHZ_len = 0;
const uint8_t *Col_LZ = NULL;
size_t   Col_LZ_len = 0;
const uint8_t *Col_MZ = NULL;
size_t   Col_MZ_len = 0;
const uint8_t *Col_SLZ = NULL;
size_t   Col_SLZ_len = 0;
const uint8_t *Col_SYZ = NULL;
size_t   Col_SYZ_len = 0;
const uint8_t *Col_SBZ = NULL;
size_t   Col_SBZ_len = 0;

/* Level start location arrays (from _inc/LevelSizeLoad & BgScrollSpeed.asm) */
const uint8_t *StartLocArray = NULL;
size_t   StartLocArray_len = 0;
const uint8_t *EndingStLocArray = NULL;
size_t   EndingStLocArray_len = 0;

/* ============================================================================
   Asset loading
   ============================================================================ */

static int load_asset(const char *name, const uint8_t **out_ptr, size_t *out_len);

static int load_asm_asset(const char *name, const uint8_t **out_ptr, size_t *out_len, int is_map);
static int load_asm_asset_named(const char *name, const char *tblname,
                                const uint8_t **out_ptr, size_t *out_len);

static const char *assets_base_path(void) {
    static char base[PATH_MAX];
    static int init = 0;
    if (init) return base;
    init = 1;
    ssize_t n = readlink("/proc/self/exe", base, sizeof(base) - 1);
    if (n > 0) {
        base[n] = '\0';
        char *slash = strrchr(base, '/');
        if (slash) {
            *slash = '\0';
            char tmp[PATH_MAX + 64];
            snprintf(tmp, sizeof(tmp), "%s/assets", base);
            strncpy(base, tmp, sizeof(base) - 1);
            base[sizeof(base) - 1] = '\0';
            return base;
        }
    }
    snprintf(base, sizeof(base), "./assets");
    return base;
}

static int load_asset(const char *name, const uint8_t **out_ptr, size_t *out_len) {
    char path[PATH_MAX + 512];
    snprintf(path, sizeof(path), "%s/%s", assets_base_path(), name);
    const uint8_t *buf = Assets_Load(path, out_len);
    if (!buf) {
        fprintf(stderr, "[Data] Failed to load asset: %s\n", path);
        return -1;
    }
    if (out_ptr) *out_ptr = buf;
    return 0;
}

/* Stage startpos asset references so stage_assets.py copies the raw BINs.
   The actual concatenation into StartLocArray/EndingStLocArray happens
   below in Data_Init. */
static void stage_startpos_assets(void) {
    (void)load_asset("startpos/ghz1.bin", NULL, NULL);
    (void)load_asset("startpos/ghz2.bin", NULL, NULL);
    (void)load_asset("startpos/ghz3.bin", NULL, NULL);
    (void)load_asset("startpos/lz1.bin", NULL, NULL);
    (void)load_asset("startpos/lz2.bin", NULL, NULL);
    (void)load_asset("startpos/lz3.bin", NULL, NULL);
    (void)load_asset("startpos/sbz3.bin", NULL, NULL);
    (void)load_asset("startpos/mz1.bin", NULL, NULL);
    (void)load_asset("startpos/mz2.bin", NULL, NULL);
    (void)load_asset("startpos/mz3.bin", NULL, NULL);
    (void)load_asset("startpos/slz1.bin", NULL, NULL);
    (void)load_asset("startpos/slz2.bin", NULL, NULL);
    (void)load_asset("startpos/slz3.bin", NULL, NULL);
    (void)load_asset("startpos/syz1.bin", NULL, NULL);
    (void)load_asset("startpos/syz2.bin", NULL, NULL);
    (void)load_asset("startpos/syz3.bin", NULL, NULL);
    (void)load_asset("startpos/sbz1.bin", NULL, NULL);
    (void)load_asset("startpos/sbz2.bin", NULL, NULL);
    (void)load_asset("startpos/fz.bin", NULL, NULL);
    (void)load_asset("startpos/end1.bin", NULL, NULL);
    (void)load_asset("startpos/end2.bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/ghz1 (Credits demo 1).bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/ghz1 (Credits demo 2).bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/lz3 (Credits demo).bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/mz2 (Credits demo).bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/sbz1 (Credits demo).bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/sbz2 (Credits demo).bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/slz3 (Credits demo).bin", NULL, NULL);
    (void)load_asset("startpos/Credits Demos/syz3 (Credits demo).bin", NULL, NULL);
}

int Data_Init(void) {
    stage_startpos_assets();

    Pal_SegaBG = NULL;
    Pal_SegaBG_len = 0;
    Pal_Sega1 = NULL;
    Pal_Sega1_len = 0;
    Pal_Sega2 = NULL;
    Pal_Sega2_len = 0;
    Nem_SegaLogo = NULL;
    Nem_SegaLogo_len = 0;
    Eni_SegaLogo = NULL;
    Eni_SegaLogo_len = 0;
    Pal_Title = NULL;
    Pal_Title_len = 0;
    Pal_TitleCycWater = NULL;
    Pal_TitleCycWater_len = 0;
    Pal_GHZCycWater = NULL;
    Pal_GHZCycWater_len = 0;
    Pal_LevelSel = NULL;
    Pal_LevelSel_len = 0;
    Pal_Sonic = NULL;
    Pal_Sonic_len = 0;
    Pal_GHZ = NULL;
    Pal_GHZ_len = 0;
    Nem_JapNames = NULL;
    Nem_JapNames_len = 0;
    Eni_JapNames = NULL;
    Eni_JapNames_len = 0;
    Nem_CreditText = NULL;
    Nem_CreditText_len = 0;
    Nem_TitleFg = NULL;
    Nem_TitleFg_len = 0;
    Nem_TitleSonic = NULL;
    Nem_TitleSonic_len = 0;
    Nem_TitleTM = NULL;
    Nem_TitleTM_len = 0;
    Art_Text = NULL;
    Art_Text_len = 0;
    Blk16_GHZ = NULL;
    Blk16_GHZ_len = 0;
    Blk256_GHZ = NULL;
    Blk256_GHZ_len = 0;
    Eni_Title = NULL;
    Eni_Title_len = 0;
    Nem_GHZ_1st = NULL;
    Nem_GHZ_1st_len = 0;
    Level_GHZ1 = NULL;
    Level_GHZ1_len = 0;
    Level_GHZbg = NULL;
    Level_GHZbg_len = 0;
    Level_GHZ2 = NULL;
    Level_GHZ2_len = 0;
    Level_GHZ3 = NULL;
    Level_GHZ3_len = 0;
    Level_LZ1 = NULL;
    Level_LZ1_len = 0;
    Level_LZ2 = NULL;
    Level_LZ2_len = 0;
    Level_LZ3 = NULL;
    Level_LZ3_len = 0;
    Level_LZbg = NULL;
    Level_LZbg_len = 0;
    Level_SBZ3 = NULL;
    Level_SBZ3_len = 0;
    Level_MZ1 = NULL;
    Level_MZ1_len = 0;
    Level_MZ1bg = NULL;
    Level_MZ1bg_len = 0;
    Level_MZ2 = NULL;
    Level_MZ2_len = 0;
    Level_MZ2bg = NULL;
    Level_MZ2bg_len = 0;
    Level_MZ3 = NULL;
    Level_MZ3_len = 0;
    Level_MZ3bg = NULL;
    Level_MZ3bg_len = 0;
    Level_SLZ1 = NULL;
    Level_SLZ1_len = 0;
    Level_SLZ2 = NULL;
    Level_SLZ2_len = 0;
    Level_SLZ3 = NULL;
    Level_SLZ3_len = 0;
    Level_SLZbg = NULL;
    Level_SLZbg_len = 0;
    Level_SYZ1 = NULL;
    Level_SYZ1_len = 0;
    Level_SYZ2 = NULL;
    Level_SYZ2_len = 0;
    Level_SYZ3 = NULL;
    Level_SYZ3_len = 0;
    Level_SYZbg = NULL;
    Level_SYZbg_len = 0;
    Level_SBZ1 = NULL;
    Level_SBZ1_len = 0;
    Level_SBZ1bg = NULL;
    Level_SBZ1bg_len = 0;
    Level_SBZ2 = NULL;
    Level_SBZ2_len = 0;
    Level_SBZ2bg = NULL;
    Level_SBZ2bg_len = 0;
    Level_End = NULL;
    Level_End_len = 0;
    Nem_TitleCard = NULL;
    Nem_TitleCard_len = 0;
    Map_Card = NULL;
    Map_Card_len = 0;
    Map_Got = NULL;
    Map_Got_len = 0;
    Ani_Sonic = NULL;
    Ani_Sonic_len = 0;
    Ani_Monitor = NULL;
    Ani_Monitor_len = 0;
    Nem_Hud = NULL;
    Nem_Hud_len = 0;
    Nem_Lives = NULL;
    Nem_Lives_len = 0;
    Art_Hud = NULL;
    Art_Hud_len = 0;
    Art_LivesNums = NULL;
    Art_LivesNums_len = 0;
    Map_HUD = NULL;
    Map_HUD_len = 0;
    SonicDynPLC = NULL;
    SonicDynPLC_len = 0;
    Art_Sonic = NULL;
    Art_Sonic_len = 0;


    if (load_asset("palette/sega_bg.bin", &Pal_SegaBG, &Pal_SegaBG_len) != 0) {
        Pal_SegaBG = NULL;
        Pal_SegaBG_len = 0;
    }

    if (load_asset("palette/sega1.bin", &Pal_Sega1, &Pal_Sega1_len) != 0) {
        Pal_Sega1 = NULL;
        Pal_Sega1_len = 0;
    }

    if (load_asset("palette/sega2.bin", &Pal_Sega2, &Pal_Sega2_len) != 0) {
        Pal_Sega2 = NULL;
        Pal_Sega2_len = 0;
    }

    if (load_asset("artnem/sega_logo.nem", &Nem_SegaLogo, &Nem_SegaLogo_len) != 0) {
        Nem_SegaLogo = NULL;
        Nem_SegaLogo_len = 0;
    }

    if (load_asset("tilemaps/sega_logo.eni", &Eni_SegaLogo, &Eni_SegaLogo_len) != 0) {
        Eni_SegaLogo = NULL;
        Eni_SegaLogo_len = 0;
    }

    if (load_asset("palette/title.bin", &Pal_Title, &Pal_Title_len) != 0) {
        Pal_Title = NULL;
        Pal_Title_len = 0;
    }

    if (load_asset("palette/cycle_water.bin", &Pal_TitleCycWater, &Pal_TitleCycWater_len) != 0) {
        Pal_TitleCycWater = NULL;
        Pal_TitleCycWater_len = 0;
    }

    if (load_asset("palette/cycle_ghz.bin", &Pal_GHZCycWater, &Pal_GHZCycWater_len) != 0) {
        Pal_GHZCycWater = NULL;
        Pal_GHZCycWater_len = 0;
    }

    if (load_asset("palette/level_select.bin", &Pal_LevelSel, &Pal_LevelSel_len) != 0) {
        Pal_LevelSel = NULL;
        Pal_LevelSel_len = 0;
    }

    if (load_asset("palette/sonic.bin", &Pal_Sonic, &Pal_Sonic_len) != 0) {
        Pal_Sonic = NULL;
        Pal_Sonic_len = 0;
    }

    if (load_asset("palette/ghz.bin", &Pal_GHZ, &Pal_GHZ_len) != 0) {
        Pal_GHZ = NULL;
        Pal_GHZ_len = 0;
    }

    if (load_asset("palette/lz.bin", &Pal_LZ, &Pal_LZ_len) != 0) {
        Pal_LZ = NULL;
        Pal_LZ_len = 0;
    }
    if (load_asset("palette/lz_underwater.bin", &Pal_LZWater, &Pal_LZWater_len) != 0) {
        Pal_LZWater = NULL;
        Pal_LZWater_len = 0;
    }
    if (load_asset("palette/sonic_lz_underwater.bin", &Pal_LZSonWater, &Pal_LZSonWater_len) != 0) {
        Pal_LZSonWater = NULL;
        Pal_LZSonWater_len = 0;
    }
    if (load_asset("palette/mz.bin", &Pal_MZ, &Pal_MZ_len) != 0) {
        Pal_MZ = NULL;
        Pal_MZ_len = 0;
    }
    if (load_asset("palette/slz.bin", &Pal_SLZ, &Pal_SLZ_len) != 0) {
        Pal_SLZ = NULL;
        Pal_SLZ_len = 0;
    }
    if (load_asset("palette/syz.bin", &Pal_SYZ, &Pal_SYZ_len) != 0) {
        Pal_SYZ = NULL;
        Pal_SYZ_len = 0;
    }
    if (load_asset("palette/sbz1.bin", &Pal_SBZ1, &Pal_SBZ1_len) != 0) {
        Pal_SBZ1 = NULL;
        Pal_SBZ1_len = 0;
    }
    if (load_asset("palette/sbz2.bin", &Pal_SBZ2, &Pal_SBZ2_len) != 0) {
        Pal_SBZ2 = NULL;
        Pal_SBZ2_len = 0;
    }
    if (load_asset("palette/sbz3.bin", &Pal_SBZ3, &Pal_SBZ3_len) != 0) {
        Pal_SBZ3 = NULL;
        Pal_SBZ3_len = 0;
    }
    if (load_asset("palette/sbz3_underwater.bin", &Pal_SBZ3Water, &Pal_SBZ3Water_len) != 0) {
        Pal_SBZ3Water = NULL;
        Pal_SBZ3Water_len = 0;
    }
    if (load_asset("palette/sonic_sbz3_underwater.bin", &Pal_SBZ3SonWat, &Pal_SBZ3SonWat_len) != 0) {
        Pal_SBZ3SonWat = NULL;
        Pal_SBZ3SonWat_len = 0;
    }
    if (load_asset("palette/special.bin", &Pal_Special, &Pal_Special_len) != 0) {
        Pal_Special = NULL;
        Pal_Special_len = 0;
    }
    if (load_asset("palette/ss_result.bin", &Pal_SSResult, &Pal_SSResult_len) != 0) {
        Pal_SSResult = NULL;
        Pal_SSResult_len = 0;
    }
    if (load_asset("palette/continue.bin", &Pal_Continue, &Pal_Continue_len) != 0) {
        Pal_Continue = NULL;
        Pal_Continue_len = 0;
    }
    if (load_asset("palette/ending.bin", &Pal_Ending, &Pal_Ending_len) != 0) {
        Pal_Ending = NULL;
        Pal_Ending_len = 0;
    }

    if (load_asset("artnem/title_fg.nem", &Nem_TitleFg, &Nem_TitleFg_len) != 0) {
        Nem_TitleFg = NULL;
        Nem_TitleFg_len = 0;
    }

    if (load_asset("artnem/title_sonic.nem", &Nem_TitleSonic, &Nem_TitleSonic_len) != 0) {
        Nem_TitleSonic = NULL;
        Nem_TitleSonic_len = 0;
    }

    if (load_asset("artnem/title_tm.nem", &Nem_TitleTM, &Nem_TitleTM_len) != 0) {
        Nem_TitleTM = NULL;
        Nem_TitleTM_len = 0;
    }

    if (load_asset("artnem/ghz1.nem", &Nem_GHZ_1st, &Nem_GHZ_1st_len) != 0) {
        Nem_GHZ_1st = NULL;
        Nem_GHZ_1st_len = 0;
    }

    if (load_asset("artnem/ghz2.nem", &Nem_GHZ_2nd, &Nem_GHZ_2nd_len) != 0) {
        Nem_GHZ_2nd = NULL;
        Nem_GHZ_2nd_len = 0;
    }

    if (load_asset("artnem/ghz_stalk.nem", &Nem_Stalk, &Nem_Stalk_len) != 0) {
        Nem_Stalk = NULL;
        Nem_Stalk_len = 0;
    }

    if (load_asset("artnem/ghz_rock.nem", &Nem_PplRock, &Nem_PplRock_len) != 0) {
        Nem_PplRock = NULL;
        Nem_PplRock_len = 0;
    }

    if (load_asset("artnem/crabmeat.nem", &Nem_Crabmeat, &Nem_Crabmeat_len) != 0) {
        Nem_Crabmeat = NULL;
        Nem_Crabmeat_len = 0;
    }

    if (load_asset("artnem/buzz.nem", &Nem_Buzz, &Nem_Buzz_len) != 0) {
        Nem_Buzz = NULL;
        Nem_Buzz_len = 0;
    }

    if (load_asset("artnem/chopper.nem", &Nem_Chopper, &Nem_Chopper_len) != 0) {
        Nem_Chopper = NULL;
        Nem_Chopper_len = 0;
    }

    if (load_asset("artnem/newtron.nem", &Nem_Newtron, &Nem_Newtron_len) != 0) {
        Nem_Newtron = NULL;
        Nem_Newtron_len = 0;
    }

    if (load_asset("artnem/motobug.nem", &Nem_Motobug, &Nem_Motobug_len) != 0) {
        Nem_Motobug = NULL;
        Nem_Motobug_len = 0;
    }

    if (load_asset("artnem/spikes.nem", &Nem_Spikes, &Nem_Spikes_len) != 0) {
        Nem_Spikes = NULL;
        Nem_Spikes_len = 0;
    }

    if (load_asset("artnem/hspring.nem", &Nem_HSpring, &Nem_HSpring_len) != 0) {
        Nem_HSpring = NULL;
        Nem_HSpring_len = 0;
    }

    if (load_asset("artnem/vspring.nem", &Nem_VSpring, &Nem_VSpring_len) != 0) {
        Nem_VSpring = NULL;
        Nem_VSpring_len = 0;
    }

    if (load_asset("artnem/monitors.nem", &Nem_Monitors, &Nem_Monitors_len) != 0) {
        Nem_Monitors = NULL;
        Nem_Monitors_len = 0;
    }

    if (load_asset("artnem/shield.nem", &Nem_Shield, &Nem_Shield_len) != 0) {
        Nem_Shield = NULL;
        Nem_Shield_len = 0;
    }

    if (load_asset("artnem/stars.nem", &Nem_Stars, &Nem_Stars_len) != 0) {
        Nem_Stars = NULL;
        Nem_Stars_len = 0;
    }

    if (load_asset("artnem/signpost.nem", &Nem_SignPost, &Nem_SignPost_len) != 0) {
        Nem_SignPost = NULL;
        Nem_SignPost_len = 0;
    }

    if (load_asset("artnem/hidden_bonus.nem", &Nem_Bonus, &Nem_Bonus_len) != 0) {
        Nem_Bonus = NULL;
        Nem_Bonus_len = 0;
    }

    if (load_asset("artnem/bigflash.nem", &Nem_BigFlash, &Nem_BigFlash_len) != 0) {
        Nem_BigFlash = NULL;
        Nem_BigFlash_len = 0;
    }

    if (load_asm_asset("anim/titlesonic.asm", &Ani_TSon, &Ani_TSon_len, 0) != 0) {
        Ani_TSon = NULL;
        Ani_TSon_len = 0;
    }

    if (load_asset("artnem/GHZ Edge Wall.nem", &Nem_GhzWall2, &Nem_GhzWall2_len) != 0) {
        Nem_GhzWall2 = NULL;
        Nem_GhzWall2_len = 0;
    }

    if (load_asset("artnem/GHZ Swinging Platform.nem", &Nem_Swing, &Nem_Swing_len) != 0) {
        Nem_Swing = NULL;
        Nem_Swing_len = 0;
    }

    if (load_asset("artnem/GHZ Bridge.nem", &Nem_Bridge, &Nem_Bridge_len) != 0) {
        Nem_Bridge = NULL;
        Nem_Bridge_len = 0;
    }

    if (load_asset("artnem/GHZ Spiked Log.nem", &Nem_SpikePole, &Nem_SpikePole_len) != 0) {
        Nem_SpikePole = NULL;
        Nem_SpikePole_len = 0;
    }

    if (load_asset("artnem/GHZ Giant Ball.nem", &Nem_Ball, &Nem_Ball_len) != 0) {
        Nem_Ball = NULL;
        Nem_Ball_len = 0;
    }

    if (load_asset("artnem/GHZ Breakable Wall.nem", &Nem_GhzWall1, &Nem_GhzWall1_len) != 0) {
        Nem_GhzWall1 = NULL;
        Nem_GhzWall1_len = 0;
    }

    if (load_asset("artnem/rabbit.nem", &Nem_Rabbit, &Nem_Rabbit_len) != 0) {
        Nem_Rabbit = NULL;
        Nem_Rabbit_len = 0;
    }

    if (load_asset("artnem/chicken.nem", &Nem_Chicken, &Nem_Chicken_len) != 0) {
        Nem_Chicken = NULL;
        Nem_Chicken_len = 0;
    }

    if (load_asset("artnem/penguin.nem", &Nem_Penguin, &Nem_Penguin_len) != 0) {
        Nem_Penguin = NULL;
        Nem_Penguin_len = 0;
    }

    if (load_asset("artnem/seal.nem", &Nem_Seal, &Nem_Seal_len) != 0) {
        Nem_Seal = NULL;
        Nem_Seal_len = 0;
    }

    if (load_asset("artnem/pig.nem", &Nem_Pig, &Nem_Pig_len) != 0) {
        Nem_Pig = NULL;
        Nem_Pig_len = 0;
    }

    if (load_asset("artnem/flicky.nem", &Nem_Flicky, &Nem_Flicky_len) != 0) {
        Nem_Flicky = NULL;
        Nem_Flicky_len = 0;
    }

    if (load_asset("artnem/squirrel.nem", &Nem_Squirrel, &Nem_Squirrel_len) != 0) {
        Nem_Squirrel = NULL;
        Nem_Squirrel_len = 0;
    }

    if (load_asset("artnem/Enemy Ball Hog.nem", &Nem_BallHog, &Nem_BallHog_len) != 0) {
        Nem_BallHog = NULL;
        Nem_BallHog_len = 0;
    }
    if (load_asset("artnem/Enemy Basaran.nem", &Nem_Basaran, &Nem_Basaran_len) != 0) {
        Nem_Basaran = NULL;
        Nem_Basaran_len = 0;
    }
    if (load_asset("artnem/Enemy Bomb.nem", &Nem_Bomb, &Nem_Bomb_len) != 0) {
        Nem_Bomb = NULL;
        Nem_Bomb_len = 0;
    }
    if (load_asset("artnem/LZ Bubbles & Countdown.nem", &Nem_Bubbles, &Nem_Bubbles_len) != 0) {
        Nem_Bubbles = NULL;
        Nem_Bubbles_len = 0;
    }
    if (load_asset("artnem/SYZ Bumper.nem", &Nem_Bumper, &Nem_Bumper_len) != 0) {
        Nem_Bumper = NULL;
        Nem_Bumper_len = 0;
    }
    if (load_asset("artnem/Enemy Burrobot.nem", &Nem_Burrobot, &Nem_Burrobot_len) != 0) {
        Nem_Burrobot = NULL;
        Nem_Burrobot_len = 0;
    }
    if (load_asset("artnem/Enemy Caterkiller.nem", &Nem_Cater, &Nem_Cater_len) != 0) {
        Nem_Cater = NULL;
        Nem_Cater_len = 0;
    }
    if (load_asset("artnem/LZ Cork.nem", &Nem_Cork, &Nem_Cork_len) != 0) {
        Nem_Cork = NULL;
        Nem_Cork_len = 0;
    }
    if (load_asset("artnem/SBZ Pizza Cutter.nem", &Nem_Cutter, &Nem_Cutter_len) != 0) {
        Nem_Cutter = NULL;
        Nem_Cutter_len = 0;
    }
    if (load_asset("artnem/Boss - Main.nem", &Nem_Eggman, &Nem_Eggman_len) != 0) {
        Nem_Eggman = NULL;
        Nem_Eggman_len = 0;
    }
    if (load_asset("artnem/SBZ Electrocuter.nem", &Nem_Electric, &Nem_Electric_len) != 0) {
        Nem_Electric = NULL;
        Nem_Electric_len = 0;
    }
    if (load_asset("artnem/Ending - Emeralds.nem", &Nem_EndEm, &Nem_EndEm_len) != 0) {
        Nem_EndEm = NULL;
        Nem_EndEm_len = 0;
    }
    if (load_asset("artnem/Ending - Flowers.nem", &Nem_EndFlower, &Nem_EndFlower_len) != 0) {
        Nem_EndFlower = NULL;
        Nem_EndFlower_len = 0;
    }
    if (load_asset("artnem/Ending - Sonic.nem", &Nem_EndSonic, &Nem_EndSonic_len) != 0) {
        Nem_EndSonic = NULL;
        Nem_EndSonic_len = 0;
    }
    if (load_asset("artnem/Ending - StH Logo.nem", &Nem_EndStH, &Nem_EndStH_len) != 0) {
        Nem_EndStH = NULL;
        Nem_EndStH_len = 0;
    }
    if (load_asset("artnem/Boss - Exhaust Flame.nem", &Nem_Exhaust, &Nem_Exhaust_len) != 0) {
        Nem_Exhaust = NULL;
        Nem_Exhaust_len = 0;
    }
    if (load_asset("artnem/SLZ Fan.nem", &Nem_Fan, &Nem_Fan_len) != 0) {
        Nem_Fan = NULL;
        Nem_Fan_len = 0;
    }
    if (load_asset("artnem/SBZ Flaming Pipe.nem", &Nem_FlamePipe, &Nem_FlamePipe_len) != 0) {
        Nem_FlamePipe = NULL;
        Nem_FlamePipe_len = 0;
    }
    if (load_asset("artnem/LZ Flapping Door.nem", &Nem_FlapDoor, &Nem_FlapDoor_len) != 0) {
        Nem_FlapDoor = NULL;
        Nem_FlapDoor_len = 0;
    }
    if (load_asset("artnem/Boss - Final Zone.nem", &Nem_FzBoss, &Nem_FzBoss_len) != 0) {
        Nem_FzBoss = NULL;
        Nem_FzBoss_len = 0;
    }
    if (load_asset("artnem/Boss - Eggman after FZ Fight.nem", &Nem_FzEggman, &Nem_FzEggman_len) != 0) {
        Nem_FzEggman = NULL;
        Nem_FzEggman_len = 0;
    }
    if (load_asset("artnem/Game Over.nem", &Nem_GameOver, &Nem_GameOver_len) != 0) {
        Nem_GameOver = NULL;
        Nem_GameOver_len = 0;
    }
    if (load_asset("artnem/LZ Gargoyle & Fireball.nem", &Nem_Gargoyle, &Nem_Gargoyle_len) != 0) {
        Nem_Gargoyle = NULL;
        Nem_Gargoyle_len = 0;
    }
    if (load_asset("artnem/SBZ Crushing Girder.nem", &Nem_Girder, &Nem_Girder_len) != 0) {
        Nem_Girder = NULL;
        Nem_Girder_len = 0;
    }
    if (load_asset("artnem/LZ Harpoon.nem", &Nem_Harpoon, &Nem_Harpoon_len) != 0) {
        Nem_Harpoon = NULL;
        Nem_Harpoon_len = 0;
    }
    if (load_asset("artnem/Enemy Jaws.nem", &Nem_Jaws, &Nem_Jaws_len) != 0) {
        Nem_Jaws = NULL;
        Nem_Jaws_len = 0;
    }
    if (load_asset("artnem/8x8 - LZ.nem", &Nem_LZ, &Nem_LZ_len) != 0) {
        Nem_LZ = NULL;
        Nem_LZ_len = 0;
    }
    if (load_asset("artnem/Lamppost.nem", &Nem_Lamp, &Nem_Lamp_len) != 0) {
        Nem_Lamp = NULL;
        Nem_Lamp_len = 0;
    }
    if (load_asset("artnem/MZ Lava.nem", &Nem_Lava, &Nem_Lava_len) != 0) {
        Nem_Lava = NULL;
        Nem_Lava_len = 0;
    }
    if (load_asset("artnem/LZ 32x32 Block.nem", &Nem_LzBlock1, &Nem_LzBlock1_len) != 0) {
        Nem_LzBlock1 = NULL;
        Nem_LzBlock1_len = 0;
    }
    if (load_asset("artnem/LZ Blocks.nem", &Nem_LzBlock2, &Nem_LzBlock2_len) != 0) {
        Nem_LzBlock2 = NULL;
        Nem_LzBlock2_len = 0;
    }
    if (load_asset("artnem/LZ 32x16 Block.nem", &Nem_LzBlock3, &Nem_LzBlock3_len) != 0) {
        Nem_LzBlock3 = NULL;
        Nem_LzBlock3_len = 0;
    }
    if (load_asset("artnem/LZ Vertical Door.nem", &Nem_LzDoor1, &Nem_LzDoor1_len) != 0) {
        Nem_LzDoor1 = NULL;
        Nem_LzDoor1_len = 0;
    }
    if (load_asset("artnem/LZ Horizontal Door.nem", &Nem_LzDoor2, &Nem_LzDoor2_len) != 0) {
        Nem_LzDoor2 = NULL;
        Nem_LzDoor2_len = 0;
    }
    if (load_asset("artnem/LZ Rising Platform.nem", &Nem_LzPlatfm, &Nem_LzPlatfm_len) != 0) {
        Nem_LzPlatfm = NULL;
        Nem_LzPlatfm_len = 0;
    }
    if (load_asset("artnem/LZ Breakable Pole.nem", &Nem_LzPole, &Nem_LzPole_len) != 0) {
        Nem_LzPole = NULL;
        Nem_LzPole_len = 0;
    }
    if (load_asset("artnem/LZ Spiked Ball & Chain.nem", &Nem_LzSpikeBall, &Nem_LzSpikeBall_len) != 0) {
        Nem_LzSpikeBall = NULL;
        Nem_LzSpikeBall_len = 0;
    }
    if (load_asset("artnem/Switch.nem", &Nem_LzSwitch, &Nem_LzSwitch_len) != 0) {
        Nem_LzSwitch = NULL;
        Nem_LzSwitch_len = 0;
    }
    if (load_asset("artnem/LZ Wheel.nem", &Nem_LzWheel, &Nem_LzWheel_len) != 0) {
        Nem_LzWheel = NULL;
        Nem_LzWheel_len = 0;
    }
    if (load_asset("artnem/8x8 - MZ.nem", &Nem_MZ, &Nem_MZ_len) != 0) {
        Nem_MZ = NULL;
        Nem_MZ_len = 0;
    }
    if (load_asset("artnem/Continue Screen Stuff.nem", &Nem_MiniSonic, &Nem_MiniSonic_len) != 0) {
        Nem_MiniSonic = NULL;
        Nem_MiniSonic_len = 0;
    }
    if (load_asset("artnem/MZ Green Pushable Block.nem", &Nem_MzBlock, &Nem_MzBlock_len) != 0) {
        Nem_MzBlock = NULL;
        Nem_MzBlock_len = 0;
    }
    if (load_asset("artnem/Fireballs.nem", &Nem_MzFire, &Nem_MzFire_len) != 0) {
        Nem_MzFire = NULL;
        Nem_MzFire_len = 0;
    }
    if (load_asset("artnem/MZ Green Glass Block.nem", &Nem_MzGlass, &Nem_MzGlass_len) != 0) {
        Nem_MzGlass = NULL;
        Nem_MzGlass_len = 0;
    }
    if (load_asset("artnem/MZ Metal Blocks.nem", &Nem_MzMetal, &Nem_MzMetal_len) != 0) {
        Nem_MzMetal = NULL;
        Nem_MzMetal_len = 0;
    }
    if (load_asset("artnem/MZ Switch.nem", &Nem_MzSwitch, &Nem_MzSwitch_len) != 0) {
        Nem_MzSwitch = NULL;
        Nem_MzSwitch_len = 0;
    }
    if (load_asset("artnem/Enemy Orbinaut.nem", &Nem_Orbinaut, &Nem_Orbinaut_len) != 0) {
        Nem_Orbinaut = NULL;
        Nem_Orbinaut_len = 0;
    }
    if (load_asset("artnem/Points.nem", &Nem_Points, &Nem_Points_len) != 0) {
        Nem_Points = NULL;
        Nem_Points_len = 0;
    }
    if (load_asset("artnem/Prison Capsule.nem", &Nem_Prison, &Nem_Prison_len) != 0) {
        Nem_Prison = NULL;
        Nem_Prison_len = 0;
    }
    if (load_asset("artnem/SLZ Pylon.nem", &Nem_Pylon, &Nem_Pylon_len) != 0) {
        Nem_Pylon = NULL;
        Nem_Pylon_len = 0;
    }
    if (load_asset("artnem/Special Result Emeralds.nem", &Nem_ResultEm, &Nem_ResultEm_len) != 0) {
        Nem_ResultEm = NULL;
        Nem_ResultEm_len = 0;
    }
    if (load_asset("artnem/Enemy Roller.nem", &Nem_Roller, &Nem_Roller_len) != 0) {
        Nem_Roller = NULL;
        Nem_Roller_len = 0;
    }
    if (load_asset("artnem/8x8 - SBZ.nem", &Nem_SBZ, &Nem_SBZ_len) != 0) {
        Nem_SBZ = NULL;
        Nem_SBZ_len = 0;
    }
    if (load_asset("artnem/8x8 - SLZ.nem", &Nem_SLZ, &Nem_SLZ_len) != 0) {
        Nem_SLZ = NULL;
        Nem_SLZ_len = 0;
    }
    if (load_asset("artnem/Special 1UP.nem", &Nem_SS1UpBlock, &Nem_SS1UpBlock_len) != 0) {
        Nem_SS1UpBlock = NULL;
        Nem_SS1UpBlock_len = 0;
    }
    if (load_asset("artnem/Special Clouds.nem", &Nem_SSBgCloud, &Nem_SSBgCloud_len) != 0) {
        Nem_SSBgCloud = NULL;
        Nem_SSBgCloud_len = 0;
    }
    if (load_asset("artnem/Special Birds & Fish.nem", &Nem_SSBgFish, &Nem_SSBgFish_len) != 0) {
        Nem_SSBgFish = NULL;
        Nem_SSBgFish_len = 0;
    }
    if (load_asset("artnem/Special Emerald Twinkle.nem", &Nem_SSEmStars, &Nem_SSEmStars_len) != 0) {
        Nem_SSEmStars = NULL;
        Nem_SSEmStars_len = 0;
    }
    if (load_asset("artnem/Special Emeralds.nem", &Nem_SSEmerald, &Nem_SSEmerald_len) != 0) {
        Nem_SSEmerald = NULL;
        Nem_SSEmerald_len = 0;
    }
    if (load_asset("artnem/Special GOAL.nem", &Nem_SSGOAL, &Nem_SSGOAL_len) != 0) {
        Nem_SSGOAL = NULL;
        Nem_SSGOAL_len = 0;
    }
    if (load_asset("artnem/Special Ghost.nem", &Nem_SSGhost, &Nem_SSGhost_len) != 0) {
        Nem_SSGhost = NULL;
        Nem_SSGhost_len = 0;
    }
    if (load_asset("artnem/Special Glass.nem", &Nem_SSGlass, &Nem_SSGlass_len) != 0) {
        Nem_SSGlass = NULL;
        Nem_SSGlass_len = 0;
    }
    if (load_asset("artnem/Special R.nem", &Nem_SSRBlock, &Nem_SSRBlock_len) != 0) {
        Nem_SSRBlock = NULL;
        Nem_SSRBlock_len = 0;
    }
    if (load_asset("artnem/Special Red-White.nem", &Nem_SSRedWhite, &Nem_SSRedWhite_len) != 0) {
        Nem_SSRedWhite = NULL;
        Nem_SSRedWhite_len = 0;
    }
    if (load_asset("artnem/Special UP-DOWN.nem", &Nem_SSUpDown, &Nem_SSUpDown_len) != 0) {
        Nem_SSUpDown = NULL;
        Nem_SSUpDown_len = 0;
    }
    if (load_asset("artnem/Special W.nem", &Nem_SSWBlock, &Nem_SSWBlock_len) != 0) {
        Nem_SSWBlock = NULL;
        Nem_SSWBlock_len = 0;
    }
    if (load_asset("artnem/Special Walls.nem", &Nem_SSWalls, &Nem_SSWalls_len) != 0) {
        Nem_SSWalls = NULL;
        Nem_SSWalls_len = 0;
    }
    if (load_asset("artnem/Special ZONE1.nem", &Nem_SSZone1, &Nem_SSZone1_len) != 0) {
        Nem_SSZone1 = NULL;
        Nem_SSZone1_len = 0;
    }
    if (load_asset("artnem/Special ZONE2.nem", &Nem_SSZone2, &Nem_SSZone2_len) != 0) {
        Nem_SSZone2 = NULL;
        Nem_SSZone2_len = 0;
    }
    if (load_asset("artnem/Special ZONE3.nem", &Nem_SSZone3, &Nem_SSZone3_len) != 0) {
        Nem_SSZone3 = NULL;
        Nem_SSZone3_len = 0;
    }
    if (load_asset("artnem/Special ZONE4.nem", &Nem_SSZone4, &Nem_SSZone4_len) != 0) {
        Nem_SSZone4 = NULL;
        Nem_SSZone4_len = 0;
    }
    if (load_asset("artnem/Special ZONE5.nem", &Nem_SSZone5, &Nem_SSZone5_len) != 0) {
        Nem_SSZone5 = NULL;
        Nem_SSZone5_len = 0;
    }
    if (load_asset("artnem/Special ZONE6.nem", &Nem_SSZone6, &Nem_SSZone6_len) != 0) {
        Nem_SSZone6 = NULL;
        Nem_SSZone6_len = 0;
    }
    if (load_asset("artnem/8x8 - SYZ.nem", &Nem_SYZ, &Nem_SYZ_len) != 0) {
        Nem_SYZ = NULL;
        Nem_SYZ_len = 0;
    }
    if (load_asset("artnem/Boss - Eggman in SBZ2 & FZ.nem", &Nem_Sbz2Eggman, &Nem_Sbz2Eggman_len) != 0) {
        Nem_Sbz2Eggman = NULL;
        Nem_Sbz2Eggman_len = 0;
    }
    if (load_asset("artnem/SBZ Vanishing Block.nem", &Nem_SbzBlock, &Nem_SbzBlock_len) != 0) {
        Nem_SbzBlock = NULL;
        Nem_SbzBlock_len = 0;
    }
    if (load_asset("artnem/SBZ Small Vertical Door.nem", &Nem_SbzDoor1, &Nem_SbzDoor1_len) != 0) {
        Nem_SbzDoor1 = NULL;
        Nem_SbzDoor1_len = 0;
    }
    if (load_asset("artnem/SBZ Large Horizontal Door.nem", &Nem_SbzDoor2, &Nem_SbzDoor2_len) != 0) {
        Nem_SbzDoor2 = NULL;
        Nem_SbzDoor2_len = 0;
    }
    if (load_asset("artnem/SBZ Collapsing Floor.nem", &Nem_SbzFloor, &Nem_SbzFloor_len) != 0) {
        Nem_SbzFloor = NULL;
        Nem_SbzFloor_len = 0;
    }
    if (load_asset("artnem/SBZ Running Disc.nem", &Nem_SbzWheel1, &Nem_SbzWheel1_len) != 0) {
        Nem_SbzWheel1 = NULL;
        Nem_SbzWheel1_len = 0;
    }
    if (load_asset("artnem/SBZ Junction Wheel.nem", &Nem_SbzWheel2, &Nem_SbzWheel2_len) != 0) {
        Nem_SbzWheel2 = NULL;
        Nem_SbzWheel2_len = 0;
    }
    if (load_asset("artnem/SLZ Seesaw.nem", &Nem_Seesaw, &Nem_Seesaw_len) != 0) {
        Nem_Seesaw = NULL;
        Nem_Seesaw_len = 0;
    }
    if (load_asset("artnem/SBZ Sliding Floor Trap.nem", &Nem_SlideFloor, &Nem_SlideFloor_len) != 0) {
        Nem_SlideFloor = NULL;
        Nem_SlideFloor_len = 0;
    }
    if (load_asset("artnem/SLZ 32x32 Block.nem", &Nem_SlzBlock, &Nem_SlzBlock_len) != 0) {
        Nem_SlzBlock = NULL;
        Nem_SlzBlock_len = 0;
    }
    if (load_asset("artnem/SLZ Cannon.nem", &Nem_SlzCannon, &Nem_SlzCannon_len) != 0) {
        Nem_SlzCannon = NULL;
        Nem_SlzCannon_len = 0;
    }
    if (load_asset("artnem/SLZ Little Spikeball.nem", &Nem_SlzSpike, &Nem_SlzSpike_len) != 0) {
        Nem_SlzSpike = NULL;
        Nem_SlzSpike_len = 0;
    }
    if (load_asset("artnem/SLZ Swinging Platform.nem", &Nem_SlzSwing, &Nem_SlzSwing_len) != 0) {
        Nem_SlzSwing = NULL;
        Nem_SlzSwing_len = 0;
    }
    if (load_asset("artnem/SLZ Breakable Wall.nem", &Nem_SlzWall, &Nem_SlzWall_len) != 0) {
        Nem_SlzWall = NULL;
        Nem_SlzWall_len = 0;
    }
    if (load_asset("artnem/SBZ Spinning Platform.nem", &Nem_SpinPform, &Nem_SpinPform_len) != 0) {
        Nem_SpinPform = NULL;
        Nem_SpinPform_len = 0;
    }
    if (load_asset("artnem/LZ Water & Splashes.nem", &Nem_Splash, &Nem_Splash_len) != 0) {
        Nem_Splash = NULL;
        Nem_Splash_len = 0;
    }
    if (load_asset("artnem/SBZ Stomper.nem", &Nem_Stomper, &Nem_Stomper_len) != 0) {
        Nem_Stomper = NULL;
        Nem_Stomper_len = 0;
    }
    if (load_asset("artnem/SYZ Large Spikeball.nem", &Nem_SyzSpike1, &Nem_SyzSpike1_len) != 0) {
        Nem_SyzSpike1 = NULL;
        Nem_SyzSpike1_len = 0;
    }
    if (load_asset("artnem/SYZ Small Spikeball.nem", &Nem_SyzSpike2, &Nem_SyzSpike2_len) != 0) {
        Nem_SyzSpike2 = NULL;
        Nem_SyzSpike2_len = 0;
    }
    if (load_asset("artnem/SBZ Trapdoor.nem", &Nem_TrapDoor, &Nem_TrapDoor_len) != 0) {
        Nem_TrapDoor = NULL;
        Nem_TrapDoor_len = 0;
    }
    if (load_asset("artnem/Ending - Try Again.nem", &Nem_TryAgain, &Nem_TryAgain_len) != 0) {
        Nem_TryAgain = NULL;
        Nem_TryAgain_len = 0;
    }
    if (load_asset("artnem/LZ Water Surface.nem", &Nem_Water, &Nem_Water_len) != 0) {
        Nem_Water = NULL;
        Nem_Water_len = 0;
    }
    if (load_asset("artnem/Boss - Weapons.nem", &Nem_Weapons, &Nem_Weapons_len) != 0) {
        Nem_Weapons = NULL;
        Nem_Weapons_len = 0;
    }
    if (load_asset("artnem/Enemy Yadrin.nem", &Nem_Yadrin, &Nem_Yadrin_len) != 0) {
        Nem_Yadrin = NULL;
        Nem_Yadrin_len = 0;
    }

    if (load_asm_asset("anim/psbtm.asm", &Ani_PSBTM, &Ani_PSBTM_len, 0) != 0) {
        Ani_PSBTM = NULL;
        Ani_PSBTM_len = 0;
    }

    if (load_asm_asset("anim/Prison Capsule.asm", &Ani_Pri, &Ani_Pri_len, 0) != 0) {
        Ani_Pri = NULL;
        Ani_Pri_len = 0;
    }

    if (load_asm_asset("maps/titlesonic.asm", &Map_TSon, &Map_TSon_len, 1) != 0) {
        Map_TSon = NULL;
        Map_TSon_len = 0;
    }

    if (load_asm_asset("maps/Prison Capsule.asm", &Map_Pri, &Map_Pri_len, 1) != 0) {
        Map_Pri = NULL;
        Map_Pri_len = 0;
    }

    if (load_asm_asset("maps/psbtm.asm", &Map_PSB, &Map_PSB_len, 1) != 0) {
        Map_PSB = NULL;
        Map_PSB_len = 0;
    }

    if (load_asm_asset("maps/credits.asm", &Map_Cred, &Map_Cred_len, 1) != 0) {
        Map_Cred = NULL;
        Map_Cred_len = 0;
    }

    if (load_asset("artnem/title_card.nem", &Nem_TitleCard, &Nem_TitleCard_len) != 0) {
        Nem_TitleCard = NULL;
        Nem_TitleCard_len = 0;
    }

    if (load_asm_asset("maps/titlecard.asm", &Map_Card, &Map_Card_len, 1) != 0) {
        Map_Card = NULL;
        Map_Card_len = 0;
    }

    /* Map_Got ("SONIC HAS PASSED" card) lives in the same file, as its own
       mappingsTable block — extract it as a standalone table (frame IDs
       match the order of Map_Got's mappingsTableEntry.w lines). */
    if (load_asm_asset_named("maps/titlecard.asm", "Map_Got",
                             &Map_Got, &Map_Got_len) != 0) {
        Map_Got = NULL;
        Map_Got_len = 0;
    }
    fprintf(stderr, "Map_Got staged: %p len=%zu\n", (const void *)Map_Got, Map_Got_len);

    if (load_asset("artnem/explosion.nem", &Nem_Explode, &Nem_Explode_len) != 0) {
        Nem_Explode = NULL;
        Nem_Explode_len = 0;
    }

    if (load_asm_asset("maps/explosions.asm", &Map_ExplodeItem, &Map_ExplodeItem_len, 1) != 0) {
        Map_ExplodeItem = NULL;
        Map_ExplodeItem_len = 0;
    }

    if (load_asm_asset("maps/explosions.asm", &Map_ExplodeBomb, &Map_ExplodeBomb_len, 1) != 0) {
        Map_ExplodeBomb = NULL;
        Map_ExplodeBomb_len = 0;
    }

    if (load_asm_asset("maps/Collapsing Ledge.asm", &Map_Ledge, &Map_Ledge_len, 1) != 0) {
        Map_Ledge = NULL;
        Map_Ledge_len = 0;
    }

    if (load_asm_asset("maps/Collapsing Floors.asm", &Map_CFlo, &Map_CFlo_len, 1) != 0) {
        Map_CFlo = NULL;
        Map_CFlo_len = 0;
    }

    /* Animals mappings (28, 29 Animals and Points.asm) — single table per file */
    if (load_asm_asset("maps/Animals 1.asm", &Map_Animal1, &Map_Animal1_len, 1) != 0) {
        Map_Animal1 = NULL;
        Map_Animal1_len = 0;
    }

    if (load_asm_asset("maps/Animals 2.asm", &Map_Animal2, &Map_Animal2_len, 1) != 0) {
        Map_Animal2 = NULL;
        Map_Animal2_len = 0;
    }

    if (load_asm_asset("maps/Animals 3.asm", &Map_Animal3, &Map_Animal3_len, 1) != 0) {
        Map_Animal3 = NULL;
        Map_Animal3_len = 0;
    }

    /* Points object mappings (same file set, own table) */
    if (load_asm_asset("maps/points.asm", &Map_Points, &Map_Points_len, 1) != 0) {
        Map_Points = NULL;
        Map_Points_len = 0;
    }

    if (load_asm_asset("maps/sonic.asm", &Map_Sonic, &Map_Sonic_len, 1) != 0) {
        Map_Sonic = NULL;
        Map_Sonic_len = 0;
    }

    if (load_asset("artunc/Sonic.unc", &Art_Sonic, &Art_Sonic_len) != 0) {
        Art_Sonic = NULL;
        Art_Sonic_len = 0;
    }

    if (load_asm_asset("maps/Sonic - Dynamic Gfx Script.asm", &SonicDynPLC, &SonicDynPLC_len, 0) != 0) {
        SonicDynPLC = NULL;
        SonicDynPLC_len = 0;
    }

    if (load_asm_asset("anim/Sonic.asm", &Ani_Sonic, &Ani_Sonic_len, 0) != 0) {
        Ani_Sonic = NULL;
        Ani_Sonic_len = 0;
        
    }

        /* ===== DEBUG TEMPORAL — quitar después ===== */
    fprintf(stdout, "ASSETS: Ani=%p/%zu DynPLC=%p/%zu Map=%p/%zu Art=%p/%zu\n",
            (void*)Ani_Sonic, Ani_Sonic_len,
            (void*)SonicDynPLC, SonicDynPLC_len,
            (void*)Map_Sonic, Map_Sonic_len,
            (void*)Art_Sonic, Art_Sonic_len);

    if (Ani_Sonic && Ani_Sonic_len >= 32) {
        for (int i = 0; i < 16; i++) {
            uint16_t off = Ani_Sonic[i*2] | (Ani_Sonic[i*2+1] << 8);
            fprintf(stdout, "  anim[%02d]=%04X\n", i, off);
        }
    }
    if (SonicDynPLC && SonicDynPLC_len >= 32) {
        for (int i = 0; i < 16; i++) {
            uint16_t off = SonicDynPLC[i*2] | (SonicDynPLC[i*2+1] << 8);
            fprintf(stdout, "  dplc[%02d]=%04X\n", i, off);
        }
    }
    /* ===== FIN DEBUG ===== */

    if (load_asm_asset("anim/Monitor.asm", &Ani_Monitor, &Ani_Monitor_len, 0) != 0) {
        Ani_Monitor = NULL;
        Ani_Monitor_len = 0;

    }


    if (load_asset("tilemaps/title.eni", &Eni_Title, &Eni_Title_len) != 0) {
        Eni_Title = NULL;
        Eni_Title_len = 0;
    }

    if (load_asset("map16/ghz.eni", &Blk16_GHZ, &Blk16_GHZ_len) != 0) {
        Blk16_GHZ = NULL;
        Blk16_GHZ_len = 0;
    }

    if (load_asset("map256/ghz.kos", &Blk256_GHZ, &Blk256_GHZ_len) != 0) {
        Blk256_GHZ = NULL;
        Blk256_GHZ_len = 0;
    }

    if (load_asset("map16/lz.eni", &Blk16_LZ, &Blk16_LZ_len) != 0) {
        Blk16_LZ = NULL;
        Blk16_LZ_len = 0;
    }

    if (load_asset("map256/lz.kos", &Blk256_LZ, &Blk256_LZ_len) != 0) {
        Blk256_LZ = NULL;
        Blk256_LZ_len = 0;
    }

    if (load_asset("map16/mz.eni", &Blk16_MZ, &Blk16_MZ_len) != 0) {
        Blk16_MZ = NULL;
        Blk16_MZ_len = 0;
    }

    if (load_asset("map256/mz.kos", &Blk256_MZ, &Blk256_MZ_len) != 0) {
        Blk256_MZ = NULL;
        Blk256_MZ_len = 0;
    }

    if (load_asset("map16/slz.eni", &Blk16_SLZ, &Blk16_SLZ_len) != 0) {
        Blk16_SLZ = NULL;
        Blk16_SLZ_len = 0;
    }

    if (load_asset("map256/slz.kos", &Blk256_SLZ, &Blk256_SLZ_len) != 0) {
        Blk256_SLZ = NULL;
        Blk256_SLZ_len = 0;
    }

    if (load_asset("map16/syz.eni", &Blk16_SYZ, &Blk16_SYZ_len) != 0) {
        Blk16_SYZ = NULL;
        Blk16_SYZ_len = 0;
    }

    if (load_asset("map256/syz.kos", &Blk256_SYZ, &Blk256_SYZ_len) != 0) {
        Blk256_SYZ = NULL;
        Blk256_SYZ_len = 0;
    }

    if (load_asset("map16/sbz.eni", &Blk16_SBZ, &Blk16_SBZ_len) != 0) {
        Blk16_SBZ = NULL;
        Blk16_SBZ_len = 0;
    }

    if (load_asset("map256/sbz.kos", &Blk256_SBZ, &Blk256_SBZ_len) != 0) {
        Blk256_SBZ = NULL;
        Blk256_SBZ_len = 0;
    }

    if (load_asset("levels/ghz1.bin", &Level_GHZ1, &Level_GHZ1_len) != 0) {
        Level_GHZ1 = NULL;
        Level_GHZ1_len = 0;
    }

    if (load_asset("levels/ghzbg.bin", &Level_GHZbg, &Level_GHZbg_len) != 0) {
        Level_GHZbg = NULL;
        Level_GHZbg_len = 0;
    }

    if (load_asset("levels/ghz2.bin", &Level_GHZ2, &Level_GHZ2_len) != 0) {
        Level_GHZ2 = NULL;
        Level_GHZ2_len = 0;
    }
    if (load_asset("levels/ghz3.bin", &Level_GHZ3, &Level_GHZ3_len) != 0) {
        Level_GHZ3 = NULL;
        Level_GHZ3_len = 0;
    }
    if (load_asset("levels/lz1.bin", &Level_LZ1, &Level_LZ1_len) != 0) {
        Level_LZ1 = NULL;
        Level_LZ1_len = 0;
    }
    if (load_asset("levels/lz2.bin", &Level_LZ2, &Level_LZ2_len) != 0) {
        Level_LZ2 = NULL;
        Level_LZ2_len = 0;
    }
    if (load_asset("levels/lz3.bin", &Level_LZ3, &Level_LZ3_len) != 0) {
        Level_LZ3 = NULL;
        Level_LZ3_len = 0;
    }
    if (load_asset("levels/lzbg.bin", &Level_LZbg, &Level_LZbg_len) != 0) {
        Level_LZbg = NULL;
        Level_LZbg_len = 0;
    }
    if (load_asset("levels/sbz3.bin", &Level_SBZ3, &Level_SBZ3_len) != 0) {
        Level_SBZ3 = NULL;
        Level_SBZ3_len = 0;
    }
    if (load_asset("levels/mz1.bin", &Level_MZ1, &Level_MZ1_len) != 0) {
        Level_MZ1 = NULL;
        Level_MZ1_len = 0;
    }
    if (load_asset("levels/mz1bg.bin", &Level_MZ1bg, &Level_MZ1bg_len) != 0) {
        Level_MZ1bg = NULL;
        Level_MZ1bg_len = 0;
    }
    if (load_asset("levels/mz2.bin", &Level_MZ2, &Level_MZ2_len) != 0) {
        Level_MZ2 = NULL;
        Level_MZ2_len = 0;
    }
    if (load_asset("levels/mz2bg.bin", &Level_MZ2bg, &Level_MZ2bg_len) != 0) {
        Level_MZ2bg = NULL;
        Level_MZ2bg_len = 0;
    }
    if (load_asset("levels/mz3.bin", &Level_MZ3, &Level_MZ3_len) != 0) {
        Level_MZ3 = NULL;
        Level_MZ3_len = 0;
    }
    if (load_asset("levels/mz3bg.bin", &Level_MZ3bg, &Level_MZ3bg_len) != 0) {
        Level_MZ3bg = NULL;
        Level_MZ3bg_len = 0;
    }
    if (load_asset("levels/slz1.bin", &Level_SLZ1, &Level_SLZ1_len) != 0) {
        Level_SLZ1 = NULL;
        Level_SLZ1_len = 0;
    }
    if (load_asset("levels/slz2.bin", &Level_SLZ2, &Level_SLZ2_len) != 0) {
        Level_SLZ2 = NULL;
        Level_SLZ2_len = 0;
    }
    if (load_asset("levels/slz3.bin", &Level_SLZ3, &Level_SLZ3_len) != 0) {
        Level_SLZ3 = NULL;
        Level_SLZ3_len = 0;
    }
    if (load_asset("levels/slzbg.bin", &Level_SLZbg, &Level_SLZbg_len) != 0) {
        Level_SLZbg = NULL;
        Level_SLZbg_len = 0;
    }
    if (load_asset("levels/syz1.bin", &Level_SYZ1, &Level_SYZ1_len) != 0) {
        Level_SYZ1 = NULL;
        Level_SYZ1_len = 0;
    }
    if (load_asset("levels/syz2.bin", &Level_SYZ2, &Level_SYZ2_len) != 0) {
        Level_SYZ2 = NULL;
        Level_SYZ2_len = 0;
    }
    if (load_asset("levels/syz3.bin", &Level_SYZ3, &Level_SYZ3_len) != 0) {
        Level_SYZ3 = NULL;
        Level_SYZ3_len = 0;
    }
    if (load_asset("levels/syzbg.bin", &Level_SYZbg, &Level_SYZbg_len) != 0) {
        Level_SYZbg = NULL;
        Level_SYZbg_len = 0;
    }
    if (load_asset("levels/sbz1.bin", &Level_SBZ1, &Level_SBZ1_len) != 0) {
        Level_SBZ1 = NULL;
        Level_SBZ1_len = 0;
    }
    if (load_asset("levels/sbz1bg.bin", &Level_SBZ1bg, &Level_SBZ1bg_len) != 0) {
        Level_SBZ1bg = NULL;
        Level_SBZ1bg_len = 0;
    }
    if (load_asset("levels/sbz2.bin", &Level_SBZ2, &Level_SBZ2_len) != 0) {
        Level_SBZ2 = NULL;
        Level_SBZ2_len = 0;
    }
    if (load_asset("levels/sbz2bg.bin", &Level_SBZ2bg, &Level_SBZ2bg_len) != 0) {
        Level_SBZ2bg = NULL;
        Level_SBZ2bg_len = 0;
    }
    if (load_asset("levels/ending.bin", &Level_End, &Level_End_len) != 0) {
        Level_End = NULL;
        Level_End_len = 0;
    }

    if (load_asset("artnem/hud.nem", &Nem_Hud, &Nem_Hud_len) != 0) {
        Nem_Hud = NULL;
        Nem_Hud_len = 0;
    }

    if (load_asset("artnem/hud_lives.nem", &Nem_Lives, &Nem_Lives_len) != 0) {
        Nem_Lives = NULL;
        Nem_Lives_len = 0;
    }

    if (load_asset("artunc/HUD Numbers.unc", &Art_Hud, &Art_Hud_len) != 0) {
        Art_Hud = NULL;
        Art_Hud_len = 0;
    }

    if (load_asset("artunc/Lives Counter Numbers.unc", &Art_LivesNums, &Art_LivesNums_len) != 0) {
        Art_LivesNums = NULL;
        Art_LivesNums_len = 0;
    }

    if (load_asm_asset("maps/hud.asm", &Map_HUD, &Map_HUD_len, 1) != 0) {
        Map_HUD = NULL;
        Map_HUD_len = 0;
    }

    if (load_asm_asset("maps/Shield and Invincibility.asm", &Map_Shield, &Map_Shield_len, 1) != 0) {
        Map_Shield = NULL;
        Map_Shield_len = 0;
    }

    if (load_asm_asset("maps/Smashable Walls.asm", &Map_Smash, &Map_Smash_len, 1) != 0) {
        Map_Smash = NULL;
        Map_Smash_len = 0;
    }

    if (load_asm_asset("maps/Spiked Pole Helix.asm", &Map_Hel, &Map_Hel_len, 1) != 0) {
        Map_Hel = NULL;
        Map_Hel_len = 0;
    }

    if (load_asm_asset("maps/Swinging Platforms (GHZ).asm", &Map_Swing_GHZ, &Map_Swing_GHZ_len, 1) != 0) {
        Map_Swing_GHZ = NULL;
        Map_Swing_GHZ_len = 0;
    }

    if (load_asm_asset("maps/Swinging Platforms (SLZ).asm", &Map_Swing_SLZ, &Map_Swing_SLZ_len, 1) != 0) {
        Map_Swing_SLZ = NULL;
        Map_Swing_SLZ_len = 0;
    }

    if (load_asm_asset("maps/Eggman.asm", &Map_Eggman, &Map_Eggman_len, 1) != 0) {
        Map_Eggman = NULL;
        Map_Eggman_len = 0;
    }

    if (load_asm_asset("maps/Boss Items.asm", &Map_BossItems, &Map_BossItems_len, 1) != 0) {
        Map_BossItems = NULL;
        Map_BossItems_len = 0;
    }

    if (load_asset("artnem/rings.nem", &Nem_Ring, &Nem_Ring_len) != 0) {
        Nem_Ring = NULL;
        Nem_Ring_len = 0;
    }

    if (load_asm_asset("maps/rings.asm", &Map_Ring, &Map_Ring_len, 1) != 0) {
        Map_Ring = NULL;
        Map_Ring_len = 0;
    }

    if (load_asm_asset("anim/rings.asm", &Ani_Ring, &Ani_Ring_len, 0) != 0) {
        Ani_Ring = NULL;
        Ani_Ring_len = 0;
    }

    if (load_asm_asset("maps/signpost.asm", &Map_Sign, &Map_Sign_len, 1) != 0) {
        Map_Sign = NULL;
        Map_Sign_len = 0;
    }

    if (load_asm_asset("anim/signpost.asm", &Ani_Sign, &Ani_Sign_len, 0) != 0) {
        Ani_Sign = NULL;
        Ani_Sign_len = 0;
    }

    if (load_asm_asset("anim/Shield and Invincibility.asm", &Ani_Shield, &Ani_Shield_len, 0) != 0) {
        Ani_Shield = NULL;
        Ani_Shield_len = 0;
    }

    if (load_asm_asset("maps/crabmeat.asm", &Map_Crab, &Map_Crab_len, 1) != 0) {
        Map_Crab = NULL;
        Map_Crab_len = 0;
    }

    if (load_asm_asset("anim/crabmeat.asm", &Ani_Crab, &Ani_Crab_len, 0) != 0) {
        Ani_Crab = NULL;
        Ani_Crab_len = 0;
    }

    if (load_asm_asset("maps/motobug.asm", &Map_Moto, &Map_Moto_len, 1) != 0) {
        Map_Moto = NULL;
        Map_Moto_len = 0;
    }

    if (load_asm_asset("anim/motobug.asm", &Ani_Moto, &Ani_Moto_len, 0) != 0) {
        Ani_Moto = NULL;
        Ani_Moto_len = 0;
    }

    if (load_asm_asset("maps/buzzbomber.asm", &Map_Buzz, &Map_Buzz_len, 1) != 0) {
        Map_Buzz = NULL;
        Map_Buzz_len = 0;
    }

    if (load_asm_asset("maps/Monitor.asm", &Map_Monitor, &Map_Monitor_len, 1) != 0) {
        Map_Monitor = NULL;
        Map_Monitor_len = 0;
    }

    if (load_asm_asset("maps/Spikes.asm", &Map_Spike, &Map_Spike_len, 1) != 0) {
        Map_Spike = NULL;
        Map_Spike_len = 0;
    }

    if (load_asm_asset("maps/Chopper.asm", &Map_Chop, &Map_Chop_len, 1) != 0) {
        Map_Chop = NULL;
        Map_Chop_len = 0;
    }

    if (load_asm_asset("maps/Platforms (GHZ).asm", &Map_Plat_GHZ, &Map_Plat_GHZ_len, 1) != 0) {
        Map_Plat_GHZ = NULL;
        Map_Plat_GHZ_len = 0;
    }

    if (load_asm_asset("maps/Springs.asm", &Map_Spring, &Map_Spring_len, 1) != 0) {
        Map_Spring = NULL;
        Map_Spring_len = 0;
    }

    if (load_asm_asset("anim/Springs.asm", &Ani_Spring, &Ani_Spring_len, 0) != 0) {
        Ani_Spring = NULL;
        Ani_Spring_len = 0;
    }


    if (load_asm_asset("anim/buzzbomber.asm", &Ani_Buzz, &Ani_Buzz_len, 0) != 0) {
        Ani_Buzz = NULL;
        Ani_Buzz_len = 0;
    }

    if (load_asm_asset("maps/buzzmissile.asm", &Map_Missile, &Map_Missile_len, 1) != 0) {
        Map_Missile = NULL;
        Map_Missile_len = 0;
    }

    if (load_asm_asset("anim/Chopper.asm", &Ani_Chop, &Ani_Chop_len, 0) != 0) {
        Ani_Chop = NULL;
        Ani_Chop_len = 0;
    }

    if (load_asm_asset("anim/Eggman.asm", &Ani_Eggman, &Ani_Eggman_len, 0) != 0) {
        Ani_Eggman = NULL;
        Ani_Eggman_len = 0;
    }

    if (load_asm_asset("anim/buzzmissile.asm", &Ani_Missile, &Ani_Missile_len, 0) != 0) {
        Ani_Missile = NULL;
        Ani_Missile_len = 0;
    }

    if (load_asm_asset("maps/bridge.asm", &Map_Bri, &Map_Bri_len, 1) != 0) {
        Map_Bri = NULL;
        Map_Bri_len = 0;
    }

    if (load_asm_asset("maps/Scenery.asm", &Map_Scen, &Map_Scen_len, 1) != 0) {
        Map_Scen = NULL;
        Map_Scen_len = 0;
    }

    if (load_asm_asset("maps/purple rock.asm", &Map_PRock, &Map_PRock_len, 1) != 0) {
        Map_PRock = NULL;
        Map_PRock_len = 0;
    }

    if (load_asm_asset("maps/ghz edge walls.asm", &Map_Edge, &Map_Edge_len, 1) != 0) {
        Map_Edge = NULL;
        Map_Edge_len = 0;
    }

    if (load_asm_asset("maps/GHZ Ball.asm", &Map_GBall, &Map_GBall_len, 1) != 0) {
        Map_GBall = NULL;
        Map_GBall_len = 0;
    }


    if (load_asset("objpos/ghz1.bin", &ObjPos_GHZ1, &ObjPos_GHZ1_len) != 0) {
        ObjPos_GHZ1 = NULL;
        ObjPos_GHZ1_len = 0;
    }

    if (load_asset("objpos/ghz2.bin", &ObjPos_GHZ2, &ObjPos_GHZ2_len) != 0) {
        ObjPos_GHZ2 = NULL;
        ObjPos_GHZ2_len = 0;
    }
    if (load_asset("objpos/ghz3.bin", &ObjPos_GHZ3, &ObjPos_GHZ3_len) != 0) {
        ObjPos_GHZ3 = NULL;
        ObjPos_GHZ3_len = 0;
    }
    if (load_asset("objpos/lz1.bin", &ObjPos_LZ1, &ObjPos_LZ1_len) != 0) {
        ObjPos_LZ1 = NULL;
        ObjPos_LZ1_len = 0;
    }
    if (load_asset("objpos/lz2.bin", &ObjPos_LZ2, &ObjPos_LZ2_len) != 0) {
        ObjPos_LZ2 = NULL;
        ObjPos_LZ2_len = 0;
    }
    if (load_asset("objpos/lz3.bin", &ObjPos_LZ3, &ObjPos_LZ3_len) != 0) {
        ObjPos_LZ3 = NULL;
        ObjPos_LZ3_len = 0;
    }
    if (load_asset("objpos/sbz3.bin", &ObjPos_SBZ3, &ObjPos_SBZ3_len) != 0) {
        ObjPos_SBZ3 = NULL;
        ObjPos_SBZ3_len = 0;
    }
    if (load_asset("objpos/mz1.bin", &ObjPos_MZ1, &ObjPos_MZ1_len) != 0) {
        ObjPos_MZ1 = NULL;
        ObjPos_MZ1_len = 0;
    }
    if (load_asset("objpos/mz2.bin", &ObjPos_MZ2, &ObjPos_MZ2_len) != 0) {
        ObjPos_MZ2 = NULL;
        ObjPos_MZ2_len = 0;
    }
    if (load_asset("objpos/mz3.bin", &ObjPos_MZ3, &ObjPos_MZ3_len) != 0) {
        ObjPos_MZ3 = NULL;
        ObjPos_MZ3_len = 0;
    }
    if (load_asset("objpos/slz1.bin", &ObjPos_SLZ1, &ObjPos_SLZ1_len) != 0) {
        ObjPos_SLZ1 = NULL;
        ObjPos_SLZ1_len = 0;
    }
    if (load_asset("objpos/slz2.bin", &ObjPos_SLZ2, &ObjPos_SLZ2_len) != 0) {
        ObjPos_SLZ2 = NULL;
        ObjPos_SLZ2_len = 0;
    }
    if (load_asset("objpos/slz3.bin", &ObjPos_SLZ3, &ObjPos_SLZ3_len) != 0) {
        ObjPos_SLZ3 = NULL;
        ObjPos_SLZ3_len = 0;
    }
    if (load_asset("objpos/syz1.bin", &ObjPos_SYZ1, &ObjPos_SYZ1_len) != 0) {
        ObjPos_SYZ1 = NULL;
        ObjPos_SYZ1_len = 0;
    }
    if (load_asset("objpos/syz2.bin", &ObjPos_SYZ2, &ObjPos_SYZ2_len) != 0) {
        ObjPos_SYZ2 = NULL;
        ObjPos_SYZ2_len = 0;
    }
    if (load_asset("objpos/syz3.bin", &ObjPos_SYZ3, &ObjPos_SYZ3_len) != 0) {
        ObjPos_SYZ3 = NULL;
        ObjPos_SYZ3_len = 0;
    }
    if (load_asset("objpos/sbz1.bin", &ObjPos_SBZ1, &ObjPos_SBZ1_len) != 0) {
        ObjPos_SBZ1 = NULL;
        ObjPos_SBZ1_len = 0;
    }
    if (load_asset("objpos/sbz2.bin", &ObjPos_SBZ2, &ObjPos_SBZ2_len) != 0) {
        ObjPos_SBZ2 = NULL;
        ObjPos_SBZ2_len = 0;
    }
    if (load_asset("objpos/fz.bin", &ObjPos_FZ, &ObjPos_FZ_len) != 0) {
        ObjPos_FZ = NULL;
        ObjPos_FZ_len = 0;
    }
    if (load_asset("objpos/ending.bin", &ObjPos_End, &ObjPos_End_len) != 0) {
        ObjPos_End = NULL;
        ObjPos_End_len = 0;
    }

    if (load_asset("collide/GHZ.bin", &Col_GHZ, &Col_GHZ_len) != 0) {
        Col_GHZ = NULL;
        Col_GHZ_len = 0;
    }
    if (load_asset("collide/LZ.bin", &Col_LZ, &Col_LZ_len) != 0) {
        Col_LZ = NULL;
        Col_LZ_len = 0;
    }
    if (load_asset("collide/MZ.bin", &Col_MZ, &Col_MZ_len) != 0) {
        Col_MZ = NULL;
        Col_MZ_len = 0;
    }
    if (load_asset("collide/SLZ.bin", &Col_SLZ, &Col_SLZ_len) != 0) {
        Col_SLZ = NULL;
        Col_SLZ_len = 0;
    }
    if (load_asset("collide/SYZ.bin", &Col_SYZ, &Col_SYZ_len) != 0) {
        Col_SYZ = NULL;
        Col_SYZ_len = 0;
    }
    if (load_asset("collide/SBZ.bin", &Col_SBZ, &Col_SBZ_len) != 0) {
        Col_SBZ = NULL;
        Col_SBZ_len = 0;
    }

    if (load_asset("artunc/GHZ Waterfall.unc", &Art_GhzWater, &Art_GhzWater_len) != 0) {
        Art_GhzWater = NULL;
        Art_GhzWater_len = 0;
    }
    if (load_asset("artunc/GHZ Flower Large.unc", &Art_GhzFlower1, &Art_GhzFlower1_len) != 0) {
        Art_GhzFlower1 = NULL;
        Art_GhzFlower1_len = 0;
    }
    if (load_asset("artunc/GHZ Flower Small.unc", &Art_GhzFlower2, &Art_GhzFlower2_len) != 0) {
        Art_GhzFlower2 = NULL;
        Art_GhzFlower2_len = 0;
    }
    if (load_asset("artunc/MZ Lava Surface.unc", &Art_MzLava1, &Art_MzLava1_len) != 0) {
        Art_MzLava1 = NULL;
        Art_MzLava1_len = 0;
    }
    if (load_asset("artunc/MZ Lava.unc", &Art_MzLava2, &Art_MzLava2_len) != 0) {
        Art_MzLava2 = NULL;
        Art_MzLava2_len = 0;
    }
    if (load_asset("artunc/MZ Background Torch.unc", &Art_MzTorch, &Art_MzTorch_len) != 0) {
        Art_MzTorch = NULL;
        Art_MzTorch_len = 0;
    }
    if (load_asset("artunc/SBZ Background Smoke.unc", &Art_SbzSmoke, &Art_SbzSmoke_len) != 0) {
        Art_SbzSmoke = NULL;
        Art_SbzSmoke_len = 0;
    }
    if (load_asset("artunc/Giant Ring.unc", &Art_BigRing, &Art_BigRing_len) != 0) {
        Art_BigRing = NULL;
        Art_BigRing_len = 0;
    }

    if (load_asset("collide/Angle_Map.bin", &Col_AngleMap, &Col_AngleMap_len) != 0) {
        Col_AngleMap = NULL;
        Col_AngleMap_len = 0;
    }
    if (load_asset("collide/Collision_Array_Normal.bin", &Col_CollArray1, &Col_CollArray1_len) != 0) {
        Col_CollArray1 = NULL;
        Col_CollArray1_len = 0;
    }
    if (load_asset("collide/Collision_Array_Rotated.bin", &Col_CollArray2, &Col_CollArray2_len) != 0) {
        Col_CollArray2 = NULL;
        Col_CollArray2_len = 0;
    }

    if (load_asset("artnem/jap_credits.nem", &Nem_JapNames, &Nem_JapNames_len) != 0) {
        Nem_JapNames = NULL;
        Nem_JapNames_len = 0;
    }

    if (load_asset("tilemaps/jap_credits.eni", &Eni_JapNames, &Eni_JapNames_len) != 0) {
        Eni_JapNames = NULL;
        Eni_JapNames_len = 0;
    }

    if (load_asset("artnem/credit_text.nem", &Nem_CreditText, &Nem_CreditText_len) != 0) {
        Nem_CreditText = NULL;
        Nem_CreditText_len = 0;
    }

    if (load_asset("artunc/Level Select & Debug Text.unc", &Art_Text, &Art_Text_len) != 0) {
        Art_Text = NULL;
        Art_Text_len = 0;
    }

    {
        static const char * const startloc_files[] = {
            "startpos/ghz1.bin", "startpos/ghz2.bin", "startpos/ghz3.bin", NULL,
            "startpos/lz1.bin", "startpos/lz2.bin", "startpos/lz3.bin", "startpos/sbz3.bin",
            "startpos/mz1.bin", "startpos/mz2.bin", "startpos/mz3.bin", NULL,
            "startpos/slz1.bin", "startpos/slz2.bin", "startpos/slz3.bin", NULL,
            "startpos/syz1.bin", "startpos/syz2.bin", "startpos/syz3.bin", NULL,
            "startpos/sbz1.bin", "startpos/sbz2.bin", "startpos/fz.bin", NULL,
            "startpos/end1.bin", "startpos/end2.bin", NULL, NULL,
        };
        size_t total = 28 * 4;
        uint8_t *buf = (uint8_t *)malloc(total);
        if (!buf) {
            StartLocArray = NULL;
            StartLocArray_len = 0;
        } else {
            size_t off = 0;
            int ok = 1;
            for (int i = 0; i < 28; i++) {
                if (!startloc_files[i]) {
                    buf[off++] = 0x00;
                    buf[off++] = 0x80;
                    buf[off++] = 0x00;
                    buf[off++] = 0xA8;
                } else {
                    size_t len;
                    char fullpath[PATH_MAX + 512];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", assets_base_path(), startloc_files[i]);
                    const uint8_t *tmp = Assets_Load(fullpath, &len);
                    if (!tmp || len < 4) { ok = 0; break; }
                    memcpy(buf + off, tmp, 4);
                    free((void *)tmp);
                    off += 4;
                }
            }
            if (!ok) {
                free(buf);
                StartLocArray = NULL;
                StartLocArray_len = 0;
            } else {
                StartLocArray = buf;
                StartLocArray_len = total;
            }
        }
    }

    {
        static const char * const ending_files[] = {
            "startpos/Credits Demos/ghz1 (Credits demo 1).bin",
            "startpos/Credits Demos/mz2 (Credits demo).bin",
            "startpos/Credits Demos/syz3 (Credits demo).bin",
            "startpos/Credits Demos/lz3 (Credits demo).bin",
            "startpos/Credits Demos/slz3 (Credits demo).bin",
            "startpos/Credits Demos/sbz1 (Credits demo).bin",
            "startpos/Credits Demos/sbz2 (Credits demo).bin",
            "startpos/Credits Demos/ghz1 (Credits demo 2).bin",
        };
        size_t total = 8 * 4;
        uint8_t *buf = (uint8_t *)malloc(total);
        if (!buf) {
            EndingStLocArray = NULL;
            EndingStLocArray_len = 0;
        } else {
            size_t off = 0;
            int ok = 1;
            for (int i = 0; i < 8; i++) {
                size_t len;
                char fullpath[PATH_MAX + 512];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", assets_base_path(), ending_files[i]);
                const uint8_t *tmp = Assets_Load(fullpath, &len);
                if (!tmp || len < 4) { ok = 0; break; }
                memcpy(buf + off, tmp, 4);
                free((void *)tmp);
                off += 4;
            }
            if (!ok) {
                free(buf);
                EndingStLocArray = NULL;
                EndingStLocArray_len = 0;
            } else {
                EndingStLocArray = buf;
                EndingStLocArray_len = total;
            }
        }
    }

    Palette_Init();
    LevelHeaders_Init();
    return 0;
}

void Data_Quit(void) {
#define FREE_ASSET(p) do { Assets_Free(p); p = NULL; } while(0)
#define MUNMAP_ASSET(p) do { free_32bit(p); p = NULL; } while(0)
    FREE_ASSET(Pal_SegaBG);
    FREE_ASSET(Pal_Sega1);
    FREE_ASSET(Pal_Sega2);
    FREE_ASSET(Nem_SegaLogo);
    FREE_ASSET(Eni_SegaLogo);
    FREE_ASSET(Pal_Title);
    FREE_ASSET(Pal_TitleCycWater);
    FREE_ASSET(Pal_GHZCycWater);
    FREE_ASSET(Pal_LevelSel);
    FREE_ASSET(Pal_Sonic);
    FREE_ASSET(Pal_GHZ);
    FREE_ASSET(Pal_LZ);
    FREE_ASSET(Pal_LZWater);
    FREE_ASSET(Pal_LZSonWater);
    FREE_ASSET(Pal_MZ);
    FREE_ASSET(Pal_SLZ);
    FREE_ASSET(Pal_SYZ);
    FREE_ASSET(Pal_SBZ1);
    FREE_ASSET(Pal_SBZ2);
    FREE_ASSET(Pal_SBZ3);
    FREE_ASSET(Pal_SBZ3Water);
    FREE_ASSET(Pal_SBZ3SonWat);
    FREE_ASSET(Pal_Special);
    FREE_ASSET(Pal_SSResult);
    FREE_ASSET(Pal_Continue);
    FREE_ASSET(Pal_Ending);
    FREE_ASSET(Nem_JapNames);
    FREE_ASSET(Eni_JapNames);
    FREE_ASSET(Nem_CreditText);
    FREE_ASSET(Nem_TitleFg);
    FREE_ASSET(Nem_TitleSonic);
    FREE_ASSET(Nem_TitleTM);
    FREE_ASSET(Art_Text);
    FREE_ASSET(Blk16_GHZ);
    FREE_ASSET(Blk256_GHZ);
    FREE_ASSET(Blk16_LZ);
    FREE_ASSET(Blk256_LZ);
    FREE_ASSET(Blk16_MZ);
    FREE_ASSET(Blk256_MZ);
    FREE_ASSET(Blk16_SLZ);
    FREE_ASSET(Blk256_SLZ);
    FREE_ASSET(Blk16_SYZ);
    FREE_ASSET(Blk256_SYZ);
    FREE_ASSET(Blk16_SBZ);
    FREE_ASSET(Blk256_SBZ);
    FREE_ASSET(Eni_Title);
    FREE_ASSET(Nem_GHZ_1st);
    FREE_ASSET(Nem_GHZ_2nd);
    FREE_ASSET(Nem_Stalk);
    FREE_ASSET(Nem_PplRock);
    FREE_ASSET(Nem_GhzWall2);
    FREE_ASSET(Nem_Swing);
    FREE_ASSET(Nem_Bridge);
    FREE_ASSET(Nem_SpikePole);
    FREE_ASSET(Nem_Ball);
    FREE_ASSET(Nem_GhzWall1);
    FREE_ASSET(Nem_Crabmeat);
    FREE_ASSET(Nem_Buzz);
    FREE_ASSET(Nem_Chopper);
    FREE_ASSET(Nem_Newtron);
    FREE_ASSET(Nem_Motobug);
    FREE_ASSET(Nem_Spikes);
    FREE_ASSET(Nem_HSpring);
    FREE_ASSET(Nem_VSpring);
    FREE_ASSET(Nem_Monitors);
    FREE_ASSET(Nem_Shield);
    FREE_ASSET(Nem_Stars);
    FREE_ASSET(Nem_SignPost);
    FREE_ASSET(Nem_Bonus);
    FREE_ASSET(Nem_BigFlash);
    FREE_ASSET(Nem_Rabbit);
    FREE_ASSET(Nem_Chicken);
    FREE_ASSET(Nem_Penguin);
    FREE_ASSET(Nem_Seal);
    FREE_ASSET(Nem_Pig);
    FREE_ASSET(Nem_Flicky);
    FREE_ASSET(Nem_Squirrel);
    FREE_ASSET(Nem_BallHog);
    FREE_ASSET(Nem_Basaran);
    FREE_ASSET(Nem_Bomb);
    FREE_ASSET(Nem_Bubbles);
    FREE_ASSET(Nem_Bumper);
    FREE_ASSET(Nem_Burrobot);
    FREE_ASSET(Nem_Cater);
    FREE_ASSET(Nem_Cork);
    FREE_ASSET(Nem_Cutter);
    FREE_ASSET(Nem_Eggman);
    FREE_ASSET(Nem_Electric);
    FREE_ASSET(Nem_EndEm);
    FREE_ASSET(Nem_EndFlower);
    FREE_ASSET(Nem_EndSonic);
    FREE_ASSET(Nem_EndStH);
    FREE_ASSET(Nem_Exhaust);
    FREE_ASSET(Nem_Fan);
    FREE_ASSET(Nem_FlamePipe);
    FREE_ASSET(Nem_FlapDoor);
    FREE_ASSET(Nem_FzBoss);
    FREE_ASSET(Nem_FzEggman);
    FREE_ASSET(Nem_GameOver);
    FREE_ASSET(Nem_Gargoyle);
    FREE_ASSET(Nem_Girder);
    FREE_ASSET(Nem_Harpoon);
    FREE_ASSET(Nem_Jaws);
    FREE_ASSET(Nem_LZ);
    FREE_ASSET(Nem_Lamp);
    FREE_ASSET(Nem_Lava);
    FREE_ASSET(Nem_LzBlock1);
    FREE_ASSET(Nem_LzBlock2);
    FREE_ASSET(Nem_LzBlock3);
    FREE_ASSET(Nem_LzDoor1);
    FREE_ASSET(Nem_LzDoor2);
    FREE_ASSET(Nem_LzPlatfm);
    FREE_ASSET(Nem_LzPole);
    FREE_ASSET(Nem_LzSpikeBall);
    FREE_ASSET(Nem_LzSwitch);
    FREE_ASSET(Nem_LzWheel);
    FREE_ASSET(Nem_MZ);
    FREE_ASSET(Nem_MiniSonic);
    FREE_ASSET(Nem_MzBlock);
    FREE_ASSET(Nem_MzFire);
    FREE_ASSET(Nem_MzGlass);
    FREE_ASSET(Nem_MzMetal);
    FREE_ASSET(Nem_MzSwitch);
    FREE_ASSET(Nem_Orbinaut);
    FREE_ASSET(Nem_Points);
    FREE_ASSET(Nem_Prison);
    FREE_ASSET(Nem_Pylon);
    FREE_ASSET(Nem_ResultEm);
    FREE_ASSET(Nem_Roller);
    FREE_ASSET(Nem_SBZ);
    FREE_ASSET(Nem_SLZ);
    FREE_ASSET(Nem_SS1UpBlock);
    FREE_ASSET(Nem_SSBgCloud);
    FREE_ASSET(Nem_SSBgFish);
    FREE_ASSET(Nem_SSEmStars);
    FREE_ASSET(Nem_SSEmerald);
    FREE_ASSET(Nem_SSGOAL);
    FREE_ASSET(Nem_SSGhost);
    FREE_ASSET(Nem_SSGlass);
    FREE_ASSET(Nem_SSRBlock);
    FREE_ASSET(Nem_SSRedWhite);
    FREE_ASSET(Nem_SSUpDown);
    FREE_ASSET(Nem_SSWBlock);
    FREE_ASSET(Nem_SSWalls);
    FREE_ASSET(Nem_SSZone1);
    FREE_ASSET(Nem_SSZone2);
    FREE_ASSET(Nem_SSZone3);
    FREE_ASSET(Nem_SSZone4);
    FREE_ASSET(Nem_SSZone5);
    FREE_ASSET(Nem_SSZone6);
    FREE_ASSET(Nem_SYZ);
    FREE_ASSET(Nem_Sbz2Eggman);
    FREE_ASSET(Nem_SbzBlock);
    FREE_ASSET(Nem_SbzDoor1);
    FREE_ASSET(Nem_SbzDoor2);
    FREE_ASSET(Nem_SbzFloor);
    FREE_ASSET(Nem_SbzWheel1);
    FREE_ASSET(Nem_SbzWheel2);
    FREE_ASSET(Nem_Seesaw);
    FREE_ASSET(Nem_SlideFloor);
    FREE_ASSET(Nem_SlzBlock);
    FREE_ASSET(Nem_SlzCannon);
    FREE_ASSET(Nem_SlzSpike);
    FREE_ASSET(Nem_SlzSwing);
    FREE_ASSET(Nem_SlzWall);
    FREE_ASSET(Nem_SpinPform);
    FREE_ASSET(Nem_Splash);
    FREE_ASSET(Nem_Stomper);
    FREE_ASSET(Nem_SyzSpike1);
    FREE_ASSET(Nem_SyzSpike2);
    FREE_ASSET(Nem_TrapDoor);
    FREE_ASSET(Nem_TryAgain);
    FREE_ASSET(Nem_Water);
    FREE_ASSET(Nem_Weapons);
    FREE_ASSET(Nem_Yadrin);
    FREE_ASSET(Nem_TitleCard);
    MUNMAP_ASSET(Map_Card);
    MUNMAP_ASSET(Map_Got);
    MUNMAP_ASSET(Map_Sonic);
    FREE_ASSET(Art_Sonic);
    MUNMAP_ASSET(SonicDynPLC);
    MUNMAP_ASSET(Ani_Sonic);
    FREE_ASSET(Nem_Hud);
    FREE_ASSET(Nem_Lives);
    FREE_ASSET(Art_Hud);
    FREE_ASSET(Art_LivesNums);
    MUNMAP_ASSET(Map_HUD);
    FREE_ASSET(Nem_Ring);
    MUNMAP_ASSET(Map_Ring);
    MUNMAP_ASSET(Ani_Ring);
    MUNMAP_ASSET(Map_Sign);
    MUNMAP_ASSET(Ani_Sign);
    MUNMAP_ASSET(Map_Crab);
    MUNMAP_ASSET(Ani_Crab);
    MUNMAP_ASSET(Map_Moto);
    MUNMAP_ASSET(Ani_Moto);
    MUNMAP_ASSET(Map_Buzz);
    MUNMAP_ASSET(Ani_Buzz);
    MUNMAP_ASSET(Map_Missile);
    MUNMAP_ASSET(Ani_Missile);
    MUNMAP_ASSET(Map_Bri);
    MUNMAP_ASSET(Map_PRock);
    MUNMAP_ASSET(Map_Edge);
    MUNMAP_ASSET(Map_ExplodeItem);
    MUNMAP_ASSET(Map_ExplodeBomb);
    MUNMAP_ASSET(Map_Animal1);
    MUNMAP_ASSET(Map_Animal2);
    MUNMAP_ASSET(Map_Animal3);
    MUNMAP_ASSET(Map_Points);
    FREE_ASSET(ObjPos_GHZ1);
    FREE_ASSET(ObjPos_GHZ2);
    FREE_ASSET(ObjPos_GHZ3);
    FREE_ASSET(ObjPos_LZ1);
    FREE_ASSET(ObjPos_LZ2);
    FREE_ASSET(ObjPos_LZ3);
    FREE_ASSET(ObjPos_SBZ3);
    FREE_ASSET(ObjPos_MZ1);
    FREE_ASSET(ObjPos_MZ2);
    FREE_ASSET(ObjPos_MZ3);
    FREE_ASSET(ObjPos_SLZ1);
    FREE_ASSET(ObjPos_SLZ2);
    FREE_ASSET(ObjPos_SLZ3);
    FREE_ASSET(ObjPos_SYZ1);
    FREE_ASSET(ObjPos_SYZ2);
    FREE_ASSET(ObjPos_SYZ3);
    FREE_ASSET(ObjPos_SBZ1);
    FREE_ASSET(ObjPos_SBZ2);
    FREE_ASSET(ObjPos_FZ);
    FREE_ASSET(ObjPos_End);
    FREE_ASSET(Col_GHZ);
    FREE_ASSET(Col_LZ);
    FREE_ASSET(Col_MZ);
    FREE_ASSET(Col_SLZ);
    FREE_ASSET(Col_SYZ);
    FREE_ASSET(Col_SBZ);
    FREE_ASSET(Level_GHZ1);
    FREE_ASSET(Level_GHZ2);
    FREE_ASSET(Level_GHZ3);
    FREE_ASSET(Level_GHZbg);
    FREE_ASSET(Level_LZ1);
    FREE_ASSET(Level_LZ2);
    FREE_ASSET(Level_LZ3);
    FREE_ASSET(Level_LZbg);
    FREE_ASSET(Level_SBZ3);
    FREE_ASSET(Level_MZ1);
    FREE_ASSET(Level_MZ1bg);
    FREE_ASSET(Level_MZ2);
    FREE_ASSET(Level_MZ2bg);
    FREE_ASSET(Level_MZ3);
    FREE_ASSET(Level_MZ3bg);
    FREE_ASSET(Level_SLZ1);
    FREE_ASSET(Level_SLZ2);
    FREE_ASSET(Level_SLZ3);
    FREE_ASSET(Level_SLZbg);
    FREE_ASSET(Level_SYZ1);
    FREE_ASSET(Level_SYZ2);
    FREE_ASSET(Level_SYZ3);
    FREE_ASSET(Level_SYZbg);
    FREE_ASSET(Level_SBZ1);
    FREE_ASSET(Level_SBZ1bg);
    FREE_ASSET(Level_SBZ2);
    FREE_ASSET(Level_SBZ2bg);
    FREE_ASSET(Level_End);
    FREE_ASSET(Art_GhzWater);
    FREE_ASSET(Art_GhzFlower1);
    FREE_ASSET(Art_GhzFlower2);
    FREE_ASSET(Art_MzLava1);
    FREE_ASSET(Art_MzLava2);
    FREE_ASSET(Art_MzTorch);
    FREE_ASSET(Art_SbzSmoke);
    FREE_ASSET(Art_BigRing);
    FREE_ASSET(Col_AngleMap);
    FREE_ASSET(Col_CollArray1);
    FREE_ASSET(Col_CollArray2);
    FREE_ASSET(StartLocArray);
    FREE_ASSET(EndingStLocArray);
#undef FREE_ASSET
}

/* Cheat data remains static for now */
const uint8_t LevSelCode_US[] = {btnUp, btnDn, btnL, btnR, 0, 0xFF};
const uint32_t LevSelCode_US_len = 6;

const uint8_t LevSelCode_J[] = {btnUp, btnDn, btnL, btnR, 0, 0xFF};
const uint32_t LevSelCode_J_len = 6;

const uint16_t LevSel_Ptrs[] = {
    id_GHZ_act1,
    id_GHZ_act2,
    id_GHZ_act3,
    id_MZ_act1,
    id_MZ_act2,
    id_MZ_act3,
    id_SYZ_act1,
    id_SYZ_act2,
    id_SYZ_act3,
    id_LZ_act1,
    id_LZ_act2,
    id_LZ_act3,
    id_SLZ_act1,
    id_SLZ_act2,
    id_SLZ_act3,
    id_SBZ_act1,
    id_SBZ_act2,
    id_LZ_act4,            /* Scrap Brain Zone 3 */
    id_FZ,                 /* Final Zone */
    (uint16_t)(id_SS << 8), /* Special Stage (dummy value) */
    0x8000                 /* Sound Test */
};
const uint32_t LevSel_Ptrs_len = sizeof(LevSel_Ptrs);

/* ===========================================================================
   Title screen animation scripts (from _anim slash .asm)
   Format: word offset to animation data, then duration + frame IDs/flags
   =========================================================================== */

const uint8_t *Ani_TSon = NULL;
size_t   Ani_TSon_len = 0;

const uint8_t *Ani_PSBTM = NULL;
size_t   Ani_PSBTM_len = 0;

const uint8_t *Ani_Pri = NULL;
size_t   Ani_Pri_len = 0;
/* ===========================================================================
   Title screen sprite mappings (from _maps slash .asm)
   These are loaded at runtime from assets/
   =========================================================================== */

const uint8_t *Map_TSon = NULL;
size_t   Map_TSon_len = 0;

const uint8_t *Map_PSB = NULL;
size_t   Map_PSB_len = 0;

const uint8_t *Map_Pri = NULL;
size_t   Map_Pri_len = 0;

const uint8_t *Map_Cred = NULL;
size_t   Map_Cred_len = 0;

const uint8_t *Nem_TitleCard = NULL;
size_t   Nem_TitleCard_len = 0;

/* Explosion art (Nem_Explode) */
const uint8_t *Nem_Explode = NULL;
size_t   Nem_Explode_len = 0;

/* Zone title card sprite mappings */
const uint8_t *Map_Card = NULL;
size_t   Map_Card_len = 0;
const uint8_t *Map_Got = NULL;
size_t   Map_Got_len = 0;

/* Explosion mappings (27 ExplosionItem / 3F Explosion) */
const uint8_t *Map_ExplodeItem = NULL;
size_t   Map_ExplodeItem_len = 0;
const uint8_t *Map_ExplodeBomb = NULL;
size_t   Map_ExplodeBomb_len = 0;

/* ===========================================================================
   ASM parser for original Sonic 1 anim/map assets
   =========================================================================== */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_comments_and_spaces(const char *p) {
    while (*p) {
        if (*p == ';') {
            while (*p && *p != '\n') p++;
        } else if (isspace((unsigned char)*p)) {
            p++;
        } else {
            break;
        }
    }
    return p;
}

static long parse_asm_number(const char *p, const char **end) {
    long total = 0;
    for (;;) {
        while (*p && isspace((unsigned char)*p)) p++;
        long term = 0;
        if (*p == '-') {
            const char *e2 = NULL;
            long neg = parse_asm_number(p + 1, &e2);
            if (end) *end = e2;
            term = -neg;
        } else if (*p == '$') {
            p++;
            while (isxdigit((unsigned char)*p)) {
                term = term * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
                p++;
            }
            if (end) *end = p;
        } else if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
            while (isxdigit((unsigned char)*p)) {
                term = term * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
                p++;
            }
            if (end) *end = p;
        } else {
            /* Symbolic animation flags used in dc.b lines (afBack, afEnd, ...)
               plus the frame flip bits (aniXFlip = $20, aniYFlip = $40). */
            static const char *af_names[] = { "afBack", "afEnd", "afChange",
                                              "afRoutine", "afReset", "af2ndRoutine", "afWait",
                                              "aniXFlip", "aniYFlip" };
            static const long  af_vals[]   = { 0xFE, 0xFF, 0xFD, 0xFC, 0xFB, 0xFA, 0x80,
                                               0x20, 0x40 };
            const char *sym = p;
            while (*sym && isalnum((unsigned char)*sym)) sym++;
            size_t sym_len = (size_t)(sym - p);
            int found = 0;
            for (int i = 0; i < 9; i++) {
                size_t n = strlen(af_names[i]);
                if (sym_len == n && strncmp(p, af_names[i], n) == 0) {
                    term = af_vals[i];
                    if (end) *end = sym;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                char *ep = NULL;
                term = strtol(p, &ep, 10);
                if (ep && end) *end = ep;
            }
        }
        total |= (unsigned long)term;
        /* Bitwise OR expression: "2|aniXFlip" etc. */
        p = (end && *end) ? *end : p;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '|') break;
        p++;
    }
    return (long)total;
}

static int is_directive(const char *line, const char *dir) {
    const char *p = skip_comments_and_spaces(line);
    if (p[0] == '\0') return 0;
    size_t len = strlen(dir);
    if (strncmp(p, dir, len) != 0) return 0;
    if (!isspace((unsigned char)p[len]) && p[len] != '\0') return 0;
    return 1;
}

/* Skip a leading "label:" prefix and return the first non-space character of
   the instruction that follows.  Returns the original pointer when the line
   has no label prefix. */
static const char *strip_label(const char *line) {
    const char *p = line;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.')) p++;
    if (*p != ':') return line;
    const char *ins = p + 1;
    while (*ins && isspace((unsigned char)*ins)) ins++;
    return ins;
}
static const char *skip_line_indent(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    return line;
}
/* Extract the target label and optional signed delta from a table entry
   expression such as ".titlesonic-Ani_TSon" or ".psb+1". */
static void parse_table_expr(const char *s, char *label_out, size_t label_sz, int *delta_out) {
    *delta_out = 0;
    if (label_sz == 0) return;
    size_t n = 0;
    while (*s && isspace((unsigned char)*s)) s++;
    while (*s && (isalnum((unsigned char)*s) || *s == '_' || *s == '.') && n + 1 < label_sz) {
        label_out[n++] = *s++;
    }
    label_out[n] = '\0';
    if (*s == '+' || *s == '-') {
        char *e2 = NULL;
        long d = strtol(s, &e2, 10);
        if (e2 != s) *delta_out = (int)d;
    }
}

typedef struct {
    char name[64];
    uint8_t *bytes;
    size_t len;
} AnimSeg;

static void buf_grow(char **buf, size_t *len, size_t *cap, const void *src, size_t n) {
    if (n == 0) return;
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap : 256;
        while (*len + n + 1 > nc) nc *= 2;
        char *nb = (char *)realloc(*buf, nc);
        if (!nb) return;
        *buf = nb;
        *cap = nc;
    }
    memcpy(*buf + *len, src, n);
    *len += n;
}

typedef struct {
    char name[64];
    int nparams;
    char params[8][32];
    char *body;
    size_t body_len;
} AsmMacroDef;

typedef struct {
    char name[64];
    long value;
} EquSymbol;

/* Recorre el texto buscando definiciones "nombre: equ valor" y sustituye
 *  cada uso de esos símbolos por su valor numérico. Imprescindible para que
 *  los scripts de animación (fr_Stand, fr_Walk13, ...) se parseen bien.
 *  Devuelve NULL si no hay definiciones equ (el llamador usa el texto original). */
static char *resolve_equ_symbols(const char *text) {
    EquSymbol syms[256];
    int sym_count = 0;

    /* Primera pasada: recolectar definiciones */
    const char *p = text;
    while (*p) {
        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *q = lp;
        while (*q && *q != '\n' && isspace((unsigned char)*q)) q++;
        if (*q == ';' || *q == '\0') continue;

        const char *name_start = q;
        while (*q && (isalnum((unsigned char)*q) || *q == '_')) q++;
        if (*q != ':') continue;
        size_t name_len = (size_t)(q - name_start);
        if (name_len == 0 || name_len >= 64) continue;
        q++;

        while (*q && *q != '\n' && isspace((unsigned char)*q)) q++;
        if (strncmp(q, "equ", 3) != 0) continue;
        if (q[3] && !isspace((unsigned char)q[3])) continue;
        q += 3;
        while (*q && *q != '\n' && isspace((unsigned char)*q)) q++;

        const char *e = NULL;
        long val = parse_asm_number(q, &e);
        if (e == q) continue;

        if (sym_count < 256) {
            memcpy(syms[sym_count].name, name_start, name_len);
            syms[sym_count].name[name_len] = '\0';
            syms[sym_count].value = val;
            sym_count++;
        }
    }

    if (sym_count == 0) return NULL;

    /* Segunda pasada: sustituir apariciones */
    char *out = NULL;
    size_t out_len = 0, out_cap = 0;
    p = text;
    while (*p) {
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *sym_start = p;
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
            size_t sym_len = (size_t)(p - sym_start);

            long found = 0;
            int ok = 0;
            for (int i = 0; i < sym_count; i++) {
                if (strlen(syms[i].name) == sym_len &&
                    strncmp(syms[i].name, sym_start, sym_len) == 0) {
                    found = syms[i].value;
                ok = 1;
                break;
                    }
            }

            if (ok) {
                char numbuf[32];
                int n = snprintf(numbuf, sizeof(numbuf), "%ld", found);
                buf_grow(&out, &out_len, &out_cap, numbuf, (size_t)n);
            } else {
                buf_grow(&out, &out_len, &out_cap, sym_start, sym_len);
            }
        } else {
            buf_grow(&out, &out_len, &out_cap, p, 1);
            p++;
        }
    }

    if (out) out[out_len] = '\0';
    return out;
}

/* Expand resource macros (e.g. the "sonani" animation macro that emits
   "dc.w anim-Ani_Sonic" table entries) into literal text before parsing.
   Returns a malloc'd buffer the caller must free, or NULL when the text
   contains no macros (or expansion produced nothing). */
static char *expand_asm_macros(const char *text) {
    AsmMacroDef macros[32] = {0};
    int macro_count = 0;

    /* First pass: record macro definitions ("<name>: macro <params>" ... "endm"). */
    const char *p = text;
    while (*p) {
        const char *line = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(line);
        if (ins == line) ins = skip_line_indent(ins);
        if (ins == line || !is_directive(ins, "macro")) continue;
        if (macro_count >= 32) break;

        AsmMacroDef *m = &macros[macro_count];
        const char *q = line;
        size_t nn = 0;
        while (*q && (isalnum((unsigned char)*q) || *q == '_') && nn + 1 < sizeof(m->name))
            m->name[nn++] = *q++;
        m->name[nn] = '\0';
        if (!m->name[0]) continue;

        const char *ap = ins + 5;
        m->nparams = 0;
        while (*ap && *ap != '\n') {
            while (*ap && (isspace((unsigned char)*ap) || *ap == ',')) ap++;
            if (*ap == '\0' || *ap == '\n') break;
            size_t al = 0;
            while (*ap && !isspace((unsigned char)*ap) && *ap != ',' && *ap != '\n' && al + 1 < 32)
                m->params[m->nparams][al++] = *ap++;
            m->params[m->nparams][al] = '\0';
            m->nparams++;
            if (m->nparams >= 8) break;
        }

        size_t blen = 0, bcap = 0;
        char *body = NULL;
        while (*p) {
            const char *bl = p;
            while (*p && *p != '\n') p++;
            int bnl = (*p == '\n');
            if (bnl) p++;
            const char *bins = strip_label(bl);
            if (is_directive(bins, "endm")) break;
            buf_grow(&body, &blen, &bcap, bl, (size_t)(p - bl));
        }
        m->body = body;
        m->body_len = blen;
        macro_count++;
    }

    if (macro_count == 0) {
        for (int i = 0; i < macro_count; i++) free(macros[i].body);
        return NULL;
    }

    char *out = NULL;
    size_t out_len = 0, out_cap = 0;
    p = text;
    while (*p) {
        const char *line = p;
        while (*p && *p != '\n') p++;
        size_t ll = (size_t)(p - line);
        int has_nl = (*p == '\n');
        if (has_nl) p++;

        const char *ins = strip_label(line);
        if (ins == line) ins = skip_line_indent(ins);
        int m_idx = -1;

        /* Si esta línea es una DEFINICIÓN de macro (label: macro ...),
           saltarla junto con todo su cuerpo (hasta 'endm' inclusive).
           Si no, la definición literal se colaría en el output y el parser
           la procesaría como si fuera código real. */
        if (ins != line && is_directive(ins, "macro")) {
            while (*p) {
                const char *bl = p;
                while (*p && *p != '\n') p++;
                int bnl = (*p == '\n');
                if (bnl) p++;
                const char *bins = strip_label(bl);
                if (bins == bl) bins = skip_line_indent(bins);
                if (is_directive(bins, "endm")) break;
            }
            continue;
        }

        if (ins != line) {
            for (int i = 0; i < macro_count; i++) {
                if (is_directive(ins, macros[i].name)) { m_idx = i; break; }
            }
        }

        if (m_idx < 0) {
            buf_grow(&out, &out_len, &out_cap, line, ll);
            if (has_nl) buf_grow(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        AsmMacroDef *m = &macros[m_idx];
        const char *args[8] = {0};
        int n = 0;
        const char *ap = ins + strlen(m->name);
        while (*ap && *ap != '\n' && n < 8) {
            while (*ap && (isspace((unsigned char)*ap) || *ap == ',')) ap++;
            if (*ap == '\0' || *ap == '\n' || *ap == ';') break;
            args[n++] = ap;
            while (*ap && !isspace((unsigned char)*ap) && *ap != ',' && *ap != '\n' && *ap != ';')
                ap++;
        }

        char call_label[64];
        size_t cn = 0;
        const char *cl = line;
        while (*cl && (isalnum((unsigned char)*cl) || *cl == '_' || *cl == '.') && cn + 1 < 64)
            call_label[cn++] = *cl++;
        call_label[cn] = '\0';

        const char *bp = m->body;
        const char *bend = m->body + m->body_len;
        while (bp < bend) {
            const char *nlp = memchr(bp, '\n', (size_t)(bend - bp));
            const char *rl = nlp ? nlp : bend;
            const char *r = bp;
            while (r < rl) {
                if (isalnum((unsigned char)*r) || *r == '_' || *r == '.') {
                    const char *t = r;
                    while (r < rl && (isalnum((unsigned char)*r) || *r == '_' || *r == '.')) r++;
                    size_t tl = (size_t)(r - t);
                    char tok[64];
                    size_t ctl = tl < 63 ? tl : 63;
                    memcpy(tok, t, ctl);
                    tok[ctl] = '\0';
                    const char *rep = NULL;
                    size_t rpl = 0;
                    if (call_label[0] && strcmp(tok, "__LABEL__") == 0) {
                        rep = call_label;
                        rpl = strlen(call_label);
                    } else {
                        for (int ai = 0; ai < m->nparams && ai < 8; ai++) {
                            if (ai < n && args[ai] && strcmp(m->params[ai], tok) == 0) {
                                const char *ae = args[ai];
                                while (*ae && !isspace((unsigned char)*ae) && *ae != ',' && *ae != '\n' && *ae != ';')
                                    ae++;
                                rep = args[ai];
                                rpl = (size_t)(ae - args[ai]);
                                break;
                            }
                        }
                    }
                    if (rep) buf_grow(&out, &out_len, &out_cap, rep, rpl);
                    else buf_grow(&out, &out_len, &out_cap, tok, tl);
                } else {
                    buf_grow(&out, &out_len, &out_cap, r, 1);
                    r++;
                }
            }
            if (nlp) {
                buf_grow(&out, &out_len, &out_cap, "\n", 1);
                bp = nlp + 1;
            } else {
                bp = bend;
            }
        }
    }

    for (int i = 0; i < macro_count; i++) free(macros[i].body);
    if (out) out[out_len] = '\0';
    return out;
}

static uint8_t *parse_anim_asm(const char *text, size_t text_len, size_t *out_len) {
    (void)text_len;
    char *expanded = expand_asm_macros(text);
    const char *src = expanded ? expanded : text;
    char *resolved = resolve_equ_symbols(src);
    const char *p = resolved ? resolved : src;

    AnimSeg segs[128] = {0};
    int seg_count = 0;
    char table_name[128][64];
    int table_delta[128];
    int table_count = 0;

    while (*p) {
        p = skip_comments_and_spaces(p);
        if (*p == '\0') break;

        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(lp);
        if (ins == lp) ins = skip_line_indent(ins);
        if (*ins == '\0') continue;

        /* --- DEBUG TEMPORAL --- */
        static int dbg_line = 0;
        if (dbg_line < 40) {
            fprintf(stdout, "L%02d: ins='%.55s' lp='%.55s' dcw=%d dcb=%d\n",
                    dbg_line, ins, lp,
                    is_directive(ins, "dc.w") || is_directive(ins, "mappingsTableEntry.w"),
                    is_directive(ins, "dc.b"));
            dbg_line++;
        }
        /* --- FIN DEBUG --- */

        if (is_directive(ins, "dc.w") || is_directive(ins, "mappingsTableEntry.w")) {
            const char *dp = ins;
            if (is_directive(ins, "dc.w")) dp += 4;
            else dp += strlen("mappingsTableEntry.w");
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (table_count < 128) {
                parse_table_expr(dp, table_name[table_count], 64, &table_delta[table_count]);
                table_count++;
            }
            continue;
        }

        if (is_directive(ins, "dc.b")) {
            const int has_label = (strip_label(lp) != lp);
            if (has_label) {
                if (seg_count < 128) {
                    segs[seg_count].bytes = NULL;
                    segs[seg_count].len = 0;
                    const char *q = lp;
                    size_t nn = 0;
                    while (*q && (isalnum((unsigned char)*q) || *q == '_' || *q == '.') && nn + 1 < 64)
                        segs[seg_count].name[nn++] = *q++;
                    segs[seg_count].name[nn] = '\0';
                    seg_count++;
                }
            } else if (seg_count == 0) {
                if (seg_count < 128) {
                    segs[seg_count].name[0] = '\0';
                    segs[seg_count].bytes = NULL;
                    segs[seg_count].len = 0;
                    seg_count++;
                }
            }
            if (seg_count == 0) continue;

            AnimSeg *sg = &segs[seg_count - 1];
            const char *dp = ins + 4;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (*dp == '<') dp++;
            while (*dp) {
                dp = skip_comments_and_spaces(dp);
                if (*dp == '\0' || *dp == ';') break;
                const char *val_end = NULL;
                long val = parse_asm_number(dp, &val_end);
                if (val_end == dp) break;
                long b = val;
                if (b < 0) b = 0;
                uint8_t *nb = (uint8_t *)realloc(sg->bytes, sg->len + 1);
                if (!nb) break;
                sg->bytes = nb;
                sg->bytes[sg->len++] = (uint8_t)(b & 0xFF);
                dp = val_end;
                while (*dp && (isspace((unsigned char)*dp) || *dp == ',')) dp++;
            }
        }
    }

    /* Layout: table of word offsets first, then each referenced frame block. */
    size_t total = 2 * (size_t)table_count;
    size_t seg_pos[128];
    int ref_idx[128];
    size_t cursor = total;

    for (int j = 0; j < 128; j++) seg_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < 128; j++) {
            if (strcmp(segs[j].name, table_name[k]) == 0) {
                idx = j;
                break;
            }
        }
        if (idx < 0 && k < seg_count) idx = k;
        ref_idx[k] = idx;
        if (idx < 0) continue;
        if (seg_pos[idx] == (size_t)-1) {
            seg_pos[idx] = cursor;
            cursor += segs[idx].len;
        }
    }
    for (int j = 0; j < 128; j++) {
        if (seg_pos[j] == (size_t)-1) {
            seg_pos[j] = cursor;
            cursor += segs[j].len;
        }
    }

    fprintf(stdout, "=== TABLE: count=%d  SEGS: count=%d  cursor=%zu ===\n",
            table_count, seg_count, cursor);
    for (int j = 0; j < table_count; j++) {
        fprintf(stdout, "  tbl[%02d]='%s' delta=%d\n",
                j, table_name[j], table_delta[j]);
    }
    for (int j = 0; j < seg_count; j++) {
        fprintf(stdout, "  seg[%02d]='%s' len=%zu\n",
                j, segs[j].name, segs[j].len);
    }

    *out_len = cursor;
    if (cursor == 0) { free(resolved); free(expanded); return NULL; }

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) { *out_len = 0; free(resolved); free(expanded); return NULL; }

    for (int k = 0; k < table_count; k++) {
        size_t off = (ref_idx[k] >= 0) ? seg_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < 128; j++)
        if (segs[j].bytes && segs[j].len)
            memcpy(out + seg_pos[j], segs[j].bytes, segs[j].len);

    for (int j = 0; j < 128; j++) free(segs[j].bytes);
    free(resolved);
    free(expanded);
    return out;
}

typedef struct {
    char name[64];
    const uint8_t *pieces;
    size_t count;
} MapFrame;

static uint8_t *parse_map_asm(const char *text, size_t text_len, size_t *out_len) {
    (void)text_len;
    const char *p = text;
    MapFrame frames[256] = {0};
    int frame_count = 0;
    char table_name[256][64];
    int table_delta[256];
    int table_count = 0;

    while (*p) {
        p = skip_comments_and_spaces(p);
        if (*p == '\0') break;

        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(lp);
        if (ins == lp) ins = skip_line_indent(ins);
        if (*ins == '\0') continue;

        if (is_directive(ins, "mappingsTableEntry.w")) {
            const char *dp = ins + 20;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (table_count < 256) {
                parse_table_expr(dp, table_name[table_count], 64, &table_delta[table_count]);
                table_count++;
            }
            continue;
        }

        if (is_directive(ins, "spriteHeader")) {
            if (frame_count < 256) {
                frames[frame_count].name[0] = '\0';
                if (strip_label(lp) != lp) {
                    const char *q = lp;
                    size_t nn = 0;
                    while (*q && (isalnum((unsigned char)*q) || *q == '_' || *q == '.') && nn + 1 < 64)
                        frames[frame_count].name[nn++] = *q++;
                    frames[frame_count].name[nn] = '\0';
                }
                frames[frame_count].pieces = NULL;
                frames[frame_count].count = 0;
                frame_count++;
            }
            continue;
        }

        if (is_directive(ins, "spritePiece")) {
            if (frame_count == 0) continue;
            MapFrame *fr = &frames[frame_count - 1];
            const char *dp = ins + 11;
            const char *args[9];
            int arg_idx = 0;
            while (*dp && arg_idx < 9) {
                dp = skip_comments_and_spaces(dp);
                if (*dp == '\0' || *dp == ';') break;
                const char *val_end2 = NULL;
                parse_asm_number(dp, &val_end2);
                if (val_end2 == dp) break;
                args[arg_idx++] = dp;
                if (arg_idx >= 9) break;
                dp = val_end2;
                while (*dp && isspace((unsigned char)*dp)) dp++;
                if (*dp == ',') dp++;
            }
            if (arg_idx < 5) continue;

            long x = parse_asm_number(args[0], NULL);
            long y = parse_asm_number(args[1], NULL);
            long w = parse_asm_number(args[2], NULL);
            long h = parse_asm_number(args[3], NULL);
            long tile = parse_asm_number(args[4], NULL);
            long xflip = (arg_idx > 5) ? parse_asm_number(args[5], NULL) : 0;
            long yflip = (arg_idx > 6) ? parse_asm_number(args[6], NULL) : 0;
            long pal   = (arg_idx > 7) ? parse_asm_number(args[7], NULL) : 0;
            long pri   = (arg_idx > 8) ? parse_asm_number(args[8], NULL) : 0;

            /* Sonic 1 sprite piece layout (spritePiece macro, ver 1):
               byte0 = ypos, byte1 = ((w-1)&3)<<2 | (h-1)&3,
               byte2 = (pri<<7)|(pal<<5)|(yflip<<4)|(xflip<<3)|tile>>8,
               byte3 = tile&$FF, byte4 = xpos */
            if (w < 1) w = 1;
            if (h < 1) h = 1;
            if (w > 4) w = 4;
            if (h > 4) h = 4;

            uint8_t piece[5];
            piece[0] = (uint8_t)(y & 0xFF);
            piece[1] = (uint8_t)((((w - 1) & 3) << 2) | ((h - 1) & 3));
            piece[2] = (uint8_t)(((pri & 1) << 7) | ((pal & 3) << 5) |
                                 ((yflip & 1) << 4) | ((xflip & 1) << 3) |
                                 ((tile >> 8) & 7));
            piece[3] = (uint8_t)(tile & 0xFF);
            piece[4] = (uint8_t)(x & 0xFF);

            const uint8_t *np = (const uint8_t *)realloc(fr->pieces, (fr->count + 1) * 5);
            if (!np) continue;
            fr->pieces = np;
            memcpy(fr->pieces + fr->count * 5, piece, 5);
            fr->count++;
        }
    }

    /* Layout: table of word offsets first, then per-frame [count][pieces]. */
    size_t total = 2 * (size_t)table_count;
    size_t frame_pos[256];
    int ref_idx[256];
    size_t cursor = total;

    for (int j = 0; j < frame_count; j++) frame_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < frame_count; j++) {
            if (strcmp(frames[j].name, table_name[k]) == 0) {
                idx = j;
                break;
            }
        }
        if (idx < 0 && k < frame_count) idx = k;
        ref_idx[k] = idx;
        if (idx < 0) continue;
        if (frame_pos[idx] == (size_t)-1) {
            frame_pos[idx] = cursor;
            cursor += 1 + frames[idx].count * 5;
        }
    }
    for (int j = 0; j < frame_count; j++) {
        if (frame_pos[j] == (size_t)-1) {
            frame_pos[j] = cursor;
            cursor += 1 + frames[j].count * 5;
        }
    }

    *out_len = cursor;
    if (cursor == 0) return NULL;

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) { *out_len = 0; return NULL; }

    for (int k = 0; k < table_count; k++) {
        size_t off = (ref_idx[k] >= 0) ? frame_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < frame_count; j++) {
        MapFrame *fr = &frames[j];
        out[frame_pos[j]] = (uint8_t)fr->count;
        if (fr->pieces)
            memcpy(out + frame_pos[j] + 1, fr->pieces, fr->count * 5);
    }

    for (int j = 0; j < frame_count; j++) free(frames[j].pieces);
    return out;
}

/* Parse only the mapping table named `tblname` from an ASM mapping file that
   defines several mappingsTable blocks (e.g. "_maps/Title Cards.asm" holds
   Map_Card, Map_Got and Map_SSR).  Returns a standalone buffer whose word
   offsets are relative to its own start, so the runtime frame IDs match the
   order of the named table's mappingsTableEntry.w lines.  Cross-referenced
   frames owned by other tables (e.g. Map_Got reusing M_Card_Oval) are
   duplicated into this buffer. */
static uint8_t *parse_map_asm_named(const char *text, const char *tblname,
                                    size_t *out_len) {
    MapFrame frames[256] = {0};
    int frame_count = 0;
    char ent_owner[256][64];
    char ent_ref[256][64];
    int ent_delta[256];
    int ent_count = 0;
    char cur_table[64] = "";

    const char *p = text;
    while (*p) {
        p = skip_comments_and_spaces(p);
        if (*p == '\0') break;

        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(lp);
        int has_label = (ins != lp);
        if (ins == lp) ins = skip_line_indent(ins);
        if (*ins == '\0') continue;

        char lab[64] = "";
        if (has_label) {
            const char *q = lp;
            size_t nn = 0;
            while (*q && (isalnum((unsigned char)*q) || *q == '_' || *q == '.') && nn + 1 < 64)
                lab[nn++] = *q++;
            lab[nn] = '\0';
        }

        if (is_directive(ins, "mappingsTableEntry.w")) {
            if (ent_count < 256) {
                strncpy(ent_owner[ent_count], cur_table, 63);
                ent_owner[ent_count][63] = '\0';
                parse_table_expr(ins + 20, ent_ref[ent_count], 64,
                                 &ent_delta[ent_count]);
                ent_count++;
            }
            continue;
        }
        if (is_directive(ins, "mappingsTable")) {
            if (has_label && lab[0]) {
                strncpy(cur_table, lab, 63);
                cur_table[63] = '\0';
            }
            continue;
        }
        if (is_directive(ins, "spriteHeader")) {
            if (frame_count < 256) {
                frames[frame_count].name[0] = '\0';
                if (has_label) {
                    const char *q = lp;
                    size_t nn = 0;
                    while (*q && (isalnum((unsigned char)*q) || *q == '_' || *q == '.') && nn + 1 < 64)
                        frames[frame_count].name[nn++] = *q++;
                    frames[frame_count].name[nn] = '\0';
                }
                frames[frame_count].pieces = NULL;
                frames[frame_count].count = 0;
                frame_count++;
            }
            continue;
        }
        if (is_directive(ins, "spritePiece")) {
            if (frame_count == 0) continue;
            MapFrame *fr = &frames[frame_count - 1];
            const char *dp = ins + 11;
            const char *args[9];
            int arg_idx = 0;
            while (*dp && arg_idx < 9) {
                dp = skip_comments_and_spaces(dp);
                if (*dp == '\0' || *dp == ';') break;
                const char *val_end2 = NULL;
                parse_asm_number(dp, &val_end2);
                if (val_end2 == dp) break;
                args[arg_idx++] = dp;
                if (arg_idx >= 9) break;
                dp = val_end2;
                while (*dp && isspace((unsigned char)*dp)) dp++;
                if (*dp == ',') dp++;
            }
            if (arg_idx < 5) continue;

            long x = parse_asm_number(args[0], NULL);
            long y = parse_asm_number(args[1], NULL);
            long w = parse_asm_number(args[2], NULL);
            long h = parse_asm_number(args[3], NULL);
            long tile = parse_asm_number(args[4], NULL);
            long xflip = (arg_idx > 5) ? parse_asm_number(args[5], NULL) : 0;
            long yflip = (arg_idx > 6) ? parse_asm_number(args[6], NULL) : 0;
            long pal   = (arg_idx > 7) ? parse_asm_number(args[7], NULL) : 0;
            long pri   = (arg_idx > 8) ? parse_asm_number(args[8], NULL) : 0;

            if (w < 1) w = 1;
            if (h < 1) h = 1;
            if (w > 4) w = 4;
            if (h > 4) h = 4;

            uint8_t piece[5];
            piece[0] = (uint8_t)(y & 0xFF);
            piece[1] = (uint8_t)((((w - 1) & 3) << 2) | ((h - 1) & 3));
            piece[2] = (uint8_t)(((pri & 1) << 7) | ((pal & 3) << 5) |
                                 ((yflip & 1) << 4) | ((xflip & 1) << 3) |
                                 ((tile >> 8) & 7));
            piece[3] = (uint8_t)(tile & 0xFF);
            piece[4] = (uint8_t)(x & 0xFF);

            const uint8_t *np = (const uint8_t *)realloc(fr->pieces, (fr->count + 1) * 5);
            if (!np) continue;
            fr->pieces = np;
            memcpy(fr->pieces + fr->count * 5, piece, 5);
            fr->count++;
        }
    }

    /* Keep only the entries owned by the requested table */
    int sel[256];
    int feat_count = 0;
    for (int k = 0; k < ent_count; k++) {
        if (strncmp(ent_owner[k], tblname, 64) == 0 && feat_count < 256) {
            sel[feat_count++] = k;
        }
    }
    if (feat_count == 0) {
        for (int j = 0; j < frame_count; j++) free(frames[j].pieces);
        return NULL;
    }

    /* Layout: table of word offsets first, then each referenced frame as
       [count][pieces] (deduplicated by frame name). */
    size_t total = 2 * (size_t)feat_count;
    size_t frame_pos[256];
    int ref_idx[256];
    size_t cursor = total;

    for (int j = 0; j < frame_count; j++) frame_pos[j] = (size_t)-1;
    for (int k = 0; k < feat_count; k++) {
        int idx = -1;
        for (int j = 0; j < frame_count; j++) {
            if (strcmp(frames[j].name, ent_ref[sel[k]]) == 0) {
                idx = j;
                break;
            }
        }
        if (idx < 0 && k < frame_count) idx = k;
        ref_idx[k] = idx;
        if (idx >= 0 && frame_pos[idx] == (size_t)-1) {
            frame_pos[idx] = cursor;
            cursor += 1 + frames[idx].count * 5;
        }
    }
    for (int j = 0; j < frame_count; j++) {
        if (frame_pos[j] == (size_t)-1) {
            frame_pos[j] = cursor;
            cursor += 1 + frames[j].count * 5;
        }
    }

    *out_len = cursor;
    if (cursor == 0) {
        for (int j = 0; j < frame_count; j++) free(frames[j].pieces);
        return NULL;
    }

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) {
        *out_len = 0;
        for (int j = 0; j < frame_count; j++) free(frames[j].pieces);
        return NULL;
    }

    for (int k = 0; k < feat_count; k++) {
        size_t off = (ref_idx[k] >= 0)
                         ? frame_pos[ref_idx[k]] + (size_t)ent_delta[sel[k]]
                         : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < frame_count; j++) {
        MapFrame *fr = &frames[j];
        out[frame_pos[j]] = (uint8_t)fr->count;
        if (fr->pieces)
            memcpy(out + frame_pos[j] + 1, fr->pieces, fr->count * 5);
    }

    for (int j = 0; j < frame_count; j++) free(frames[j].pieces);
    return out;
}

typedef struct {
    char name[64];
    uint8_t *bytes;
    size_t len;
} PlcSeg;

/* Parse a Sonic 1 "Dynamic Gfx Script" (DPLC) asset, e.g. "Sonic - Dynamic
   Gfx Script.asm".  Format per _maps/_MapMacros.asm with SonicDplcVer=1:
     mappingsTableEntry.w <label>   -> word-offset table entry
     dplcHeader                     -> dc.b (number of entries)
     dplcEntry <tiles>, <offset>    -> dc.w (((tiles-1)&$F)<<12)|(offset&$FFF)
   Output layout: the word-offset table (stored little-endian, matching how
   the runtime reads it as native uint16), then each script as
   [count byte][big-endian entry words]. */
static uint8_t *parse_plc_asm(const char *text, size_t text_len, size_t *out_len) {
    (void)text_len;
    PlcSeg segs[128] = {0};
    int seg_count = 0;
    int cur = -1;
    char table_name[128][64];
    int table_delta[128];
    int table_count = 0;

    const char *p = text;
    while (*p) {
        p = skip_comments_and_spaces(p);
        if (*p == '\0') break;

        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(lp);
        if (ins == lp) ins = skip_line_indent(ins);
        if (*ins == '\0') continue;

        if (is_directive(ins, "mappingsTableEntry.w")) {
            const char *dp = ins + 20;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (table_count < 128) {
                parse_table_expr(dp, table_name[table_count], 64, &table_delta[table_count]);
                table_count++;
            }
            continue;
        }

        if (is_directive(ins, "dplcHeader")) {
            if (seg_count < 128) {
                PlcSeg *sg = &segs[seg_count];
                if (strip_label(lp) != lp) {
                    const char *q = lp;
                    size_t nn = 0;
                    while (*q && (isalnum((unsigned char)*q) || *q == '_' || *q == '.') && nn + 1 < 64)
                        sg->name[nn++] = *q++;
                    sg->name[nn] = '\0';
                }
                seg_count++;
                cur = seg_count - 1;
            }
            continue;
        }

        if (is_directive(ins, "dplcEntry")) {
            if (cur < 0) continue;
            const char *dp = ins + 9;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            const char *e1 = NULL;
            long tiles = parse_asm_number(dp, &e1);
            dp = e1;
            while (*dp && (isspace((unsigned char)*dp) || *dp == ',')) dp++;
            long offset = parse_asm_number(dp, NULL);

            uint8_t *nb = (uint8_t *)realloc(segs[cur].bytes, segs[cur].len + 2);
            if (!nb) break;
            segs[cur].bytes = nb;
            unsigned entry = (unsigned)(((tiles - 1) & 0xF) << 12) | (unsigned)(offset & 0xFFF);
            segs[cur].bytes[segs[cur].len]     = (uint8_t)(entry >> 8);
            segs[cur].bytes[segs[cur].len + 1] = (uint8_t)(entry & 0xFF);
            segs[cur].len += 2;
            continue;
        }

        /* mappingsTable, label-only lines, even, ... are irrelevant here */
    }

    size_t total = 2 * (size_t)table_count;
    size_t seg_pos[128];
    int ref_idx[128];
    size_t cursor = total;

    for (int j = 0; j < 128; j++) seg_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < seg_count; j++) {
            if (strcmp(segs[j].name, table_name[k]) == 0) {
                idx = j;
                break;
            }
        }
        if (idx < 0 && k < seg_count) idx = k;
        ref_idx[k] = idx;
        if (idx < 0) continue;
        if (seg_pos[idx] == (size_t)-1) {
            seg_pos[idx] = cursor;
            cursor += 1 + segs[idx].len;
        }
    }
    for (int j = 0; j < 128; j++) {
        if (seg_pos[j] == (size_t)-1) {
            seg_pos[j] = cursor;
            cursor += 1 + segs[j].len;
        }
    }

    *out_len = cursor;
    if (cursor == 0) return NULL;

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) { *out_len = 0; return NULL; }

    for (int k = 0; k < table_count; k++) {
        size_t off = (ref_idx[k] >= 0) ? seg_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < 128; j++) {
        if (seg_count == 0) break;
        if (segs[j].len == 0) continue;
        out[seg_pos[j]] = (uint8_t)(segs[j].len / 2);
        memcpy(out + seg_pos[j] + 1, segs[j].bytes, segs[j].len);
    }

    for (int j = 0; j < seg_count; j++) free(segs[j].bytes);
    return out;
}

typedef struct {
    const uint8_t *ptr;
    size_t len;
} MapEntry;

#define MAP_REGISTRY_MAX 128
static MapEntry g_map_registry[MAP_REGISTRY_MAX];
static int g_map_registry_count = 0;

static void register_map(const uint8_t *ptr, size_t len) {
    if (g_map_registry_count < MAP_REGISTRY_MAX) {
        g_map_registry[g_map_registry_count].ptr = ptr;
        g_map_registry[g_map_registry_count].len = len;
        g_map_registry_count++;
    }
}

size_t Map_LookupLength(const uint8_t *ptr) {
    for (int i = 0; i < g_map_registry_count; i++) {
        if (g_map_registry[i].ptr == ptr) return g_map_registry[i].len;
    }
    return 0;
}

static int load_asm_asset(const char *name, const uint8_t **out_ptr, size_t *out_len, int is_map) {
    char path[PATH_MAX + 512];
    snprintf(path, sizeof(path), "%s/%s", assets_base_path(), name);
    size_t text_len = 0;
    char *text = (char *)Assets_Load(path, &text_len);
    if (!text) {
        fprintf(stderr, "[Data] Failed to load ASM asset: %s\n", path);
        return -1;
    }

    const uint8_t *data = NULL;
    size_t data_len = 0;
    if (is_map) {
        data = parse_map_asm(text, text_len, &data_len);
    } else if (strstr(text, "dplcEntry")) {
        data = parse_plc_asm(text, text_len, &data_len);
    } else {
        data = parse_anim_asm(text, text_len, &data_len);
    }

    free(text);

    if (!data || data_len == 0) {
        fprintf(stderr, "[Data] Failed to parse ASM asset: %s\n", path);
        return -1;
    }

    /* Move the parsed buffer into 32-bit addressable space (needed by obMap). */
    const uint8_t *low = alloc_32bit(data_len);
    if (!low) {
        fprintf(stderr, "[Data] mmap MAP_32BIT failed for: %s\n", path);
        free(data);
        return -1;
    }
    memcpy(low, data, data_len);
    free((void *)data);
    register_map(low, data_len);
    *out_ptr = low;
    *out_len = data_len;
    return 0;
}

/* Like load_asm_asset(), but extracts only the mapping table named `tblname`
   (e.g. "Map_Got") from a file that defines several of them. */
static int load_asm_asset_named(const char *name, const char *tblname,
                                const uint8_t **out_ptr, size_t *out_len) {
    char path[PATH_MAX + 512];
    snprintf(path, sizeof(path), "%s/%s", assets_base_path(), name);
    size_t text_len = 0;
    char *text = (char *)Assets_Load(path, &text_len);
    if (!text) {
        fprintf(stderr, "[Data] Failed to load ASM asset: %s\n", path);
        return -1;
    }

    const uint8_t *data = parse_map_asm_named(text, tblname, &text_len);
    free(text);

    if (!data || text_len == 0) {
        fprintf(stderr, "[Data] Failed to parse ASM asset (table '%s'): %s\n",
                tblname, path);
        return -1;
    }

    size_t data_len = text_len;
    const uint8_t *low = alloc_32bit(data_len);
    if (!low) {
        fprintf(stderr, "[Data] mmap MAP_32BIT failed for: %s\n", path);
        free((void *)data);
        return -1;
    }
    memcpy(low, data, data_len);
    free((void *)data);
    register_map(low, data_len);
    *out_ptr = low;
    *out_len = data_len;
    return 0;
}

