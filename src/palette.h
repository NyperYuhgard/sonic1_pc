#ifndef SONIC1_PALETTE_H
#define SONIC1_PALETTE_H

#include "types.h"

/* Palette buffer: 4 lines of 16 colors each = 64 colors */
#define PALETTE_LINE_SIZE  16
#define PALETTE_NUM_LINES  4
#define PALETTE_TOTAL      (PALETTE_LINE_SIZE * PALETTE_NUM_LINES)

/* Palette as MD 9-bit colors (stored in cram format) */
extern uint16_t palette_main[PALETTE_TOTAL];
extern uint16_t palette_water[PALETTE_TOTAL];
extern uint16_t palette_fading[PALETTE_TOTAL];

/* Load a palette from raw data (big-endian 9-bit MD colors) */
void Palette_LoadFromData(const uint8_t *data, uint16_t *dest, int count);

/* Load a palette by ID into v_palette (active palette in RAM) */
void PalLoad(int index);

/* Initialize palette index from loaded assets */
void Palette_Init(void);

/* Load a palette by ID into v_palette_fading (fade buffer) */
void PalLoad_Fade(int index);

/* Palette fading - fade active palette (v_palette) to black over 22 frames */
void Palette_FadeOut(void);

/* Palette fading - fade active palette in from v_palette_fading */
void Palette_FadeIn(void);

/* Sega screen palette cycling - returns nonzero while active, 0 when done */
int PalCycle_Sega(void);

/* Title screen palette cycling - stub */
void PalCycle_Title(void);

/* Update palette_main from v_palette: called during VBlank transfers */
void Palette_Update(void);

#endif /* SONIC1_PALETTE_H */
