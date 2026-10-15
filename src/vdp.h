#ifndef SONIC1_VDP_H
#define SONIC1_VDP_H

#include "types.h"

struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;

/* VDP memory */
#define VRAM_SIZE   0x10000  /* 64KB per bank (identical to MD) */
#define CRAM_SIZE   64       /* 64 colors */
#define VSRAM_SIZE  64       /* 64 entries */

/* ===================================================================
   VRAM BANKS
   -------------------------------------------------------------------
   El VRAM se divide en bancos de 64 KB (idénticos al VRAM del MD).
   Bank 0 es el visible para toda la semántica MD (nametables, sprite
   table, hscroll). Banks 1..N-1 son VRAM extendida para uso del código
   moderno (sprites HD, overlays, arte adicional).

   Cambiar VRAM_BANK_COUNT reconfigura automáticamente:
     - el tamaño de vram_ext[]
     - la cantidad de pestañas del viewer
     - el layout del viewer (auto-shrink de tab width)
     - el manejo de teclas/click

   No hay ningún otro lugar donde tocar el número de bancos.
   =================================================================== */
#define VRAM_BANK_SIZE   0x10000                    /* 64 KB por banco (MD) */
#define VRAM_BANK_COUNT  4                          /* <-- único punto de config */
#define VRAM_TOTAL_SIZE  (VRAM_BANK_SIZE * (size_t)VRAM_BANK_COUNT)

/* VDP state */
typedef struct {
    uint8_t  vram[VRAM_BANK_SIZE];     /* bank 0: MD-visible */
    uint8_t *vram_ext;                 /* banks 1..N-1 (NULL when VRAM_BANK_COUNT==1) */
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
void VDP_ApplyWidescreen(int new_w);

/* ===================================================================
   VRAM BANKS: accesores y banco de escritura "actual"
   -------------------------------------------------------------------
   Los writers públicos (VDP_WriteVRAM, VDP_FillVRAM,
   VDP_CopyTilemapToVRAM, NemDecToVRAM) escriben en el banco que
   devuelve VDP_GetWriteBank() (por defecto 0 = MD-visible).

   Para redirigir temporalmente:

       VDP_SetWriteBank(2);
       NemDecToVRAM(art, 0);
       VDP_SetWriteBank(0);   // o el que estuviera antes

   Para forzar un banco sin cambiar el estado global, usa las
   variantes *_Bank, que reciben el banco explícito.
   =================================================================== */

/* Banco de escritura actual (default 0). Los writers públicos lo
   consultan; las variantes *_Bank lo ignoran. */
extern int g_vdp_write_bank;

/* Cambia el banco de escritura actual. Clampea a [0, VRAM_BANK_COUNT-1]. */
void VDP_SetWriteBank(int bank);
int  VDP_GetWriteBank(void);

/* Accesores por banco. bank 0 = vdp.vram; bank >= 1 = vram_ext.
   Clampea a [0, VRAM_BANK_COUNT-1]. Nunca devuelve NULL. */
uint8_t       *VDP_VRAM_BankW(int bank);
const uint8_t *VDP_VRAM_Bank (int bank);

/* Escritura directa a un tile (32 bytes) en un banco específico. */
void VDP_WriteTile(int bank, int tile_index, const uint8_t tile[32]);

/* --- Writers con banco explícito (ignoran g_vdp_write_bank) --- */
void VDP_WriteVRAM_Bank       (int bank, const uint8_t *src,
                               uint32_t vram_addr, uint32_t len);
void VDP_FillVRAM_Bank        (int bank, uint8_t byte,
                               uint32_t vram_addr, uint32_t len);
void VDP_CopyTilemapToVRAM_Bank(int bank, const uint16_t *source,
                               uint32_t vram_dest, int width, int height);

/* --- Writers públicos (usan g_vdp_write_bank) --- */
void VDP_WriteVRAM       (const uint8_t *src, uint32_t vram_addr, uint32_t len);
void VDP_WriteCRAM       (const uint8_t *src, uint32_t cram_addr, uint32_t len);
void VDP_FillVRAM        (uint8_t byte, uint32_t vram_addr, uint32_t len);
void VDP_CopyTilemapToVRAM(const uint16_t *source, uint32_t vram_dest,
                            int width, int height);

/* VDP register access */
void VDP_SetRegister(uint8_t reg, uint16_t value);

/* Screen operations (siempre tocan bank 0: nametables + sprite table) */
void VDP_ClearScreen(void);

/* Palette transfer: v_palette (RAM) -> palette_main (rendering) */
void VDP_TransferPalette(void);

/* Save the last rendered framebuffer to a PPM file (debug only) */
void VDP_SaveScreenshot(const char *path);

/* Debug VRAM viewer: toggle a second SDL window that renders the whole
   VRAM tile sheet (tiles $000-$7FF) + a CRAM strip, refreshed every
   frame. Call once per key press (down-edge). */
void VDP_ToggleVRAMViewer(void);

/* SDL window ID of the open VRAM viewer (-1 when closed). */
int VDP_ViewerWindowID(void);

/* Pasa eventos SDL al viewer (close, teclado, click en pestañas). Llamar
   desde el bucle de SDL_PollEvent en main.c. No-op si el viewer está
   cerrado. */
void VDP_ViewerHandleEvent(const union SDL_Event *ev);

/* Number of rows (in tiles) of a plane from register $10
   (plane_b: 0 = plane A, 1 = plane B). */
int VDP_PlaneRows(int plane_b);

/* Debug/test overlay: when >= 0, draw a counter bar in VDP_RenderFrame.
   Set to -1 to disable (normal operation). */
extern int vdp_test_counter;
extern int g_render_w; extern int g_render_left;
/* Helper: convert MD 9-bit color to SDL 32-bit RGBA */
uint32_t MD_ColorToRGBA(uint16_t md_color);

#endif /* SONIC1_VDP_H */
