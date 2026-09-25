#include "data.h"
#include "assets.h"
#include "palette.h"
#include "constants.h"
#include "level.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>

/* ============================================================================
   mmap helpers
   Los punteros de mappings se guardan en campos de 32 bits (obMap), así que
   los datos deben vivir por debajo de 4 GB. MAP_32BIT fuerza esa dirección.
   ========================================================================== */

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
   Forward declarations
   ========================================================================== */

static int  load_asset(const char *name, const uint8_t **out_ptr, size_t *out_len);
static int  load_asm_asset(const char *name, const uint8_t **out_ptr,
                           size_t *out_len, int is_map);
static int  load_asm_asset_named(const char *name, const char *tblname,
                                  const uint8_t **out_ptr, size_t *out_len);

/* ============================================================================
   ASSETS — declaraciones
   Los globals arrancan en 0/NULL. Data_Init() los rellena; en fallo quedan
   como están (la carga no toca out_ptr cuando falla).
   ========================================================================== */

/* ---------------- Paletas ---------------- */
const uint8_t *Pal_SegaBG;          size_t Pal_SegaBG_len;
const uint8_t *Pal_Sega1;           size_t Pal_Sega1_len;
const uint8_t *Pal_Sega2;           size_t Pal_Sega2_len;
const uint8_t *Pal_Title;           size_t Pal_Title_len;
const uint8_t *Pal_TitleCycWater;   size_t Pal_TitleCycWater_len;
const uint8_t *Pal_GHZCycWater;     size_t Pal_GHZCycWater_len;
const uint8_t *Pal_LevelSel;        size_t Pal_LevelSel_len;
const uint8_t *Pal_Sonic;           size_t Pal_Sonic_len;
const uint8_t *Pal_GHZ;             size_t Pal_GHZ_len;
const uint8_t *Pal_LZ;              size_t Pal_LZ_len;
const uint8_t *Pal_LZWater;         size_t Pal_LZWater_len;
const uint8_t *Pal_LZSonWater;      size_t Pal_LZSonWater_len;
const uint8_t *Pal_MZ;              size_t Pal_MZ_len;
const uint8_t *Pal_SLZ;             size_t Pal_SLZ_len;
const uint8_t *Pal_SYZ;             size_t Pal_SYZ_len;
const uint8_t *Pal_SBZ1;            size_t Pal_SBZ1_len;
const uint8_t *Pal_SBZ2;            size_t Pal_SBZ2_len;
const uint8_t *Pal_SBZ3;            size_t Pal_SBZ3_len;
const uint8_t *Pal_SBZ3Water;       size_t Pal_SBZ3Water_len;
const uint8_t *Pal_SBZ3SonWat;      size_t Pal_SBZ3SonWat_len;
const uint8_t *Pal_Special;         size_t Pal_Special_len;
const uint8_t *Pal_SSResult;        size_t Pal_SSResult_len;
const uint8_t *Pal_Continue;        size_t Pal_Continue_len;
const uint8_t *Pal_Ending;          size_t Pal_Ending_len;
const uint8_t *Pal_SSCyc1;          size_t Pal_SSCyc1_len;
const uint8_t *Pal_SSCyc2;          size_t Pal_SSCyc2_len;

/* ---------------- Arte: Sega / Title / Misc ---------------- */
const uint8_t *Nem_SegaLogo;        size_t Nem_SegaLogo_len;
const uint8_t *Nem_TitleFg;         size_t Nem_TitleFg_len;
const uint8_t *Nem_TitleSonic;      size_t Nem_TitleSonic_len;
const uint8_t *Nem_TitleTM;         size_t Nem_TitleTM_len;
const uint8_t *Nem_TitleCard;       size_t Nem_TitleCard_len;
const uint8_t *Nem_CreditText;      size_t Nem_CreditText_len;
const uint8_t *Nem_JapNames;        size_t Nem_JapNames_len;
const uint8_t *Eni_SegaLogo;        size_t Eni_SegaLogo_len;
const uint8_t *Eni_Title;           size_t Eni_Title_len;
const uint8_t *Eni_JapNames;        size_t Eni_JapNames_len;
const uint8_t *Art_Text;            size_t Art_Text_len;
const uint8_t *Eni_SSBg1;           size_t Eni_SSBg1_len;
const uint8_t *Eni_SSBg2;           size_t Eni_SSBg2_len;

/* ---------------- Arte: Zonas (8x8) ---------------- */
const uint8_t *Nem_GHZ_1st;         size_t Nem_GHZ_1st_len;
const uint8_t *Nem_GHZ_2nd;         size_t Nem_GHZ_2nd_len;
const uint8_t *Nem_LZ;              size_t Nem_LZ_len;
const uint8_t *Nem_MZ;              size_t Nem_MZ_len;
const uint8_t *Nem_SLZ;             size_t Nem_SLZ_len;
const uint8_t *Nem_SYZ;             size_t Nem_SYZ_len;
const uint8_t *Nem_SBZ;             size_t Nem_SBZ_len;

/* ---------------- Arte: Objetos comunes ---------------- */
const uint8_t *Nem_Ring;            size_t Nem_Ring_len;
const uint8_t *Nem_SignPost;        size_t Nem_SignPost_len;
const uint8_t *Nem_Bonus;           size_t Nem_Bonus_len;
const uint8_t *Nem_BigFlash;        size_t Nem_BigFlash_len;
const uint8_t *Nem_Shield;          size_t Nem_Shield_len;
const uint8_t *Nem_Stars;           size_t Nem_Stars_len;
const uint8_t *Nem_Spikes;          size_t Nem_Spikes_len;
const uint8_t *Nem_HSpring;         size_t Nem_HSpring_len;
const uint8_t *Nem_VSpring;         size_t Nem_VSpring_len;
const uint8_t *Nem_Monitors;        size_t Nem_Monitors_len;
const uint8_t *Nem_Points;          size_t Nem_Points_len;
const uint8_t *Nem_Prison;          size_t Nem_Prison_len;
const uint8_t *Nem_Explode;         size_t Nem_Explode_len;

/* ---------------- Arte: Enemigos ---------------- */
const uint8_t *Nem_Crabmeat;        size_t Nem_Crabmeat_len;
const uint8_t *Nem_Motobug;         size_t Nem_Motobug_len;
const uint8_t *Nem_Buzz;            size_t Nem_Buzz_len;
const uint8_t *Nem_Chopper;         size_t Nem_Chopper_len;
const uint8_t *Nem_Newtron;         size_t Nem_Newtron_len;
const uint8_t *Nem_BallHog;         size_t Nem_BallHog_len;
const uint8_t *Nem_Basaran;         size_t Nem_Basaran_len;
const uint8_t *Nem_Bomb;            size_t Nem_Bomb_len;
const uint8_t *Nem_Burrobot;        size_t Nem_Burrobot_len;
const uint8_t *Nem_Cater;           size_t Nem_Cater_len;
const uint8_t *Nem_Jaws;            size_t Nem_Jaws_len;
const uint8_t *Nem_Orbinaut;        size_t Nem_Orbinaut_len;
const uint8_t *Nem_Roller;          size_t Nem_Roller_len;
const uint8_t *Nem_Yadrin;          size_t Nem_Yadrin_len;
const uint8_t *Nem_Gargoyle;        size_t Nem_Gargoyle_len;
const uint8_t *Nem_Harpoon;         size_t Nem_Harpoon_len;
const uint8_t *Nem_LzSpikeBall;     size_t Nem_LzSpikeBall_len;
const uint8_t *Nem_Cutter;          size_t Nem_Cutter_len;
const uint8_t *Nem_SyzSpike1;       size_t Nem_SyzSpike1_len;
const uint8_t *Nem_SyzSpike2;       size_t Nem_SyzSpike2_len;
const uint8_t *Nem_Bumper;          size_t Nem_Bumper_len;
const uint8_t *Nem_MzFire;          size_t Nem_MzFire_len;

/* ---------------- Arte: Jefes y objetos de zona ---------------- */
const uint8_t *Nem_Eggman;          size_t Nem_Eggman_len;
const uint8_t *Nem_Weapons;         size_t Nem_Weapons_len;
const uint8_t *Nem_Exhaust;         size_t Nem_Exhaust_len;
const uint8_t *Nem_FzBoss;          size_t Nem_FzBoss_len;
const uint8_t *Nem_FzEggman;        size_t Nem_FzEggman_len;
const uint8_t *Nem_Sbz2Eggman;      size_t Nem_Sbz2Eggman_len;
const uint8_t *Nem_GhzWall1;        size_t Nem_GhzWall1_len;
const uint8_t *Nem_GhzWall2;        size_t Nem_GhzWall2_len;
const uint8_t *Nem_Swing;           size_t Nem_Swing_len;
const uint8_t *Nem_Bridge;          size_t Nem_Bridge_len;
const uint8_t *Nem_SpikePole;       size_t Nem_SpikePole_len;
const uint8_t *Nem_Ball;            size_t Nem_Ball_len;

/* ---------------- Arte: Objetos de zona (bloques, puertas, lava…) ---------------- */
/* LZ */
const uint8_t *Nem_LzBlock1;        size_t Nem_LzBlock1_len;
const uint8_t *Nem_LzBlock2;        size_t Nem_LzBlock2_len;
const uint8_t *Nem_LzBlock3;        size_t Nem_LzBlock3_len;
const uint8_t *Nem_LzDoor1;         size_t Nem_LzDoor1_len;
const uint8_t *Nem_LzDoor2;         size_t Nem_LzDoor2_len;
const uint8_t *Nem_LzPlatfm;        size_t Nem_LzPlatfm_len;
const uint8_t *Nem_LzPole;          size_t Nem_LzPole_len;
const uint8_t *Nem_LzWheel;         size_t Nem_LzWheel_len;
const uint8_t *Nem_LzSwitch;        size_t Nem_LzSwitch_len;
const uint8_t *Nem_Splash;          size_t Nem_Splash_len;
const uint8_t *Nem_Water;           size_t Nem_Water_len;
const uint8_t *Nem_FlapDoor;        size_t Nem_FlapDoor_len;
const uint8_t *Nem_Bubbles;         size_t Nem_Bubbles_len;
const uint8_t *Nem_Cork;            size_t Nem_Cork_len;
const uint8_t *Nem_MiniSonic;       size_t Nem_MiniSonic_len;

/* MZ */
const uint8_t *Nem_MzBlock;         size_t Nem_MzBlock_len;
const uint8_t *Nem_MzGlass;         size_t Nem_MzGlass_len;
const uint8_t *Nem_MzMetal;         size_t Nem_MzMetal_len;
const uint8_t *Nem_MzSwitch;        size_t Nem_MzSwitch_len;
const uint8_t *Nem_Lava;            size_t Nem_Lava_len;

/* SLZ */
const uint8_t *Nem_Pylon;           size_t Nem_Pylon_len;
const uint8_t *Nem_Seesaw;          size_t Nem_Seesaw_len;
const uint8_t *Nem_SlzBlock;        size_t Nem_SlzBlock_len;
const uint8_t *Nem_SlzCannon;       size_t Nem_SlzCannon_len;
const uint8_t *Nem_SlzSpike;        size_t Nem_SlzSpike_len;
const uint8_t *Nem_SlzSwing;        size_t Nem_SlzSwing_len;
const uint8_t *Nem_SlzWall;         size_t Nem_SlzWall_len;
const uint8_t *Nem_Fan;             size_t Nem_Fan_len;

/* SBZ */
const uint8_t *Nem_SbzBlock;        size_t Nem_SbzBlock_len;
const uint8_t *Nem_SbzDoor1;        size_t Nem_SbzDoor1_len;
const uint8_t *Nem_SbzDoor2;        size_t Nem_SbzDoor2_len;
const uint8_t *Nem_SbzFloor;        size_t Nem_SbzFloor_len;
const uint8_t *Nem_SbzWheel1;       size_t Nem_SbzWheel1_len;
const uint8_t *Nem_SbzWheel2;       size_t Nem_SbzWheel2_len;
const uint8_t *Nem_SlideFloor;      size_t Nem_SlideFloor_len;
const uint8_t *Nem_SpinPform;       size_t Nem_SpinPform_len;
const uint8_t *Nem_Stomper;         size_t Nem_Stomper_len;
const uint8_t *Nem_TrapDoor;        size_t Nem_TrapDoor_len;
const uint8_t *Nem_Girder;          size_t Nem_Girder_len;
const uint8_t *Nem_FlamePipe;       size_t Nem_FlamePipe_len;
const uint8_t *Nem_Electric;        size_t Nem_Electric_len;

/* Comunes / GHZ */
const uint8_t *Nem_Lamp;            size_t Nem_Lamp_len;
const uint8_t *Nem_Stalk;           size_t Nem_Stalk_len;
const uint8_t *Nem_PplRock;         size_t Nem_PplRock_len;

/* ---------------- Arte: Animales ---------------- */
const uint8_t *Nem_Rabbit;          size_t Nem_Rabbit_len;
const uint8_t *Nem_Chicken;         size_t Nem_Chicken_len;
const uint8_t *Nem_Penguin;         size_t Nem_Penguin_len;
const uint8_t *Nem_Seal;            size_t Nem_Seal_len;
const uint8_t *Nem_Pig;             size_t Nem_Pig_len;
const uint8_t *Nem_Flicky;          size_t Nem_Flicky_len;
const uint8_t *Nem_Squirrel;        size_t Nem_Squirrel_len;

/* ---------------- Arte: Special Stage ---------------- */
const uint8_t *Nem_SS1UpBlock;      size_t Nem_SS1UpBlock_len;
const uint8_t *Nem_SSBgCloud;       size_t Nem_SSBgCloud_len;
const uint8_t *Nem_SSBgFish;        size_t Nem_SSBgFish_len;
const uint8_t *Nem_SSEmStars;       size_t Nem_SSEmStars_len;
const uint8_t *Nem_SSEmerald;       size_t Nem_SSEmerald_len;
const uint8_t *Nem_SSGOAL;          size_t Nem_SSGOAL_len;
const uint8_t *Nem_SSGhost;         size_t Nem_SSGhost_len;
const uint8_t *Nem_SSGlass;         size_t Nem_SSGlass_len;
const uint8_t *Nem_SSRBlock;        size_t Nem_SSRBlock_len;
const uint8_t *Nem_SSRedWhite;      size_t Nem_SSRedWhite_len;
const uint8_t *Nem_SSUpDown;        size_t Nem_SSUpDown_len;
const uint8_t *Nem_SSWBlock;        size_t Nem_SSWBlock_len;
const uint8_t *Nem_SSWalls;         size_t Nem_SSWalls_len;
const uint8_t *Nem_SSZone1;         size_t Nem_SSZone1_len;
const uint8_t *Nem_SSZone2;         size_t Nem_SSZone2_len;
const uint8_t *Nem_SSZone3;         size_t Nem_SSZone3_len;
const uint8_t *Nem_SSZone4;         size_t Nem_SSZone4_len;
const uint8_t *Nem_SSZone5;         size_t Nem_SSZone5_len;
const uint8_t *Nem_SSZone6;         size_t Nem_SSZone6_len;

/* ---------------- Arte: Ending ---------------- */
const uint8_t *Nem_EndEm;           size_t Nem_EndEm_len;
const uint8_t *Nem_EndFlower;       size_t Nem_EndFlower_len;
const uint8_t *Nem_EndSonic;        size_t Nem_EndSonic_len;
const uint8_t *Nem_EndStH;          size_t Nem_EndStH_len;
const uint8_t *Nem_TryAgain;        size_t Nem_TryAgain_len;
const uint8_t *Nem_GameOver;        size_t Nem_GameOver_len;
const uint8_t *Nem_ResultEm;        size_t Nem_ResultEm_len;

/* ---------------- Arte: HUD ---------------- */
const uint8_t *Nem_Hud;             size_t Nem_Hud_len;
const uint8_t *Nem_Lives;           size_t Nem_Lives_len;
const uint8_t *Art_Hud;             size_t Art_Hud_len;
const uint8_t *Art_LivesNums;       size_t Art_LivesNums_len;

/* ---------------- Arte sin comprimir (nivel + Sonic) ---------------- */
const uint8_t *Art_Sonic;           size_t Art_Sonic_len;
const uint8_t *Art_GhzWater;        size_t Art_GhzWater_len;
const uint8_t *Art_GhzFlower1;      size_t Art_GhzFlower1_len;
const uint8_t *Art_GhzFlower2;      size_t Art_GhzFlower2_len;
const uint8_t *Art_MzLava1;         size_t Art_MzLava1_len;
const uint8_t *Art_MzLava2;         size_t Art_MzLava2_len;
const uint8_t *Art_MzTorch;         size_t Art_MzTorch_len;
const uint8_t *Art_SbzSmoke;        size_t Art_SbzSmoke_len;
const uint8_t *Art_BigRing;         size_t Art_BigRing_len;

/* ---------------- Tilemaps (bloques 16x16 y chunks 256x256) ---------------- */
const uint8_t *Blk16_GHZ;           size_t Blk16_GHZ_len;
const uint8_t *Blk16_LZ;            size_t Blk16_LZ_len;
const uint8_t *Blk16_MZ;            size_t Blk16_MZ_len;
const uint8_t *Blk16_SLZ;           size_t Blk16_SLZ_len;
const uint8_t *Blk16_SYZ;           size_t Blk16_SYZ_len;
const uint8_t *Blk16_SBZ;           size_t Blk16_SBZ_len;
const uint8_t *Blk256_GHZ;          size_t Blk256_GHZ_len;
const uint8_t *Blk256_LZ;           size_t Blk256_LZ_len;
const uint8_t *Blk256_MZ;           size_t Blk256_MZ_len;
const uint8_t *Blk256_SLZ;          size_t Blk256_SLZ_len;
const uint8_t *Blk256_SYZ;          size_t Blk256_SYZ_len;
const uint8_t *Blk256_SBZ;          size_t Blk256_SBZ_len;

/* ---------------- Mappings (Map_*) ---------------- */
const uint8_t *Map_Sonic;           size_t Map_Sonic_len;
const uint8_t *Map_TSon;            size_t Map_TSon_len;
const uint8_t *Map_PSB;             size_t Map_PSB_len;
const uint8_t *Map_Cred;            size_t Map_Cred_len;
const uint8_t *Map_Card;            size_t Map_Card_len;
const uint8_t *Map_Got;             size_t Map_Got_len;
const uint8_t *Map_SSR;             size_t Map_SSR_len;
const uint8_t *Map_SSRC;            size_t Map_SSRC_len;
const uint8_t *Map_HUD;             size_t Map_HUD_len;
const uint8_t *Map_Ring;            size_t Map_Ring_len;
const uint8_t *Map_Sign;            size_t Map_Sign_len;
const uint8_t *Map_Pri;             size_t Map_Pri_len;
const uint8_t *Map_Shield;          size_t Map_Shield_len;
const uint8_t *Map_Smash;           size_t Map_Smash_len;
const uint8_t *Map_Hel;             size_t Map_Hel_len;
const uint8_t *Map_Swing_GHZ;       size_t Map_Swing_GHZ_len;
const uint8_t *Map_Swing_SLZ;       size_t Map_Swing_SLZ_len;
const uint8_t *Map_Eggman;          size_t Map_Eggman_len;
const uint8_t *Map_BossItems;       size_t Map_BossItems_len;
const uint8_t *Map_Bri;             size_t Map_Bri_len;
const uint8_t *Map_PRock;           size_t Map_PRock_len;
const uint8_t *Map_Edge;            size_t Map_Edge_len;
const uint8_t *Map_GBall;           size_t Map_GBall_len;
const uint8_t *Map_Scen;            size_t Map_Scen_len;
const uint8_t *Map_Spring;          size_t Map_Spring_len;
const uint8_t *Map_Monitor;         size_t Map_Monitor_len;
const uint8_t *Map_Spike;           size_t Map_Spike_len;
const uint8_t *Map_Chop;            size_t Map_Chop_len;
const uint8_t *Map_Crab;            size_t Map_Crab_len;
const uint8_t *Map_Moto;            size_t Map_Moto_len;
const uint8_t *Map_Buzz;            size_t Map_Buzz_len;
const uint8_t *Map_Missile;         size_t Map_Missile_len;
const uint8_t *Map_Plat_GHZ;        size_t Map_Plat_GHZ_len;
const uint8_t *Map_Ledge;           size_t Map_Ledge_len;
const uint8_t *Map_CFlo;            size_t Map_CFlo_len;
const uint8_t *Map_ExplodeItem;     size_t Map_ExplodeItem_len;
const uint8_t *Map_ExplodeBomb;     size_t Map_ExplodeBomb_len;
const uint8_t *Map_Animal1;         size_t Map_Animal1_len;
const uint8_t *Map_Animal2;         size_t Map_Animal2_len;
const uint8_t *Map_Animal3;         size_t Map_Animal3_len;
const uint8_t *Map_Points;          size_t Map_Points_len;
const uint8_t *Map_SSWalls;         size_t Map_SSWalls_len;
const uint8_t *Map_Bump;            size_t Map_Bump_len;
const uint8_t *Map_SS_Shared;       size_t Map_SS_Shared_len;
const uint8_t *Map_SS_Up;           size_t Map_SS_Up_len;
const uint8_t *Map_SS_Down;         size_t Map_SS_Down_len;
const uint8_t *Map_SS_Glass;        size_t Map_SS_Glass_len;
const uint8_t *Map_SS_Chaos1;       size_t Map_SS_Chaos1_len;
const uint8_t *Map_SS_Chaos2;       size_t Map_SS_Chaos2_len;
const uint8_t *Map_SS_Chaos3;       size_t Map_SS_Chaos3_len;
const uint8_t *Map_Flash;           size_t Map_Flash_len;
const uint8_t *Map_Glass;           size_t Map_Glass_len;
const uint8_t *Map_CStom;           size_t Map_CStom_len;
const uint8_t *Map_LGrass;          size_t Map_LGrass_len;
const uint8_t *Map_Fire;            size_t Map_Fire_len;

/* Mappings referenciados por DebugMode (aún sin portar en su mayoría) */
const uint8_t *Map_Newt;            size_t Map_Newt_len;
const uint8_t *Map_Lamp;
const uint8_t *Map_GRing;           size_t Map_GRing_len;
const uint8_t *Map_Bonus;
const uint8_t *Map_Jaws;
const uint8_t *Map_Burro;
const uint8_t *Map_Harp;
const uint8_t *Map_Push;            size_t Map_Push_len;
const uint8_t *Map_But;             size_t Map_But_len;
const uint8_t *Map_MBlockLZ;        size_t Map_MBlockLZ_len;
const uint8_t *Map_LBlock;
const uint8_t *Map_Gar;
const uint8_t *Map_LConv;
const uint8_t *Map_Orb;
const uint8_t *Map_Bub;
const uint8_t *Map_WFall;
const uint8_t *Map_Pole;
const uint8_t *Map_Flap;
const uint8_t *Map_Fire;
const uint8_t *Map_Brick;           size_t Map_Brick_len;
const uint8_t *Map_Geyser;          size_t Map_Geyser_len;
const uint8_t *Map_LWall;           size_t Map_LWall_len;
const uint8_t *Map_Yad;             size_t Map_Yad_len;
const uint8_t *Map_Smab;            size_t Map_Smab_len;
const uint8_t *Map_MBlock;          size_t Map_MBlock_len;
const uint8_t *Map_LTag;            size_t Map_LTag_len;
const uint8_t *Map_Bas;             size_t Map_Bas_len;
const uint8_t *Map_Cat;             size_t Map_Cat_len;
const uint8_t *Map_Elev;
const uint8_t *Map_Plat_SLZ;        size_t Map_Plat_SLZ_len;
const uint8_t *Map_Circ;
const uint8_t *Map_Stair;
const uint8_t *Map_Fan;
const uint8_t *Map_Seesaw;
const uint8_t *Map_Bomb;
const uint8_t *Map_Roll;
const uint8_t *Map_Light;           size_t Map_Light_len;
const uint8_t *Map_Bump;
const uint8_t *Map_Plat_SYZ;        size_t Map_Plat_SYZ_len;
const uint8_t *Map_FBlock;          size_t Map_FBlock_len;
const uint8_t *Map_BBall;
const uint8_t *Map_Disc;
const uint8_t *Map_Trap;
const uint8_t *Map_Spin;
const uint8_t *Map_Saw;
const uint8_t *Map_Stomp;
const uint8_t *Map_ADoor;
const uint8_t *Map_VanP;
const uint8_t *Map_Flame;
const uint8_t *Map_Elec;
const uint8_t *Map_Gird;
const uint8_t *Map_Invis;
const uint8_t *Map_Hog;

/* ---------------- Animaciones (Ani_*) y DPLC ---------------- */
const uint8_t *Ani_Sonic;           size_t Ani_Sonic_len;
const uint8_t *Ani_TSon;            size_t Ani_TSon_len;
const uint8_t *Ani_PSBTM;           size_t Ani_PSBTM_len;
const uint8_t *Ani_Pri;             size_t Ani_Pri_len;
const uint8_t *Ani_Ring;            size_t Ani_Ring_len;
const uint8_t *Ani_Sign;            size_t Ani_Sign_len;
const uint8_t *Ani_Shield;          size_t Ani_Shield_len;
const uint8_t *Ani_Crab;            size_t Ani_Crab_len;
const uint8_t *Ani_Moto;            size_t Ani_Moto_len;
const uint8_t *Ani_Buzz;            size_t Ani_Buzz_len;
const uint8_t *Ani_Missile;         size_t Ani_Missile_len;
const uint8_t *Ani_Chop;            size_t Ani_Chop_len;
const uint8_t *Ani_Eggman;          size_t Ani_Eggman_len;
const uint8_t *Ani_Monitor;         size_t Ani_Monitor_len;
const uint8_t *Ani_Spring;          size_t Ani_Spring_len;
const uint8_t *Ani_Newt;            size_t Ani_Newt_len;
const uint8_t *Ani_Bas;             size_t Ani_Bas_len;
const uint8_t *Ani_GFire;           size_t Ani_GFire_len;
const uint8_t *Ani_Fire;            size_t Ani_Fire_len;
const uint8_t *Ani_Geyser;          size_t Ani_Geyser_len;
const uint8_t *Ani_LWall;           size_t Ani_LWall_len;
const uint8_t *Ani_Yad;             size_t Ani_Yad_len;
const uint8_t *Ani_Bump;            size_t Ani_Bump_len;
const uint8_t *SonicDynPLC;         size_t SonicDynPLC_len;

/* ---------------- Layouts de nivel (Level_*) ---------------- */
const uint8_t *Level_GHZ1;          size_t Level_GHZ1_len;
const uint8_t *Level_GHZ2;          size_t Level_GHZ2_len;
const uint8_t *Level_GHZ3;          size_t Level_GHZ3_len;
const uint8_t *Level_GHZbg;         size_t Level_GHZbg_len;
const uint8_t *Level_LZ1;           size_t Level_LZ1_len;
const uint8_t *Level_LZ2;           size_t Level_LZ2_len;
const uint8_t *Level_LZ3;           size_t Level_LZ3_len;
const uint8_t *Level_LZbg;          size_t Level_LZbg_len;
const uint8_t *Level_SBZ3;          size_t Level_SBZ3_len;
const uint8_t *Level_MZ1;           size_t Level_MZ1_len;
const uint8_t *Level_MZ1bg;         size_t Level_MZ1bg_len;
const uint8_t *Level_MZ2;           size_t Level_MZ2_len;
const uint8_t *Level_MZ2bg;         size_t Level_MZ2bg_len;
const uint8_t *Level_MZ3;           size_t Level_MZ3_len;
const uint8_t *Level_MZ3bg;         size_t Level_MZ3bg_len;
const uint8_t *Level_SLZ1;          size_t Level_SLZ1_len;
const uint8_t *Level_SLZ2;          size_t Level_SLZ2_len;
const uint8_t *Level_SLZ3;          size_t Level_SLZ3_len;
const uint8_t *Level_SLZbg;         size_t Level_SLZbg_len;
const uint8_t *Level_SYZ1;          size_t Level_SYZ1_len;
const uint8_t *Level_SYZ2;          size_t Level_SYZ2_len;
const uint8_t *Level_SYZ3;          size_t Level_SYZ3_len;
const uint8_t *Level_SYZbg;         size_t Level_SYZbg_len;
const uint8_t *Level_SBZ1;          size_t Level_SBZ1_len;
const uint8_t *Level_SBZ1bg;        size_t Level_SBZ1bg_len;
const uint8_t *Level_SBZ2;          size_t Level_SBZ2_len;
const uint8_t *Level_SBZ2bg;        size_t Level_SBZ2bg_len;
const uint8_t *Level_End;           size_t Level_End_len;
const uint8_t *SS_1;                size_t SS_1_len;
const uint8_t *SS_2;                size_t SS_2_len;
const uint8_t *SS_3;                size_t SS_3_len;
const uint8_t *SS_4;                size_t SS_4_len;
const uint8_t *SS_5;                size_t SS_5_len;
const uint8_t *SS_6;                size_t SS_6_len;

/* ---------------- Posiciones de objetos (ObjPos_*) ---------------- */
const uint8_t *ObjPos_GHZ1;         size_t ObjPos_GHZ1_len;
const uint8_t *ObjPos_GHZ2;         size_t ObjPos_GHZ2_len;
const uint8_t *ObjPos_GHZ3;         size_t ObjPos_GHZ3_len;
const uint8_t *ObjPos_LZ1;          size_t ObjPos_LZ1_len;
const uint8_t *ObjPos_LZ2;          size_t ObjPos_LZ2_len;
const uint8_t *ObjPos_LZ3;          size_t ObjPos_LZ3_len;
const uint8_t *ObjPos_SBZ3;         size_t ObjPos_SBZ3_len;
const uint8_t *ObjPos_MZ1;          size_t ObjPos_MZ1_len;
const uint8_t *ObjPos_MZ2;          size_t ObjPos_MZ2_len;
const uint8_t *ObjPos_MZ3;          size_t ObjPos_MZ3_len;
const uint8_t *ObjPos_SLZ1;         size_t ObjPos_SLZ1_len;
const uint8_t *ObjPos_SLZ2;         size_t ObjPos_SLZ2_len;
const uint8_t *ObjPos_SLZ3;         size_t ObjPos_SLZ3_len;
const uint8_t *ObjPos_SYZ1;         size_t ObjPos_SYZ1_len;
const uint8_t *ObjPos_SYZ2;         size_t ObjPos_SYZ2_len;
const uint8_t *ObjPos_SYZ3;         size_t ObjPos_SYZ3_len;
const uint8_t *ObjPos_SBZ1;         size_t ObjPos_SBZ1_len;
const uint8_t *ObjPos_SBZ2;         size_t ObjPos_SBZ2_len;
const uint8_t *ObjPos_FZ;           size_t ObjPos_FZ_len;
const uint8_t *ObjPos_End;          size_t ObjPos_End_len;

/* ---------------- Colisión ---------------- */
const uint8_t *Col_GHZ;             size_t Col_GHZ_len;
const uint8_t *Col_LZ;              size_t Col_LZ_len;
const uint8_t *Col_MZ;              size_t Col_MZ_len;
const uint8_t *Col_SLZ;             size_t Col_SLZ_len;
const uint8_t *Col_SYZ;             size_t Col_SYZ_len;
const uint8_t *Col_SBZ;             size_t Col_SBZ_len;
const uint8_t *Col_AngleMap;        size_t Col_AngleMap_len;
const uint8_t *Col_CollArray1;      size_t Col_CollArray1_len;
const uint8_t *Col_CollArray2;      size_t Col_CollArray2_len;

/* ---------------- Start locations ---------------- */
const uint8_t *StartLocArray;       size_t StartLocArray_len;
const uint8_t *EndingStLocArray;    size_t EndingStLocArray_len;
const uint8_t *SS_StartLoc;         size_t SS_StartLoc_len;

/* ============================================================================
   Macros de carga / liberación
   ========================================================================== */

/* Carga estándar: el path relativo se resuelve contra assets_base_path().
   En caso de fallo no se toca el puntero (queda NULL desde static init). */
#define LOAD(path_, name_)     load_asset((path_), &(name_), &(name_##_len))
#define LOAD_MAP(path_, name_) load_asm_asset((path_), &(name_), &(name_##_len), 1)
#define LOAD_ANIM(path_, name_)load_asm_asset((path_), &(name_), &(name_##_len), 0)

/* Liberación idempotente. */
#define FREE(name_)  do { Assets_Free((void *)(name_)); (name_) = NULL; } while (0)
#define UNMAP(name_) do { free_32bit((name_));          (name_) = NULL; } while (0)

/* ============================================================================
   Resolución del directorio assets/
   ========================================================================== */

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

/* ============================================================================
   Carga raw (blobs / paletas / .nem / .eni / .kos / .unc / .bin)
   ========================================================================== */

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

/* ============================================================================
   Data_Init
   ========================================================================== */

int Data_Init(void) {
    LOAD("palette/Sega Background.bin",                 Pal_SegaBG);
    LOAD("palette/Sega1.bin",                   Pal_Sega1);
    LOAD("palette/Sega2.bin",                   Pal_Sega2);
    LOAD("palette/Title Screen.bin",                   Pal_Title);
    LOAD("palette/Cycle - Title Screen Water.bin",             Pal_TitleCycWater);
    LOAD("palette/Cycle - GHZ.bin",               Pal_GHZCycWater);
    LOAD("palette/Level Select.bin",            Pal_LevelSel);
    LOAD("palette/Sonic.bin",                   Pal_Sonic);
    LOAD("palette/Green Hill Zone.bin",                     Pal_GHZ);
    LOAD("palette/Labyrinth Zone.bin",                      Pal_LZ);
    LOAD("palette/Labyrinth Zone Underwater.bin",           Pal_LZWater);
    LOAD("palette/Sonic - LZ Underwater.bin",     Pal_LZSonWater);
    LOAD("palette/Marble Zone.bin",                      Pal_MZ);
    LOAD("palette/Star Light Zone.bin",                     Pal_SLZ);
    LOAD("palette/Spring Yard Zone.bin",                     Pal_SYZ);
    LOAD("palette/SBZ Act 1.bin",                    Pal_SBZ1);
    LOAD("palette/SBZ Act 2.bin",                    Pal_SBZ2);
    LOAD("palette/SBZ Act 3.bin",                    Pal_SBZ3);
    LOAD("palette/SBZ Act 3 Underwater.bin",         Pal_SBZ3Water);
    LOAD("palette/Sonic - SBZ3 Underwater.bin",   Pal_SBZ3SonWat);
    LOAD("palette/Special Stage.bin",                 Pal_Special);
    LOAD("palette/Special Stage Results.bin",               Pal_SSResult);
    LOAD("palette/Special Stage Continue Bonus.bin",                Pal_Continue);
    LOAD("palette/Ending.bin",                  Pal_Ending);
    LOAD("palette/Cycle - Special Stage 1.bin",  Pal_SSCyc1);
    LOAD("palette/Cycle - Special Stage 2.bin",  Pal_SSCyc2);

    /* ---------------- Arte: Sega / Title / Misc ---------------- */
    LOAD("artnem/Sega Logo (REV00).nem",                Nem_SegaLogo);
    LOAD("artnem/Title Screen Foreground.nem",                 Nem_TitleFg);
    LOAD("artnem/Title Screen Sonic.nem",              Nem_TitleSonic);
    LOAD("artnem/Title Screen TM.nem",                 Nem_TitleTM);
    LOAD("artnem/Title Cards.nem",               Nem_TitleCard);
    LOAD("artnem/Ending - Credits.nem",              Nem_CreditText);
    LOAD("artnem/Hidden Japanese Credits.nem",              Nem_JapNames);
    LOAD("artunc/Level Select & Debug Text.unc",Art_Text);
    LOAD("tilemaps/Sega Logo (REV00).eni",              Eni_SegaLogo);
    LOAD("tilemaps/Title Screen.eni",                  Eni_Title);
    LOAD("tilemaps/Hidden Japanese Credits.eni",            Eni_JapNames);
    LOAD("tilemaps/SS Background 1.eni", Eni_SSBg1);
    LOAD("tilemaps/SS Background 2.eni", Eni_SSBg2);

    /* ---------------- Arte: Zonas (8x8) ---------------- */
    LOAD("artnem/8x8 - GHZ1.nem",         Nem_GHZ_1st);
    LOAD("artnem/8x8 - GHZ2.nem",         Nem_GHZ_2nd);
    LOAD("artnem/8x8 - LZ.nem",     Nem_LZ);
    LOAD("artnem/8x8 - MZ.nem",     Nem_MZ);
    LOAD("artnem/8x8 - SLZ.nem",    Nem_SLZ);
    LOAD("artnem/8x8 - SYZ.nem",    Nem_SYZ);
    LOAD("artnem/8x8 - SBZ.nem",    Nem_SBZ);

    /* ---------------- Arte: Objetos comunes ---------------- */
    LOAD("artnem/Rings.nem",            Nem_Ring);
    LOAD("artnem/Signpost.nem",         Nem_SignPost);
    LOAD("artnem/Hidden Bonuses.nem",     Nem_Bonus);
    LOAD("artnem/Giant Ring Flash.nem",         Nem_BigFlash);
    LOAD("artnem/Shield.nem",           Nem_Shield);
    LOAD("artnem/Invincibility Stars.nem",            Nem_Stars);
    LOAD("artnem/Spikes.nem",           Nem_Spikes);
    LOAD("artnem/Spring Horizontal.nem",          Nem_HSpring);
    LOAD("artnem/Spring Vertical.nem",          Nem_VSpring);
    LOAD("artnem/Monitors.nem",         Nem_Monitors);
    LOAD("artnem/Points.nem",           Nem_Points);
    LOAD("artnem/Prison Capsule.nem",   Nem_Prison);
    LOAD("artnem/Explosion.nem",        Nem_Explode);

    /* ---------------- Arte: Enemigos ---------------- */
    LOAD("artnem/Enemy Crabmeat.nem",                     Nem_Crabmeat);
    LOAD("artnem/Enemy Motobug.nem",                      Nem_Motobug);
    LOAD("artnem/Enemy Buzz Bomber.nem",                         Nem_Buzz);
    LOAD("artnem/Enemy Chopper.nem",                      Nem_Chopper);
    LOAD("artnem/Enemy Newtron.nem",                      Nem_Newtron);
    LOAD("artnem/Enemy Ball Hog.nem",               Nem_BallHog);
    LOAD("artnem/Enemy Basaran.nem",                Nem_Basaran);
    LOAD("artnem/Enemy Bomb.nem",                   Nem_Bomb);
    LOAD("artnem/Enemy Burrobot.nem",               Nem_Burrobot);
    LOAD("artnem/Enemy Caterkiller.nem",            Nem_Cater);
    LOAD("artnem/Enemy Jaws.nem",                   Nem_Jaws);
    LOAD("artnem/Enemy Orbinaut.nem",               Nem_Orbinaut);
    LOAD("artnem/Enemy Roller.nem",                 Nem_Roller);
    LOAD("artnem/Enemy Yadrin.nem",                 Nem_Yadrin);
    LOAD("artnem/LZ Gargoyle & Fireball.nem",       Nem_Gargoyle);
    LOAD("artnem/LZ Harpoon.nem",                   Nem_Harpoon);
    LOAD("artnem/LZ Spiked Ball & Chain.nem",       Nem_LzSpikeBall);
    LOAD("artnem/SBZ Pizza Cutter.nem",             Nem_Cutter);
    LOAD("artnem/SYZ Large Spikeball.nem",          Nem_SyzSpike1);
    LOAD("artnem/SYZ Small Spikeball.nem",          Nem_SyzSpike2);
    LOAD("artnem/SYZ Bumper.nem",                   Nem_Bumper);
    LOAD("artnem/Fireballs.nem",                    Nem_MzFire);

    /* ---------------- Arte: Jefes / objetos de zona ---------------- */
    LOAD("artnem/Boss - Main.nem",                  Nem_Eggman);
    LOAD("artnem/Boss - Weapons.nem",               Nem_Weapons);
    LOAD("artnem/Boss - Exhaust Flame.nem",         Nem_Exhaust);
    LOAD("artnem/Boss - Final Zone.nem",            Nem_FzBoss);
    LOAD("artnem/Boss - Eggman after FZ Fight.nem", Nem_FzEggman);
    LOAD("artnem/Boss - Eggman in SBZ2 & FZ.nem",   Nem_Sbz2Eggman);
    LOAD("artnem/GHZ Breakable Wall.nem",           Nem_GhzWall1);
    LOAD("artnem/GHZ Edge Wall.nem",                Nem_GhzWall2);
    LOAD("artnem/GHZ Swinging Platform.nem",        Nem_Swing);
    LOAD("artnem/GHZ Bridge.nem",                   Nem_Bridge);
    LOAD("artnem/GHZ Spiked Log.nem",               Nem_SpikePole);
    LOAD("artnem/GHZ Giant Ball.nem",               Nem_Ball);

    /* ---------------- Arte: Objetos de zona ---------------- */
    /* LZ */
    LOAD("artnem/LZ 32x32 Block.nem",         Nem_LzBlock1);
    LOAD("artnem/LZ Blocks.nem",              Nem_LzBlock2);
    LOAD("artnem/LZ 32x16 Block.nem",         Nem_LzBlock3);
    LOAD("artnem/LZ Vertical Door.nem",       Nem_LzDoor1);
    LOAD("artnem/LZ Horizontal Door.nem",     Nem_LzDoor2);
    LOAD("artnem/LZ Rising Platform.nem",     Nem_LzPlatfm);
    LOAD("artnem/LZ Breakable Pole.nem",      Nem_LzPole);
    LOAD("artnem/LZ Wheel.nem",               Nem_LzWheel);
    LOAD("artnem/Switch.nem",                 Nem_LzSwitch);
    LOAD("artnem/LZ Water & Splashes.nem",    Nem_Splash);
    LOAD("artnem/LZ Water Surface.nem",       Nem_Water);
    LOAD("artnem/LZ Flapping Door.nem",       Nem_FlapDoor);
    LOAD("artnem/LZ Bubbles & Countdown.nem", Nem_Bubbles);
    LOAD("artnem/LZ Cork.nem",                Nem_Cork);
    LOAD("artnem/Continue Screen Stuff.nem",  Nem_MiniSonic);

    /* MZ */
    LOAD("artnem/MZ Green Pushable Block.nem", Nem_MzBlock);
    LOAD("artnem/MZ Green Glass Block.nem",    Nem_MzGlass);
    LOAD("artnem/MZ Metal Blocks.nem",         Nem_MzMetal);
    LOAD("artnem/MZ Switch.nem",               Nem_MzSwitch);
    LOAD("artnem/MZ Lava.nem",                 Nem_Lava);

    /* SLZ */
    LOAD("artnem/SLZ Pylon.nem",               Nem_Pylon);
    LOAD("artnem/SLZ Seesaw.nem",              Nem_Seesaw);
    LOAD("artnem/SLZ 32x32 Block.nem",         Nem_SlzBlock);
    LOAD("artnem/SLZ Cannon.nem",              Nem_SlzCannon);
    LOAD("artnem/SLZ Little Spikeball.nem",    Nem_SlzSpike);
    LOAD("artnem/SLZ Swinging Platform.nem",   Nem_SlzSwing);
    LOAD("artnem/SLZ Breakable Wall.nem",      Nem_SlzWall);
    LOAD("artnem/SLZ Fan.nem",                 Nem_Fan);

    /* SBZ */
    LOAD("artnem/SBZ Vanishing Block.nem",     Nem_SbzBlock);
    LOAD("artnem/SBZ Small Vertical Door.nem", Nem_SbzDoor1);
    LOAD("artnem/SBZ Large Horizontal Door.nem", Nem_SbzDoor2);
    LOAD("artnem/SBZ Collapsing Floor.nem",    Nem_SbzFloor);
    LOAD("artnem/SBZ Running Disc.nem",        Nem_SbzWheel1);
    LOAD("artnem/SBZ Junction Wheel.nem",      Nem_SbzWheel2);
    LOAD("artnem/SBZ Sliding Floor Trap.nem",  Nem_SlideFloor);
    LOAD("artnem/SBZ Spinning Platform.nem",   Nem_SpinPform);
    LOAD("artnem/SBZ Stomper.nem",             Nem_Stomper);
    LOAD("artnem/SBZ Trapdoor.nem",            Nem_TrapDoor);
    LOAD("artnem/SBZ Crushing Girder.nem",     Nem_Girder);
    LOAD("artnem/SBZ Flaming Pipe.nem",        Nem_FlamePipe);
    LOAD("artnem/SBZ Electrocuter.nem",        Nem_Electric);

    /* Comunes / GHZ */
    LOAD("artnem/Lamppost.nem",                Nem_Lamp);
    LOAD("artnem/GHZ Flower Stalk.nem",               Nem_Stalk);
    LOAD("artnem/GHZ Purple Rock.nem",                Nem_PplRock);

    /* ---------------- Arte: Animales ---------------- */
    LOAD("artnem/Animal Rabbit.nem",   Nem_Rabbit);
    LOAD("artnem/Animal Chicken.nem",  Nem_Chicken);
    LOAD("artnem/Animal Penguin.nem",  Nem_Penguin);
    LOAD("artnem/Animal Seal.nem",     Nem_Seal);
    LOAD("artnem/Animal Pig.nem",      Nem_Pig);
    LOAD("artnem/Animal Flicky.nem",   Nem_Flicky);
    LOAD("artnem/Animal Squirrel.nem", Nem_Squirrel);

    /* ---------------- Arte: Special Stage ---------------- */
    LOAD("artnem/Special 1UP.nem",             Nem_SS1UpBlock);
    LOAD("artnem/Special Clouds.nem",          Nem_SSBgCloud);
    LOAD("artnem/Special Birds & Fish.nem",    Nem_SSBgFish);
    LOAD("artnem/Special Emerald Twinkle.nem", Nem_SSEmStars);
    LOAD("artnem/Special Emeralds.nem",        Nem_SSEmerald);
    LOAD("artnem/Special GOAL.nem",            Nem_SSGOAL);
    LOAD("artnem/Special Ghost.nem",           Nem_SSGhost);
    LOAD("artnem/Special Glass.nem",           Nem_SSGlass);
    LOAD("artnem/Special R.nem",               Nem_SSRBlock);
    LOAD("artnem/Special Red-White.nem",       Nem_SSRedWhite);
    LOAD("artnem/Special UP-DOWN.nem",         Nem_SSUpDown);
    LOAD("artnem/Special W.nem",               Nem_SSWBlock);
    LOAD("artnem/Special Walls.nem",           Nem_SSWalls);
    LOAD("artnem/Special ZONE1.nem",           Nem_SSZone1);
    LOAD("artnem/Special ZONE2.nem",           Nem_SSZone2);
    LOAD("artnem/Special ZONE3.nem",           Nem_SSZone3);
    LOAD("artnem/Special ZONE4.nem",           Nem_SSZone4);
    LOAD("artnem/Special ZONE5.nem",           Nem_SSZone5);
    LOAD("artnem/Special ZONE6.nem",           Nem_SSZone6);

    /* ---------------- Arte: Ending ---------------- */
    LOAD("artnem/Ending - Emeralds.nem",       Nem_EndEm);
    LOAD("artnem/Ending - Flowers.nem",        Nem_EndFlower);
    LOAD("artnem/Ending - Sonic.nem",          Nem_EndSonic);
    LOAD("artnem/Ending - StH Logo.nem",       Nem_EndStH);
    LOAD("artnem/Ending - Try Again.nem",      Nem_TryAgain);
    LOAD("artnem/Game Over.nem",               Nem_GameOver);
    LOAD("artnem/Special Result Emeralds.nem", Nem_ResultEm);

    /* ---------------- Arte: HUD ---------------- */
    LOAD("artnem/HUD.nem",                     Nem_Hud);
    LOAD("artnem/HUD - Life Counter Icon.nem",               Nem_Lives);
    LOAD("artunc/HUD Numbers.unc",             Art_Hud);
    LOAD("artunc/Lives Counter Numbers.unc",   Art_LivesNums);

    /* ---------------- Arte sin comprimir (nivel + Sonic) ---------------- */
    LOAD("artunc/Sonic.unc",                   Art_Sonic);
    LOAD("artunc/GHZ Waterfall.unc",           Art_GhzWater);
    LOAD("artunc/GHZ Flower Large.unc",        Art_GhzFlower1);
    LOAD("artunc/GHZ Flower Small.unc",        Art_GhzFlower2);
    LOAD("artunc/MZ Lava Surface.unc",         Art_MzLava1);
    LOAD("artunc/MZ Lava.unc",                 Art_MzLava2);
    LOAD("artunc/MZ Background Torch.unc",     Art_MzTorch);
    LOAD("artunc/SBZ Background Smoke.unc",    Art_SbzSmoke);
    LOAD("artunc/Giant Ring.unc",              Art_BigRing);

    /* ---------------- Tilemaps ---------------- */
    LOAD("map16/GHZ.eni",   Blk16_GHZ);
    LOAD("map16/LZ.eni",    Blk16_LZ);
    LOAD("map16/MZ.eni",    Blk16_MZ);
    LOAD("map16/SLZ.eni",   Blk16_SLZ);
    LOAD("map16/SYZ.eni",   Blk16_SYZ);
    LOAD("map16/SBZ.eni",   Blk16_SBZ);
    LOAD("map256/GHZ.kos",  Blk256_GHZ);
    LOAD("map256/LZ.kos",   Blk256_LZ);
    LOAD("map256/MZ (REV01).kos",   Blk256_MZ);
    LOAD("map256/SLZ.kos",  Blk256_SLZ);
    LOAD("map256/SYZ.kos",  Blk256_SYZ);
    LOAD("map256/SBZ (REV01).kos",  Blk256_SBZ);

    /* ---------------- Mappings ---------------- */
    LOAD_MAP("_maps/Sonic.asm",                      Map_Sonic);
    LOAD_MAP("_maps/Title Screen Sonic.asm",                 Map_TSon);
    LOAD_MAP("_maps/Press Start and TM.asm",                      Map_PSB);
    LOAD_MAP("_maps/Credits.asm",                    Map_Cred);
    LOAD_MAP("_maps/Title Cards.asm",                  Map_Card);
    /* "SONIC HAS PASSED" vive en el mismo archivo, tabla propia: */
    load_asm_asset_named("_maps/Title Cards.asm", "Map_Got",
                         &Map_Got, &Map_Got_len);
    /* "SPECIAL STAGE"/"CHAOS EMERALDS" results screen, misma tabla propia: */
    load_asm_asset_named("_maps/Title Cards.asm", "Map_SSR",
                         &Map_SSR, &Map_SSR_len);
    /* Esmeraldas de la pantalla de resultados (tabla Map_SSRC_internal): */
    load_asm_asset_named("_maps/SS Result Chaos Emeralds.asm", "Map_SSRC_internal",
                         &Map_SSRC, &Map_SSRC_len);
    LOAD_MAP("_maps/HUD.asm",                        Map_HUD);
    LOAD_MAP("_maps/Rings (REV00).asm",                      Map_Ring);
    LOAD_MAP("_maps/Signpost.asm",                   Map_Sign);
    LOAD_MAP("_maps/Prison Capsule.asm",             Map_Pri);
    LOAD_MAP("_maps/Shield and Invincibility.asm",   Map_Shield);
    LOAD_MAP("_maps/Smashable Walls.asm",            Map_Smash);
    LOAD_MAP("_maps/Spiked Pole Helix.asm",          Map_Hel);
    LOAD_MAP("_maps/Swinging Platforms (GHZ).asm",   Map_Swing_GHZ);
    LOAD_MAP("_maps/Swinging Platforms (SLZ).asm",   Map_Swing_SLZ);
    LOAD_MAP("_maps/Eggman.asm",                     Map_Eggman);
    LOAD_MAP("_maps/Boss Items.asm",                 Map_BossItems);
    LOAD_MAP("_maps/Bridge.asm",                     Map_Bri);
    LOAD_MAP("_maps/Purple Rock.asm",                Map_PRock);
    LOAD_MAP("_maps/GHZ Edge Walls.asm",             Map_Edge);
    LOAD_MAP("_maps/GHZ Ball.asm",                   Map_GBall);
    LOAD_MAP("_maps/Scenery.asm",                    Map_Scen);
    LOAD_MAP("_maps/Springs.asm",                    Map_Spring);
    LOAD_MAP("_maps/Monitor.asm",                    Map_Monitor);
    LOAD_MAP("_maps/Spikes.asm",                     Map_Spike);
    LOAD_MAP("_maps/Chopper.asm",                    Map_Chop);
    LOAD_MAP("_maps/Crabmeat.asm",                   Map_Crab);
    LOAD_MAP("_maps/Moto Bug.asm",                    Map_Moto);
    LOAD_MAP("_maps/Buzz Bomber.asm",                 Map_Buzz);
    LOAD_MAP("_maps/Buzz Bomber Missile.asm",                Map_Missile);
    LOAD_MAP("_maps/Platforms (GHZ).asm",            Map_Plat_GHZ);
    LOAD_MAP("_maps/Collapsing Ledge.asm",           Map_Ledge);
    LOAD_MAP("_maps/Collapsing Floors.asm",          Map_CFlo);
    LOAD_MAP("_maps/Explosions.asm",                 Map_ExplodeItem);
    LOAD_MAP("_maps/Explosions.asm",                 Map_ExplodeBomb);
    LOAD_MAP("_maps/Animals 1.asm",                  Map_Animal1);
    LOAD_MAP("_maps/Animals 2.asm",                  Map_Animal2);
    LOAD_MAP("_maps/Animals 3.asm",                  Map_Animal3);
    LOAD_MAP("_maps/Points.asm",                     Map_Points);
    LOAD_MAP("_maps/Newtron.asm",                    Map_Newt);
    LOAD_MAP("_maps/SS Walls.asm",   Map_SSWalls);
    LOAD_MAP("_maps/Bumper.asm",  Map_Bump);
    LOAD_MAP("_maps/SS Shared Block.asm",  Map_SS_Shared);
    LOAD_MAP("_maps/SS UP Block.asm", Map_SS_Up);
    LOAD_MAP("_maps/SS DOWN Block.asm", Map_SS_Down);
    LOAD_MAP("_maps/SS Glass Block.asm",   Map_SS_Glass);
    LOAD_MAP("_maps/SS Chaos Emeralds.asm", Map_SS_Chaos1);
    LOAD_MAP("_maps/SS Chaos Emeralds.asm", Map_SS_Chaos2);
    LOAD_MAP("_maps/SS Chaos Emeralds.asm", Map_SS_Chaos3);
    LOAD_MAP("_maps/Ring Flash.asm", Map_Flash);
    LOAD_MAP("_maps/Giant Ring.asm", Map_GRing);
    LOAD_MAP("_maps/MZ Large Green Glass Blocks.asm", Map_Glass);
    LOAD_MAP("_maps/Chained Stompers.asm", Map_CStom);
    LOAD_MAP("_maps/Button.asm", Map_But);
    LOAD_MAP("_maps/Pushable Blocks.asm", Map_Push);
    LOAD_MAP("_maps/Moving Blocks (MZ and SBZ).asm", Map_MBlock);
    LOAD_MAP("_maps/Moving Blocks (LZ).asm", Map_MBlockLZ);
    LOAD_MAP("_maps/Lava Tag.asm", Map_LTag);
    LOAD_MAP("_maps/MZ Bricks.asm", Map_Brick);
    LOAD_MAP("_maps/Basaran.asm", Map_Bas);
    LOAD_MAP("_maps/MZ Large Grassy Platforms.asm", Map_LGrass);
    LOAD_MAP("_maps/Fireballs.asm", Map_Fire);
    LOAD_MAP("_maps/Caterkiller.asm", Map_Cat);
    LOAD_MAP("_maps/Lava Geyser.asm", Map_Geyser);
    LOAD_MAP("_maps/Wall of Lava.asm", Map_LWall);
    LOAD_MAP("_maps/Smashable Green Block.asm", Map_Smab);
    LOAD_MAP("_maps/Light.asm", Map_Light);
    LOAD_MAP("_maps/Floating Blocks and Doors.asm", Map_FBlock);
    LOAD_MAP("_maps/Platforms (SYZ).asm", Map_Plat_SYZ);
    LOAD_MAP("_maps/Platforms (SLZ).asm", Map_Plat_SLZ);
    LOAD_MAP("_maps/Yadrin.asm", Map_Yad);

    /* ---------------- Animaciones ---------------- */
    LOAD_ANIM("_anim/Sonic.asm",                     Ani_Sonic);
    LOAD_ANIM("_anim/Title Screen Sonic.asm",                Ani_TSon);
    LOAD_ANIM("_anim/Press Start and TM.asm",                     Ani_PSBTM);
    LOAD_ANIM("_anim/Prison Capsule.asm",            Ani_Pri);
    LOAD_ANIM("_anim/Rings.asm",                     Ani_Ring);
    LOAD_ANIM("_anim/Signpost.asm",                  Ani_Sign);
    LOAD_ANIM("_anim/Shield and Invincibility.asm",  Ani_Shield);
    LOAD_ANIM("_anim/Crabmeat.asm",                  Ani_Crab);
    LOAD_ANIM("_anim/Moto Bug.asm",                  Ani_Moto);
    LOAD_ANIM("_anim/Buzz Bomber.asm",               Ani_Buzz);
    LOAD_ANIM("_anim/Buzz Bomber Missile.asm",       Ani_Missile);
    LOAD_ANIM("_anim/Chopper.asm",                   Ani_Chop);
    LOAD_ANIM("_anim/Eggman.asm",                    Ani_Eggman);
    LOAD_ANIM("_anim/Monitor.asm",                   Ani_Monitor);
    LOAD_ANIM("_anim/Springs.asm",                   Ani_Spring);
    LOAD_ANIM("_anim/Newtron.asm",                   Ani_Newt);
    LOAD_ANIM("_anim/Basaran.asm",                   Ani_Bas);
    LOAD_ANIM("_anim/Burning Grass.asm",             Ani_GFire);
    LOAD_ANIM("_anim/Fireballs.asm",                 Ani_Fire);
    LOAD_ANIM("_anim/Lava Geyser.asm",               Ani_Geyser);
    LOAD_ANIM("_anim/Wall of Lava.asm",              Ani_LWall);
    LOAD_ANIM("_anim/Yadrin.asm",                    Ani_Yad);
    LOAD_ANIM("_anim/Bumper.asm",                    Ani_Bump);
    /* El DPLC de Sonic no es un anim script, pero comparte el parser:  */
    LOAD_ANIM("_maps/Sonic - Dynamic Gfx Script.asm", SonicDynPLC);

    /* ---------------- Layouts de nivel ---------------- */
    LOAD("levels/ghz1.bin",     Level_GHZ1);
    LOAD("levels/ghz2.bin",     Level_GHZ2);
    LOAD("levels/ghz3.bin",     Level_GHZ3);
    LOAD("levels/ghzbg.bin",    Level_GHZbg);
    LOAD("levels/lz1.bin",      Level_LZ1);
    LOAD("levels/lz2.bin",      Level_LZ2);
    LOAD("levels/lz3.bin",      Level_LZ3);
    LOAD("levels/lzbg.bin",     Level_LZbg);
    LOAD("levels/sbz3.bin",     Level_SBZ3);
    LOAD("levels/mz1.bin",      Level_MZ1);
    LOAD("levels/mz1bg.bin",    Level_MZ1bg);
    LOAD("levels/mz2.bin",      Level_MZ2);
    LOAD("levels/mz2bg.bin",    Level_MZ2bg);
    LOAD("levels/mz3.bin",      Level_MZ3);
    LOAD("levels/mz3bg.bin",    Level_MZ3bg);
    LOAD("levels/slz1.bin",     Level_SLZ1);
    LOAD("levels/slz2.bin",     Level_SLZ2);
    LOAD("levels/slz3.bin",     Level_SLZ3);
    LOAD("levels/slzbg.bin",    Level_SLZbg);
    LOAD("levels/syz1.bin",     Level_SYZ1);
    LOAD("levels/syz2.bin",     Level_SYZ2);
    LOAD("levels/syz3.bin",     Level_SYZ3);
    LOAD("levels/syzbg (REV01).bin",    Level_SYZbg);
    LOAD("levels/sbz1.bin",     Level_SBZ1);
    LOAD("levels/sbz1bg.bin",   Level_SBZ1bg);
    LOAD("levels/sbz2.bin",     Level_SBZ2);
    LOAD("levels/sbz2bg.bin",   Level_SBZ2bg);
    LOAD("levels/ending.bin",   Level_End);
    LOAD("sslayout/1.eni", SS_1);
    LOAD("sslayout/2.eni", SS_2);
    LOAD("sslayout/3.eni", SS_3);
    LOAD("sslayout/4.eni", SS_4);
    LOAD("sslayout/5 (REV01).eni", SS_5);
    LOAD("sslayout/6 (REV01).eni", SS_6);

    /* ---------------- Posiciones de objetos ---------------- */
    LOAD("objpos/ghz1.bin",     ObjPos_GHZ1);
    LOAD("objpos/ghz2.bin",     ObjPos_GHZ2);
    LOAD("objpos/ghz3 (REV01).bin",     ObjPos_GHZ3);
    LOAD("objpos/lz1 (REV01).bin",      ObjPos_LZ1);
    LOAD("objpos/lz2.bin",      ObjPos_LZ2);
    LOAD("objpos/lz3 (REV01).bin",      ObjPos_LZ3);
    LOAD("objpos/sbz3.bin",     ObjPos_SBZ3);
    LOAD("objpos/mz1 (REV01).bin",      ObjPos_MZ1);
    LOAD("objpos/mz2.bin",      ObjPos_MZ2);
    LOAD("objpos/mz3.bin",      ObjPos_MZ3);
    LOAD("objpos/slz1.bin",     ObjPos_SLZ1);
    LOAD("objpos/slz2.bin",     ObjPos_SLZ2);
    LOAD("objpos/slz3.bin",     ObjPos_SLZ3);
    LOAD("objpos/syz1.bin",     ObjPos_SYZ1);
    LOAD("objpos/syz2.bin",     ObjPos_SYZ2);
    LOAD("objpos/syz3 (REV01).bin",     ObjPos_SYZ3);
    LOAD("objpos/sbz1 (REV01).bin",     ObjPos_SBZ1);
    LOAD("objpos/sbz2.bin",     ObjPos_SBZ2);
    LOAD("objpos/fz.bin",       ObjPos_FZ);
    LOAD("objpos/ending.bin",   ObjPos_End);

    /* ---------------- Colisión ---------------- */
    LOAD("collide/GHZ.bin",                     Col_GHZ);
    LOAD("collide/LZ.bin",                      Col_LZ);
    LOAD("collide/MZ.bin",                      Col_MZ);
    LOAD("collide/SLZ.bin",                     Col_SLZ);
    LOAD("collide/SYZ.bin",                     Col_SYZ);
    LOAD("collide/SBZ.bin",                     Col_SBZ);
    LOAD("collide/Angle Map.bin",               Col_AngleMap);
    LOAD("collide/Collision Array (Normal).bin",  Col_CollArray1);
    LOAD("collide/Collision Array (Rotated).bin", Col_CollArray2);

    /* ---------------- Start locations (concatenadas) ---------------- */
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
                    snprintf(fullpath, sizeof(fullpath), "%s/%s",
                             assets_base_path(), startloc_files[i]);
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
                snprintf(fullpath, sizeof(fullpath), "%s/%s",
                         assets_base_path(), ending_files[i]);
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

    {
        static const char * const ss_start_files[6] = {
            "startpos/Special Stages/ss1.bin",
            "startpos/Special Stages/ss2.bin",
            "startpos/Special Stages/ss3.bin",
            "startpos/Special Stages/ss4.bin",
            "startpos/Special Stages/ss5.bin",
            "startpos/Special Stages/ss6.bin",
        };
        uint8_t *buf = (uint8_t *)malloc(24);
        if (buf) {
            int ok = 1;
            for (int i = 0; i < 6; i++) {
                size_t len;
                char fullpath[PATH_MAX + 512];
                snprintf(fullpath, sizeof(fullpath), "%s/%s",
                         assets_base_path(), ss_start_files[i]);
                const uint8_t *tmp = Assets_Load(fullpath, &len);
                if (!tmp || len < 4) { ok = 0; break; }
                memcpy(buf + i * 4, tmp, 4);
                free((void *)tmp);
            }
            if (ok) { SS_StartLoc = buf; SS_StartLoc_len = 24; }
            else    { free(buf); SS_StartLoc = NULL; SS_StartLoc_len = 0; }
        }
    }

    Palette_Init();
    LevelHeaders_Init();
    return 0;
}

/* ============================================================================
   Data_Quit
   ========================================================================== */

void Data_Quit(void) {
    /* Paletas */
    FREE(Pal_SegaBG); FREE(Pal_Sega1); FREE(Pal_Sega2);
    FREE(Pal_Title); FREE(Pal_TitleCycWater); FREE(Pal_GHZCycWater);
    FREE(Pal_LevelSel); FREE(Pal_Sonic);
    FREE(Pal_GHZ); FREE(Pal_LZ); FREE(Pal_LZWater); FREE(Pal_LZSonWater);
    FREE(Pal_MZ); FREE(Pal_SLZ); FREE(Pal_SYZ);
    FREE(Pal_SBZ1); FREE(Pal_SBZ2); FREE(Pal_SBZ3);
    FREE(Pal_SBZ3Water); FREE(Pal_SBZ3SonWat);
    FREE(Pal_Special); FREE(Pal_SSResult); FREE(Pal_Continue); FREE(Pal_Ending);
    FREE(Pal_SSCyc1); FREE(Pal_SSCyc2);

    /* Sega / Title / Misc */
    FREE(Nem_SegaLogo); FREE(Nem_TitleFg); FREE(Nem_TitleSonic);
    FREE(Nem_TitleTM); FREE(Nem_TitleCard); FREE(Nem_CreditText);
    FREE(Nem_JapNames); FREE(Art_Text);
    FREE(Eni_SegaLogo); FREE(Eni_Title); FREE(Eni_JapNames);
    FREE(Eni_SSBg1); FREE(Eni_SSBg2);

    /* Zonas */
    FREE(Nem_GHZ_1st); FREE(Nem_GHZ_2nd);
    FREE(Nem_LZ); FREE(Nem_MZ); FREE(Nem_SLZ); FREE(Nem_SYZ); FREE(Nem_SBZ);

    /* Objetos comunes */
    FREE(Nem_Ring); FREE(Nem_SignPost); FREE(Nem_Bonus); FREE(Nem_BigFlash);
    FREE(Nem_Shield); FREE(Nem_Stars); FREE(Nem_Spikes);
    FREE(Nem_HSpring); FREE(Nem_VSpring); FREE(Nem_Monitors);
    FREE(Nem_Points); FREE(Nem_Prison); FREE(Nem_Explode);

    /* Enemigos */
    FREE(Nem_Crabmeat); FREE(Nem_Motobug); FREE(Nem_Buzz); FREE(Nem_Chopper);
    FREE(Nem_Newtron); FREE(Nem_BallHog); FREE(Nem_Basaran); FREE(Nem_Bomb);
    FREE(Nem_Burrobot); FREE(Nem_Cater); FREE(Nem_Jaws); FREE(Nem_Orbinaut);
    FREE(Nem_Roller); FREE(Nem_Yadrin); FREE(Nem_Gargoyle); FREE(Nem_Harpoon);
    FREE(Nem_LzSpikeBall); FREE(Nem_Cutter);
    FREE(Nem_SyzSpike1); FREE(Nem_SyzSpike2); FREE(Nem_Bumper); FREE(Nem_MzFire);

    /* Jefes / zona */
    FREE(Nem_Eggman); FREE(Nem_Weapons); FREE(Nem_Exhaust);
    FREE(Nem_FzBoss); FREE(Nem_FzEggman); FREE(Nem_Sbz2Eggman);
    FREE(Nem_GhzWall1); FREE(Nem_GhzWall2);
    FREE(Nem_Swing); FREE(Nem_Bridge); FREE(Nem_SpikePole); FREE(Nem_Ball);

    /* Objetos de zona */
    FREE(Nem_LzBlock1); FREE(Nem_LzBlock2); FREE(Nem_LzBlock3);
    FREE(Nem_LzDoor1); FREE(Nem_LzDoor2); FREE(Nem_LzPlatfm);
    FREE(Nem_LzPole); FREE(Nem_LzWheel); FREE(Nem_LzSwitch);
    FREE(Nem_Splash); FREE(Nem_Water); FREE(Nem_FlapDoor);
    FREE(Nem_Bubbles); FREE(Nem_Cork); FREE(Nem_MiniSonic);

    FREE(Nem_MzBlock); FREE(Nem_MzGlass); FREE(Nem_MzMetal);
    FREE(Nem_MzSwitch); FREE(Nem_Lava);

    FREE(Nem_Pylon); FREE(Nem_Seesaw); FREE(Nem_SlzBlock);
    FREE(Nem_SlzCannon); FREE(Nem_SlzSpike); FREE(Nem_SlzSwing);
    FREE(Nem_SlzWall); FREE(Nem_Fan);

    FREE(Nem_SbzBlock); FREE(Nem_SbzDoor1); FREE(Nem_SbzDoor2);
    FREE(Nem_SbzFloor); FREE(Nem_SbzWheel1); FREE(Nem_SbzWheel2);
    FREE(Nem_SlideFloor); FREE(Nem_SpinPform); FREE(Nem_Stomper);
    FREE(Nem_TrapDoor); FREE(Nem_Girder); FREE(Nem_FlamePipe);
    FREE(Nem_Electric);

    FREE(Nem_Lamp); FREE(Nem_Stalk); FREE(Nem_PplRock);

    /* Animales */
    FREE(Nem_Rabbit); FREE(Nem_Chicken); FREE(Nem_Penguin); FREE(Nem_Seal);
    FREE(Nem_Pig); FREE(Nem_Flicky); FREE(Nem_Squirrel);

    /* Special Stage */
    FREE(Nem_SS1UpBlock); FREE(Nem_SSBgCloud); FREE(Nem_SSBgFish);
    FREE(Nem_SSEmStars); FREE(Nem_SSEmerald); FREE(Nem_SSGOAL);
    FREE(Nem_SSGhost); FREE(Nem_SSGlass); FREE(Nem_SSRBlock);
    FREE(Nem_SSRedWhite); FREE(Nem_SSUpDown); FREE(Nem_SSWBlock);
    FREE(Nem_SSWalls);
    FREE(Nem_SSZone1); FREE(Nem_SSZone2); FREE(Nem_SSZone3);
    FREE(Nem_SSZone4); FREE(Nem_SSZone5); FREE(Nem_SSZone6);

    /* Ending */
    FREE(Nem_EndEm); FREE(Nem_EndFlower); FREE(Nem_EndSonic); FREE(Nem_EndStH);
    FREE(Nem_TryAgain); FREE(Nem_GameOver); FREE(Nem_ResultEm);

    /* HUD */
    FREE(Nem_Hud); FREE(Nem_Lives); FREE(Art_Hud); FREE(Art_LivesNums);

    /* Arte sin comprimir */
    FREE(Art_Sonic);
    FREE(Art_GhzWater); FREE(Art_GhzFlower1); FREE(Art_GhzFlower2);
    FREE(Art_MzLava1); FREE(Art_MzLava2); FREE(Art_MzTorch);
    FREE(Art_SbzSmoke); FREE(Art_BigRing);

    /* Tilemaps */
    FREE(Blk16_GHZ); FREE(Blk256_GHZ);
    FREE(Blk16_LZ);  FREE(Blk256_LZ);
    FREE(Blk16_MZ);  FREE(Blk256_MZ);
    FREE(Blk16_SLZ); FREE(Blk256_SLZ);
    FREE(Blk16_SYZ); FREE(Blk256_SYZ);
    FREE(Blk16_SBZ); FREE(Blk256_SBZ);

    /* Mappings mmap-eados */
    UNMAP(Map_Sonic); UNMAP(Map_TSon); UNMAP(Map_PSB); UNMAP(Map_Cred);
    UNMAP(Map_Card); UNMAP(Map_Got); UNMAP(Map_SSR); UNMAP(Map_SSRC); UNMAP(Map_HUD); UNMAP(Map_Ring);
    UNMAP(Map_Sign); UNMAP(Map_Pri); UNMAP(Map_Shield); UNMAP(Map_Smash);
    UNMAP(Map_Hel); UNMAP(Map_Swing_GHZ); UNMAP(Map_Swing_SLZ);
    UNMAP(Map_Eggman); UNMAP(Map_BossItems);
    UNMAP(Map_Bri); UNMAP(Map_PRock); UNMAP(Map_Edge); UNMAP(Map_GBall);
    UNMAP(Map_Scen); UNMAP(Map_Spring); UNMAP(Map_Monitor); UNMAP(Map_Spike);
    UNMAP(Map_Chop); UNMAP(Map_Crab); UNMAP(Map_Moto); UNMAP(Map_Buzz);
    UNMAP(Map_Missile); UNMAP(Map_Plat_GHZ); UNMAP(Map_Ledge); UNMAP(Map_CFlo);
    UNMAP(Map_ExplodeItem); UNMAP(Map_ExplodeBomb);
    UNMAP(Map_Animal1); UNMAP(Map_Animal2); UNMAP(Map_Animal3); UNMAP(Map_Points);
    UNMAP(Ani_Newt);
    UNMAP(Map_SSWalls); UNMAP(Map_Bump); UNMAP(Map_SS_Shared);
    UNMAP(Map_SS_Up); UNMAP(Map_SS_Down); UNMAP(Map_SS_Glass);
    UNMAP(Map_SS_Chaos1); UNMAP(Map_SS_Chaos2); UNMAP(Map_SS_Chaos3);
    UNMAP(Map_Flash); UNMAP(Map_GRing);

    /* Animaciones / DPLC */
    UNMAP(Ani_Sonic); UNMAP(Ani_TSon); UNMAP(Ani_PSBTM); UNMAP(Ani_Pri);
    UNMAP(Ani_Ring); UNMAP(Ani_Sign); UNMAP(Ani_Shield);
    UNMAP(Ani_Crab); UNMAP(Ani_Moto); UNMAP(Ani_Buzz); UNMAP(Ani_Missile);
    UNMAP(Ani_Chop); UNMAP(Ani_Eggman); UNMAP(Ani_Monitor); UNMAP(Ani_Spring);
    UNMAP(SonicDynPLC);

    /* Layouts de nivel */
    FREE(Level_GHZ1); FREE(Level_GHZ2); FREE(Level_GHZ3); FREE(Level_GHZbg);
    FREE(Level_LZ1);  FREE(Level_LZ2);  FREE(Level_LZ3);  FREE(Level_LZbg);
    FREE(Level_SBZ3);
    FREE(Level_MZ1);  FREE(Level_MZ1bg); FREE(Level_MZ2); FREE(Level_MZ2bg);
    FREE(Level_MZ3);  FREE(Level_MZ3bg);
    FREE(Level_SLZ1); FREE(Level_SLZ2);  FREE(Level_SLZ3); FREE(Level_SLZbg);
    FREE(Level_SYZ1); FREE(Level_SYZ2);  FREE(Level_SYZ3); FREE(Level_SYZbg);
    FREE(Level_SBZ1); FREE(Level_SBZ1bg);FREE(Level_SBZ2); FREE(Level_SBZ2bg);
    FREE(Level_End);
    FREE(SS_1); FREE(SS_2); FREE(SS_3); FREE(SS_4); FREE(SS_5); FREE(SS_6);

    /* ObjPos */
    FREE(ObjPos_GHZ1); FREE(ObjPos_GHZ2); FREE(ObjPos_GHZ3);
    FREE(ObjPos_LZ1);  FREE(ObjPos_LZ2);  FREE(ObjPos_LZ3); FREE(ObjPos_SBZ3);
    FREE(ObjPos_MZ1);  FREE(ObjPos_MZ2);  FREE(ObjPos_MZ3);
    FREE(ObjPos_SLZ1); FREE(ObjPos_SLZ2); FREE(ObjPos_SLZ3);
    FREE(ObjPos_SYZ1); FREE(ObjPos_SYZ2); FREE(ObjPos_SYZ3);
    FREE(ObjPos_SBZ1); FREE(ObjPos_SBZ2); FREE(ObjPos_FZ); FREE(ObjPos_End);

    /* Colisión */
    FREE(Col_GHZ); FREE(Col_LZ); FREE(Col_MZ); FREE(Col_SLZ);
    FREE(Col_SYZ); FREE(Col_SBZ);
    FREE(Col_AngleMap); FREE(Col_CollArray1); FREE(Col_CollArray2);

    /* Start locations */
    FREE(StartLocArray); FREE(EndingStLocArray); FREE(SS_StartLoc);
}

#undef FREE
#undef UNMAP
#undef LOAD
#undef LOAD_MAP
#undef LOAD_ANIM

/* ============================================================================
   Cheat data (estático)
   ========================================================================== */

const uint8_t  LevSelCode_US[] = {btnUp, btnDn, btnL, btnR, 0, 0xFF};
const uint32_t LevSelCode_US_len = 6;

const uint8_t  LevSelCode_J[]  = {btnUp, btnDn, btnL, btnR, 0, 0xFF};
const uint32_t LevSelCode_J_len = 6;

const uint16_t LevSel_Ptrs[] = {
    id_GHZ_act1, id_GHZ_act2, id_GHZ_act3,
    id_MZ_act1,  id_MZ_act2,  id_MZ_act3,
    id_SYZ_act1, id_SYZ_act2, id_SYZ_act3,
    id_LZ_act1,  id_LZ_act2,  id_LZ_act3,
    id_SLZ_act1, id_SLZ_act2, id_SLZ_act3,
    id_SBZ_act1, id_SBZ_act2,
    id_LZ_act4,             /* Scrap Brain Zone 3 */
    id_FZ,                  /* Final Zone */
    (uint16_t)(id_SS << 8), /* Special Stage (dummy) */
    0x8000                  /* Sound Test */
};
const uint32_t LevSel_Ptrs_len = sizeof(LevSel_Ptrs);

/* ============================================================================
   PARSER ASM — animaciones, mappings y DPLC
   ============================================================================ */

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
                term = term * 16 + (isdigit((unsigned char)*p) ? *p - '0'
                                    : tolower((unsigned char)*p) - 'a' + 10);
                p++;
            }
            if (end) *end = p;
        } else if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
            while (isxdigit((unsigned char)*p)) {
                term = term * 16 + (isdigit((unsigned char)*p) ? *p - '0'
                                    : tolower((unsigned char)*p) - 'a' + 10);
                p++;
            }
            if (end) *end = p;
        } else {
            static const char *af_names[] = { "afBack", "afEnd", "afChange",
                                              "afRoutine", "afReset",
                                              "af2ndRoutine", "afWait",
                                              "aniXFlip", "aniYFlip" };
            static const long  af_vals[]   = { 0xFE, 0xFF, 0xFD, 0xFC, 0xFB,
                                               0xFA, 0x80, 0x20, 0x40 };
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

static void parse_table_expr(const char *s, char *label_out, size_t label_sz,
                             int *delta_out) {
    *delta_out = 0;
    if (label_sz == 0) return;
    size_t n = 0;
    while (*s && isspace((unsigned char)*s)) s++;
    while (*s && (isalnum((unsigned char)*s) || *s == '_' || *s == '.')
           && n + 1 < label_sz) {
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

static void buf_grow(char **buf, size_t *len, size_t *cap,
                     const void *src, size_t n) {
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

static char *resolve_equ_symbols(const char *text) {
    EquSymbol syms[256];
    int sym_count = 0;

    /* Primera pasada: recolectar definiciones "nombre: equ valor" */
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

static char *expand_asm_macros(const char *text) {
    AsmMacroDef macros[32] = {0};
    int macro_count = 0;

    /* Primera pasada: registrar definiciones "<name>: macro <params>" ... "endm" */
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
        while (*q && (isalnum((unsigned char)*q) || *q == '_')
               && nn + 1 < sizeof(m->name)) {
            m->name[nn++] = *q++;
        }
        m->name[nn] = '\0';
        if (!m->name[0]) continue;

        const char *ap = ins + 5;
        m->nparams = 0;
        while (*ap && *ap != '\n') {
            while (*ap && (isspace((unsigned char)*ap) || *ap == ',')) ap++;
            if (*ap == '\0' || *ap == '\n') break;
            size_t al = 0;
            while (*ap && !isspace((unsigned char)*ap) && *ap != ','
                   && *ap != '\n' && al + 1 < 32) {
                m->params[m->nparams][al++] = *ap++;
            }
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

        /* Saltar definiciones de macro completas */
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
            while (*ap && !isspace((unsigned char)*ap) && *ap != ','
                   && *ap != '\n' && *ap != ';') {
                ap++;
            }
        }

        char call_label[64];
        size_t cn = 0;
        const char *cl = line;
        while (*cl && (isalnum((unsigned char)*cl) || *cl == '_' || *cl == '.')
               && cn + 1 < 64) {
            call_label[cn++] = *cl++;
        }
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
                    while (r < rl && (isalnum((unsigned char)*r) || *r == '_'
                                      || *r == '.')) r++;
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
                            if (ai < n && args[ai]
                                && strcmp(m->params[ai], tok) == 0) {
                                const char *ae = args[ai];
                                while (*ae && !isspace((unsigned char)*ae)
                                       && *ae != ',' && *ae != '\n'
                                       && *ae != ';') {
                                    ae++;
                                }
                                rep = args[ai];
                                rpl = (size_t)(ae - args[ai]);
                                break;
                            }
                        }
                    }
                    if (rep) buf_grow(&out, &out_len, &out_cap, rep, rpl);
                    else     buf_grow(&out, &out_len, &out_cap, tok, tl);
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

static uint8_t *parse_anim_asm(const char *text, size_t text_len,
                               size_t *out_len) {
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

        if (is_directive(ins, "dc.w") ||
            is_directive(ins, "mappingsTableEntry.w")) {
            const char *dp = ins;
            if (is_directive(ins, "dc.w")) dp += 4;
            else dp += strlen("mappingsTableEntry.w");
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (table_count < 128) {
                parse_table_expr(dp, table_name[table_count], 64,
                                 &table_delta[table_count]);
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
                    while (*q && (isalnum((unsigned char)*q) || *q == '_'
                                  || *q == '.') && nn + 1 < 64) {
                        segs[seg_count].name[nn++] = *q++;
                    }
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

    size_t total = 2 * (size_t)table_count;
    size_t seg_pos[128];
    int ref_idx[128];
    size_t cursor = total;

    for (int j = 0; j < 128; j++) seg_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < 128; j++) {
            if (strcmp(segs[j].name, table_name[k]) == 0) { idx = j; break; }
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

    *out_len = cursor;
    if (cursor == 0) { free(resolved); free(expanded); return NULL; }

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) { *out_len = 0; free(resolved); free(expanded); return NULL; }

    for (int k = 0; k < table_count; k++) {
        size_t off = (ref_idx[k] >= 0)
                     ? seg_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < 128; j++) {
        if (segs[j].bytes && segs[j].len)
            memcpy(out + seg_pos[j], segs[j].bytes, segs[j].len);
    }

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

static uint8_t *parse_map_asm(const char *text, size_t text_len,
                              size_t *out_len) {
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
                parse_table_expr(dp, table_name[table_count], 64,
                                 &table_delta[table_count]);
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
                    while (*q && (isalnum((unsigned char)*q) || *q == '_'
                                  || *q == '.') && nn + 1 < 64) {
                        frames[frame_count].name[nn++] = *q++;
                    }
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

            const uint8_t *np = (const uint8_t *)realloc(
                fr->pieces, (fr->count + 1) * 5);
            if (!np) continue;
            fr->pieces = np;
            memcpy(fr->pieces + fr->count * 5, piece, 5);
            fr->count++;
        }
    }

    size_t total = 2 * (size_t)table_count;
    size_t frame_pos[256];
    int ref_idx[256];
    size_t cursor = total;

    for (int j = 0; j < frame_count; j++) frame_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < frame_count; j++) {
            if (strcmp(frames[j].name, table_name[k]) == 0) { idx = j; break; }
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
        size_t off = (ref_idx[k] >= 0)
                     ? frame_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
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
            while (*q && (isalnum((unsigned char)*q) || *q == '_'
                          || *q == '.') && nn + 1 < 64) {
                lab[nn++] = *q++;
            }
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
                    while (*q && (isalnum((unsigned char)*q) || *q == '_'
                                  || *q == '.') && nn + 1 < 64) {
                        frames[frame_count].name[nn++] = *q++;
                    }
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

            const uint8_t *np = (const uint8_t *)realloc(
                fr->pieces, (fr->count + 1) * 5);
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
                     ? frame_pos[ref_idx[k]] + (size_t)ent_delta[sel[k]] : 0;
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
static uint8_t *parse_plc_asm(const char *text, size_t text_len,
                              size_t *out_len) {
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
                parse_table_expr(dp, table_name[table_count], 64,
                                 &table_delta[table_count]);
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
                    while (*q && (isalnum((unsigned char)*q) || *q == '_'
                                  || *q == '.') && nn + 1 < 64) {
                        sg->name[nn++] = *q++;
                    }
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

            uint8_t *nb = (uint8_t *)realloc(segs[cur].bytes,
                                             segs[cur].len + 2);
            if (!nb) break;
            segs[cur].bytes = nb;
            unsigned entry = (unsigned)(((tiles - 1) & 0xF) << 12)
                             | (unsigned)(offset & 0xFFF);
            segs[cur].bytes[segs[cur].len]     = (uint8_t)(entry >> 8);
            segs[cur].bytes[segs[cur].len + 1] = (uint8_t)(entry & 0xFF);
            segs[cur].len += 2;
        }
    }

    size_t total = 2 * (size_t)table_count;
    size_t seg_pos[128];
    int ref_idx[128];
    size_t cursor = total;

    for (int j = 0; j < 128; j++) seg_pos[j] = (size_t)-1;
    for (int k = 0; k < table_count; k++) {
        int idx = -1;
        for (int j = 0; j < seg_count; j++) {
            if (strcmp(segs[j].name, table_name[k]) == 0) { idx = j; break; }
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
        size_t off = (ref_idx[k] >= 0)
                     ? seg_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
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

/* ============================================================================
   Carga de ASM assets: parsea, copia a memoria 32-bit y registra la longitud
   ========================================================================== */

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

static int load_asm_asset(const char *name, const uint8_t **out_ptr,
                          size_t *out_len, int is_map) {
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
