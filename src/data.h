#ifndef SONIC1_DATA_H
#define SONIC1_DATA_H

#include "types.h"

/* Sega Screen assets */
extern const uint8_t Pal_SegaBG[];
extern const uint32_t Pal_SegaBG_len;

extern const uint8_t Pal_Sega1[];
extern const uint32_t Pal_Sega1_len;

extern const uint8_t Pal_Sega2[];
extern const uint32_t Pal_Sega2_len;

extern const uint8_t Nem_SegaLogo[];
extern const uint32_t Nem_SegaLogo_len;

extern const uint8_t Eni_SegaLogo[];
extern const uint32_t Eni_SegaLogo_len;

/* Palette index IDs (from Palette Index.asm) */
#define palid_SegaBG    0
#define palid_Title     1
#define palid_LevelSel  2
#define palid_Sonic     3

#endif /* SONIC1_DATA_H */
