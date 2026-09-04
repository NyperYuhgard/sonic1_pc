#include "data.h"
#include "assets.h"
#include "palette.h"
#include "constants.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
   Asset pointers
   ============================================================================ */
uint8_t *Pal_SegaBG = NULL;
size_t   Pal_SegaBG_len = 0;

uint8_t *Pal_Sega1 = NULL;
size_t   Pal_Sega1_len = 0;

uint8_t *Pal_Sega2 = NULL;
size_t   Pal_Sega2_len = 0;

uint8_t *Nem_SegaLogo = NULL;
size_t   Nem_SegaLogo_len = 0;

uint8_t *Eni_SegaLogo = NULL;
size_t   Eni_SegaLogo_len = 0;

uint8_t *Pal_Title = NULL;
size_t   Pal_Title_len = 0;

uint8_t *Pal_LevelSel = NULL;
size_t   Pal_LevelSel_len = 0;

uint8_t *Pal_Sonic = NULL;
size_t   Pal_Sonic_len = 0;

uint8_t *Nem_JapNames = NULL;
size_t   Nem_JapNames_len = 0;

uint8_t *Eni_JapNames = NULL;
size_t   Eni_JapNames_len = 0;

uint8_t *Nem_CreditText = NULL;
size_t   Nem_CreditText_len = 0;

uint8_t *Nem_TitleFg = NULL;
size_t   Nem_TitleFg_len = 0;

uint8_t *Nem_TitleSonic = NULL;
size_t   Nem_TitleSonic_len = 0;

uint8_t *Nem_TitleTM = NULL;
size_t   Nem_TitleTM_len = 0;

uint8_t *Art_Text = NULL;
size_t   Art_Text_len = 0;

uint8_t *Blk16_GHZ = NULL;
size_t   Blk16_GHZ_len = 0;

uint8_t *Blk256_GHZ = NULL;
size_t   Blk256_GHZ_len = 0;

uint8_t *Eni_Title = NULL;
size_t   Eni_Title_len = 0;

uint8_t *Nem_GHZ_1st = NULL;
size_t   Nem_GHZ_1st_len = 0;

/* ============================================================================
   Asset loading
   ============================================================================ */

static int load_asset(const char *name, uint8_t **out_ptr, size_t *out_len) {
    char path[512];
    snprintf(path, sizeof(path), "./assets/%s", name);
    uint8_t *buf = Assets_Load(path, out_len);
    if (!buf) {
        fprintf(stderr, "[Data] Failed to load asset: %s\n", path);
        return -1;
    }
    *out_ptr = buf;
    return 0;
}

int Data_Init(void) {
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
    Pal_LevelSel = NULL;
    Pal_LevelSel_len = 0;
    Pal_Sonic = NULL;
    Pal_Sonic_len = 0;
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

    if (load_asset("palette/level_select.bin", &Pal_LevelSel, &Pal_LevelSel_len) != 0) {
        Pal_LevelSel = NULL;
        Pal_LevelSel_len = 0;
    }

    if (load_asset("palette/sonic.bin", &Pal_Sonic, &Pal_Sonic_len) != 0) {
        Pal_Sonic = NULL;
        Pal_Sonic_len = 0;
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
    FREE_ASSET(Pal_LevelSel);
    FREE_ASSET(Pal_Sonic);
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
