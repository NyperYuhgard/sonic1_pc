#ifndef SONIC1_DATA_H
#define SONIC1_DATA_H

#include "types.h"

/* Sega Screen assets (loaded at runtime from assets/) */
extern uint8_t *Pal_SegaBG;
extern size_t   Pal_SegaBG_len;

extern uint8_t *Pal_Sega1;
extern size_t   Pal_Sega1_len;

extern uint8_t *Pal_Sega2;
extern size_t   Pal_Sega2_len;

extern uint8_t *Nem_SegaLogo;
extern size_t   Nem_SegaLogo_len;

extern uint8_t *Eni_SegaLogo;
extern size_t   Eni_SegaLogo_len;

/* Palette index IDs (from Palette Index.asm) */
#define palid_SegaBG    0
#define palid_Title     1
#define palid_LevelSel  2
#define palid_Sonic     3

/* Title Screen assets */
extern uint8_t *Pal_Title;
extern size_t   Pal_Title_len;

extern uint8_t *Pal_LevelSel;
extern size_t   Pal_LevelSel_len;

extern uint8_t *Pal_Sonic;
extern size_t   Pal_Sonic_len;

extern uint8_t *Nem_JapNames;
extern size_t   Nem_JapNames_len;

extern uint8_t *Eni_JapNames;
extern size_t   Eni_JapNames_len;

extern uint8_t *Nem_CreditText;
extern size_t   Nem_CreditText_len;

extern uint8_t *Nem_TitleFg;
extern size_t   Nem_TitleFg_len;

extern uint8_t *Nem_TitleSonic;
extern size_t   Nem_TitleSonic_len;

extern uint8_t *Nem_TitleTM;
extern size_t   Nem_TitleTM_len;

extern uint8_t *Art_Text;
extern size_t   Art_Text_len;

extern uint8_t *Blk16_GHZ;
extern size_t   Blk16_GHZ_len;

extern uint8_t *Blk256_GHZ;
extern size_t   Blk256_GHZ_len;

extern uint8_t *Eni_Title;
extern size_t   Eni_Title_len;

extern uint8_t *Nem_GHZ_1st;
extern size_t   Nem_GHZ_1st_len;

/* Cheat codes */
extern const uint8_t LevSelCode_US[];
extern const uint32_t LevSelCode_US_len;

extern const uint8_t LevSelCode_J[];
extern const uint32_t LevSelCode_J_len;

extern const uint16_t LevSel_Ptrs[];
extern const uint32_t LevSel_Ptrs_len;

/* Initialize all assets from the assets/ directory.
   Returns 0 on success, -1 on any failure. */
int Data_Init(void);

/* Free all loaded assets */
void Data_Quit(void);

#endif /* SONIC1_DATA_H */
