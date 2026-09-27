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

/* NUEVO: paleta submarina activa (paralela a palette_main). Se usa por
   scanline en VDP_RenderFrame para dibujar con la paleta de agua. */
extern uint16_t palette_water_main[PALETTE_TOTAL];

/* Load a palette from raw data (big-endian 9-bit MD colors) */
void Palette_LoadFromData(const uint8_t *data, uint16_t *dest, int count);

/* Load a palette by ID into v_palette (active palette in RAM) */
void PalLoad(int index);

/* Initialize palette index from loaded assets */
void Palette_Init(void);

/* Load a palette by ID into v_palette_fading (fade buffer) */
void PalLoad_Fade(int index);

/* NUEVO: variantes "water" — mismas entradas de pal_index pero el destino es
   v_palette_water (activa) o v_palette_water_fading (fade buffer). */
void PalLoad_Water(int index);
void PalLoad_Fade_Water(int index);

/* NUEVO: copia v_palette_water -> palette_water_main (rendering buffer). */
void Palette_Water_Update(void);

/* Palette fading - fade active palette (v_palette) to black over 22 frames */
void Palette_FadeOut(void);

/* Palette fading - fade active palette in from v_palette_fading */
void Palette_FadeIn(void);

/* Palette fading - fade active palette (v_palette) to white over 22 frames */
void PaletteWhiteIn(void);
void PaletteWhiteOut(void);

/* Brighten/darken the palette one step toward/from white */
void WhiteIn_FromWhite(void);
void WhiteOut_ToWhite(void);

/* Sega screen palette cycling - returns nonzero while active, 0 when done */
int PalCycle_Sega(void);

/* Title screen palette cycling */
void PalCycle_Title(void);

/* Update palette_main from v_palette: called during VBlank transfers */
void Palette_Update(void);

#endif /* SONIC1_PALETTE_H */