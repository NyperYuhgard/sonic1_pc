#ifndef SONIC1_DATA_H
#define SONIC1_DATA_H

#include "types.h"

/* Sega Screen assets (loaded at runtime from assets/) */
extern const uint8_t *Pal_SegaBG;
extern size_t   Pal_SegaBG_len;

extern const uint8_t *Pal_Sega1;
extern size_t   Pal_Sega1_len;

extern const uint8_t *Pal_Sega2;
extern size_t   Pal_Sega2_len;

extern const uint8_t *Nem_SegaLogo;
extern size_t   Nem_SegaLogo_len;

extern const uint8_t *Eni_SegaLogo;
extern size_t   Eni_SegaLogo_len;

/* Palette index IDs (from Palette Index.asm) */
#define palid_SegaBG    0
#define palid_Title     1
#define palid_LevelSel  2
#define palid_Sonic     3
#define palid_GHZ       4
#define palid_ZoneStart palid_GHZ   /* first level palette entry (Pal_Levels) */
#define palid_LZ        5
#define palid_MZ        6
#define palid_SLZ       7
#define palid_SYZ       8
#define palid_SBZ1      9
#define palid_Special   10
#define palid_LZWater   11
#define palid_SBZ3      12
#define palid_SBZ3Water 13
#define palid_SBZ2      14
#define palid_LZSonWater 15
#define palid_SBZ3SonWat 16
#define palid_SSResult  17
#define palid_Continue  18
#define palid_Ending    19

/* Title Screen assets */
extern const uint8_t *Pal_Title;
extern size_t   Pal_Title_len;

/* Title screen water palette cycle data (assets/palette/cycle_water.bin) */
extern const uint8_t *Pal_TitleCycWater;
extern size_t   Pal_TitleCycWater_len;

/* GHZ (and Ending) waterfall palette cycle data (assets/palette/cycle_ghz.bin) */
extern const uint8_t *Pal_GHZCycWater;
extern size_t   Pal_GHZCycWater_len;

extern const uint8_t *Pal_LevelSel;
extern size_t   Pal_LevelSel_len;

extern const uint8_t *Pal_Sonic;
extern size_t   Pal_Sonic_len;

extern const uint8_t *Pal_GHZ;
extern size_t   Pal_GHZ_len;

extern const uint8_t *Pal_LZ;
extern size_t   Pal_LZ_len;
extern const uint8_t *Pal_LZWater;
extern size_t   Pal_LZWater_len;
extern const uint8_t *Pal_LZSonWater;
extern size_t   Pal_LZSonWater_len;
extern const uint8_t *Pal_MZ;
extern size_t   Pal_MZ_len;
extern const uint8_t *Pal_SLZ;
extern size_t   Pal_SLZ_len;
extern const uint8_t *Pal_SYZ;
extern size_t   Pal_SYZ_len;
extern const uint8_t *Pal_SBZ1;
extern size_t   Pal_SBZ1_len;
extern const uint8_t *Pal_SBZ2;
extern size_t   Pal_SBZ2_len;
extern const uint8_t *Pal_SBZ3;
extern size_t   Pal_SBZ3_len;
extern const uint8_t *Pal_SBZ3Water;
extern size_t   Pal_SBZ3Water_len;
extern const uint8_t *Pal_SBZ3SonWat;
extern size_t   Pal_SBZ3SonWat_len;
extern const uint8_t *Pal_Special;
extern size_t   Pal_Special_len;
extern const uint8_t *Pal_SSResult;
extern size_t   Pal_SSResult_len;
extern const uint8_t *Pal_Continue;
extern size_t   Pal_Continue_len;
extern const uint8_t *Pal_Ending;
extern size_t   Pal_Ending_len;

extern const uint8_t *Nem_JapNames;
extern size_t   Nem_JapNames_len;

extern const uint8_t *Eni_JapNames;
extern size_t   Eni_JapNames_len;

extern const uint8_t *Nem_CreditText;
extern size_t   Nem_CreditText_len;

extern const uint8_t *Nem_TitleFg;
extern size_t   Nem_TitleFg_len;

extern const uint8_t *Nem_TitleSonic;
extern size_t   Nem_TitleSonic_len;

extern const uint8_t *Nem_TitleTM;
extern size_t   Nem_TitleTM_len;

extern const uint8_t *Art_Text;
extern size_t   Art_Text_len;

extern const uint8_t *Blk16_GHZ;
extern size_t   Blk16_GHZ_len;

extern const uint8_t *Blk256_GHZ;
extern size_t   Blk256_GHZ_len;

extern const uint8_t *Blk16_LZ;
extern size_t   Blk16_LZ_len;

extern const uint8_t *Blk256_LZ;
extern size_t   Blk256_LZ_len;

extern const uint8_t *Blk16_MZ;
extern size_t   Blk16_MZ_len;

extern const uint8_t *Blk256_MZ;
extern size_t   Blk256_MZ_len;

extern const uint8_t *Blk16_SLZ;
extern size_t   Blk16_SLZ_len;

extern const uint8_t *Blk256_SLZ;
extern size_t   Blk256_SLZ_len;

extern const uint8_t *Blk16_SYZ;
extern size_t   Blk16_SYZ_len;

extern const uint8_t *Blk256_SYZ;
extern size_t   Blk256_SYZ_len;

extern const uint8_t *Blk16_SBZ;
extern size_t   Blk16_SBZ_len;

extern const uint8_t *Blk256_SBZ;
extern size_t   Blk256_SBZ_len;

extern const uint8_t *Eni_Title;
extern size_t   Eni_Title_len;

extern const uint8_t *Nem_GHZ_1st;
extern size_t   Nem_GHZ_1st_len;

extern const uint8_t *Level_GHZ1;
extern size_t   Level_GHZ1_len;

extern const uint8_t *Level_GHZbg;
extern size_t   Level_GHZbg_len;
extern const uint8_t *Level_GHZ2;
extern size_t   Level_GHZ2_len;
extern const uint8_t *Level_GHZ3;
extern size_t   Level_GHZ3_len;
extern const uint8_t *Level_LZ1;
extern size_t   Level_LZ1_len;
extern const uint8_t *Level_LZ2;
extern size_t   Level_LZ2_len;
extern const uint8_t *Level_LZ3;
extern size_t   Level_LZ3_len;
extern const uint8_t *Level_LZbg;
extern size_t   Level_LZbg_len;
extern const uint8_t *Level_SBZ3;
extern size_t   Level_SBZ3_len;
extern const uint8_t *Level_MZ1;
extern size_t   Level_MZ1_len;
extern const uint8_t *Level_MZ1bg;
extern size_t   Level_MZ1bg_len;
extern const uint8_t *Level_MZ2;
extern size_t   Level_MZ2_len;
extern const uint8_t *Level_MZ2bg;
extern size_t   Level_MZ2bg_len;
extern const uint8_t *Level_MZ3;
extern size_t   Level_MZ3_len;
extern const uint8_t *Level_MZ3bg;
extern size_t   Level_MZ3bg_len;
extern const uint8_t *Level_SLZ1;
extern size_t   Level_SLZ1_len;
extern const uint8_t *Level_SLZ2;
extern size_t   Level_SLZ2_len;
extern const uint8_t *Level_SLZ3;
extern size_t   Level_SLZ3_len;
extern const uint8_t *Level_SLZbg;
extern size_t   Level_SLZbg_len;
extern const uint8_t *Level_SYZ1;
extern size_t   Level_SYZ1_len;
extern const uint8_t *Level_SYZ2;
extern size_t   Level_SYZ2_len;
extern const uint8_t *Level_SYZ3;
extern size_t   Level_SYZ3_len;
extern const uint8_t *Level_SYZbg;
extern size_t   Level_SYZbg_len;
extern const uint8_t *Level_SBZ1;
extern size_t   Level_SBZ1_len;
extern const uint8_t *Level_SBZ1bg;
extern size_t   Level_SBZ1bg_len;
extern const uint8_t *Level_SBZ2;
extern size_t   Level_SBZ2_len;
extern const uint8_t *Level_SBZ2bg;
extern size_t   Level_SBZ2bg_len;
extern const uint8_t *Level_End;
extern size_t   Level_End_len;

/* Level PLC graphics (PLC_GHZ + PLC_Main2). Only Nem_GHZ_1st is staged so
   far; the rest are NULL and skipped by AddPLC until their assets land. */
extern const uint8_t *Nem_GHZ_2nd;
extern size_t   Nem_GHZ_2nd_len;
extern const uint8_t *Nem_Stalk;
extern size_t   Nem_Stalk_len;
extern const uint8_t *Nem_PplRock;
extern size_t   Nem_PplRock_len;
extern const uint8_t *Nem_Buzz;
extern size_t   Nem_Buzz_len;
extern const uint8_t *Nem_Chopper;
extern size_t   Nem_Chopper_len;
extern const uint8_t *Nem_Newtron;
extern size_t   Nem_Newtron_len;
extern const uint8_t *Nem_Spikes;
extern size_t   Nem_Spikes_len;
extern const uint8_t *Nem_HSpring;
extern size_t   Nem_HSpring_len;
extern const uint8_t *Nem_VSpring;
extern size_t   Nem_VSpring_len;
extern const uint8_t *Nem_Monitors;
extern size_t   Nem_Monitors_len;
extern const uint8_t *Nem_Shield;
extern size_t   Nem_Shield_len;
extern const uint8_t *Nem_Stars;
extern size_t   Nem_Stars_len;

extern const uint8_t *Nem_SignPost;
extern size_t   Nem_SignPost_len;
extern const uint8_t *Nem_Bonus;
extern size_t   Nem_Bonus_len;
extern const uint8_t *Nem_BigFlash;
extern size_t   Nem_BigFlash_len;

extern const uint8_t *Nem_GhzWall2;
extern size_t   Nem_GhzWall2_len;
extern const uint8_t *Nem_Swing;
extern size_t   Nem_Swing_len;
extern const uint8_t *Nem_Bridge;
extern size_t   Nem_Bridge_len;
extern const uint8_t *Nem_SpikePole;
extern size_t   Nem_SpikePole_len;
extern const uint8_t *Nem_Ball;
extern size_t   Nem_Ball_len;
extern const uint8_t *Nem_GhzWall1;
extern size_t   Nem_GhzWall1_len;

/* Animal art (artnem/Animal *.nem) — used by PLC_*Animals */
extern const uint8_t *Nem_Rabbit;
extern size_t   Nem_Rabbit_len;
extern const uint8_t *Nem_Chicken;
extern size_t   Nem_Chicken_len;
extern const uint8_t *Nem_Penguin;
extern size_t   Nem_Penguin_len;
extern const uint8_t *Nem_Seal;
extern size_t   Nem_Seal_len;
extern const uint8_t *Nem_Pig;
extern size_t   Nem_Pig_len;
extern const uint8_t *Nem_Flicky;
extern size_t   Nem_Flicky_len;
extern const uint8_t *Nem_Squirrel;
extern size_t   Nem_Squirrel_len;

/* Remaining PLC art — staged verbatim from disasm/artnem/ */
extern const uint8_t *Nem_BallHog;
extern size_t   Nem_BallHog_len;
extern const uint8_t *Nem_Basaran;
extern size_t   Nem_Basaran_len;
extern const uint8_t *Nem_Bomb;
extern size_t   Nem_Bomb_len;
extern const uint8_t *Nem_Bubbles;
extern size_t   Nem_Bubbles_len;
extern const uint8_t *Nem_Bumper;
extern size_t   Nem_Bumper_len;
extern const uint8_t *Nem_Burrobot;
extern size_t   Nem_Burrobot_len;
extern const uint8_t *Nem_Cater;
extern size_t   Nem_Cater_len;
extern const uint8_t *Nem_Cork;
extern size_t   Nem_Cork_len;
extern const uint8_t *Nem_Cutter;
extern size_t   Nem_Cutter_len;
extern const uint8_t *Nem_Eggman;
extern size_t   Nem_Eggman_len;
extern const uint8_t *Nem_Electric;
extern size_t   Nem_Electric_len;
extern const uint8_t *Nem_EndEm;
extern size_t   Nem_EndEm_len;
extern const uint8_t *Nem_EndFlower;
extern size_t   Nem_EndFlower_len;
extern const uint8_t *Nem_EndSonic;
extern size_t   Nem_EndSonic_len;
extern const uint8_t *Nem_EndStH;
extern size_t   Nem_EndStH_len;
extern const uint8_t *Nem_Exhaust;
extern size_t   Nem_Exhaust_len;
extern const uint8_t *Nem_Fan;
extern size_t   Nem_Fan_len;
extern const uint8_t *Nem_FlamePipe;
extern size_t   Nem_FlamePipe_len;
extern const uint8_t *Nem_FlapDoor;
extern size_t   Nem_FlapDoor_len;
extern const uint8_t *Nem_FzBoss;
extern size_t   Nem_FzBoss_len;
extern const uint8_t *Nem_FzEggman;
extern size_t   Nem_FzEggman_len;
extern const uint8_t *Nem_GameOver;
extern size_t   Nem_GameOver_len;
extern const uint8_t *Nem_Gargoyle;
extern size_t   Nem_Gargoyle_len;
extern const uint8_t *Nem_Girder;
extern size_t   Nem_Girder_len;
extern const uint8_t *Nem_Harpoon;
extern size_t   Nem_Harpoon_len;
extern const uint8_t *Nem_Jaws;
extern size_t   Nem_Jaws_len;
extern const uint8_t *Nem_LZ;
extern size_t   Nem_LZ_len;
extern const uint8_t *Nem_Lamp;
extern size_t   Nem_Lamp_len;
extern const uint8_t *Nem_Lava;
extern size_t   Nem_Lava_len;
extern const uint8_t *Nem_LzBlock1;
extern size_t   Nem_LzBlock1_len;
extern const uint8_t *Nem_LzBlock2;
extern size_t   Nem_LzBlock2_len;
extern const uint8_t *Nem_LzBlock3;
extern size_t   Nem_LzBlock3_len;
extern const uint8_t *Nem_LzDoor1;
extern size_t   Nem_LzDoor1_len;
extern const uint8_t *Nem_LzDoor2;
extern size_t   Nem_LzDoor2_len;
extern const uint8_t *Nem_LzPlatfm;
extern size_t   Nem_LzPlatfm_len;
extern const uint8_t *Nem_LzPole;
extern size_t   Nem_LzPole_len;
extern const uint8_t *Nem_LzSpikeBall;
extern size_t   Nem_LzSpikeBall_len;
extern const uint8_t *Nem_LzSwitch;
extern size_t   Nem_LzSwitch_len;
extern const uint8_t *Nem_LzWheel;
extern size_t   Nem_LzWheel_len;
extern const uint8_t *Nem_MZ;
extern size_t   Nem_MZ_len;
extern const uint8_t *Nem_MiniSonic;
extern size_t   Nem_MiniSonic_len;
extern const uint8_t *Nem_MzBlock;
extern size_t   Nem_MzBlock_len;
extern const uint8_t *Nem_MzFire;
extern size_t   Nem_MzFire_len;
extern const uint8_t *Nem_MzGlass;
extern size_t   Nem_MzGlass_len;
extern const uint8_t *Nem_MzMetal;
extern size_t   Nem_MzMetal_len;
extern const uint8_t *Nem_MzSwitch;
extern size_t   Nem_MzSwitch_len;
extern const uint8_t *Nem_Orbinaut;
extern size_t   Nem_Orbinaut_len;
extern const uint8_t *Nem_Points;
extern size_t   Nem_Points_len;
extern const uint8_t *Nem_Prison;
extern size_t   Nem_Prison_len;
extern const uint8_t *Nem_Pylon;
extern size_t   Nem_Pylon_len;
extern const uint8_t *Nem_ResultEm;
extern size_t   Nem_ResultEm_len;
extern const uint8_t *Nem_Roller;
extern size_t   Nem_Roller_len;
extern const uint8_t *Nem_SBZ;
extern size_t   Nem_SBZ_len;
extern const uint8_t *Nem_SLZ;
extern size_t   Nem_SLZ_len;
extern const uint8_t *Nem_SS1UpBlock;
extern size_t   Nem_SS1UpBlock_len;
extern const uint8_t *Nem_SSBgCloud;
extern size_t   Nem_SSBgCloud_len;
extern const uint8_t *Nem_SSBgFish;
extern size_t   Nem_SSBgFish_len;
extern const uint8_t *Nem_SSEmStars;
extern size_t   Nem_SSEmStars_len;
extern const uint8_t *Nem_SSEmerald;
extern size_t   Nem_SSEmerald_len;
extern const uint8_t *Nem_SSGOAL;
extern size_t   Nem_SSGOAL_len;
extern const uint8_t *Nem_SSGhost;
extern size_t   Nem_SSGhost_len;
extern const uint8_t *Nem_SSGlass;
extern size_t   Nem_SSGlass_len;
extern const uint8_t *Nem_SSRBlock;
extern size_t   Nem_SSRBlock_len;
extern const uint8_t *Nem_SSRedWhite;
extern size_t   Nem_SSRedWhite_len;
extern const uint8_t *Nem_SSUpDown;
extern size_t   Nem_SSUpDown_len;
extern const uint8_t *Nem_SSWBlock;
extern size_t   Nem_SSWBlock_len;
extern const uint8_t *Nem_SSWalls;
extern size_t   Nem_SSWalls_len;
extern const uint8_t *Nem_SSZone1;
extern size_t   Nem_SSZone1_len;
extern const uint8_t *Nem_SSZone2;
extern size_t   Nem_SSZone2_len;
extern const uint8_t *Nem_SSZone3;
extern size_t   Nem_SSZone3_len;
extern const uint8_t *Nem_SSZone4;
extern size_t   Nem_SSZone4_len;
extern const uint8_t *Nem_SSZone5;
extern size_t   Nem_SSZone5_len;
extern const uint8_t *Nem_SSZone6;
extern size_t   Nem_SSZone6_len;
extern const uint8_t *Nem_SYZ;
extern size_t   Nem_SYZ_len;
extern const uint8_t *Nem_Sbz2Eggman;
extern size_t   Nem_Sbz2Eggman_len;
extern const uint8_t *Nem_SbzBlock;
extern size_t   Nem_SbzBlock_len;
extern const uint8_t *Nem_SbzDoor1;
extern size_t   Nem_SbzDoor1_len;
extern const uint8_t *Nem_SbzDoor2;
extern size_t   Nem_SbzDoor2_len;
extern const uint8_t *Nem_SbzFloor;
extern size_t   Nem_SbzFloor_len;
extern const uint8_t *Nem_SbzWheel1;
extern size_t   Nem_SbzWheel1_len;
extern const uint8_t *Nem_SbzWheel2;
extern size_t   Nem_SbzWheel2_len;
extern const uint8_t *Nem_Seesaw;
extern size_t   Nem_Seesaw_len;
extern const uint8_t *Nem_SlideFloor;
extern size_t   Nem_SlideFloor_len;
extern const uint8_t *Nem_SlzBlock;
extern size_t   Nem_SlzBlock_len;
extern const uint8_t *Nem_SlzCannon;
extern size_t   Nem_SlzCannon_len;
extern const uint8_t *Nem_SlzSpike;
extern size_t   Nem_SlzSpike_len;
extern const uint8_t *Nem_SlzSwing;
extern size_t   Nem_SlzSwing_len;
extern const uint8_t *Nem_SlzWall;
extern size_t   Nem_SlzWall_len;
extern const uint8_t *Nem_SpinPform;
extern size_t   Nem_SpinPform_len;
extern const uint8_t *Nem_Splash;
extern size_t   Nem_Splash_len;
extern const uint8_t *Nem_Stomper;
extern size_t   Nem_Stomper_len;
extern const uint8_t *Nem_SyzSpike1;
extern size_t   Nem_SyzSpike1_len;
extern const uint8_t *Nem_SyzSpike2;
extern size_t   Nem_SyzSpike2_len;
extern const uint8_t *Nem_TrapDoor;
extern size_t   Nem_TrapDoor_len;
extern const uint8_t *Nem_TryAgain;
extern size_t   Nem_TryAgain_len;
extern const uint8_t *Nem_Water;
extern size_t   Nem_Water_len;
extern const uint8_t *Nem_Weapons;
extern size_t   Nem_Weapons_len;
extern const uint8_t *Nem_Yadrin;
extern size_t   Nem_Yadrin_len;

/* Cheat codes */
extern const uint8_t LevSelCode_US[];
extern const uint32_t LevSelCode_US_len;

extern const uint8_t LevSelCode_J[];
extern const uint32_t LevSelCode_J_len;

extern const uint16_t LevSel_Ptrs[];
extern const uint32_t LevSel_Ptrs_len;

/* Title screen animation scripts and sprite mappings
   Loaded at runtime from assets/ */
extern const uint8_t *Ani_TSon;
extern size_t   Ani_TSon_len;

extern const uint8_t *Ani_PSBTM;
extern size_t   Ani_PSBTM_len;

extern const uint8_t *Map_TSon;
extern size_t   Map_TSon_len;

extern const uint8_t *Map_PSB;
extern size_t   Map_PSB_len;

extern const uint8_t *Map_Pri;
extern size_t   Map_Pri_len;

extern const uint8_t *Map_Cred;
extern size_t   Map_Cred_len;

extern const uint8_t *Nem_TitleCard;
extern size_t   Nem_TitleCard_len;

/* Explosion art (Nem_Explode) */
extern const uint8_t *Nem_Explode;
extern size_t   Nem_Explode_len;

/* Zone title card sprite mappings */
extern const uint8_t *Map_Card;
extern size_t   Map_Card_len;

/* "SONIC HAS PASSED" card sprite mappings (same file, own table) */
extern const uint8_t *Map_Got;
extern size_t   Map_Got_len;

/* Explosion mappings (27 ExplosionItem / 3F Explosion) */
extern const uint8_t *Map_ExplodeItem;
extern size_t   Map_ExplodeItem_len;
extern const uint8_t *Map_ExplodeBomb;
extern size_t   Map_ExplodeBomb_len;

/* Sonic sprite mappings */
extern const uint8_t *Map_Sonic;
extern size_t   Map_Sonic_len;
extern const uint8_t *Map_Shield;
extern size_t   Map_Shield_len;
extern const uint8_t *Map_Smash;
extern size_t   Map_Smash_len;
extern const uint8_t *Map_Hel;
extern size_t   Map_Hel_len;
extern const uint8_t *Map_Swing_SLZ;
extern size_t   Map_Swing_SLZ_len;
extern const uint8_t *Map_Swing_GHZ;
extern size_t   Map_Swing_GHZ_len;
extern const uint8_t *Map_Eggman;
extern size_t   Map_Eggman_len;
extern const uint8_t *Map_BossItems;
extern size_t  Map_BossItems_len;
/* Sonic animation and dynamic PLC data (raw ASM assets) */
extern const uint8_t *Art_Sonic;
extern size_t   Art_Sonic_len;
extern const uint8_t *SonicDynPLC;
extern size_t   SonicDynPLC_len;
extern const uint8_t *Ani_Sonic;
extern size_t   Ani_Sonic_len;
extern const uint8_t *Ani_Chop;
extern size_t   Ani_Chop_len;
extern const uint8_t *Ani_Eggman;
extern size_t   Ani_Eggman_len;
extern const uint8_t *Ani_Monitor;
extern size_t   Ani_Monitor_len;
extern const uint8_t *Ani_Shield;
extern size_t   Ani_Shield_len;
/* HUD graphics (artnem/HUD.nem = SCOR/TIME/RING text; artnem/hud_lives.nem
   = lives icon + "SONIC x N"; artunc numbers are the 8x16/8x8 digits) */
extern const uint8_t *Nem_Hud;
extern size_t   Nem_Hud_len;
extern const uint8_t *Nem_Lives;
extern size_t   Nem_Lives_len;
extern const uint8_t *Art_Hud;
extern size_t   Art_Hud_len;
extern const uint8_t *Art_LivesNums;
extern size_t   Art_LivesNums_len;

/* HUD sprite mappings */
extern const uint8_t *Map_HUD;
extern size_t   Map_HUD_len;

/* Ring graphics, mappings and animation scripts */
extern const uint8_t *Nem_Ring;
extern size_t   Nem_Ring_len;
extern const uint8_t *Map_Ring;
extern size_t   Map_Ring_len;
extern const uint8_t *Ani_Ring;
extern size_t   Ani_Ring_len;

/* Signpost graphics, mappings and animation scripts */
extern const uint8_t *Map_Sign;
extern size_t   Map_Sign_len;
extern const uint8_t *Ani_Sign;
extern size_t   Ani_Sign_len;

/* Crabmeat graphics, mappings and animation scripts */
extern const uint8_t *Nem_Crabmeat;
extern size_t   Nem_Crabmeat_len;
extern const uint8_t *Map_Crab;
extern size_t   Map_Crab_len;
extern const uint8_t *Ani_Crab;
extern size_t   Ani_Crab_len;

/* Motobug graphics, mappings and animation scripts */
extern const uint8_t *Nem_Motobug;
extern size_t   Nem_Motobug_len;
extern const uint8_t *Map_Moto;
extern size_t   Map_Moto_len;
extern const uint8_t *Ani_Moto;
extern size_t   Ani_Moto_len;

/* Buzz Bomber mappings and animation scripts */
extern const uint8_t *Map_Buzz;
extern size_t   Map_Buzz_len;
extern const uint8_t *Ani_Buzz;
extern size_t   Ani_Buzz_len;
extern const uint8_t *Map_Missile;
extern size_t   Map_Missile_len;
extern const uint8_t *Ani_Missile;
extern size_t   Ani_Missile_len;

extern const uint8_t *Ani_Spring;
extern size_t   Ani_Spring_len;

extern const uint8_t *Ani_Pri;
extern size_t   Ani_Pri_len;

extern const uint8_t *Ani_Newt;
extern size_t   Ani_Newt_len;

/* GHZ bridge (id_Bridge) mappings */
extern const uint8_t *Map_Bri;
extern size_t   Map_Bri_len;

extern const uint8_t *Map_Scen;
extern size_t   Map_Scen_len;

/* Object mappings referenced by the DebugMode item lists (DebugMode.asm).
   Most are unstaged (NULL) until their maps ".asm" assets are ported; the
   debug list itself (debugmode.c) still references them faithfully.  */
extern const uint8_t *Map_Monitor;
extern const uint8_t *Map_Chop;
extern const uint8_t *Map_Spike;
extern const uint8_t *Map_Plat_GHZ;
extern const uint8_t *Map_PRock;
extern const uint8_t *Map_Spring;
extern const uint8_t *Map_Newt;
extern const uint8_t *Map_Edge;
extern const uint8_t *Map_GBall;
extern const uint8_t *Map_Lamp;
extern const uint8_t *Map_GRing;
extern const uint8_t *Map_Bonus;
extern const uint8_t *Map_Jaws;
extern const uint8_t *Map_Burro;
extern const uint8_t *Map_Harp;
extern const uint8_t *Map_Push;
extern const uint8_t *Map_But;
extern const uint8_t *Map_MBlockLZ;
extern const uint8_t *Map_LBlock;
extern const uint8_t *Map_Gar;
extern const uint8_t *Map_LConv;
extern const uint8_t *Map_Orb;
extern const uint8_t *Map_Bub;
extern const uint8_t *Map_WFall;
extern const uint8_t *Map_Pole;
extern const uint8_t *Map_Flap;
extern const uint8_t *Map_Fire;
extern const uint8_t *Map_Brick;
extern const uint8_t *Map_Geyser;
extern const uint8_t *Map_LWall;
extern const uint8_t *Map_Yad;
extern const uint8_t *Map_Smab;
extern const uint8_t *Map_MBlock;
extern const uint8_t *Map_CFlo;
extern size_t   Map_CFlo_len;
extern const uint8_t *Map_LTag;
extern const uint8_t *Map_Bas;
extern const uint8_t *Map_Cat;
extern const uint8_t *Map_Elev;
extern const uint8_t *Map_Plat_SLZ;
extern const uint8_t *Map_Circ;
extern const uint8_t *Map_Stair;
extern const uint8_t *Map_Fan;
extern const uint8_t *Map_Seesaw;
extern const uint8_t *Map_Scen;
extern const uint8_t *Map_Bomb;
extern const uint8_t *Map_Roll;
extern const uint8_t *Map_Light;
extern const uint8_t *Map_Bump;
extern const uint8_t *Map_Plat_SYZ;
extern const uint8_t *Map_FBlock;
extern const uint8_t *Map_BBall;
extern const uint8_t *Map_Disc;
extern const uint8_t *Map_Trap;
extern const uint8_t *Map_Spin;
extern const uint8_t *Map_Saw;
extern const uint8_t *Map_Stomp;
extern const uint8_t *Map_ADoor;
extern const uint8_t *Map_VanP;
extern const uint8_t *Map_Flame;
extern const uint8_t *Map_Elec;
extern const uint8_t *Map_Gird;
extern const uint8_t *Map_Invis;
extern const uint8_t *Map_Hog;
extern const uint8_t *Map_Animal1;
extern size_t   Map_Animal1_len;
extern const uint8_t *Map_Animal2;
extern size_t   Map_Animal2_len;
extern const uint8_t *Map_Animal3;
extern size_t   Map_Animal3_len;
extern const uint8_t *Map_Ledge;
extern size_t   Map_Ledge_len;
extern const uint8_t *Map_Newt;
extern size_t   Map_Newt_len;

/* Points object mappings (28, 29 Animals and Points.asm: Map_Points) */
extern const uint8_t *Map_Points;
extern size_t   Map_Points_len;

/* Object placement data (objpos binaries) */
extern const uint8_t *ObjPos_GHZ1;
extern size_t   ObjPos_GHZ1_len;
extern const uint8_t *ObjPos_GHZ2;
extern size_t   ObjPos_GHZ2_len;
extern const uint8_t *ObjPos_GHZ3;
extern size_t   ObjPos_GHZ3_len;
extern const uint8_t *ObjPos_LZ1;
extern size_t   ObjPos_LZ1_len;
extern const uint8_t *ObjPos_LZ2;
extern size_t   ObjPos_LZ2_len;
extern const uint8_t *ObjPos_LZ3;
extern size_t   ObjPos_LZ3_len;
extern const uint8_t *ObjPos_SBZ3;
extern size_t   ObjPos_SBZ3_len;
extern const uint8_t *ObjPos_MZ1;
extern size_t   ObjPos_MZ1_len;
extern const uint8_t *ObjPos_MZ2;
extern size_t   ObjPos_MZ2_len;
extern const uint8_t *ObjPos_MZ3;
extern size_t   ObjPos_MZ3_len;
extern const uint8_t *ObjPos_SLZ1;
extern size_t   ObjPos_SLZ1_len;
extern const uint8_t *ObjPos_SLZ2;
extern size_t   ObjPos_SLZ2_len;
extern const uint8_t *ObjPos_SLZ3;
extern size_t   ObjPos_SLZ3_len;
extern const uint8_t *ObjPos_SYZ1;
extern size_t   ObjPos_SYZ1_len;
extern const uint8_t *ObjPos_SYZ2;
extern size_t   ObjPos_SYZ2_len;
extern const uint8_t *ObjPos_SYZ3;
extern size_t   ObjPos_SYZ3_len;
extern const uint8_t *ObjPos_SBZ1;
extern size_t   ObjPos_SBZ1_len;
extern const uint8_t *ObjPos_SBZ2;
extern size_t   ObjPos_SBZ2_len;
extern const uint8_t *ObjPos_FZ;
extern size_t   ObjPos_FZ_len;
extern const uint8_t *ObjPos_End;
extern size_t   ObjPos_End_len;

/* Per-zone collision indexes (ColPointers, sonic.asm:3116-3121).
   Only GHZ is staged; the rest are NULL until their collide ".bin" lands.  */
extern const uint8_t *Col_GHZ;
extern size_t   Col_GHZ_len;
extern const uint8_t *Col_LZ;
extern size_t   Col_LZ_len;
extern const uint8_t *Col_MZ;
extern size_t   Col_MZ_len;
extern const uint8_t *Col_SLZ;
extern size_t   Col_SLZ_len;
extern const uint8_t *Col_SYZ;
extern size_t   Col_SYZ_len;
extern const uint8_t *Col_SBZ;
extern size_t   Col_SBZ_len;

/* Uncompressed level art for AnimateLevelAct (AnimateLevelGfx.asm).
   MZ/SBZ art only loads once those zones are playable. */
extern const uint8_t *Art_GhzWater;
extern size_t   Art_GhzWater_len;
extern const uint8_t *Art_GhzFlower1;
extern size_t   Art_GhzFlower1_len;
extern const uint8_t *Art_GhzFlower2;
extern size_t   Art_GhzFlower2_len;
extern const uint8_t *Art_MzLava1;
extern size_t   Art_MzLava1_len;
extern const uint8_t *Art_MzLava2;
extern size_t   Art_MzLava2_len;
extern const uint8_t *Art_MzTorch;
extern size_t   Art_MzTorch_len;
extern const uint8_t *Art_SbzSmoke;
extern size_t   Art_SbzSmoke_len;
extern const uint8_t *Art_BigRing;
extern size_t   Art_BigRing_len;

/* Collision index tables (AngleMap, CollArray1, CollArray2) */
extern const uint8_t *Col_AngleMap;
extern size_t   Col_AngleMap_len;
extern const uint8_t *Col_CollArray1;
extern size_t   Col_CollArray1_len;
extern const uint8_t *Col_CollArray2;
extern size_t   Col_CollArray2_len;

/* Level start location arrays (from _inc/LevelSizeLoad & BgScrollSpeed.asm) */
extern const uint8_t *StartLocArray;
extern size_t   StartLocArray_len;
extern const uint8_t *EndingStLocArray;
extern size_t   EndingStLocArray_len;

/* SS: Pal Cycle and Other Maps */
extern const uint8_t *Pal_SSCyc1;       extern size_t Pal_SSCyc1_len;
extern const uint8_t *Pal_SSCyc2;       extern size_t Pal_SSCyc2_len;
extern const uint8_t *Eni_SSBg1;        extern size_t Eni_SSBg1_len;
extern const uint8_t *Eni_SSBg2;        extern size_t Eni_SSBg2_len;
extern const uint8_t *SS_1;             extern size_t SS_1_len;
extern const uint8_t *SS_2;             extern size_t SS_2_len;
extern const uint8_t *SS_3;             extern size_t SS_3_len;
extern const uint8_t *SS_4;             extern size_t SS_4_len;
extern const uint8_t *SS_5;             extern size_t SS_5_len;
extern const uint8_t *SS_6;             extern size_t SS_6_len;
extern const uint8_t *SS_StartLoc;      extern size_t SS_StartLoc_len;

/* SS block mappings */
extern const uint8_t *Map_SSWalls;         extern size_t Map_SSWalls_len;
extern const uint8_t *Map_Bump;            extern size_t Map_Bump_len;
extern const uint8_t *Map_SS_Shared;       extern size_t Map_SS_Shared_len;
extern const uint8_t *Map_SS_Up;           extern size_t Map_SS_Up_len;
extern const uint8_t *Map_SS_Down;         extern size_t Map_SS_Down_len;
extern const uint8_t *Map_SS_Glass;        extern size_t Map_SS_Glass_len;
extern const uint8_t *Map_SS_Chaos1;       extern size_t Map_SS_Chaos1_len;
extern const uint8_t *Map_SS_Chaos2;       extern size_t Map_SS_Chaos2_len;
extern const uint8_t *Map_SS_Chaos3;       extern size_t Map_SS_Chaos3_len;

/* Initialize all assets from the assets/ directory.
   Returns 0 on success, -1 on any failure. */
int Data_Init(void);

/* Free all loaded assets */
void Data_Quit(void);
size_t Map_LookupLength(const uint8_t *ptr);
#endif /* SONIC1_DATA_H */
