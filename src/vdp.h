#ifndef SONIC1_VDP_H
#define SONIC1_VDP_H

#include "types.h"

struct SDL_Renderer;
struct SDL_Texture;

/* VDP memory */
#define VRAM_SIZE   0x10000  /* 64KB */
#define CRAM_SIZE   64       /* 64 colors */
#define VSRAM_SIZE  64       /* 64 entries */

/* VDP state */
typedef struct {
    uint8_t  vram[VRAM_SIZE];
    uint16_t cram[CRAM_SIZE];
    uint16_t vsram[VSRAM_SIZE];
    uint16_t registers[24];  /* VDP registers $80-$97 */
    uint16_t status;         /* VDP status register */
    uint32_t vdp_cmd;        /* pending VDP command */

    /* Rendering output */
    struct SDL_Texture *framebuffer;
} VDP_State;

extern VDP_State vdp;

void VDP_Init(void);
void VDP_Reset(void);
void VDP_RenderFrame(struct SDL_Renderer *renderer);

/* DMA / memory transfers */
void VDP_WriteVRAM(const uint8_t *src, uint32_t vram_addr, uint32_t len);
void VDP_WriteCRAM(const uint8_t *src, uint32_t cram_addr, uint32_t len);
void VDP_FillVRAM(uint8_t byte, uint32_t vram_addr, uint32_t len);

/* VDP register access */
void VDP_SetRegister(uint8_t reg, uint16_t value);

/* Screen operations */
void VDP_ClearScreen(void);
void VDP_CopyTilemapToVRAM(const uint16_t *source, uint32_t vram_dest,
                            int width, int height);

/* Palette transfer: v_palette (RAM) -> palette_main (rendering) */
void VDP_TransferPalette(void);

/* Save the last rendered framebuffer to a PPM file (debug only) */
void VDP_SaveScreenshot(const char *path);

/* Debug/test overlay: when >= 0, draw a counter bar in VDP_RenderFrame.
   Set to -1 to disable (normal operation). */
extern int vdp_test_counter;

/* Helper: convert MD 9-bit color to SDL 32-bit RGBA */
uint32_t MD_ColorToRGBA(uint16_t md_color);

#endif /* SONIC1_VDP_H */
