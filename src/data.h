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
#define palid_Ending    19

/* Title Screen assets */
extern const uint8_t *Pal_Title;
extern size_t   Pal_Title_len;

/* Title screen water palette cycle data (assets/palette/cycle_water.bin) */
extern const uint8_t *Pal_TitleCycWater;
extern size_t   Pal_TitleCycWater_len;

extern const uint8_t *Pal_LevelSel;
extern size_t   Pal_LevelSel_len;

extern const uint8_t *Pal_Sonic;
extern size_t   Pal_Sonic_len;

extern const uint8_t *Pal_GHZ;
extern size_t   Pal_GHZ_len;

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

extern const uint8_t *Eni_Title;
extern size_t   Eni_Title_len;

extern const uint8_t *Nem_GHZ_1st;
extern size_t   Nem_GHZ_1st_len;

extern const uint8_t *Level_GHZ1;
extern size_t   Level_GHZ1_len;

extern const uint8_t *Level_GHZbg;
extern size_t   Level_GHZbg_len;

/* Level PLC graphics (PLC_GHZ + PLC_Main2). Only Nem_GHZ_1st is staged so
   far; the rest are NULL and skipped by AddPLC until their assets land. */
extern const uint8_t *Nem_GHZ_2nd;
extern size_t   Nem_GHZ_2nd_len;
extern const uint8_t *Nem_Stalk;
extern size_t   Nem_Stalk_len;
extern const uint8_t *Nem_PplRock;
extern size_t   Nem_PplRock_len;
extern const uint8_t *Nem_Crabmeat;
extern size_t   Nem_Crabmeat_len;
extern const uint8_t *Nem_Buzz;
extern size_t   Nem_Buzz_len;
extern const uint8_t *Nem_Chopper;
extern size_t   Nem_Chopper_len;
extern const uint8_t *Nem_Newtron;
extern size_t   Nem_Newtron_len;
extern const uint8_t *Nem_Motobug;
extern size_t   Nem_Motobug_len;
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

extern const uint8_t *Map_Cred;
extern size_t   Map_Cred_len;

extern const uint8_t *Nem_TitleCard;
extern size_t   Nem_TitleCard_len;

/* Zone title card sprite mappings */
extern const uint8_t *Map_Card;
extern size_t   Map_Card_len;

/* Sonic sprite mappings */
extern const uint8_t *Map_Sonic;
extern size_t   Map_Sonic_len;

/* Sonic animation and dynamic PLC data (raw ASM assets) */
extern const uint8_t *Art_Sonic;
extern size_t   Art_Sonic_len;
extern const uint8_t *SonicDynPLC;
extern size_t   SonicDynPLC_len;
extern const uint8_t *Ani_Sonic;
extern size_t   Ani_Sonic_len;

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

/* Object placement data (objpos binaries) */
extern const uint8_t *ObjPos_GHZ1;
extern size_t   ObjPos_GHZ1_len;

/* Per-zone collision indexes (ColPointers, sonic.asm:3116-3121).
   Only GHZ is staged; the rest are NULL until their collide/*.bin lands. */
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

/* Initialize all assets from the assets/ directory.
   Returns 0 on success, -1 on any failure. */
int Data_Init(void);

/* Free all loaded assets */
void Data_Quit(void);

#endif /* SONIC1_DATA_H */
