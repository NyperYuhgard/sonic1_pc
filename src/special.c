#include "special.h"
#include "ram.h"
#include "constants.h"
#include "data.h"
#include "vdp.h"
#include "palette.h"
#include "objects.h"
#include "sprites.h"
#include "sound.h"
#include "decomp.h"
#include "plc.h"
#include "collision.h"
#include "public.h"
#include "level.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ===========================================================================
 *  Special Stage
 *  Ported from _inc/SpecCode.asm (REV01, FixBugs=0)
 * =========================================================================== */
// Helpers
/* ---------------------------------------------------------------------------
 *  SS_SpriteSetting: entrada de v_ss_spritesettings (8 bytes / entry)
 *    offset 0-3: mappings pointer (nuestro host order LE)
 *    offset 4:   pad (0)
 *    offset 5:   frame byte (0-15)
 *    offset 6-7: vram word (palette | art tile)
 *  El ASM lo almacena como [frame][map_hi][map_mid][map_lo][0][frame][vram_hi][vram_lo]
 *  pero nosotros no necesitamos el truco del frame-in-top-byte porque nuestros
 *  punteros son de 32 bits completos.
 * --------------------------------------------------------------------------- */
typedef struct {
    uint32_t mappings;
    uint8_t  pad;
    uint8_t  frame;
    uint16_t vram;
} SS_SpriteSetting;

/* SS_MapIndex: tabla de definiciones de bloques (78 entradas, IDs $01-$4E).
 * El índice en esta tabla corresponde a (block_id - 1). */
typedef struct {
    uint8_t         frame;
    const uint8_t  *mappings;
    uint16_t        palette;   /* Tile_Pal1..Tile_Pal4 */
    uint16_t        art_tile;  /* ArtTile_SS_* */
} SS_MapIndexEntry;

/* Macro para reducir el ruido de la tabla. Las 9 variantes de cada color
 * son idénticas salvo por el frame (que aquí dejamos en 0 porque SS_AnimateBlocks
 * lo sobreescribe cada frame). */
#define SS_WALLS(pal)   { 0, NULL, pal, ArtTile_SS_Wall }
#define SS_WALLS_NULL   { 0, NULL, 0, 0 }   /* placeholder para rellenar después */

static const SS_MapIndexEntry SS_MapIndex_Table[78] = {
    /* 0x01-0x09: WallBlue_0..8 */
    SS_WALLS(Tile_Pal1), SS_WALLS(Tile_Pal1), SS_WALLS(Tile_Pal1),
    SS_WALLS(Tile_Pal1), SS_WALLS(Tile_Pal1), SS_WALLS(Tile_Pal1),
    SS_WALLS(Tile_Pal1), SS_WALLS(Tile_Pal1), SS_WALLS(Tile_Pal1),
    /* 0x0A-0x12: WallYellow_0..8 */
    SS_WALLS(Tile_Pal2), SS_WALLS(Tile_Pal2), SS_WALLS(Tile_Pal2),
    SS_WALLS(Tile_Pal2), SS_WALLS(Tile_Pal2), SS_WALLS(Tile_Pal2),
    SS_WALLS(Tile_Pal2), SS_WALLS(Tile_Pal2), SS_WALLS(Tile_Pal2),
    /* 0x13-0x1B: WallPink_0..8 */
    SS_WALLS(Tile_Pal3), SS_WALLS(Tile_Pal3), SS_WALLS(Tile_Pal3),
    SS_WALLS(Tile_Pal3), SS_WALLS(Tile_Pal3), SS_WALLS(Tile_Pal3),
    SS_WALLS(Tile_Pal3), SS_WALLS(Tile_Pal3), SS_WALLS(Tile_Pal3),
    /* 0x1C-0x24: WallGreen_0..8 */
    SS_WALLS(Tile_Pal4), SS_WALLS(Tile_Pal4), SS_WALLS(Tile_Pal4),
    SS_WALLS(Tile_Pal4), SS_WALLS(Tile_Pal4), SS_WALLS(Tile_Pal4),
    SS_WALLS(Tile_Pal4), SS_WALLS(Tile_Pal4), SS_WALLS(Tile_Pal4),
    /* 0x25-0x2C: Bumper, W, GOAL, 1Up, UP, DOWN, R, RedWhite */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Bumper         },  /* 0x25 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_W_Block        },  /* 0x26 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Goal           },  /* 0x27 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Extra_Life     },  /* 0x28 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Up_Down        },  /* 0x29 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Up_Down        },  /* 0x2A */
    { 0, NULL, Tile_Pal2, ArtTile_SS_R_Block        },  /* 0x2B */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Red_White_Block},  /* 0x2C */
    /* 0x2D-0x30: Glass1..4 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Glass          },  /* 0x2D */
    { 0, NULL, Tile_Pal4, ArtTile_SS_Glass          },  /* 0x2E */
    { 0, NULL, Tile_Pal2, ArtTile_SS_Glass          },  /* 0x2F */
    { 0, NULL, Tile_Pal3, ArtTile_SS_Glass          },  /* 0x30 */
    /* 0x31-0x33: R_Ani, Bumper_Ani1, Bumper_Ani2 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_R_Block        },  /* 0x31 */
    { 1, NULL, Tile_Pal1, ArtTile_SS_Bumper         },  /* 0x32 */
    { 2, NULL, Tile_Pal1, ArtTile_SS_Bumper         },  /* 0x33 */
    /* 0x34-0x39: ZONE1..6 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Zone_1         },  /* 0x34 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Zone_2         },  /* 0x35 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Zone_3         },  /* 0x36 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Zone_4         },  /* 0x37 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Zone_5         },  /* 0x38 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Zone_6         },  /* 0x39 */
    /* 0x3A-0x40: Ring, Emerald1..6 */
    { 0, NULL, Tile_Pal2, ArtTile_Ring              },  /* 0x3A */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Emerald        },  /* 0x3B */
    { 0, NULL, Tile_Pal2, ArtTile_SS_Emerald        },  /* 0x3C */
    { 0, NULL, Tile_Pal3, ArtTile_SS_Emerald        },  /* 0x3D */
    { 0, NULL, Tile_Pal4, ArtTile_SS_Emerald        },  /* 0x3E */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Emerald        },  /* 0x3F */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Emerald        },  /* 0x40 */
    /* 0x41: Ghost */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Ghost_Block    },  /* 0x41 */
    /* 0x42-0x45: Ring_Ani1..4 (frame 4..7) */
    { 4, NULL, Tile_Pal2, ArtTile_Ring              },  /* 0x42 */
    { 5, NULL, Tile_Pal2, ArtTile_Ring              },  /* 0x43 */
    { 6, NULL, Tile_Pal2, ArtTile_Ring              },  /* 0x44 */
    { 7, NULL, Tile_Pal2, ArtTile_Ring              },  /* 0x45 */
    /* 0x46-0x49: Emerald_Ani1..4 (frame 0..3) */
    { 0, NULL, Tile_Pal2, ArtTile_SS_Emerald_Sparkle},  /* 0x46 */
    { 1, NULL, Tile_Pal2, ArtTile_SS_Emerald_Sparkle},  /* 0x47 */
    { 2, NULL, Tile_Pal2, ArtTile_SS_Emerald_Sparkle},  /* 0x48 */
    { 3, NULL, Tile_Pal2, ArtTile_SS_Emerald_Sparkle},  /* 0x49 */
    /* 0x4A: InvGhostTrigger */
    { 2, NULL, Tile_Pal1, ArtTile_SS_Ghost_Block    },  /* 0x4A */
    /* 0x4B-0x4E: Glass_Ani1..4 */
    { 0, NULL, Tile_Pal1, ArtTile_SS_Glass          },  /* 0x4B */
    { 0, NULL, Tile_Pal4, ArtTile_SS_Glass          },  /* 0x4C */
    { 0, NULL, Tile_Pal2, ArtTile_SS_Glass          },  /* 0x4D */
    { 0, NULL, Tile_Pal3, ArtTile_SS_Glass          },  /* 0x4E */
};

#undef SS_WALLS

/* Cachea la tabla con los mappings resueltos. Se llama una vez. */
static SS_MapIndexEntry g_ss_mapindex[78];
static int g_ss_mapindex_init = 0;

static const uint8_t *SS_MappingsForBlock(uint8_t bid)
{
    if (bid >= 0x01 && bid <= 0x24)             return Map_SSWalls;
    if (bid == id_SS_Bumper)                    return Map_Bump;
    if (bid == id_SS_Bumper_Ani1)               return Map_Bump;
    if (bid == id_SS_Bumper_Ani2)               return Map_Bump;
    if (bid == id_SS_W)                         return Map_SS_Shared;
    if (bid == id_SS_GOAL)                      return Map_SS_Shared;
    if (bid == id_SS_1Up)                       return Map_SS_Shared;
    if (bid == id_SS_UP)                        return Map_SS_Up;
    if (bid == id_SS_DOWN)                      return Map_SS_Down;
    if (bid == id_SS_R)                         return Map_SS_Shared;
    if (bid == id_SS_R_Ani)                     return Map_SS_Shared;
    if (bid == id_SS_RedWhite)                  return Map_SS_Glass;
    if (bid >= id_SS_Glass1_Blue
        && bid <= id_SS_Glass4_Pink)               return Map_SS_Glass;
    if (bid >= id_SS_ZONE1 && bid <= id_SS_ZONE6) return Map_SS_Shared;
    if (bid == id_SS_Ring)                      return Map_Ring;
    if (bid >= id_SS_Emerald1_Blue
        && bid <= id_SS_Emerald4_Green)            return Map_SS_Chaos3;
    if (bid == id_SS_Emerald5_Red)              return Map_SS_Chaos1;
    if (bid == id_SS_Emerald6_Grey)             return Map_SS_Chaos2;
    if (bid == id_SS_Ghost)                     return Map_SS_Shared;
    if (bid == id_SS_InvGhostTrigger)           return Map_SS_Shared;
    if (bid >= id_SS_Ring_Ani1
        && bid <= id_SS_Ring_Ani4)                 return Map_Ring;
    if (bid >= id_SS_Emerald_Ani1
        && bid <= id_SS_Emerald_Ani4)              return Map_SS_Glass;
    if (bid >= id_SS_Glass_Ani1
        && bid <= id_SS_Glass_Ani4)                return Map_SS_Glass;
    return NULL;
}

static void SS_MapIndex_Init(void)
{
    if (g_ss_mapindex_init) return;
    g_ss_mapindex_init = 1;
    for (int i = 0; i < 78; i++) {
        SS_MapIndexEntry e = SS_MapIndex_Table[i];
        e.mappings = SS_MappingsForBlock((uint8_t)(i + 1));
        g_ss_mapindex[i] = e;
    }
}

static const SS_MapIndexEntry *SS_MapIndex_Get(void)
{
    SS_MapIndex_Init();
    return g_ss_mapindex;
}

/* ===========================================================================
 *  SS_Load: carga el layout del próximo SS no completado y los settings.
 *  Portado de SS_Load (SpecCode.asm).
 * =========================================================================== */
void SS_Load(void) {
    uint8_t d0 = v_lastspecial;
    v_lastspecial++;
    if (v_lastspecial >= ss_emeralds_num) v_lastspecial = 0;

retry:
    if (v_emeralds != ss_emeralds_num) {
        uint8_t d1 = v_emeralds;
        if (d1 > 0) {
            d1--;
            const uint8_t *emld = RAM_ADDR(v_emldlist);
            for (int i = 0; i <= d1; i++) {
                if (emld[i] == d0) {
                    d0 = v_lastspecial;
                    v_lastspecial++;
                    if (v_lastspecial >= ss_emeralds_num) v_lastspecial = 0;
                    goto retry;
                }
            }
        }
    }

    /* Start position para Sonic */
    if (SS_StartLoc && d0 < 6) {
        uint8_t *player = RAM_ADDR(v_player);
        obX(player) = (int16_t)((SS_StartLoc[d0*4] << 8) | SS_StartLoc[d0*4+1]);
        obY(player) = (int16_t)((SS_StartLoc[d0*4+2] << 8) | SS_StartLoc[d0*4+3]);
    }

    

    /* EniDec del layout a v_sslayout_decompress (=$3020) */
    const uint8_t *const layouts[6] = { SS_1, SS_2, SS_3, SS_4, SS_5, SS_6 };
    if (d0 < 6 && layouts[d0]) {
        EniDec(layouts[d0], (uint16_t *)RAM_ADDR(v_sslayout_decompress), 0);
    } else {
        memset(RAM_ADDR(v_sslayout_decompress), 0, 0x2000);
    }

    /* Limpiar v_sslayout_base..v_sslayout_decompress (todo el buffer) */
    {
        uint8_t *p = RAM_ADDR(v_sslayout_base);
        int count = (v_sslayout_decompress - v_sslayout_base) / 4;
        for (int i = 0; i < count; i++) {
            p[0] = p[1] = p[2] = p[3] = 0;
            p += 4;
        }
    }

    /* Copiar decompress -> actual, con padding de $40 bytes por fila.
     * El 68000 almacena cada palabra de EniDec como [hi, lo]; este host es
     * little-endian, así que en memoria las palabras quedan [lo, hi].
     * Como el layout se consume como FLUJO DE BYTES (1 byte = 1 celda),
     * desempaquetamos cada palabra en orden 68000 para replicar el stream:
     * bytes [hi, lo] por palabra. (Vería huecos entre piezas si se copiara
     * la memoria cruda del host.) */
    {
        const uint16_t *src = (const uint16_t *)RAM_ADDR(v_sslayout_decompress);
        uint8_t *dst = RAM_ADDR(v_sslayout_actual);
        int rows = (v_sslayout_end - v_sslayout_actual) / ss_layout_rowlength;
        int words_per_row = (ss_layout_rowlength / 2) / 2;
        for (int r = 0; r < rows; r++) {
            for (int i = 0; i < words_per_row; i++) {
                uint16_t w = src[i];
                dst[i * 2]     = (uint8_t)(w >> 8);   /* high byte, primero */
                dst[i * 2 + 1] = (uint8_t)(w & 0xFF); /* low byte  */
            }
            src += words_per_row;
            dst += ss_layout_rowlength;
        }
    }

    /* Cargar SS_MapIndex a v_ss_spritesettings (offset +8, skip blank).
     * v_ss_spritesettings[0] queda en 0 (blank block). */
    {
        const SS_MapIndexEntry *tbl = SS_MapIndex_Get();
        uint8_t *settings = RAM_ADDR(v_ss_spritesettings);
        memset(settings, 0, 8 * 79);

        for (int i = 0; i < 78; i++) {
            SS_SpriteSetting s;
            s.mappings = (uint32_t)(uintptr_t)tbl[i].mappings;
            s.pad      = 0;
            s.frame    = tbl[i].frame;
            s.vram     = (uint16_t)(tbl[i].palette | tbl[i].art_tile);
            memcpy(settings + (i + 1) * 8, &s, 8);
        }
    }

    /* Limpiar la cola de animaciones */
    memset(RAM_ADDR(v_ss_animations), 0,
           v_ss_animations_end - v_ss_animations);
}

/* ===========================================================================
 *  SS_BGLoad: carga los tilemaps del fondo a VRAM.
 *  Portado de SS_BGLoad (SpecCode.asm).
 * =========================================================================== */
void SS_BGLoad(void) {
    /* --- Birds & Fish ---
       Eni_SSBg1 produce 8 tilemaps de 8x8 celdas (una por canvas + checkerboard).
       Buffer layout:
         +0x000: checkerboard    (canvas 6, celda par)
         +0x080: canvas d7=6     (bird frame 1)
         +0x100: canvas d7=5     (bird frame 2)
         ...
         +0x380: canvas d7=0     (fish frame 6)
    */
    if (Eni_SSBg1) {
        static uint16_t bg_buf[1024];   /* 8 canvases × 64 celdas */
        EniDec(Eni_SSBg1, bg_buf, ArtTile_SS_Background_Fish | Tile_Pal3);

        uint32_t vram_addr = ArtTile_SS_Plane_1 * tile_size + 0x1000;

        /* ASM: lea (v_ram_start + 8*8*2).l,a2
           a2 apunta al canvas 1 (se salta el checkerboard en +0x000). */
        const uint16_t *a2 = bg_buf + 64;

        for (int d7 = 7 - 1; d7 >= 0; d7--) {
            uint32_t d0 = vram_addr;

            /* ASM: cmpi.w #4-1,d7 / bhs.s .loop_rows
               d4 = 0 si d7 >= 3 (bird), d4 = 1 si d7 < 3 (fish). */
            int d4 = (d7 >= 4 - 1) ? 0 : 1;

            /* 4 filas visibles */
            for (int d6 = 4 - 1; d6 >= 0; d6--) {
                /* 8 celdas por fila */
                for (int d5 = 8 - 1; d5 >= 0; d5--) {
                    const uint16_t *a1 = a2;   /* ASM: movea.l a2,a1 */

                    d4 ^= 1;                    /* ASM: eori.b #1,d4 */
                    if (d4 != 0) {
                        /* .is_birdfish: dibuja el animal actual */
                        VDP_CopyTilemapToVRAM(a1, d0, 8, 8);
                    } else if (d7 == 7 - 1) {
                        /* ASM: cmpi.w #7-1,d7 / bne.s .skip
                                 lea (v_ram_start).l,a1 (checkerboard) */
                        VDP_CopyTilemapToVRAM(bg_buf, d0, 8, 8);
                    }
                    /* else: skip (blank) */

                    /* ASM: addi.l #(8*2)<<16,d0  → +$10 bytes */
                    d0 += 8 * 2;
                }

                /* ASM: addi.l #((8-1)*$80)<<16,d0  → +$380 bytes */
                d0 += (8 - 1) * 0x80;

                /* ASM: eori.b #1,d4  (stagger para la próxima fila) */
                d4 ^= 1;
            }

            /* ASM: addi.l #$1000<<16,d3  → siguiente canvas en VRAM */
            vram_addr += 0x1000;

            /* ASM: adda.w #8*8*2,a2  → siguiente canvas en RAM */
            a2 += 8 * 8;
        }
    }

    /* --- Clouds & Bubbles --- (igual que antes) */
    if (Eni_SSBg2) {
        static uint16_t cloud_buf[64 * 64];
        EniDec(Eni_SSBg2, cloud_buf, ArtTile_SS_Background_Clouds | Tile_Pal3);
        VDP_CopyTilemapToVRAM(cloud_buf,
                              ArtTile_SS_Plane_5 * tile_size,        64, 32);
        VDP_CopyTilemapToVRAM(cloud_buf,
                              ArtTile_SS_Plane_5 * tile_size + 0x1000, 64, 64);
    }
    
}

/* ===========================================================================
 *  PalCycle_SS — cicla la paleta del SS y cambia el canvas del fondo.
 *  Portado de PalCycle_SS + PalCycle_SS_2 (SpecCode.asm).
 * =========================================================================== */

/* SS_Timing_Values: 32 entradas × 4 bytes.
   { time, anim, vram_byte, flags }
   flags bit 7 = usepalcycle2, bit 0 = extrapalline4. */
static const uint8_t SS_Timing_Values[32][4] = {
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x12|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x10|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x0E|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x0C|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x0A|0x80|0x01 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x00|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x02|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x04|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x06|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_6*32)>>13, 0x08|0x80 },
    { 8-1, 0x08, (ArtTile_SS_Plane_6*32)>>13, 0x00 },
    { 8-1, 0x0A, (ArtTile_SS_Plane_6*32)>>13, 0x0C },
    { 0-1, 0x0C, (ArtTile_SS_Plane_6*32)>>13, 0x18 },
    { 0-1, 0x0C, (ArtTile_SS_Plane_6*32)>>13, 0x18 },
    { 8-1, 0x0A, (ArtTile_SS_Plane_6*32)>>13, 0x0C },
    { 8-1, 0x08, (ArtTile_SS_Plane_6*32)>>13, 0x00 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x08|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x06|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x04|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x02|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x00|0x80|0x01 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x0A|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x0C|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x0E|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x10|0x80 },
    { 4-1, 0x00, (ArtTile_SS_Plane_5*32)>>13, 0x12|0x80 },
    { 8-1, 0x02, (ArtTile_SS_Plane_5*32)>>13, 0x24 },
    { 8-1, 0x04, (ArtTile_SS_Plane_5*32)>>13, 0x30 },
    { 0-1, 0x06, (ArtTile_SS_Plane_5*32)>>13, 0x3C },
    { 0-1, 0x06, (ArtTile_SS_Plane_5*32)>>13, 0x3C },
    { 8-1, 0x04, (ArtTile_SS_Plane_5*32)>>13, 0x30 },
    { 8-1, 0x02, (ArtTile_SS_Plane_5*32)>>13, 0x24 },
};

/* SS_BG_Modes: { vram_bits, yscroll } */
static const uint8_t SS_BG_Modes[8][2] = {
    { (ArtTile_SS_Plane_1*32)>>10, 1 },  /* 0 - grid */
    { (ArtTile_SS_Plane_2*32)>>10, 0 },  /* 2 */
    { (ArtTile_SS_Plane_2*32)>>10, 1 },  /* 4 */
    { (ArtTile_SS_Plane_3*32)>>10, 0 },  /* 6 */
    { (ArtTile_SS_Plane_3*32)>>10, 1 },  /* 8 */
    { (ArtTile_SS_Plane_4*32)>>10, 0 },  /* A */
    { (ArtTile_SS_Plane_4*32)>>10, 1 },  /* C */
    { 0, 0 },
};

void PalCycle_SS(void) {
    if (f_pause) return;

    v_palss_time--;
    if ((int16_t)v_palss_time >= 0) return;

    uint16_t d0 = v_palss_num;
    v_palss_num++;
    d0 &= 0x1F;
    d0 <<= 2;
    const uint8_t *a0 = SS_Timing_Values[d0 >> 2];

    /* Time */
    {
        int8_t t = (int8_t)a0[0];
        v_palss_time = (t < 0) ? 0x1FF : (uint8_t)t;
    }

    /* Anim / BG mode */
    uint8_t anim = a0[1];
    v_ssbganim = anim;
    const uint8_t *a1 = SS_BG_Modes[anim >> 1];

    /* FG VRAM register $02: bits 6-7 = FG nametable */
    VDP_SetRegister(0x02, (uint16_t)a1[0]);
    /* Y scroll en VSRAM */
    v_scrposy_vdp = (uint16_t)(a1[1] << 8);
    //VDP_SetVSRAM(0, (uint16_t)v_scrposy_vdp);
    /* BG VRAM register $04 */
    VDP_SetRegister(0x04, (uint16_t)a0[2]);

    /* Palette cycle */
    uint8_t pal_off = a0[3];
    if (pal_off & 0x80) {
        /* Ramo PalCycle_SS_2 */
        if (!Pal_SSCyc2) return;
        uint16_t idx = v_palss_index;
        if (pal_off < (0x80 | 0x0A)) idx += 1;
        idx *= 0x2A;
        if (idx + 0x2A > Pal_SSCyc2_len) return;
        const uint8_t *cyc = Pal_SSCyc2 + idx;
        uint8_t sub = pal_off & 0x7F;
        int extra = sub & 1;
        sub &= 0xFE;
        if (extra) {
            uint16_t *p4 = (uint16_t *)RAM_ADDR(v_palette_line_4 + 0x0E);
            p4[0] = (uint16_t)((cyc[0] << 8) | cyc[1]);
            p4[1] = (uint16_t)((cyc[4] << 8) | cyc[5]);
            p4[2] = (uint16_t)((cyc[8] << 8) | cyc[9]);
        }
        cyc += 0x0C;
        uint16_t *dst;
        if (sub < 0x0A) {
            dst = (uint16_t *)RAM_ADDR(v_palette_line_3 + 0x1A);
        } else {
            sub -= 0x0A;
            dst = (uint16_t *)RAM_ADDR(v_palette_line_4 + 0x1A);
        }
        uint16_t off = sub * 3;
        dst[0] = (uint16_t)((cyc[off+0] << 8) | cyc[off+1]);
        dst[1] = (uint16_t)((cyc[off+2] << 8) | cyc[off+3]);
        dst[2] = (uint16_t)((cyc[off+4] << 8) | cyc[off+5]);
    } else {
        /* Pal_SSCyc1 path: 12 bytes a v_palette_line_3+$E */
        if (!Pal_SSCyc1 || Pal_SSCyc1_len < 12) return;
        memcpy(RAM_ADDR(v_palette_line_3 + 0x0E), Pal_SSCyc1, 12);
    }
}

/* ===========================================================================
 *  SS_BGAnimate — anima bubbles/clouds y actualiza la hscroll table.
 *  Portado de SS_BGAnimate (SpecCode.asm).
 * =========================================================================== */

static const uint8_t SS_Bubble_ScrollBlocks[11] = {
    10-1, 0x28, 0x18, 0x10, 0x28, 0x18, 0x10, 0x30, 0x18, 8, 0x10
};
static const uint8_t SS_Cloud_ScrollBlocks[8] = {
    7-1, 0x30, 0x30, 0x30, 0x28, 0x18, 0x18, 0x18
};
static const int8_t SS_Bubble_WobbleData[20] = {
    8, 2,   4, -1,   2, 3,   8, -1,   4, 2,
    2, 3,   8, -3,   4, 2,   2, 3,    2, -1
};

void SS_BGAnimate(void) {
    /* ASM: move.w (v_ssbganim).w,d0 / bne.s .not_0 */
    uint16_t d0 = v_ssbganim;

    if (d0 == 0) {
        v_bgscreenposy   = 0;
        v_bgscrposy_vdp  = 0;
    }

    const uint8_t *a2;
    const uint8_t *a3;

    /* ASM: cmpi.w #8,d0 / bhs.s SS_BGBirdCloud */
    if (d0 >= 8) {
        /* ---- SS_BGBirdCloud ---- */

        /* ASM: cmpi.w #$C,d0 / bne.s .not_C */
        if (d0 == 0x0C) {
            v_bg3screenposx--;
            uint32_t *clouds = (uint32_t *)RAM_ADDR(v_ss_scroll_clouds);
            uint32_t dd = 0x18000;
            for (int i = 0; i < 7; i++) {
                clouds[i] -= dd;
                dd        -= 0x2000;
            }
        }

        a2 = SS_Cloud_ScrollBlocks;
        a3 = (const uint8_t *)RAM_ADDR(v_ss_scroll_clouds);
    } else {
        /* ---- SS_BGWobble ---- */

        /* ASM: cmpi.w #6,d0 / bne.s .not_6 */
        if (d0 == 6) {
            v_bg3screenposx++;
            v_bgscreenposy++;
            v_bgscrposy_vdp = (uint16_t)v_bgscreenposy;
        }

        /* ASM: SS_BGWobbleLoop */
        int8_t *buf = (int8_t *)RAM_ADDR(v_ss_scroll_bubbles);
        for (int i = 0; i < 10; i++) {
            int16_t *w0 = (int16_t *)(buf + i * 4);       /* amplitude*sin */
            int16_t *w1 = (int16_t *)(buf + i * 4 + 2);   /* accumulated angle */
            int16_t s0, s1;
            CalcSine((uint8_t)*w1, &s0, &s1);              /* sin=s0, cos=s1 */
            int8_t amp  = SS_Bubble_WobbleData[i * 2];
            int8_t offv = SS_Bubble_WobbleData[i * 2 + 1];
            int32_t prod = (int32_t)amp * s0;
            *w0 = (int16_t)(prod >> 8);
            *w1 = (int16_t)(*w1 + offv);
        }

        a2 = SS_Bubble_ScrollBlocks;
        a3 = (const uint8_t *)RAM_ADDR(v_ss_scroll_bubbles);
    }

    /* ---- SS_Scroll_CloudsBubbles (tail común) ----
       ASM:
           lea (v_hscrolltablebuffer).w,a1
           move.w (v_bg3screenposx).w,d0
           neg.w d0
           swap d0                 ; d0 = (-bg3x << 16) | 0
           ...
           .loop_line:
               move.l d0,(a1,d2.w) ; escribe AMBOS words (plane A = -bg3x, plane B = word0)
               addq.w #4,d2
               andi.w #$3FC,d2
               dbf d1,.loop_line
    */
    uint8_t *a1 = RAM_ADDR(v_hscrolltablebuffer);
    int16_t bg3x_neg = -(int16_t)v_bg3screenposx;
    int d2_off = (-(int16_t)v_bgscreenposy) & 0xFF;
    d2_off <<= 2;                        /* byte offset = row * 4 */

    int block_count = *a2++;
    for (int b = 0; b <= block_count; b++) {
        /* ASM: move.w (a3)+,d0 ; addq.w #2,a3 */
        uint16_t word0 = *(const uint16_t *)a3;
        a3 += 4;

        int line_count = *a2++;
        for (int l = 0; l < line_count; l++) {
            /* Escribe el par de words (little-endian en el host) */
            int16_t *row = (int16_t *)(a1 + d2_off);
            row[0] = bg3x_neg;              /* plane A / FG scroll */
            row[1] = (int16_t)word0;        /* plane B / BG scroll */

            d2_off = (d2_off + 4) & 0x3FC;
        }
    }
}

/* ===========================================================================
 *  SS_AnimateBlocks — rota walls, anima rings/glass/etc, y actualiza
 *  los VRAM settings de las paredes.
 *  Portado de SS_AnimateBlocks (SpecCode.asm).
 * =========================================================================== */
void SS_AnimateBlocks(void) {
    SS_SpriteSetting *settings = (SS_SpriteSetting *)RAM_ADDR(v_ss_spritesettings);

    /* Rotate square walls: escribe el frame = (angle / 4) & 0xF en todas
       las entradas de wall (1..36). */
    {
        uint8_t d0 = (v_ssangle >> 10) & 0xF;
        for (int i = 1; i <= 36; i++) {
            settings[i].frame = d0;
        }
    }

    /* Rings (frame 0..3) */
    {
        v_ani1_time--;
        if ((int8_t)v_ani1_time < 0) {
            v_ani1_time = 8 - 1;
            v_ani1_frame = (v_ani1_frame + 1) & 3;
        }
        settings[id_SS_Ring].frame = v_ani1_frame;
    }

    /* Varios bloques alternados (frame 0..1) */
    {
        v_ani2_time--;
        if ((int8_t)v_ani2_time < 0) {
            v_ani2_time = 8 - 1;
            v_ani2_frame = (v_ani2_frame + 1) & 1;
        }
        uint8_t f = v_ani2_frame;
        settings[id_SS_GOAL].frame           = f;
        settings[id_SS_RedWhite].frame       = f;
        settings[id_SS_UP].frame             = f;
        settings[id_SS_DOWN].frame           = f;
        settings[id_SS_Emerald1_Blue].frame  = f;
        settings[id_SS_Emerald2_Yellow].frame= f;
        settings[id_SS_Emerald3_Pink].frame  = f;
        settings[id_SS_Emerald4_Green].frame = f;
        settings[id_SS_Emerald5_Red].frame   = f;
        settings[id_SS_Emerald6_Grey].frame  = f;
    }

    /* Glass blocks (frame 0..3) */
    {
        v_ani3_time--;
        if ((int8_t)v_ani3_time < 0) {
            v_ani3_time = 5 - 1;
            v_ani3_frame = (v_ani3_frame + 1) & 3;
        }
        uint8_t f = v_ani3_frame;
        settings[id_SS_Glass1_Blue].frame  = f;
        settings[id_SS_Glass2_Green].frame = f;
        settings[id_SS_Glass3_Yellow].frame= f;
        settings[id_SS_Glass4_Pink].frame  = f;
    }

    /* Wall palette cycle: modifica los bits de palette line de las walls.
       En nuestro port simplificamos: como el vram se guarda en settings[i].vram,
       aplicamos el ciclo solo cambiando el campo palette. */
    {
        v_ani0_time--;
        if ((int8_t)v_ani0_time < 0) {
            v_ani0_time = 8 - 1;
            v_ani0_frame = (v_ani0_frame - 1) & 7;
        }
        /* En el ASM esto lee 8 valores (uno por wall) de una tabla para
           cambiar dinámicamente el palette line de cada wall. Los 8 frames
           son la secuencia de blink: nBnnnnnB / nnBnnnnn... etc. */
        static const uint8_t blink_pattern[8] = {
            0x00, 0x02, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x02,
        };
        uint8_t f = v_ani0_frame;
        for (int wall_set = 0; wall_set < 4; wall_set++) {
            for (int w = 0; w < 9; w++) {
                int idx = 1 + wall_set * 9 + w;
                if (idx >= 1 && idx <= 36) {
                    /* Solo las walls pares (índices específicos) parpadean.
                       En el ASM solo las posiciones con 'B' en el patrón. */
                    int blink = (blink_pattern[f] >> (w & 1)) & 1;
                    if (blink) {
                        uint16_t pal = settings[idx].vram >> 13;
                        pal = (pal - 1) & 3;
                        settings[idx].vram = (uint16_t)((settings[idx].vram & 0x1FFF) | (pal << 13));
                    }
                }
            }
        }
    }
}

/* ===========================================================================
 *  Animation queue (SS_AniItems del ASM)
 * =========================================================================== */

#define ss_ani_id(o)     ((o)[0])
#define ss_ani_delay(o)  ((o)[2])
#define ss_ani_frame(o)  ((o)[3])
#define ss_ani_block(o)  (*(uint32_t *)((o) + 4))

/* SS_AniRingData / SS_AniBumpData / SS_Ani1UpData / SS_AniRevData /
   SS_AniEmerData / SS_AniGlassData (byte sequences). */
static const uint8_t SS_AniRingData[5] = {
    id_SS_Ring_Ani1, id_SS_Ring_Ani2, id_SS_Ring_Ani3, id_SS_Ring_Ani4, 0
};
static const uint8_t SS_AniBumpData[5] = {
    id_SS_Bumper_Ani1, id_SS_Bumper_Ani2, id_SS_Bumper_Ani1, id_SS_Bumper_Ani2, 0
};
static const uint8_t SS_Ani1UpData[5] = {
    id_SS_Emerald_Ani1, id_SS_Emerald_Ani2, id_SS_Emerald_Ani3, id_SS_Emerald_Ani4, 0
};
static const uint8_t SS_AniRevData[5] = {
    id_SS_R, id_SS_R_Ani, id_SS_R, id_SS_R_Ani, 0
};
static const uint8_t SS_AniEmerData[5] = {
    id_SS_Emerald_Ani1, id_SS_Emerald_Ani2, id_SS_Emerald_Ani3, id_SS_Emerald_Ani4, 0
};
static const uint8_t SS_AniGlassData[9] = {
    id_SS_Glass_Ani1, id_SS_Glass_Ani2, id_SS_Glass_Ani3, id_SS_Glass_Ani4,
    id_SS_Glass_Ani1, id_SS_Glass_Ani2, id_SS_Glass_Ani3, id_SS_Glass_Ani4, 0
};

/* Encontrar slot libre en la cola de animaciones. */
uint8_t *SS_FindFreeAnimationSlot(void) {
    uint8_t *a2 = RAM_ADDR(v_ss_animations);
    for (int i = 0; i < (v_ss_animations_end - v_ss_animations) / 8; i++) {
        if (a2[0] == 0) return a2;
        a2 += 8;
    }
    return a2;   /* cola llena: sobreescribe el último */
}

/* --- Handlers de animación --- */

static void SS_AniRingSparks(uint8_t *a0) {
    if ((int8_t)(--a0[2]) >= 0) return;   /* ← fix */
    a0[2] = 5;
    uint8_t d0 = a0[3]++;
    uint8_t *block = RAM_ADDR(ss_ani_block(a0));
    uint8_t new_id = SS_AniRingData[d0];
    *block = new_id;
    if (new_id == 0) {
        *(uint32_t *)a0 = 0;
        ss_ani_block(a0) = 0;
    }
}

static void SS_AniBumper(uint8_t *a0) {
    if ((int8_t)(--a0[2]) >= 0) return;   /* ← fix */
    a0[2] = 7;
    uint8_t d0 = a0[3]++;
    uint8_t *block = RAM_ADDR(ss_ani_block(a0));
    uint8_t new_id = SS_AniBumpData[d0];
    if (new_id == 0) {
        *(uint32_t *)a0 = 0;
        ss_ani_block(a0) = 0;
        *block = id_SS_Bumper;
    } else {
        *block = new_id;
    }
}

static void SS_Ani1Up(uint8_t *a0) {
    if ((int8_t)(--a0[2]) >= 0) return;   /* ← fix */
    a0[2] = 5;
    uint8_t d0 = a0[3]++;
    uint8_t *block = RAM_ADDR(ss_ani_block(a0));
    uint8_t new_id = SS_Ani1UpData[d0];
    *block = new_id;
    if (new_id == 0) {
        *(uint32_t *)a0 = 0;
        ss_ani_block(a0) = 0;
    }
}

static void SS_AniReverse(uint8_t *a0) {
    if ((int8_t)(--a0[2]) >= 0) return;   /* ← fix */
    a0[2] = 7;
    uint8_t d0 = a0[3]++;
    uint8_t *block = RAM_ADDR(ss_ani_block(a0));
    uint8_t new_id = SS_AniRevData[d0];
    if (new_id == 0) {
        *(uint32_t *)a0 = 0;
        ss_ani_block(a0) = 0;
        *block = id_SS_R;
    } else {
        *block = new_id;
    }
}

static void SS_AniEmeraldSparks(uint8_t *a0) {
    if ((int8_t)(--a0[2]) >= 0) return;   /* ← fix */
    a0[2] = 5;
    uint8_t d0 = a0[3]++;
    uint8_t *block = RAM_ADDR(ss_ani_block(a0));
    uint8_t new_id = SS_AniEmerData[d0];
    *block = new_id;
    if (new_id == 0) {
        *(uint32_t *)a0 = 0;
        ss_ani_block(a0) = 0;
        obRoutine(RAM_ADDR(v_player)) = 4;
        Sound_Queue(sfx_SSGoal, false);
    }
}

static void SS_AniGlassBlock(uint8_t *a0) {
    if ((int8_t)(--a0[2]) >= 0) return;
    a0[2] = 1;
    uint8_t d0 = a0[3]++;                       /* frame index 0..8 */
    uint8_t *block = RAM_ADDR(ss_ani_block(a0));
    uint8_t new_id = SS_AniGlassData[d0];
    *block = new_id;
    if (new_id == 0) {
        *block = a0[1];                         /* ← restaurar el next_id guardado */
        *(uint32_t *)a0 = 0;
        ss_ani_block(a0) = 0;
    }
}

typedef void (*SS_AniFunc)(uint8_t *);
static const SS_AniFunc SS_AniIndex[6] = {
    SS_AniRingSparks,
    SS_AniBumper,
    SS_Ani1Up,
    SS_AniReverse,
    SS_AniEmeraldSparks,
    SS_AniGlassBlock,
};

void SS_ExecuteAnimationQueue(void) {
    uint8_t *a0 = RAM_ADDR(v_ss_animations);
    int count = (v_ss_animations_end - v_ss_animations) / 8;
    for (int i = 0; i < count; i++) {
        uint8_t id = a0[0];
        if (id != 0 && id <= 6) {
            SS_AniIndex[id - 1](a0);
        }
        a0 += 8;
    }
}

/* ===========================================================================
 *  SS_ShowLayout — genera la matriz de rotación y emite sprites.
 *  Portado de SS_ShowLayout (SpecCode.asm).
 *
 *  La emisión de sprites (BuildSpr_Normal en el ASM) se delega a
 *  SS_EmitSpriteFrame, que tenés que integrar con tu sistema de sprites.
 * =========================================================================== */

/* Emite un sprite (uno o varios pieces) al sprite table.
 * Retorna el nuevo sprite count.
 *
 * En el ASM esto es BuildSpr_Normal. Adaptá esta función a tu
 * implementación de sprites.
 *
 * Argumentos:
 *   sprite_count — contador actual (entrada y salida)
 *   x, y         — posición del sprite en coordenadas del sprite table
 *                  (d2 y d3 en el ASM; incluyen el offset $80+$A0 / $80+$70)
 *   piece_data   — puntero al primer piece (después del byte de count)
 *   piece_count  — cantidad de pieces (>= 1)
 *   vram_base    — tile base + palette (en bits 15-13)
 *
 * Formato de piece (5 bytes):
 *   [y][size][tile_hi][tile_lo][x]
 *   y es signed, x es signed.
 *   size = ((w-1) << 2) | (h-1).
 *   tile_hi: bit 7 = pri, bits 6-5 = pal, bit 4 = yflip, bit 3 = xflip,
 *            bits 2-0 = tile bits 10-8.
 *   tile_lo = tile bits 7-0.
 */
static int SS_EmitSpriteFrame(int sprite_count,
                              int16_t x, int16_t y,
                              const uint8_t *piece_data,
                              int piece_count,
                              uint16_t vram_base);

/* ---------------------------------------------------------------------------
 *  SS_ShowLayout implementation
 * --------------------------------------------------------------------------- */
void SS_ShowLayout(void) {
    SS_AnimateBlocks();
    SS_ExecuteAnimationQueue();

    /* ------------------------------------------------------------------
     * v_screenposx/y en este port se guardan como PIXELES PLANOS
     * (a diferencia del ASM original, que usa 16.16).
     * Leer el valor directo, SIN shift.
     * ------------------------------------------------------------------ */
    int16_t screenx_pix = (int16_t)v_screenposx;
    int16_t screeny_pix = (int16_t)v_screenposy;

    /* --- Calcular matriz de rotación --- */
    int16_t sine, cosine;
    CalcSine((uint16_t)(v_ssangle >> 8) & 0xFC, &sine, &cosine);

    int32_t sin_step = (int32_t)sine * ss_blocksize;
    int32_t cos_step = (int32_t)cosine * ss_blocksize;

    int16_t remx = (int16_t)((uint16_t)screenx_pix % ss_blocksize);
    int16_t remy = (int16_t)((uint16_t)screeny_pix % ss_blocksize);

    int16_t d2 = (int16_t)(-remx - (ss_matrixsize - 1) * ss_blocksize / 2);
    int16_t d3 = (int16_t)(-remy - (ss_matrixsize - 1) * ss_blocksize / 2);

    int16_t *a1 = (int16_t *)RAM_ADDR(v_ss_rotationmatrix);

    for (int row = 0; row < ss_matrixsize; row++) {
        int32_t rx = (int32_t)d2 * cosine - (int32_t)d3 * sine;
        int32_t ry = (int32_t)d2 * sine   + (int32_t)d3 * cosine;

        for (int col = 0; col < ss_matrixsize; col++) {
            *a1++ = (int16_t)(rx >> 8);
            *a1++ = (int16_t)(ry >> 8);
            rx += cos_step;
            ry += sin_step;
        }
        d3 = (int16_t)(d3 + ss_blocksize);
    }

    /* --- Insert block types and emit sprites --- */
    int row_idx = screeny_pix / ss_blocksize;
    int col_idx = screenx_pix / ss_blocksize;

    const uint8_t *layout = RAM_ADDR(v_sslayout_base)
                          + row_idx * ss_layout_rowlength
                          + col_idx;

    const int16_t *rotm = (const int16_t *)RAM_ADDR(v_ss_rotationmatrix);
    int sprite_count = v_spritecount;

    const SS_SpriteSetting *settings =
        (const SS_SpriteSetting *)RAM_ADDR(v_ss_spritesettings);

    for (int row = 0; row < ss_matrixsize; row++) {
        for (int col = 0; col < ss_matrixsize; col++) {
            uint8_t block_id = *layout++;
            if (block_id == 0 || block_id > id_SS_Glass_Ani4) {
                rotm += 2;
                continue;
            }

            int16_t x = rotm[0] + 128 + (320 / 2);
            if (x < 128 - 16 || x >= 128 + 320 + 16) {
                rotm += 2;
                continue;
            }
            int16_t y = rotm[1] + 128 + (224 / 2);
            if (y < 128 - 16 || y >= 128 + 224 + 16) {
                rotm += 2;
                continue;
            }

            const SS_SpriteSetting *s = &settings[block_id];
            const uint8_t *map_base = (const uint8_t *)(uintptr_t)s->mappings;
            if (!map_base) { rotm += 2; continue; }

            uint16_t offset = *(const uint16_t *)(map_base + s->frame * 2);
            const uint8_t *pieces = map_base + offset;
            uint8_t piece_count = *pieces++;

            if (piece_count == 0) { rotm += 2; continue; }

            sprite_count = SS_EmitSpriteFrame(sprite_count, x, y,
                                              pieces, piece_count,
                                              s->vram);
            rotm += 2;
        }
        layout += ss_layout_rowlength - ss_matrixsize;
    }

    v_spritecount = (uint8_t)sprite_count;

    if (sprite_count > 0 && sprite_count < sprites_max) {
        uint8_t *last = RAM_ADDR(v_spritetablebuffer)
                      + (sprite_count - 1) * spritetable_entrysize;
        last[2] = 0;
    }
}

/* ---------------------------------------------------------------------------
 *  SS_EmitSpriteFrame — emisor de sprites.
 *
 *  INTEGRACIÓN REQUERIDA: esta función debe escribir a v_spritetablebuffer
 *  en el formato de sprite de tu port. Como no veo tu sprites.c, la dejo
 *  como stub que NO emite nada. Cuando la conectes, vas a ver los bloques.
 *
 *  Esta es la implementación de REFERENCIA (formato MD estándar de 8 bytes).
 *  Si tu port usa otro formato, adaptala.
 * --------------------------------------------------------------------------- */
static int SS_EmitSpriteFrame(int sprite_count,
                              int16_t x, int16_t y,
                              const uint8_t *piece_data,
                              int piece_count,
                              uint16_t vram_base)
{
    uint8_t *sprite_tbl = RAM_ADDR(v_spritetablebuffer);
    const uint8_t *pd = piece_data;

    for (int p = 0; p < piece_count && sprite_count < sprites_max; p++) {
        Sprites_EmitPiece(sprite_tbl, &sprite_count, y, x, &pd,
                          vram_base, 0, 0);
    }
    return sprite_count;
}

/* ===========================================================================
 *  SS_InitVars — reset de las variables internas del SS.
 * =========================================================================== */
void SS_InitVars(void) {
    memset(RAM_ADDR(v_ss_animations), 0,
           v_ss_animations_end - v_ss_animations);
    memset(RAM_ADDR(v_ss_rotationmatrix), 0, 0x400);
    memset(RAM_ADDR(v_ss_scroll_bubbles), 0, 0x28);
    memset(RAM_ADDR(v_ss_scroll_clouds), 0, 0x1C);
    v_spritecount = 0;
}

/* ===========================================================================
 *  GM_Special — entry point, bloqueante.
 *  Portado de GM_Special (SpecCode.asm). Simplificado para Fase 1:
 *    - FadeIn/FadeOut negro por ahora (debería ser blanco con PaletteWhiteIn/Out)
 *    - SSResult 7E se omite por ahora
 * =========================================================================== */
void GM_Special_Stage_Main(void) {
    /* ================== Setup una vez ================== */
    Sound_Queue(sfx_EnterSS, false);
    Palette_FadeOut();   /* TODO: PaletteWhiteOut */

    v_vdp_buffer1 &= ~0x0040;
    VDP_ClearScreen();

    /* VDP para SS */
    VDP_SetRegister(0x0B, 0x03);       /* per-row hscroll, full vscr */
    VDP_SetRegister(0x00, 0x04);       /* 8-colour mode */
    VDP_SetRegister(0x0A, 175);        /* HInt rate (unused) */
    VDP_SetRegister(0x10, 0x11);       /* 128-cell hscroll */

    /* Clear nametables Plane 1..4 */
    VDP_FillVRAM(0,
             ArtTile_SS_Plane_1 * tile_size + plane_size_64x32,
             (ArtTile_SS_Plane_5 - ArtTile_SS_Plane_1) * tile_size - plane_size_64x32);

    SS_BGLoad();

    /* PLCs */
    NewPLC(plcid_SpecialStage);
    while (!PLC_IsEmpty()) RunPLC();

    /* Clear RAM */
    memset(RAM_ADDR(v_objspace), 0, 0x2000);
    memset(RAM_ADDR(0xF700), 0, 0x100);
    memset(RAM_ADDR(0xFE60), 0, 0xB0);
    memset(RAM_ADDR(v_ngfx_buffer), 0, v_ngfx_buffer_end - v_ngfx_buffer);

    f_wtr_state = 0;
    f_restart   = 0;

    /* Paleta */
    PalLoad_Fade(palid_Special);

    /* Layout */
    SS_Load();
    SS_InitVars();

    /* Reset cámara */
    v_screenposx = 0;
    v_screenposy = 0;

    /* Sonic del SS */
    RAM_BYTE(v_player) = id_SonicSpecial;
    /* TODO: object 09 va a auto-inicializarse */

    PalCycle_SS();

    v_ssangle  = 0;
    v_ssrotate = ss_rotatespeed;

    Sound_Queue(bgm_SS, true);
    v_btnpushtime1 = 0;
    v_rings       = 0;
    v_lifecount   = 0;
    v_debuguse    = 0;
    v_generictimer = 1800;

    /* Enable display + fade in */
    v_vdp_buffer1 |= 0x0040;
    Palette_FadeIn();   /* TODO: PaletteWhiteIn */

    /* ================== Main loop ================== */
    while (v_gamemode == GM_Special) {
        PauseGame();
        v_vblank_routine = id_VBlank_SpecialStage;
        WaitForVBlank();

        v_jpadhold2  = v_jpadhold1;
        v_jpadpress2 = v_jpadpress1;

        ExecuteObjects();
        BuildSprites();
        SS_ShowLayout();
        SS_BGAnimate();

        if (f_demo && v_generictimer == 0) {
            v_gamemode = GM_Sega;
            return;
        }
    }

    /* ================== Exit ================== */
    if (f_demo) {
        v_gamemode = GM_Title;
        return;
    }

    /* Normal exit: volver al nivel */
    v_gamemode = GM_Level;
    if (RAM_U16(0xFE10) >= (id_FZ + 1)) {
        RAM_SET_U16(0xFE10, 0);
    }
    v_generictimer = 60;

    /* Fade out */
    while (v_generictimer > 0) {
        v_vblank_routine = id_VBlank_Levels;
        WaitForVBlank();
        ExecuteObjects();
        BuildSprites();
        SS_ShowLayout();
        SS_BGAnimate();
        v_generictimer--;
    }
    Palette_FadeOut();   /* TODO: PaletteWhiteOut */

    /* TODO: cargar SSResult (object 7E) y esperar f_restart */
}
