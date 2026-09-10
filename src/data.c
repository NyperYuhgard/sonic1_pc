#include "data.h"
#include "assets.h"
#include "palette.h"
#include "constants.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>

/* Object mapping pointers are stored in 32-bit fields (obMap), so mapping
   data must live below 4 GB. MAP_32BIT asks the kernel for such an address. */
static const uint8_t *alloc_32bit(size_t n) {
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    return (p == MAP_FAILED) ? NULL : (const uint8_t *)p;
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

const uint8_t *Pal_LevelSel = NULL;
size_t   Pal_LevelSel_len = 0;

const uint8_t *Pal_Sonic = NULL;
size_t   Pal_Sonic_len = 0;

const uint8_t *Pal_GHZ = NULL;
size_t   Pal_GHZ_len = 0;

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

const uint8_t *Eni_Title = NULL;
size_t   Eni_Title_len = 0;

const uint8_t *Nem_GHZ_1st = NULL;
size_t   Nem_GHZ_1st_len = 0;

const uint8_t *Level_GHZ1 = NULL;
size_t   Level_GHZ1_len = 0;

const uint8_t *Level_GHZbg = NULL;
size_t   Level_GHZbg_len = 0;

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

const uint8_t *Map_Sonic = NULL;
size_t   Map_Sonic_len = 0;

const uint8_t *Art_Sonic = NULL;
size_t   Art_Sonic_len = 0;
const uint8_t *SonicDynPLC = NULL;
size_t   SonicDynPLC_len = 0;
const uint8_t *Ani_Sonic = NULL;
size_t   Ani_Sonic_len = 0;

const uint8_t *ObjPos_GHZ1 = NULL;
size_t   ObjPos_GHZ1_len = 0;

const uint8_t *Nem_Ring = NULL;
size_t   Nem_Ring_len = 0;

const uint8_t *Map_Ring = NULL;
size_t   Map_Ring_len = 0;

const uint8_t *Ani_Ring = NULL;
size_t   Ani_Ring_len = 0;

/* Object mappings referenced by the DebugMode item lists.
   Most are unstaged (NULL) until their maps/*.asm assets are ported. */
const uint8_t *Map_Monitor = NULL;
const uint8_t *Map_Crab = NULL;
const uint8_t *Map_Buzz = NULL;
const uint8_t *Map_Chop = NULL;
const uint8_t *Map_Spike = NULL;
const uint8_t *Map_Plat_GHZ = NULL;
const uint8_t *Map_PRock = NULL;
const uint8_t *Map_Moto = NULL;
const uint8_t *Map_Spring = NULL;
const uint8_t *Map_Newt = NULL;
const uint8_t *Map_Edge = NULL;
const uint8_t *Map_GBall = NULL;
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
const uint8_t *Map_Animal2 = NULL;
const uint8_t *Map_Animal3 = NULL;

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
   NULL pointer) until their collide/*.bin lands. */
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
    Nem_TitleCard = NULL;
    Nem_TitleCard_len = 0;
    Map_Card = NULL;
    Map_Card_len = 0;

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

    if (load_asm_asset("anim/titlesonic.asm", &Ani_TSon, &Ani_TSon_len, 0) != 0) {
        Ani_TSon = NULL;
        Ani_TSon_len = 0;
    }

    if (load_asm_asset("anim/psbtm.asm", &Ani_PSBTM, &Ani_PSBTM_len, 0) != 0) {
        Ani_PSBTM = NULL;
        Ani_PSBTM_len = 0;
    }

    if (load_asm_asset("maps/titlesonic.asm", &Map_TSon, &Map_TSon_len, 1) != 0) {
        Map_TSon = NULL;
        Map_TSon_len = 0;
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

    if (load_asset("levels/ghz1.bin", &Level_GHZ1, &Level_GHZ1_len) != 0) {
        Level_GHZ1 = NULL;
        Level_GHZ1_len = 0;
    }

    if (load_asset("levels/ghzbg.bin", &Level_GHZbg, &Level_GHZbg_len) != 0) {
        Level_GHZbg = NULL;
        Level_GHZbg_len = 0;
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

    if (load_asset("objpos/ghz1.bin", &ObjPos_GHZ1, &ObjPos_GHZ1_len) != 0) {
        ObjPos_GHZ1 = NULL;
        ObjPos_GHZ1_len = 0;
    }

    if (load_asset("collide/GHZ.bin", &Col_GHZ, &Col_GHZ_len) != 0) {
        Col_GHZ = NULL;
        Col_GHZ_len = 0;
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
            NULL, NULL, NULL, NULL,
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
                    const uint8_t *tmp = Assets_Load(startloc_files[i], &len);
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
            "startpos/Credits Demos/ghz1 (Credits demo 2).bin",
            "startpos/Credits Demos/lz3 (Credits demo).bin",
            "startpos/Credits Demos/mz2 (Credits demo).bin",
            "startpos/Credits Demos/sbz1 (Credits demo).bin",
            "startpos/Credits Demos/sbz2 (Credits demo).bin",
            "startpos/Credits Demos/slz3 (Credits demo).bin",
            "startpos/Credits Demos/syz3 (Credits demo).bin",
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
                const uint8_t *tmp = Assets_Load(ending_files[i], &len);
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
    return 0;
}

void Data_Quit(void) {
#define FREE_ASSET(p) do { Assets_Free(p); p = NULL; } while(0)
    FREE_ASSET(Pal_SegaBG);
    FREE_ASSET(Pal_Sega1);
    FREE_ASSET(Pal_Sega2);
    FREE_ASSET(Nem_SegaLogo);
    FREE_ASSET(Eni_SegaLogo);
    FREE_ASSET(Pal_Title);
    FREE_ASSET(Pal_TitleCycWater);
    FREE_ASSET(Pal_LevelSel);
    FREE_ASSET(Pal_Sonic);
    FREE_ASSET(Pal_GHZ);
    FREE_ASSET(Nem_JapNames);
    FREE_ASSET(Eni_JapNames);
    FREE_ASSET(Nem_CreditText);
    FREE_ASSET(Nem_TitleFg);
    FREE_ASSET(Nem_TitleSonic);
    FREE_ASSET(Nem_TitleTM);
    FREE_ASSET(Art_Text);
    FREE_ASSET(Blk16_GHZ);
    FREE_ASSET(Blk256_GHZ);
    FREE_ASSET(Eni_Title);
    FREE_ASSET(Nem_GHZ_1st);
    FREE_ASSET(Nem_GHZ_2nd);
    FREE_ASSET(Nem_Stalk);
    FREE_ASSET(Nem_PplRock);
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
    FREE_ASSET(Nem_TitleCard);
    FREE_ASSET(Map_Card);
    FREE_ASSET(Map_Sonic);
    FREE_ASSET(Art_Sonic);
    FREE_ASSET(SonicDynPLC);
    FREE_ASSET(Ani_Sonic);
    FREE_ASSET(Nem_Hud);
    FREE_ASSET(Nem_Lives);
    FREE_ASSET(Art_Hud);
    FREE_ASSET(Art_LivesNums);
    FREE_ASSET(Map_HUD);
    FREE_ASSET(Nem_Ring);
    FREE_ASSET(Map_Ring);
    FREE_ASSET(Ani_Ring);
    FREE_ASSET(ObjPos_GHZ1);
    FREE_ASSET(Col_GHZ);
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
  0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
  0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00,
  0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700,
  0x1800, 0x1900, 0x1A00, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x1F00,
  0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700,
  0x2800, 0x2900, 0x2A00, 0x2B00, 0x2C00, 0x2D00, 0x2E00, 0x2F00,
  0x3000, 0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700,
  0x3800, 0x3900, 0x3A00, 0x3B00, 0x3C00, 0x3D00, 0x3E00, 0x3F00,
  0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4700,
  0x4800, 0x4900, 0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00,
  0x5000, 0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5600, 0x5700,
  0x5800, 0x5900, 0x5A00, 0x5B00, 0x5C00, 0x5D00, 0x5E00, 0x5F00,
  0x6000, 0x6100, 0x6200, 0x6300, 0x6400, 0x6500, 0x6600, 0x6700,
  0x6800, 0x6900, 0x6A00, 0x6B00, 0x6C00, 0x6D00, 0x6E00, 0x6F00,
  0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500, 0x7600, 0x7700,
  0x7800, 0x7900, 0x7A00, 0x7B00, 0x7C00, 0x7D00, 0x7E00, 0x7F00,
  0x8000 | id_SS, 0x8100 | id_SS
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

/* ===========================================================================
   Title screen sprite mappings (from _maps slash .asm)
   These are loaded at runtime from assets/
   =========================================================================== */

const uint8_t *Map_TSon = NULL;
size_t   Map_TSon_len = 0;

const uint8_t *Map_PSB = NULL;
size_t   Map_PSB_len = 0;

const uint8_t *Map_Cred = NULL;
size_t   Map_Cred_len = 0;

const uint8_t *Nem_TitleCard = NULL;
size_t   Nem_TitleCard_len = 0;

/* Zone title card sprite mappings */
const uint8_t *Map_Card = NULL;
size_t   Map_Card_len = 0;

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
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '-') {
        const char *e2 = NULL;
        long neg = parse_asm_number(p + 1, &e2);
        if (end) *end = e2;
        return -neg;
    }
    if (*p == '$') {
        p++;
        long val = 0;
        while (isxdigit((unsigned char)*p)) {
            val = val * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
            p++;
        }
        if (end) *end = p;
        return val;
    }
    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        long val = 0;
        while (isxdigit((unsigned char)*p)) {
            val = val * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
            p++;
        }
        if (end) *end = p;
        return val;
    }
    /* Symbolic animation flags used in dc.b lines (afBack, afEnd, ...) */
    static const char *af_names[] = { "afBack", "afEnd", "afChange",
                                      "afRoutine", "afReset", "af2ndRoutine", "afWait" };
    static const long  af_vals[]   = { 0xFE, 0xFF, 0xFD, 0xFC, 0xFB, 0xFA, 0x80 };
    const char *sym = p;
    while (*sym && isalnum((unsigned char)*sym)) sym++;
    size_t sym_len = (size_t)(sym - p);
    for (int i = 0; i < 7; i++) {
        size_t n = strlen(af_names[i]);
        if (sym_len == n && strncmp(p, af_names[i], n) == 0) {
            if (end) *end = sym;
            return af_vals[i];
        }
    }
    char *ep = NULL;
    long val = strtol(p, &ep, 10);
    if (ep && end) *end = ep;
    return val;
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

static uint8_t *parse_anim_asm(const char *text, size_t text_len, size_t *out_len) {
    (void)text_len;
    const char *p = text;
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
        if (*ins == '\0') continue;

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

    *out_len = cursor;
    if (cursor == 0) return NULL;

    uint8_t *out = (uint8_t *)calloc(1, cursor);
    if (!out) { *out_len = 0; return NULL; }

    for (int k = 0; k < table_count; k++) {
        size_t off = (ref_idx[k] >= 0) ? seg_pos[ref_idx[k]] + (size_t)table_delta[k] : 0;
        out[2 * k]     = (uint8_t)(off & 0xFF);
        out[2 * k + 1] = (uint8_t)((off >> 8) & 0xFF);
    }

    for (int j = 0; j < 128; j++)
        memcpy(out + seg_pos[j], segs[j].bytes, segs[j].len);

    for (int j = 0; j < 128; j++) free(segs[j].bytes);
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
    MapFrame frames[32] = {0};
    int frame_count = 0;
    char table_name[32][64];
    int table_delta[32];
    int table_count = 0;

    while (*p) {
        p = skip_comments_and_spaces(p);
        if (*p == '\0') break;

        const char *lp = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        const char *ins = strip_label(lp);
        if (*ins == '\0') continue;

        if (is_directive(ins, "mappingsTableEntry.w")) {
            const char *dp = ins + 20;
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (table_count < 32) {
                parse_table_expr(dp, table_name[table_count], 64, &table_delta[table_count]);
                table_count++;
            }
            continue;
        }

        if (is_directive(ins, "spriteHeader")) {
            if (frame_count < 32) {
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
    size_t frame_pos[32];
    int ref_idx[32];
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
        memcpy(out + frame_pos[j] + 1, fr->pieces, fr->count * 5);
    }

    for (int j = 0; j < frame_count; j++) free(frames[j].pieces);
    return out;
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
    free(data);

    *out_ptr = low;
    *out_len = data_len;
    return 0;
}

