#include "level.h"
#include "types.h"
#include "constants.h"
#include "ram.h"
#include "vdp.h"
#include "input.h"
#include "palette.h"
#include "objects.h"
#include "sprites.h"
#include "sound.h"
#include "data.h"
#include "decomp.h"
#include "deform.h"
#include "plc.h"
#include "hud.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations for main.c helpers we call */
extern void ClearScreen(void);
extern void WaitForVBlank(void);

/* VBlank routine IDs (from main.c) */
#define id_VBlank_Lag          0x00
#define id_VBlank_TitleCards   0x0C
#define id_VBlank_Levels       0x08

/* Has Level_Process done its one-time init? */
static int level_init_done = 0;

/* ===================================================================
   Level Headers (from _inc/LevelHeaders.asm)
   16 bytes per entry, one per zone, selected by v_zone (ASM indexes
   by v_zone*$10). Byte offsets match the ASM lhead layout:
     dc.l (plc1<<24)+lvlgfx ; dc.l (plc2<<24)+sixteen ; dc.l twofivesix
     dc.b 0, music, pal, pal
   The 24-bit pointers are mapped onto C pointers; they are filled at
   runtime by LevelHeaders_Init() once data.c has loaded the assets
   (NULL = asset not staged yet, skipped safely by the loaders).
   =================================================================== */
typedef struct {
    uint8_t          plc1;     /* +0:   first level PLC id */
    const uint8_t   *gfx;      /* +1..3: level gfx pointer */
    uint8_t          plc2;     /* +4:   second level PLC id */
    const uint8_t   *map16;    /* +5..7: 16x16 block data pointer */
    const uint8_t   *map256;   /* +8..B: 256x256 chunk data pointer */
    uint8_t          reserved; /* +C:   0 */
    uint8_t          music;    /* +D:   music (unused; MusicList used instead) */
    uint8_t          pal;      /* +E:   palette id */
    uint8_t          pal2;     /* +F:   palette id (duplicate) */
} level_header;

#define LHEAD(plc1, gfx, plc2, map16, map256, music, pal) \
    { plc1, gfx, plc2, map16, map256, 0, music, pal, pal }

static level_header level_headers[] = {
    /*                     gfx         plc2    map16     map256   music     palette     */
    LHEAD(plcid_GHZ, NULL, plcid_GHZ2,  NULL,    NULL,    bgm_GHZ, palid_GHZ),  /* 0: Green Hill */
    LHEAD(plcid_LZ,  NULL, plcid_LZ2,   NULL,    NULL,    bgm_LZ,  palid_LZ),   /* 1: Labyrinth */
    LHEAD(plcid_MZ,  NULL, plcid_MZ2,   NULL,    NULL,    bgm_MZ,  palid_MZ),   /* 2: Marble */
    LHEAD(plcid_SLZ, NULL, plcid_SLZ2,  NULL,    NULL,    bgm_SLZ, palid_SLZ),  /* 3: Star Light */
    LHEAD(plcid_SYZ, NULL, plcid_SYZ2,  NULL,    NULL,    bgm_SYZ, palid_SYZ),  /* 4: Spring Yard */
    LHEAD(plcid_SBZ, NULL, plcid_SBZ2,  NULL,    NULL,    bgm_SBZ, palid_SBZ1), /* 5: Scrap Brain */
    LHEAD(0,         NULL, 0,           NULL,    NULL,    bgm_SBZ, palid_Ending),/* 6: Ending */
};

/* Level_Index entry (sonic.asm:4906): "foreground, background, leftover".
   FG = d1 offset 0, BG = d1 offset 2, third slot never read. */
typedef struct {
    const uint8_t *fg;    /* d1 = 0 */
    const uint8_t *bg;    /* d1 = 2 */
    const uint8_t *left;  /* leftover/unused */
} level_index_entry;

/* Level_Index rows (defined below LevelLayoutLoad); filled at runtime. */
static level_index_entry level_index[28];

/* Point the zones at their staged assets (called at the end of Data_Init).
   The Ending reuses the GHZ data, matching the LevelHeaders.asm entry. */
void LevelHeaders_Init(void) {
    level_headers[0].gfx    = Nem_GHZ_2nd;
    level_headers[0].map16  = Blk16_GHZ;
    level_headers[0].map256 = Blk256_GHZ;

    level_headers[6].gfx    = Nem_GHZ_2nd;
    level_headers[6].map16  = Blk16_GHZ;
    level_headers[6].map256 = Blk256_GHZ;

    /* Layout pointers (level_index); rows for other zones stay NULL. */
    level_index[0].fg = Level_GHZ1;   level_index[0].bg = Level_GHZbg;
    level_index[1].fg = Level_GHZ1;   level_index[1].bg = Level_GHZbg;
    level_index[2].fg = Level_GHZ1;   level_index[2].bg = Level_GHZbg;
    level_index[3].fg = Level_GHZbg;  level_index[3].bg = Level_GHZbg;
    level_index[24].bg = Level_GHZbg; /* Ending rows 1-2 */
    level_index[25].bg = Level_GHZbg;
}

/* ===================================================================
   Level size loading (from _inc/LevelSizeLoad & BgScrollSpeed.asm)
   =================================================================== */

/* LevelSizeArray (from _inc/LevelSizeArray.asm): one 6-word entry per act,
   indexed by zone*4 + act. */
static const uint16_t level_size_array[][6] = {
    {0x0004, 0x0000, 0x24BF, 0x0000, 0x0300, 0x0060},
    {0x0004, 0x0000, 0x1EBF, 0x0000, 0x0300, 0x0060},
    {0x0004, 0x0000, 0x2960, 0x0000, 0x0300, 0x0060},
    {0x0004, 0x0000, 0x2ABF, 0x0000, 0x0300, 0x0060},
    {0x0004, 0x0000, 0x19BF, 0x0000, 0x0530, 0x0060},
    {0x0004, 0x0000, 0x10AF, 0x0000, 0x0720, 0x0060},
    {0x0004, 0x0000, 0x202F, 0xFF00, 0x0800, 0x0060},
    {0x0004, 0x0000, 0x20BF, 0x0000, 0x0720, 0x0060},
    {0x0004, 0x0000, 0x17BF, 0x0000, 0x01D0, 0x0060},
    {0x0004, 0x0000, 0x17BF, 0x0000, 0x0520, 0x0060},
    {0x0004, 0x0000, 0x1800, 0x0000, 0x0720, 0x0060},
    {0x0004, 0x0000, 0x16BF, 0x0000, 0x0720, 0x0060},
    {0x0004, 0x0000, 0x1FBF, 0x0000, 0x0640, 0x0060},
    {0x0004, 0x0000, 0x1FBF, 0x0000, 0x0640, 0x0060},
    {0x0004, 0x0000, 0x2000, 0x0000, 0x06C0, 0x0060},
    {0x0004, 0x0000, 0x3EC0, 0x0000, 0x0720, 0x0060},
    {0x0004, 0x0000, 0x22C0, 0x0000, 0x0420, 0x0060},
    {0x0004, 0x0000, 0x28C0, 0x0000, 0x0520, 0x0060},
    {0x0004, 0x0000, 0x2C00, 0x0000, 0x0620, 0x0060},
    {0x0004, 0x0000, 0x2EC0, 0x0000, 0x0620, 0x0060},
    {0x0004, 0x0000, 0x21C0, 0x0000, 0x0720, 0x0060},
    {0x0004, 0x0000, 0x1E40, 0xFF00, 0x0800, 0x0060},
    {0x0004, 0x2080, 0x2460, 0x0510, 0x0510, 0x0060},
    {0x0004, 0x0000, 0x3EC0, 0x0000, 0x0720, 0x0060},
    {0x0004, 0x0000, 0x0500, 0x0110, 0x0110, 0x0060},
    {0x0004, 0x0000, 0x0DC0, 0x0110, 0x0110, 0x0060},
    {0x0004, 0x0000, 0x2FFF, 0x0000, 0x0320, 0x0060},
    {0x0004, 0x0000, 0x2FFF, 0x0000, 0x0320, 0x0060},
};

/* LoopChunkNums (from _inc/LevelSizeLoad & BgScrollSpeed.asm): 4 bytes per zone. */
static const uint8_t loop_chunk_nums[] = {
    0xB5, 0x7F, 0x1F, 0x20,
    0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
    0xAA, 0xB4, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
};

/* BGScrollBlockSizes (REV00 only, kept for 1:1 structure). */
static const uint16_t bg_scroll_block_sizes[] = {
    0x0070, 0x0100, 0x0100, 0x0100,
    0x0800, 0x0100, 0x0100, 0x0000,
    0x0800, 0x0100, 0x0100, 0x0000,
    0x0800, 0x0100, 0x0100, 0x0000,
    0x0800, 0x0100, 0x0100, 0x0000,
    0x0800, 0x0100, 0x0100, 0x0000,
    0x0800, 0x0100, 0x0100, 0x0000,
    0x0070, 0x0100, 0x0100, 0x0100,
};

/* Forward declaration */
static void BgScrollSpeed(int16_t y, int16_t x);

/* Stub for Lamp_LoadInfo (Object 79) until ported. */
static void Lamp_LoadInfo(void) {}

void LevelSizeLoad(void) {
    uint8_t zone = (uint8_t)(v_zone_act >> 8);
    uint8_t act  = (uint8_t)(v_zone_act & 0xFF);
    uint16_t d0, d1;
    const uint16_t *a0;
    const uint8_t *a1;

    v_unused7 = 0;
    v_unused8 = 0;
    v_unused9 = 0;
    v_unused10 = 0;
    v_dle_routine = 0;
    /* f_nobgscroll is NOT cleared in REV01 (FixBugs=0) */

    uint16_t idx = (uint16_t)zone * 4 + (uint16_t)act;
    if (idx >= sizeof(level_size_array) / sizeof(level_size_array[0])) {
        idx = 0;
    }
    a0 = level_size_array[idx];

    v_unused11      = a0[0];
    v_limitleft2    = v_limitleft1  = a0[1];
    v_limitright2   = v_limitright1 = a0[2];
    v_limittop2     = v_limittop1   = a0[3];
    v_limitbtm2     = v_limitbtm1   = a0[4];
    v_limitleft3    = (uint16_t)(v_limitleft2 + 0x240);
    v_fg_xblock     = 0x10;
    v_fg_yblock     = 0x10;

    d0 = a0[5];
    v_lookshift     = d0;

    if (RAM_BYTE(v_lastlamp) != 0) {
        Lamp_LoadInfo();
        d1 = obX(&ram[v_player]);
        d0 = obY(&ram[v_player]);
    } else {
        uint16_t si = (uint16_t)((uint16_t)zone * 4 + (uint16_t)act);
        if (StartLocArray && si < 28) {
            a1 = StartLocArray + si * 4;
        }
        if ((int16_t)f_demo >= 0) {
            if (StartLocArray && si < 28) {
                d1 = (uint16_t)((a1[0] << 8) | a1[1]);  /* move.w (a1)+,d1 big-endian */
                obX(&ram[v_player]) = (int16_t)d1;
                d0 = (uint16_t)((a1[2] << 8) | a1[3]);  /* move.w (a1)+,d0 big-endian */
                obY(&ram[v_player]) = (int16_t)d0;
            } else {
                d1 = 0x0050;
                obX(&ram[v_player]) = 0x0050;
                d0 = 0x03B0;
                obY(&ram[v_player]) = 0x03B0;
            }
        } else {
            uint16_t ci = (uint16_t)((uint16_t)v_creditsnum - 1);
            if (EndingStLocArray && ci < 8) {
                a1 = EndingStLocArray + ci * 4;
                d1 = (uint16_t)((a1[0] << 8) | a1[1]);
                obX(&ram[v_player]) = (int16_t)d1;
                d0 = (uint16_t)((a1[2] << 8) | a1[3]);
                obY(&ram[v_player]) = (int16_t)d0;
            } else {
                d1 = 0x0050;
                obX(&ram[v_player]) = 0x0050;
                d0 = 0x03B0;
                obY(&ram[v_player]) = 0x03B0;
            }
        }
    }

    {
        int16_t camX = (int16_t)d1 - (320 / 2);
        if (camX < 0) camX = 0;
        if (camX >= (int16_t)v_limitright2) camX = (int16_t)v_limitright2;
        RAM_WORD(0xF700) = (uint16_t)camX;        /* v_screenposx integer word */

        int16_t camY = (int16_t)d0 - ((224 / 2) - 16);
        if (camY < 0) camY = 0;
        if (camY >= (int16_t)v_limitbtm2) camY = (int16_t)v_limitbtm2;
        RAM_WORD(0xF704) = (uint16_t)camY;        /* v_screenposy integer word */
    }

    BgScrollSpeed(d0, d1);

    if (zone < sizeof(loop_chunk_nums) / 4) {
        uint8_t *p = RAM_ADDR(0xF7AC);           /* v_256loop1 */
        p[0] = loop_chunk_nums[zone * 4 + 0];
        p[1] = loop_chunk_nums[zone * 4 + 1];
        p[2] = loop_chunk_nums[zone * 4 + 2];
        p[3] = loop_chunk_nums[zone * 4 + 3];
    }

    /* LevSz_LoadScrollBlockSize (REV00): load scroll block sizes for BG
       deformation from the table (LevelSizeLoad & BgScrollSpeed.asm:246-254). */
    if (zone < 7) {
        const uint16_t *src = &bg_scroll_block_sizes[zone * 4];
        v_scroll_block_1_size = src[0];
        v_scroll_block_2_size = src[1];
        v_scroll_block_3_size = src[2];
        v_scroll_block_4_size = src[3];
    }
}

/* ===================================================================
   Background scroll speed setup (from _inc/LevelSizeLoad & BgScrollSpeed.asm)
   =================================================================== */
static void BgScrollSpeed(int16_t y, int16_t x) {
    if (RAM_BYTE(v_lastlamp) == 0) {
        RAM_WORD(0xF70C) = (uint16_t)y;          /* v_bgscreenposy */
        RAM_WORD(0xF714) = (uint16_t)y;          /* v_bg2screenposy */
        RAM_WORD(0xF708) = (uint16_t)x;          /* v_bgscreenposx */
        RAM_WORD(0xF710) = (uint16_t)x;          /* v_bg2screenposx */
        RAM_WORD(0xF718) = (uint16_t)x;          /* v_bg3screenposx */
    }

    switch (v_zone) {
    case 0:
        RAM_LONG(0xF708) = 0;                    /* clr.l v_bgscreenposx */
        RAM_LONG(0xF70C) = 0;                    /* clr.l v_bgscreenposy */
        RAM_LONG(0xF714) = 0;                    /* clr.l v_bg2screenposy */
        RAM_LONG(0xF71C) = 0;                    /* clr.l v_bg3screenposy */
        memset(RAM_ADDR(v_bgscroll_buffer), 0, 12);
        break;
    case 1:
        RAM_WORD(0xF70C) = (int16_t)(y >> 1);    /* v_bgscreenposy */
        break;
    case 2:
        break;
    case 3:
        RAM_WORD(0xF70C) = (int16_t)((y >> 1) + 0xC0);  /* v_bgscreenposy */
        RAM_LONG(0xF708) = 0;                    /* clr.l v_bgscreenposx */
        break;
    case 4: {
        int32_t d0 = (int32_t)y << 4;
        int32_t d2 = d0;
        d0 = (d0 << 1) + d2;
        d0 >>= 8;
        d0 += 1;
        RAM_WORD(0xF70C) = (int16_t)d0;          /* v_bgscreenposy */
        RAM_LONG(0xF708) = 0;                    /* clr.l v_bgscreenposx */
        break;
    }
    case 5: {
        int16_t d0 = (int16_t)((uint16_t)y & 0x7F8);
        d0 >>= 3;
        d0 += 1;
        RAM_WORD(0xF70C) = (int16_t)d0;          /* v_bgscreenposy */
        break;
    }
    case 6: {
        int16_t d0 = (int16_t)RAM_WORD(0xF700);  /* v_screenposx */
        d0 >>= 1;
        RAM_WORD(0xF708) = (uint16_t)d0;         /* v_bgscreenposx */
        RAM_WORD(0xF710) = (uint16_t)d0;         /* v_bg2screenposx */
        int16_t d1 = d0;
        d0 >>= 2;
        d1 = d0;
        d0 += d0;
        d0 += d1;
        RAM_WORD(0xF718) = (uint16_t)d0;         /* v_bg3screenposx */
        RAM_LONG(0xF70C) = 0;                    /* clr.l v_bgscreenposy */
        RAM_LONG(0xF714) = 0;                    /* clr.l v_bg2screenposy */
        RAM_LONG(0xF71C) = 0;                    /* clr.l v_bg3screenposy */
        memset(RAM_ADDR(v_bgscroll_buffer), 0, 12);
        break;
    }
    }
}

/* ===================================================================
   Level layout loading (from _inc/LevelLayoutLoad.asm)
   =================================================================== */

/* Copy one layout blob to RAM. Header is [width][height], followed by
   (height+1) rows of (width+1) bytes (dbf semantics), rows stored
   layout_row ($80) apart in RAM. */
static void level_layout_load2(const uint8_t *src, uint8_t *dst) {
    size_t width = src[0] + 1;
    size_t rows  = src[1] + 1;
    src += 2;
    for (size_t r = 0; r < rows; r++) {
        memcpy(dst, src, width);
        dst += layout_row;
        src += width;
    }
}

/* ------------------------------------------------------------------
   Level_Index (sonic.asm:4906-4945)
   One row per (zone*4 + act); LevelLayoutLoad2 selects FG (d1 offset 0)
   or BG (d1 offset 2); the third slot is never read. Rows with NULL
   layouts (assets not staged yet) are skipped. GHZ acts 2/3 share the
   staged GHZ1 layout until their blobs land.
   ------------------------------------------------------------------ */
static level_index_entry level_index[] = {
    /* GHZ */
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    /* LZ */
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    /* MZ */
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    /* SLZ */
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    /* SYZ */
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    /* SBZ */
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    /* Ending (FG = Level_End, unstaged) */
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
};

void LevelLayoutLoad(void) {
    uint8_t zone = (uint8_t)(v_zone_act >> 8);
    uint8_t act  = (uint8_t)(v_zone_act & 0xFF);

    /* Clear the entire layout buffer (FixBugs) */
    memset(RAM_ADDR(v_lvllayout), 0, v_lvllayout_end - v_lvllayout);

    /* LevelLayoutLoad2: d0 = zone*$18 + act*6 bytes == row zone*4+act,
       then d1 (0 = FG, 2 = BG) selects the layout word. */
    unsigned row = zone * 4 + act;
    if (row >= sizeof(level_index) / sizeof(level_index[0])) {
        return;
    }

    const level_index_entry *lr = &level_index[row];
    if (lr->fg) {
        level_layout_load2(lr->fg, RAM_ADDR(v_lvllayout_fg));
    }
    if (lr->bg) {
        level_layout_load2(lr->bg, RAM_ADDR(v_lvllayout_bg));
    }
}

/* ===================================================================
   Level drawing (from _inc/Level Drawing (REV00).asm)
   =================================================================== */

static uint16_t data_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* Draw a 16x16 block into a nametable plane. Mirrors GetBlockData +
   DrawBlock: resolves the chunk from the layout, the block from the
   chunk, applies X/Y flips and stores the four tile words big-endian. */
static void draw_chunks_block(const uint8_t *layout, int cam_x, int cam_y,
                              int sx, int sy, uint32_t vram) {
    int ly = cam_y + sy;                        /* level Y pixel position */
    int lx = cam_x + sx;                        /* level X pixel position */

    /* Turn Y coordinate into a layout row offset */
    int row_off = ((ly >> 1) & 0x380);
    /* Turn X coordinate into a layout chunk column */
    int col_off = ((lx >> 8) & 0x7F);

    /* Get the chunk ID from the level layout */
    uint8_t chunk_id = layout[row_off + col_off];

    uint16_t block_id;
    int flip_x, flip_y;
    if (chunk_id == 0) {
        /* Empty chunk: GetBlockData (REV00:594) early-returns leaving a1 at
           the v_16x16 base, so DrawBlock still draws 16x16 block $00 — the
           blank block — overwriting anything previously there. */
        block_id = 0;
        flip_x = 0;
        flip_y = 0;
    } else {
        /* Chunk RAM address: (chunk_id-1) * $200 */
        uint32_t chunk_off = (uint32_t)((chunk_id - 1) & 0x7F) * chunk_size;

        /* Block cell within the chunk: 2 bytes per 16x16 block */
        int cell_y = (ly * 2) & 0x1E0;
        int cell_x = ((lx >> 3) & 0x1E);
        const uint8_t *cell = RAM_ADDR(v_256x256) + chunk_off + cell_y + cell_x;

        /* Cell word: block ID (low byte + low 2 bits of flag byte) */
        uint16_t cell_word = data_be16(cell);
        block_id = cell_word & 0x3FF;

        /* Flipping */
        flip_x = (cell[0] >> 3) & 1;
        flip_y = (cell[0] >> 4) & 1;
    }

    /* Block data: 4 words (TL, TR, BL, BR) in RAM, native order from EniDec */
    const uint16_t *blk = (const uint16_t *)RAM_ADDR(v_16x16) + block_id * 4;
    uint16_t t[4];
    for (int i = 0; i < 4; i++) t[i] = blk[i];

    uint16_t r0a, r0b, r1a, r1b;
    if (flip_y) { r0a = t[2]; r0b = t[3]; r1a = t[0]; r1b = t[1]; }
    else        { r0a = t[0]; r0b = t[1]; r1a = t[2]; r1b = t[3]; }
    if (flip_x) { uint16_t sw; sw = r0a; r0a = r0b; r0b = sw;
                         sw = r1a; r1a = r1b; r1b = sw; }
    if (flip_x) { r0a ^= 0x0800; r0b ^= 0x0800; r1a ^= 0x0800; r1b ^= 0x0800; }
    if (flip_y) { r0a ^= 0x1000; r0b ^= 0x1000; r1a ^= 0x1000; r1b ^= 0x1000; }

    /* Store the four tile words big-endian (top row, then bottom row) */
    vdp.vram[vram]           = (uint8_t)(r0a >> 8);
    vdp.vram[vram + 1]       = (uint8_t)r0a;
    vdp.vram[vram + 2]       = (uint8_t)(r0b >> 8);
    vdp.vram[vram + 3]       = (uint8_t)r0b;
    vdp.vram[vram + 0x80]    = (uint8_t)(r1a >> 8);
    vdp.vram[vram + 0x81]    = (uint8_t)r1a;
    vdp.vram[vram + 0x82]    = (uint8_t)(r1b >> 8);
    vdp.vram[vram + 0x83]    = (uint8_t)r1b;
}

/* Draw one plane's worth of level graphics (16 strips x 32 blocks), mirroring
   DrawChunks + DrawBlocks_LR_2 + Calc_VRAM_Pos. A block vertically spans two
   tile rows, so the stride is $100 bytes ($80 per tile row). */
static void draw_chunks_plane(uint32_t plane_base, int cam_x, int cam_y,
                              const uint8_t *layout) {
    for (int strip = 0; strip < 16; strip++) {
        int sy = -16 + strip * 16;               /* d4: start 16px above screen */
        int block_row = ((cam_y + sy) & 0xF0) >> 4;  /* plane block row (0-15) */
        for (int bx = 0; bx < 32; bx++) {
            int sx = bx * 16;                    /* d5: from far left */
            int col = ((cam_x + sx) & 0x1F0) >> 4;
            uint32_t vram = plane_base + block_row * 0x100 + col * 4;
            draw_chunks_block(layout, cam_x, cam_y, sx, sy, vram);
        }
    }
}

/* Draw the initial background layer. The title screen (GM_Title) calls the
   original with a3=v_bgscreenposx, a4=v_lvllayout_bg, d2=$6000, so only the
   BG plane is drawn here. */
void DrawChunks(void) {
    draw_chunks_plane(vram_bg,
                      (int16_t)RAM_WORD(0xF708),   /* v_bgscreenposx */
                      (int16_t)RAM_WORD(0xF70C),   /* v_bgscreenposy */
                      RAM_ADDR(v_lvllayout_bg));
}

/* ===================================================================
   Strip drawing — incremental redraw (Level Drawing (REV00).asm)
   DrawBlocks_LR_2 / DrawBlocks_TB_2 / DrawBG_Top / DrawBG_Bottom /
   LoadTilesAsYouMove
   =================================================================== */

/* Draw a horizontal row of blocks (DrawBlocks_LR_2 equivalent).
   d6 = number of blocks to draw minus 1 (inclusive count). */
static void draw_strip_lr(const uint8_t *layout, int cam_x, int cam_y,
                           uint32_t plane_base, int screen_y, int screen_x,
                           int count) {
    for (int i = 0; i <= count; i++) {
        int sx = screen_x + i * 16;
        int sy = screen_y;
        int block_row = ((cam_y + sy) & 0xF0) >> 4;
        int col = ((cam_x + sx) & 0x1F0) >> 4;
        uint32_t vram = plane_base + block_row * 0x100 + col * 4;
        draw_chunks_block(layout, cam_x, cam_y, sx, sy, vram);
    }
}

/* Draw a vertical column of blocks (DrawBlocks_TB_2 equivalent).
   d6 = number of blocks to draw minus 1 (inclusive count). */
static void draw_strip_tb(const uint8_t *layout, int cam_x, int cam_y,
                           uint32_t plane_base, int screen_y, int screen_x,
                           int count) {
    for (int i = 0; i <= count; i++) {
        int sy = screen_y + i * 16;
        int sx = screen_x;
        int block_row = ((cam_y + sy) & 0xF0) >> 4;
        int col = ((cam_x + sx) & 0x1F0) >> 4;
        uint32_t vram = plane_base + block_row * 0x100 + col * 4;
        draw_chunks_block(layout, cam_x, cam_y, sx, sy, vram);
    }
}

/* Draw BG top section strips (DrawBG_Top at REV00:130-221).
   Reads and clears flag bits from *flags. cam_x/cam_y come from the
   _dup screen position. */
static void draw_bg_top(uint16_t *flags, int cam_x, int cam_y,
                         uint32_t plane_base, const uint8_t *layout) {
    if (!(*flags & 0xFF)) return;

    /* bit 0 — draw new tiles at the top (entire plane width) */
    if (*flags & 0x01) {
        *flags &= ~0x01;
        draw_strip_lr(layout, cam_x, cam_y, plane_base,
                      -16, -16, (512 / 16) - 1);
    }

    /* bit 1 — draw new tiles at the bottom (entire plane width) */
    if (*flags & 0x02) {
        *flags &= ~0x02;
        draw_strip_lr(layout, cam_x, cam_y, plane_base,
                      224, -16, (512 / 16) - 1);
    }

    /* bit 2 — left column, top scroll section */
    if (*flags & 0x04) {
        *flags &= ~0x04;
        int d6 = (int)(int16_t)v_scroll_block_1_size - (cam_y & 0xFFF0);
        if (d6 >= 0) {
            d6 >>= 4;
            if (d6 > ((224 + 16 + 16) / 16) - 1)
                d6 = (224 + 16 + 16) / 16 - 1;
            draw_strip_tb(layout, cam_x, cam_y, plane_base,
                          -16, -16, d6);
        }
    }

    /* bit 3 — right column, top scroll section */
    if (*flags & 0x08) {
        *flags &= ~0x08;
        int d6 = (int)(int16_t)v_scroll_block_1_size - (cam_y & 0xFFF0);
        if (d6 >= 0) {
            d6 >>= 4;
            if (d6 > ((224 + 16 + 16) / 16) - 1)
                d6 = (224 + 16 + 16) / 16 - 1;
            draw_strip_tb(layout, cam_x, cam_y, plane_base,
                          -16, 320, d6);
        }
    }
}

/* Draw BG bottom section strips (DrawBG_Bottom at REV00:230-293).
   The bottom section draws below the scroll-block-A boundary. */
static void draw_bg_bottom(uint16_t *flags, int cam_x, int cam_y,
                            uint32_t plane_base, const uint8_t *layout) {
    if (!(*flags & 0xFF)) return;

    int scroll_a = (int)(int16_t)v_scroll_block_1_size;

    /* bit 2 — left column, bottom section */
    if (*flags & 0x04) {
        *flags &= ~0x04;
        if ((uint16_t)cam_x >= 16) {    /* cmpi.w #16,(a3) ; blo */
            int d4 = scroll_a - (cam_y & 0xFFF0);
            if (d4 >= 0) {
                int d6 = (d4 >> 4) - ((224 + 16) / 16 - 1);
                if (d6 < 0) {
                    d6 = -d6;
                    draw_strip_tb(layout, cam_x, cam_y, plane_base,
                                  d4, -16, d6);
                }
            }
        }
    }

    /* bit 3 — right column, bottom section */
    if (*flags & 0x08) {
        *flags &= ~0x08;
        int d4 = scroll_a - (cam_y & 0xFFF0);
        if (d4 >= 0) {
            int d6 = (d4 >> 4) - ((224 + 16) / 16 - 1);
            if (d6 < 0) {
                d6 = -d6;
                draw_strip_tb(layout, cam_x, cam_y, plane_base,
                              d4, 320, d6);
            }
        }
    }
}

/* Strip drawing: incremental redraw of FG + BG while screen is moving
   (LoadTilesAsYouMove at REV00:31-121).
   Called from VBlank after the _dup position/flag copies. */
void LoadTilesAsYouMove(void) {
    /* --- BG top section (scroll block A) --- */
    int bg1x = (int16_t)(uint16_t)v_bgscreenposx_dup;
    int bg1y = (int16_t)(uint16_t)v_bgscreenposy_dup;
    draw_bg_top(&v_bg1_scroll_flags_dup, bg1x, bg1y,
                vram_bg, RAM_ADDR(v_lvllayout_bg));

    /* --- BG bottom section (scroll blocks B/C) --- */
    int bg2x = (int16_t)(uint16_t)v_bg2screenposx_dup;
    int bg2y = (int16_t)(uint16_t)v_bg2screenposy_dup;
    draw_bg_bottom(&v_bg2_scroll_flags_dup, bg2x, bg2y,
                   vram_bg, RAM_ADDR(v_lvllayout_bg));

    /* --- Foreground --- */
    uint16_t fgf = v_fg_scroll_flags_dup;
    if (!(fgf & 0xFF)) return;

    int fgx = (int16_t)(uint16_t)v_screenposx_dup;
    int fgy = (int16_t)(uint16_t)v_screenposy_dup;
    const uint8_t *fg_layout = RAM_ADDR(v_lvllayout_fg);

    /* bit 0 — top row */
    if (fgf & 0x01) {
        fgf &= ~0x01;
        draw_strip_lr(fg_layout, fgx, fgy, vram_fg,
                      -16, -16, ((320 + 16 + 16) / 16) - 1);
    }

    /* bit 1 — bottom row */
    if (fgf & 0x02) {
        fgf &= ~0x02;
        draw_strip_lr(fg_layout, fgx, fgy, vram_fg,
                      224, -16, ((320 + 16 + 16) / 16) - 1);
    }

    /* bit 2 — left column */
    if (fgf & 0x04) {
        fgf &= ~0x04;
        draw_strip_tb(fg_layout, fgx, fgy, vram_fg,
                      -16, -16, ((224 + 16 + 16) / 16) - 1);
    }

    /* bit 3 — right column */
    if (fgf & 0x08) {
        fgf &= ~0x08;
        draw_strip_tb(fg_layout, fgx, fgy, vram_fg,
                      -16, 320, ((224 + 16 + 16) / 16) - 1);
    }

    v_fg_scroll_flags_dup = fgf;
}

/* ===================================================================
   Level entry (once per level, before first frame)
   Ported from sonic.asm GM_Level lines 2702-2957
   =================================================================== */
static void Level_Enter(void) {
    /* ------------------------------------------------------------------
       Phase A: Fade out + clear PLC (sonic.asm:2702-2712)
       ------------------------------------------------------------------ */
    /* bset #7, v_gamemode — mark as "in pre-level sequence" */
    v_gamemode = 0x8C;  /* GM_Level | 0x80 */

    /* Fade out music (skipped for ending-sequence demos) */
    if ((int16_t)f_demo >= 0) {
        Sound_Queue(bgm_Fade, false);
    }

    ClearPLC();
    Palette_FadeOut();

/* ------------------------------------------------------------------
       Phase B: Title card art + level PLC (sonic.asm:2716-2737)
       ------------------------------------------------------------------ */
    /* Decompress the zone title card art to VRAM */
    if (Nem_TitleCard) {
        NemDecToVRAM(Nem_TitleCard, ArtTile_Title_Card * tile_size);
    }
    /* Queue the level art PLCs (sonic.asm:2725-2737): read the 1st PLC id
       from the current zone's LevelHeaders entry, then plcid_Main2.
       Unstaged entries (NULL) are skipped by AddPLC. Nem_GHZ_1st is
       queued here per the ASM and decompressed by RunPLC during the
       title-card reveal (Phase G). */
    {
        uint8_t zone = (uint8_t)v_zone;
        if (zone < (uint8_t)(sizeof(level_headers) / sizeof(level_headers[0]))) {
            uint8_t plc = level_headers[zone].plc1;
            if (plc != 0) {
                AddPLC(plc);
            }
        }
    }
    AddPLC(plcid_Main2);

    /* ------------------------------------------------------------------
       Phase C: Clear RAM regions (sonic.asm:2739-2743)
       ------------------------------------------------------------------ */
    memset(RAM_ADDR(v_objspace), 0, 0x2000);          /* clear object RAM */
    memset(RAM_ADDR(0xF628), 0, 0x58);                /* v_misc_variables */
    memset(RAM_ADDR(0xF700), 0, 0x100);               /* v_levelvariables */
    memset(RAM_ADDR(0xFE60), 0, 0xB0);                /* v_timingandscreenvariables */

    /* ------------------------------------------------------------------
       Phase D: VDP setup for levels (sonic.asm:2745-2756)
       ------------------------------------------------------------------ */
    v_vdp_buffer1 &= ~0x0040;   /* disable display */
    ClearScreen();

    /* VDP register configuration for level mode */
    VDP_SetRegister(0x0B, 0x03);  /* mode3: per-row hscroll, full-screen vscroll */
    VDP_SetRegister(0x02, (vram_fg >> 10) & 0x38);  /* FG nametable at $C000 */
    VDP_SetRegister(0x04, (vram_bg >> 13) & 0x07);  /* BG nametable at $E000 */
    VDP_SetRegister(0x05, (vram_sprites >> 9) & 0x7F); /* sprite table */
    VDP_SetRegister(0x10, 0x01);  /* 64-cell hscroll size */
    VDP_SetRegister(0x00, 0x04);  /* 8-colour mode */
    VDP_SetRegister(0x07, 0x20);  /* background colour (line 2, colour 0) */
    VDP_SetRegister(0x0A, 223);   /* HBlank rate: scanline 223 (for water) */

    /* ------------------------------------------------------------------
       Phase E: Level palette (sonic.asm:2773-2778)
       ------------------------------------------------------------------ */
    v_air = 30;                    /* Sonic's air timer */
    v_vdp_buffer1 |= 0x0040;      /* re-enable display */

    PalLoad(palid_Sonic);          /* load Sonic palette to active palette */

    /* LZ water palette — skip for now (GHZ only) */

    /* ------------------------------------------------------------------
       Phase F: Music + title card object (sonic.asm:2792-2811)
       ------------------------------------------------------------------ */
    /* Level_GetBgm: play the zone's music from MusicList. Skipped in
       credits demos (f_demo negative). SBZ3 (LZ act 4) and Final Zone
       pick their dedicated entries. */
    if ((int16_t)f_demo >= 0) {
        static const uint8_t music_list[] = {
            bgm_GHZ,   /* 0: Green Hill */
            bgm_LZ,    /* 1: Labyrinth */
            bgm_MZ,    /* 2: Marble */
            bgm_SLZ,   /* 3: Star Light */
            bgm_SYZ,   /* 4: Spring Yard */
            bgm_SBZ,   /* 5: Scrap Brain */
            bgm_FZ,    /* 6: Final */
        };
        int d0 = v_zone;
        if (v_zone_act == id_LZ_act4) d0 = 5;  /* SBZ3 uses Scrap Brain */
        else if (v_zone_act == id_FZ) d0 = 6;  /* Final Zone */
        Sound_Queue(music_list[d0], false);
    }

    /* Load zone title cards (move.b #id_TitleCard,(v_titlecard).w) */
    memset(RAM_ADDR(v_titlecard), 0, 4 * OBJECT_SIZE);
    obID(&ram[v_titlecard]) = id_TitleCard;

    /* ------------------------------------------------------------------
       Phase G: Title card move-in loop (sonic.asm:2814-2842)
       Execute objects each frame until every element has reached its
       resting X-position. PLCs are synchronous in this port, so the
       only remaining loop condition is the cards settling.
       ------------------------------------------------------------------ */
    v_vblank_routine = id_VBlank_Levels;
    do {
        WaitForVBlank();
        ExecuteObjects();        /* first call spawns the four card elements */
        BuildSprites();
        RunPLC();                /* ASM processes the level PLCs each VBlank */
    } while (!TitleCardsSettled() || RAM_LONG(v_plc_buffer) != 0);

    /* ------------------------------------------------------------------
       Phase H: HUD base graphics (sonic.asm:2857)
       Decompress HUD art to VRAM and draw the static "E______0", "0:00",
       "__0" digits plus the lives counter.
       ------------------------------------------------------------------ */
    Hud_Base();

    /* ------------------------------------------------------------------
       Phase I: Post-title-card init (sonic.asm:2860-2919)
       ------------------------------------------------------------------ */
    PalLoad_Fade(palid_Sonic);     /* load Sonic palette to fade-in buffer */
    LevelSizeLoad();               /* set level boundaries */
    DeformLayers();                /* initialize background deformation */
    v_fg_scroll_flags |= 0x04;     /* bset #2: draw extra column at left side during start */

    LevelDataLoad();               /* load block mappings, layout and palette */
    LoadTilesFromStart();          /* draw FG + BG once before fade-in */

    ConvertCollisionArray();       /* no-op stub */
    ColIndexLoad();                /* sets v_collindex */
    LZWaterFeatures();             /* stub — no-op for GHZ */

    /* Spawn player and HUD */
    LevelSpawnPlayer();

    /* HUD: skipped in credits demos (sonic.asm:2873-2875) */
    if ((int16_t)f_demo >= 0) {
        LevelSpawnHUD();
    }

    /* Debug cheat (sonic.asm:2878-2882) */
    if (f_debugcheat && (v_jpadhold1 & btnA)) {
        f_debugmode = 1;
    }

    /* Clear button input states (sonic.asm:2885-2886: move.w #0 clears both hold+press bytes) */
    v_jpadhold2 = 0;
    v_jpadpress2 = 0;
    v_jpadhold1 = 0;
    v_jpadpress1 = 0;

    /* Initialize object position manager */
    ObjPosLoad();

    /* Execute objects once to initialize everything */
    ExecuteObjects();
    BuildSprites();

    /* ------------------------------------------------------------------
       Phase J: Clear gameplay counters (sonic.asm:2900-2919)
       ------------------------------------------------------------------ */
    /* d0 = 0; if starting from checkpoint, skip rings/time/lifecount clear */
    if (RAM_BYTE(v_lastlamp) == 0) {
        v_rings = 0;
        v_time = 0;
        v_lifecount = 0;
    }
    f_timeover = 0;
    v_shield = 0;
    v_invinc = 0;
    v_shoes = 0;
    v_unused1 = 0;
    v_debuguse = 0;
    f_restart = 0;
    v_framecount = 0;

    OscillateNumInit();

    f_scorecount = 1;
    f_ringcount  = 1;
    f_timecount  = 1;

    /* ------------------------------------------------------------------
       Phase K: Fade in (sonic.asm:2935-2966)
       ------------------------------------------------------------------ */
    /* Demo data setup (sonic.asm:2921-2944) — v_generictimer for demo end */
    v_btnpushtime1 = 0;
    v_generictimer = 1800;           /* 30 seconds for regular play */
    if ((int16_t)f_demo < 0) {
        v_generictimer = 540;        /* 9 seconds for credits demos */
        if (v_creditsnum == 4) {
            v_generictimer = 510;    /* 0.5s less for demo 4 */
        }
    }

    /* 4-frame VBlank delay for palette transfers (sonic.asm:2957-2963) */
    for (int i = 0; i < 4; i++) {
        v_vblank_routine = id_VBlank_Levels;
        WaitForVBlank();
    }
    Palette_FadeIn();

    /* Level has faded in (sonic.asm:2970-2988) */
    if ((int16_t)f_demo >= 0) {
        /* Normal: make title cards start moving */
        obRoutine(&ram[v_titlecard])                     += 2;
        obRoutine(&ram[v_titlecard + OBJECT_SIZE * 1])   += 4;
        obRoutine(&ram[v_titlecard + OBJECT_SIZE * 2])   += 4;
        obRoutine(&ram[v_titlecard + OBJECT_SIZE * 3])   += 4;
    } else {
        /* Credits demo: load explosion + animal graphics (Level_ClrCardArt) */
        AddPLC(plcid_Explode);
        int d0 = (uint8_t)v_zone + plcid_GHZAnimals;
        AddPLC(d0);
    }

    /* bclr #7, v_gamemode — end pre-level sequence (sonic.asm:2991) */
    v_gamemode = 0x0C;  /* GM_Level: clear bit 7 */
}

/* ===================================================================
   Draw the current level graphics to the whole screen (once, at level
   entry). Ported from _inc/Level Drawing (REV00).asm LoadTilesFromStart:
   draws the FG plane from v_screenposx/y + v_lvllayout_fg, then the BG
   plane from v_bgscreenposx/y + v_lvllayout_bg.
   =================================================================== */
void LoadTilesFromStart(void) {
    draw_chunks_plane(vram_fg,
                      (int16_t)RAM_WORD(0xF700),   /* v_screenposx */
                      (int16_t)RAM_WORD(0xF704),   /* v_screenposy */
                      RAM_ADDR(v_lvllayout_fg));
    draw_chunks_plane(vram_bg,
                      (int16_t)RAM_WORD(0xF708),   /* v_bgscreenposx */
                      (int16_t)RAM_WORD(0xF70C),   /* v_bgscreenposy */
                      RAM_ADDR(v_lvllayout_bg));
}

/* ===================================================================
   LevelDataLoad (from _inc/LevelLayoutLoad.asm)
   Loads the level header data: 16x16 block mappings, 256x256 chunk
   mappings, FG/BG layout, and the zone palette into the fade buffer.
   =================================================================== */
void LevelDataLoad(void) {
    uint8_t zone = (uint8_t)(v_zone_act >> 8);

    /* --- Level Header ---
       ASM: lea LevelHeaders.l, a2; lea (a2,d0.w), a2 with d0 = v_zone*$10
       (skip the 1st PLC and level gfx entry — handled in GM_Level). */
    if (zone >= sizeof(level_headers) / sizeof(level_headers[0])) {
        return;
    }
    const level_header *lp = &level_headers[zone];

    /* --- 16x16 Block Mappings: +(a2) = second dc.l (plc2<<24)|sixteen --- */
    if (lp->map16) {
        uint16_t *buf = (uint16_t *)RAM_ADDR(v_16x16);
        EniDec(lp->map16, buf, ArtTile_Level);
    }

    /* --- 256x256 Chunk Mappings: +(a2) = third dc.l (twofivesix) --- */
    if (lp->map256) {
        uint8_t *buf = RAM_ADDR(v_256x256);
        KosDec(lp->map256, buf);
    }

    /* --- Level Layout (FG/BG) --- */
    LevelLayoutLoad();

    /* --- Music (unused) --- */

    /* --- Palette: low byte of the header (palid duplicated in headers) --- */
    {
        uint16_t pal = lp->pal & 0xFF;

        if (v_zone_act == id_LZ_act4) {          /* SBZ3 (LZ4)? */
            pal = palid_SBZ3;
        } else if (v_zone_act == id_SBZ_act2 || v_zone_act == id_FZ) {
            pal = palid_SBZ2;                    /* SBZ2 / FZ */
        }
        PalLoad_Fade(pal);
    }

    /* --- 2nd PLC: first byte of the second dc.l (0 = ending, skip) --- */
    if (lp->plc2 != 0) {
        AddPLC(lp->plc2);
    }
}

/* ===================================================================
   LZWaterFeatures — stub (GHZ has no water)
   =================================================================== */
void LZWaterFeatures(void) {
    /* TODO: LZ water initialization */
}

/* ===================================================================
   ConvertCollisionArray — no-op stub (disabled in original ASM)
   =================================================================== */
void ConvertCollisionArray(void) {
    /* Intentionally empty — this is a disabled development function */
}

/* ===================================================================
   Collision index (ColIndexLoad, sonic.asm:3104-3110 + ColPointers
   table 3116-3121).  The ASM stores a 32-bit ROM pointer in the RAM
   long v_collindex (0xF796); host asset pointers are malloc'd and can
   exceed 32 bits, so the current zone's index lives in this static
   (same fix the opl_* pointers use).  Empty zones pick a NULL pointer.
   =================================================================== */
const uint8_t *col_index_ptr = NULL;

const uint8_t *GetColIndex(void) {
    return col_index_ptr;
}

void ColIndexLoad(void) {
    /* ColPointers (sonic.asm:3116-3121); locals so the runtime asset
       pointers are allowed as initializers. */
    const uint8_t *const col_pointers[] = {
        Col_GHZ,            /* 0: Green Hill */
        Col_LZ,             /* 1: Labyrinth */
        Col_MZ,             /* 2: Marble */
        Col_SLZ,            /* 3: Star Light */
        Col_SYZ,            /* 4: Spring Yard */
        Col_SBZ,            /* 5: Scrap Brain */
    };

    uint8_t zone = (uint8_t)v_zone;                     /* move.b (v_zone).w,d0 */
    if (zone >= (uint8_t)(sizeof(col_pointers) / sizeof(col_pointers[0]))) {
        return;                                         /* no Ending entry */
    }
    col_index_ptr = col_pointers[zone];                 /* move.l ColPointers(pc,d0.w),(v_collindex).w */
}

/* ===================================================================
   OscillateNumInit — stub
   Initializes oscillation data used by swings, platforms, etc.
   =================================================================== */
void OscillateNumInit(void) {
    /* TODO: fill v_oscillate table with initial values */
}

/* ===================================================================
   OscillateNumDo — stub (sonic.asm OscillateNumDo)
   Advance oscillation values each frame.
   =================================================================== */
void OscillateNumDo(void) {
    /* TODO: advance v_oscillate table values */
}

/* ===================================================================
   PauseGame — basic pause toggle
   =================================================================== */
void PauseGame(void) {
    if (joypad_hold[0] & btnStart) {
        f_pause = 1;
        Sound_Queue(bgm_Fade, false);
        while (f_pause) {
            Input_Read();
            WaitForVBlank();
            /* Unpause when Start is pressed again */
            if (joypad_press[0] & btnStart) {
                f_pause = 0;
            }
        }
    }
}

/* ===================================================================
   LevelSpawnPlayer — create Sonic object at level start
   =================================================================== */
void LevelSpawnPlayer(void) {
    uint8_t *player_slot = RAM_ADDR(v_player);
    obID(player_slot) = 0x01;  /* id_SonicPlayer = $01 — only the ID byte, no memset (matches ASM) */
}

/* ===================================================================
   LevelSpawnHUD — create HUD object
   =================================================================== */
void LevelSpawnHUD(void) {
    uint8_t *hud_slot = RAM_ADDR(v_hud);
    obID(hud_slot) = 0x21;  /* id_HUD = $21 — only the ID byte, no memset (matches ASM) */
}

/* ===================================================================
   ObjPosLoad — object position manager (ported 1:1 from
   _inc/ObjPosLoad.asm, REV01, FixBugs=0)
   Reads the objpos list and spawns objects as the camera scrolls.
   =================================================================== */

/* The four objpos list pointers at v_opl_data (0xF770/+4/+8/+0xC) hold full
   heap addresses in this port (the RAM words would truncate them to 32 bits),
   so they live in statics instead of the mirror RAM. opl_ptr_right/
   opl_ptr_left mirror v_opl_data/+4; opl_ptr_sec mirrors +8/+0xC (the
   secondary list, always blank). */
static uint8_t *opl_ptr_right;
static uint8_t *opl_ptr_left;
static uint8_t *opl_ptr_sec;

static uint16_t opl_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* OPL_SpawnObj: check the respawn flag and spawn one object.
   a0p: in/out pointer into the objpos list; a2: v_objstate;
   d2: position in the respawn list.
   Returns 0 if the object was spawned (or skipped because it was already
   broken), nonzero if there was no free object slot. */
static int OPL_SpawnObj(uint8_t **a0p, uint8_t *a2, uint8_t d2) {
    uint8_t *a0 = *a0p;
    uint8_t *a1;
    uint16_t d0;

    if (a0[4] & 0x80) {                         /* remember respawn flag */
        uint8_t old = a2[2 + d2];
        a2[2 + d2] = (uint8_t)(old | 0x80);     /* bset #7 (FixBugs=0: set always) */
        if (old & 0x80) {                       /* already destroyed before */
            a0 += 6;
            *a0p = a0;
            return 0;
        }
    }

    a1 = (uint8_t *)FindFreeObj();
    if (!a1) return 1;                          /* bne .fail */

    obX(a1) = (int16_t)opl_be16(a0);            /* move.w (a0)+,obX */
    a0 += 2;
    d0 = opl_be16(a0);                          /* move.w (a0)+,d0 (y + flip bits) */
    a0 += 2;
    obY(a1) = (int16_t)(d0 & 0x0FFF);           /* andi.w #$FFF: ignore flip bits */
    obRender(a1) = (uint8_t)((d0 & 0x4000) ? sprite_xflip : 0)
                 | (uint8_t)((d0 & 0x8000) ? sprite_yflip : 0); /* rol #2 + andi.b */
    obStatus(a1) = obRender(a1);
    d0 = a0[0];                                 /* move.b (a0)+,d0 (object id) */
    a0 += 1;
    if (d0 & 0x80) obRespawnNo(a1) = d2;        /* remember bit: give respawn slot */
    obID(a1)       = (uint8_t)(d0 & 0x7F);      /* ignore respawn bit */
    obSubtype(a1)  = a0[0];                     /* move.b (a0)+,obSubtype */
    a0 += 1;

    *a0p = a0;
    return 0;
}

static void OPL_Next(void);

/* OPL_Main: initialise the spawn windows and respawn list. */
static void OPL_Main(void) {
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t *a0, *start;
    uint16_t d6;

    if (ObjPos_GHZ1 == NULL) return;            /* no objpos data mapped */

    v_opl_routine = (uint8_t)(v_opl_routine + 2); /* goto OPL_Next next */

    /* d0 = (v_zone_act << 2) & 0xFFF indexes ObjPos_Index; the only mapped
       entry (GHZ1) points at ObjPos_GHZ1, so a0 is that list directly. */
    a0 = ObjPos_GHZ1;
    opl_ptr_right = a0;                         /* move.l a0,(v_opl_data)   */
    opl_ptr_left  = a0;                         /* move.l a0,(v_opl_data+4) */
    opl_ptr_sec   = NULL;                       /* move.l a1,(v_opl_data+8/+C) */

    *a2 = 0x01;                                 /* move.w #$101,(a2)+ */
    *(a2 + 1) = 0x01;
    a2 += 2;
    /* FixBugs=0: the loop counter is measured in words ($5E), but the loop
       clears longwords, so $17C bytes are cleared instead of $BE. */
    for (int i = 0x5E; i >= 0; i--) {
        *(uint32_t *)a2 = 0;
        a2 += 4;
    }

    /* .use_screen_x: d6 = (v_screenposx - 128), clamped at 0, & ~0x7F */
    d6 = (uint16_t)v_screenposx;
    if (d6 >= 128) d6 -= 128;
    else           d6 = 0;
    d6 &= 0xFF80;

    a0   = opl_ptr_right;
    start = a0;
    while (opl_be16(a0) < d6) {                 /* bls .found_right: stop when x >= d6 */
        if (a0[4] & 0x80) {                     /* remember flag */
            (*a2)++;                            /* addq.b #1,(a2) (ASM's d2 read is dead) */
        }
        a0 += 6;
    }
    opl_ptr_right = a0;                         /* .found_right: move.l a0,(v_opl_data) */

    a0 = start;                                 /* movea.l (v_opl_data+4),a0 */
    if (d6 >= 128) {                            /* bcs.s .found_left (borrow when < 128) */
        d6 -= 128;                              /* subi.w #128,d6 */
        while (opl_be16(a0) < d6) {             /* bls .found_left */
            if (a0[4] & 0x80) (*(a2 + 1))++;    /* addq.b #1,1(a2) */
            a0 += 6;
        }
    }
    opl_ptr_left = a0;                          /* .found_left: move.l a0,(v_opl_data+4) */

    v_opl_screen = 0xFFFF;                      /* move.w #-1,(v_opl_screen) */

    OPL_Next();                                 /* fall-through to OPL_Next */
}

/* OPL_MovedLeft: recycle the respawn list while the camera moves left. */
static void OPL_MovedLeft(uint16_t d6) {
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t *a0;
    uint8_t d2 = 0;
    int16_t d6s;

    v_opl_screen = d6;                          /* move.w d6,(v_opl_screen) */
    a0 = opl_ptr_left;                          /* movea.l (v_opl_data+4),a0 */
    d6s = (int16_t)d6 - 128;                    /* subi.w #128,d6 */
    if (d6s >= 0) {                             /* bcs.s .found_left */
        while (1) {
            uint16_t cx = opl_be16(a0 - 6);     /* cmp.w -6(a0),d6 */
            if ((int16_t)cx <= d6s) break;      /* bge.s .found_left: stop when x <= d6 */
            a0 -= 6;                            /* subq.w #6,a0 */
            if (a0[4] & 0x80) {                 /* remember flag */
                (*(a2 + 1))--;                  /* subq.b #1,1(a2) */
                d2 = *(a2 + 1);                 /* move.b 1(a2),d2 */
            }
            if (OPL_SpawnObj(&a0, a2, d2)) {    /* bne.s .failed_to_spawn */
                if (a0[4] & 0x80) (*(a2 + 1))++; /* revert second counter */
                a0 += 6;                        /* addq.w #6,a0 */
                break;
            }
            a0 -= 6;                            /* goto previous objpos entry */
        }
    }
    opl_ptr_left = a0;                          /* .found_left: move.l a0,(v_opl_data+4) */

    /* right side: recycle the right respawn slots, no spawning */
    a0 = opl_ptr_right;                         /* movea.l (v_opl_data),a0 */
    d6s += 128 + 320 + 320;                     /* addi.w #128+320+320,d6 */
    while (1) {
        uint16_t cx = opl_be16(a0 - 6);         /* cmp.w -6(a0),d6 */
        if (d6s > (int16_t)cx) break;           /* bgt.s .found_right: stop when x < d6 */
        if (a0[-2] & 0x80) (*a2)--;             /* subq.b #1,(a2) (flag at -2(a0)) */
        a0 -= 6;                                /* subq.w #6,a0 */
    }
    opl_ptr_right = a0;                         /* .found_right: move.l a0,(v_opl_data) */
}

/* OPL_MovedRight: spawn objects as the camera moves right (or on the first
   frame, since v_opl_screen starts at -1). */
static void OPL_MovedRight(uint16_t d6) {
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t *a0;
    uint8_t d2 = 0;
    uint16_t d6u;

    v_opl_screen = d6;                          /* move.w d6,(v_opl_screen) */
    a0 = opl_ptr_right;                         /* movea.l (v_opl_data),a0 */
    d6u = d6 + 320 + 320;                       /* addi.w #320+320,d6 */

    while (1) {                                 /* .loop_find_right */
        if (opl_be16(a0) <= d6u) break;         /* bls.s .found_right: stop when x >= d6 */
        if (a0[4] & 0x80) {                     /* remember flag */
            d2 = *a2;                           /* move.b (a2),d2 */
            (*a2)++;                            /* addq.b #1,(a2) */
        }
        if (OPL_SpawnObj(&a0, a2, d2)) break;   /* bne -> .found_right (no free slot) */
    }
    opl_ptr_right = a0;                         /* .found_right: move.l a0,(v_opl_data) */

    a0 = opl_ptr_left;                          /* movea.l (v_opl_data+4),a0 */
    if (d6u >= 768) {                           /* bcs.s .found_left (borrow when < 768) */
        d6u -= 320 + 320 + 128;                 /* subi.w #320+320+128,d6 */
        while (1) {                             /* .loop_find_left */
            if (opl_be16(a0) <= d6u) break;     /* bls.s .found_left */
            if (a0[4] & 0x80) (*(a2 + 1))++;    /* addq.b #1,1(a2) */
            a0 += 6;
        }
    }
    opl_ptr_left = a0;                          /* .found_left: move.l a0,(v_opl_data+4) */
}

/* OPL_Next: process one frame of object loading. */
static void OPL_Next(void) {
    uint16_t d6;
    int16_t prev;

    if (opl_ptr_right == NULL) return;          /* empty list */
    d6 = (uint16_t)v_screenposx & 0xFF80;       /* andi.w #$FF80 */
    prev = (int16_t)v_opl_screen;               /* cmp.w (v_opl_screen),d6 */
    if ((int16_t)d6 == prev) return;            /* beq.w OPL_NoMove */
    if ((int16_t)d6 >= prev) OPL_MovedRight(d6); /* bge.s OPL_MovedRight */
    else                     OPL_MovedLeft(d6);
}

/* ObjPosLoad: dispatch on the opl routine. */
void ObjPosLoad(void) {
    if (v_opl_routine == 0) OPL_Main();
    else                    OPL_Next();
}

/* ===================================================================
   AnimateLevelAct — ported from _inc/AnimateLevelGfx.asm

   LoadTiles (AnimateLevelGfx.asm:415-421): copy `count` 8x8 tiles
   (Raw art, 4BPP) from src to VRAM at a byte address.
   =================================================================== */
static void load_tiles_vram(const uint8_t *src, uint32_t vram_byte, int count) {
    VDP_WriteVRAM(src, vram_byte, (uint32_t)count * tile_size);
}

/* AniArt_GiantRing (AnimateLevelGfx.asm:577-601): the giant ring object
   sets v_gfxbigring to Art_BigRing_size; each frame we copy 14 tiles
   further forward through the (uncompressed) art.  No giant ring exists
   in GHZ act 1, so v_gfxbigring stays 0 here; the body is just the same
   count-down copy (host-bounds-guarded, per the v_collindex fix). */
static void anis_giant_ring(void) {
    if (v_gfxbigring == 0) {
        return;
    }
    v_gfxbigring = (uint16_t)(v_gfxbigring - 14 * tile_size);  /* subi.w #.size*tile_size */
    if ((int16_t)v_gfxbigring < 0) {
        v_gfxbigring = 0;                                      /* host safety: no wrap loop */
        return;
    }
    const uint8_t *a1 = Art_BigRing + v_gfxbigring;            /* lea (a1,d0.w),a1 */
    if ((uint32_t)v_gfxbigring + 14 * tile_size > Art_BigRing_len) {
        return;                                                /* host safety */
    }
    load_tiles_vram(a1, ArtTile_Giant_Ring * tile_size + v_gfxbigring, 14);
}

/* AniArt_GHZ (AnimateLevelGfx.asm:41-113): waterfall, big flower and
   small flower, each on its own v_laniX timer/frame pair. */
static const uint8_t flower_seq[4] = { 0, 1, 2, 1 };           /* .flowerSeq */

static void anis_ghz(void) {
    if (!Art_GhzWater || !Art_GhzFlower1 || !Art_GhzFlower2) {
        return;
    }

    /* AniArt_GHZ_Waterfall (.size = 8) */
    if ((int8_t)(--v_lani0_time) < 0) {                        /* bpl = time remains */
        v_lani0_time = 6 - 1;
        const uint8_t *a1 = Art_GhzWater;
        uint8_t d0 = v_lani0_frame;                            /* frame ID before increment */
        v_lani0_frame++;
        if (d0 & 1) {                                          /* 2 frames */
            a1 += 8 * tile_size;
        }
        load_tiles_vram(a1, ArtTile_GHZ_Waterfall * tile_size, 8);
    }

    /* AniArt_GHZ_Bigflower (.size = 16) */
    if ((int8_t)(--v_lani1_time) < 0) {
        v_lani1_time = 16 - 1;
        const uint8_t *a1 = Art_GhzFlower1;
        uint8_t d0 = v_lani1_frame;
        v_lani1_frame++;
        if (d0 & 1) {                                          /* 2 frames */
            a1 += 16 * tile_size;
        }
        load_tiles_vram(a1, ArtTile_GHZ_Big_Flower_1 * tile_size, 16);
    }

    /* AniArt_GHZ_Smallflower (.size = 12) */
    if ((int8_t)(--v_lani2_time) >= 0) {
        return;
    }
    v_lani2_time = 8 - 1;
    uint8_t d0 = v_lani2_frame;
    v_lani2_frame++;
    d0 &= 3;                                                   /* 4 counter frames */
    d0 = flower_seq[d0];                                       /* actual flower frame 0-2 */
    if (!(d0 & 1)) {                                           /* frames 0 and 2 hold longer */
        v_lani2_time = 128 - 1;
    }
    uint32_t off = (uint32_t)d0 * 3 * 0x80;                    /* lsl #7 * 3 (frame * 12 tiles) */
    load_tiles_vram(Art_GhzFlower2 + off, ArtTile_GHZ_Small_Flower * tile_size, 12);
}

/* AniArt_MZ_Magma (AnimateLevelGfx.asm:428-566): the 16 routines select
   a 4-byte window [j..j+3] (mod 16) out of each 16-byte art row; the
   table below mirrors AniArt_MZMagma's offsets (each entry is the byte
   start of the longword the ASM routine writes). */
static const uint8_t magma_col_start[16] = {
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  9,  10, 11, 12, 13, 14, 15,
};

static void anis_mz(void) {
    if (!Art_MzLava1 || !Art_MzLava2 || !Art_MzTorch) {
        return;
    }

    /* AniArt_MZ_Lava (.size = 8) */
    if ((int8_t)(--v_lani0_time) < 0) {
        v_lani0_time = 20 - 1;
        uint8_t d0 = v_lani0_frame;
        v_lani0_frame = (uint8_t)((d0 + 1) % 3);               /* 3 frames, wraps */
        d0 = v_lani0_frame;                                    /* mulu uses the NEW frame */
        load_tiles_vram(Art_MzLava1 + (uint32_t)d0 * (8 * tile_size),
                        ArtTile_MZ_Animated_Lava * tile_size, 8);
    }

    /* AniArt_MZ_Magma: column collation, oscillated over 4 columns.
       v_oscillate+$A is the sine-wave sync position (OscillateNumDo);
       the port holds v_lani1_frame increment too (unused downstream). */
    if ((int8_t)(--v_lani1_time) < 0) {
        v_lani1_time = 2 - 1;
        v_lani1_frame++;                                       /* increment frame counter (unused) */
        const uint8_t *a4 = Art_MzLava2 + ((uint32_t)v_lani0_frame << 9);  /* ror.w #7 -> *$200 */
        uint16_t d3 = RAM_WORD(0xFE5E + 0xA);                  /* v_oscillate+$A */
        uint8_t *dst = &vdp.vram[ArtTile_MZ_Animated_Magma * tile_size];
        for (int iter = 0; iter < 4; iter++) {                 /* move.w #4-1,d2 */
            int j = ((d3 * 2) & 0x1E) >> 1;                    /* andi.w #$1E */
            j = magma_col_start[j];                            /* jump into collation table */
            const uint8_t *a1 = a4;
            for (int row = 0; row < 0x20; row++) {             /* dbf d1,#$20-1 */
                for (int b = 0; b < 4; b++) {
                    dst[b] = a1[(j + b) & 15];
                }
                dst += 4;
                a1 += 0x10;
            }
            d3 += 4;
        }
    }

    /* AniArt_MZ_Torch (.size = 6) */
    if ((int8_t)(--v_lani2_time) < 0) {
        v_lani2_time = 8 - 1;
        uint8_t d0 = v_lani3_frame;                            /* old frame */
        v_lani3_frame++;
        v_lani3_frame &= 3;                                    /* 3 frames, wraps */
        if ((uint32_t)d0 * (6 * tile_size) + 6 * tile_size <= Art_MzTorch_len) {
            load_tiles_vram(Art_MzTorch + (uint32_t)d0 * (6 * tile_size),
                            ArtTile_MZ_Torch * tile_size, 6);
        }
    }
}

/* AniArt_SBZ (AnimateLevelGfx.asm:207-286): two smoke puffs, each with
   an 8-frame cycle (frame 0 = blank + reschedule) shared on one art. */
static void anis_sbz(void) {
    if (!Art_SbzSmoke) {
        return;
    }

    /* .check_smokePuff1 */
    if (v_lani2_frame != 0) {
        v_lani2_frame--;                                       /* subq.b #1 */
        goto check_smokePuff2;
    }
    /* .smokePuff1 */
    if ((int8_t)(--v_lani0_time) >= 0) {
        goto check_smokePuff2;
    }
    v_lani0_time = 8 - 1;
    {
        uint8_t d0 = v_lani0_frame;
        v_lani0_frame++;
        d0 &= 7;                                               /* 8 frames */
        if (d0 != 0) {                                         /* beq .untilNextPuff1 */
            d0--;
            load_tiles_vram(Art_SbzSmoke + (uint32_t)d0 * (12 * tile_size),
                            ArtTile_SBZ_Smoke_Puff_1 * tile_size, 12);
            return;
        }
        /* .untilNextPuff1 */
        v_lani2_frame = 3 * 60;
        /* .clearSky — write 6 blank tiles twice (12 total) */
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_1 * tile_size, 6);
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_1 * tile_size + 6 * tile_size, 6);
    }

check_smokePuff2:
    /* .check_smokePuff2 */
    if (v_lani2_time != 0) {
        v_lani2_time--;
        return;
    }
    /* .smokePuff2 */
    if ((int8_t)(--v_lani1_time) >= 0) {
        return;
    }
    v_lani1_time = 8 - 1;
    {
        uint8_t d0 = v_lani1_frame;
        v_lani1_frame++;
        d0 &= 7;
        if (d0 != 0) {                                         /* beq .untilNextPuff2 */
            d0--;
            load_tiles_vram(Art_SbzSmoke + (uint32_t)d0 * (12 * tile_size),
                            ArtTile_SBZ_Smoke_Puff_2 * tile_size, 12);
            return;
        }
        /* .untilNextPuff2 */
        v_lani2_time = 2 * 60;
        /* .clearSky */
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_2 * tile_size, 6);
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_2 * tile_size + 6 * tile_size, 6);
    }
}

/* AniArt_Ending (AnimateLevelGfx.asm:294-392): the flowers.  The flower
   patterns are prerendered into the 256x256-definition RAM by the ending
   loader (not ported yet), so the RAM-sourced reads are zero until then. */
static const uint8_t ending_flower2_seq[8] = { 0, 0, 0, 1, 2, 2, 2, 1 };
static const uint8_t ending_flower34_seq[4] = { 0, 1, 2, 1 };

static void anis_ending(void) {
    if (!Art_GhzFlower1 || !Art_GhzFlower2) {
        return;
    }

    /* AniArt_Ending_BigFlower (.size = 16) */
    if ((int8_t)(--v_lani1_time) < 0) {
        v_lani1_time = 8 - 1;
        const uint8_t *a1 = Art_GhzFlower1;
        uint8_t *a2 = RAM_ADDR(v_256x256 + 0x4A * chunk_size); /* v_256x256_def+$4A*chunk_size */
        uint8_t d0 = v_lani1_frame;
        v_lani1_frame++;
        if (d0 & 1) {                                          /* 2 frames */
            a1 += 16 * tile_size;
            a2 += 16 * tile_size;
        }
        load_tiles_vram(a1, ArtTile_GHZ_Big_Flower_1 * tile_size, 16);
        load_tiles_vram(a2, ArtTile_GHZ_Big_Flower_2 * tile_size, 16);
    }

    /* AniArt_Ending_SmallFlower (.size = 12) */
    if ((int8_t)(--v_lani2_time) < 0) {
        v_lani2_time = 8 - 1;
        uint8_t d0 = v_lani2_frame;
        v_lani2_frame++;
        d0 &= 7;                                               /* 8 counter frames */
        d0 = ending_flower2_seq[d0];                           /* actual flower frame */
        load_tiles_vram(Art_GhzFlower2 + (uint32_t)d0 * 3 * 0x80,
                        ArtTile_GHZ_Small_Flower * tile_size, 12);
    }

    /* AniArt_Ending_Flower3 (.size = 16) */
    if ((int8_t)(--v_lani4_time) < 0) {
        v_lani4_time = 15 - 1;
        uint8_t d0 = v_lani4_frame;
        v_lani4_frame++;
        d0 &= 3;
        d0 = ending_flower34_seq[d0];
        uint32_t off = (uint32_t)d0 * 2 * 0x100;               /* lsl #8 * 2 */
        load_tiles_vram(RAM_ADDR(v_256x256 + 0x4C * chunk_size) + off,
                        ArtTile_GHZ_Flower_3 * tile_size, 16);
    }

    /* AniArt_Ending_Flower4 (.size = 16) */
    if ((int8_t)(--v_lani5_time) >= 0) {
        return;
    }
    v_lani5_time = 12 - 1;
    uint8_t d0 = v_lani5_frame;
    v_lani5_frame++;
    d0 &= 3;
    d0 = ending_flower34_seq[d0];
    uint32_t off = (uint32_t)d0 * 2 * 0x100;
    load_tiles_vram(RAM_ADDR(v_256x256 + 0x4F * chunk_size) + off,
                    ArtTile_GHZ_Flower_4 * tile_size, 16);
}

static void anis_none(void) {
    /* AniArt_none (AnimateLevelGfx.asm:400-401) — zones without animated gfx */
}

/* AniArt_Index (AnimateLevelGfx.asm:25-32): word-relative jump table. */
static const void (*const aniart_index[])(void) = {
    anis_ghz,     /* GHZ */
    anis_none,    /* LZ */
    anis_mz,      /* MZ */
    anis_none,    /* SLZ */
    anis_none,    /* SYZ */
    anis_sbz,     /* SBZ */
    anis_ending,  /* Ending */
};

/* AnimateLevelAct — AnimateLevelGfx (AnimateLevelGfx.asm:6-21) */
void AnimateLevelAct(void) {
    if (f_pause != 0) {                                        /* don't animate gfx while paused */
        return;
    }
    anis_giant_ring();                                         /* public call inside due to (a6) */
    uint8_t zone = (uint8_t)v_zone;                            /* get current zone ID */
    if (zone >= (uint8_t)(sizeof(aniart_index) / sizeof(aniart_index[0]))) {
        return;                                                /* zonewarning AniArt_Index,2 */
    }
    aniart_index[zone]();                                      /* jmp AniArt_Index(pc,d0.w) */
}

/* ===================================================================
   PaletteCycle — stub (sonic.asm PaletteCycle)
   Zone-specific palette cycling (waterfalls, lava, etc.)
   =================================================================== */
void PaletteCycle(void) {
    /* TODO: zone-specific palette cycling */
}

/* ===================================================================
   SignpostArtLoad — stub (sonic.asm SignpostArtLoad)
   Load sign post art when approaching end of act.
   =================================================================== */
void SignpostArtLoad(void) {
    /* TODO: sign post art loading at act end */
}

/* ===================================================================
   MoveSonicInDemo — stub (sonic.asm MoveSonicInDemo)
   Simulate controls during demo playback. Returns immediately
   outside demo mode.
   =================================================================== */
void MoveSonicInDemo(void) {
    /* TODO: demo playback control simulation */
}

/* ===================================================================
   Level_Process — called once per frame from MainGameLoop
   Ported from sonic.asm Level_MainLoop (lines 2998-3046)
   =================================================================== */
void Level_Process(void) {
    /* One-time init */
    if (!level_init_done) {
        Level_Enter();
        level_init_done = 1;
        return;
    }

    /* --- Per-frame gameplay (Level_MainLoop, sonic.asm:2998-3046) --- */

    PauseGame();                        /* handle pausing (sonic.asm:2999) */

    v_vblank_routine = id_VBlank_Levels;
    WaitForVBlank();                    /* sonic.asm:3000-3001 */
    v_framecount = v_framecount + 1;    /* sonic.asm:3002 */

    MoveSonicInDemo();                  /* demo controls (stub for now) */
    LZWaterFeatures();                  /* water features (stub for GHZ) */
    ExecuteObjects();                   /* sonic.asm:3006 */

    /* f_restart check (FixBugs placement, sonic.asm:3008-3010) */
    if (f_restart) {
        level_init_done = 0;
        return;
    }

    /* DeformLayers: skip if Sonic is dying (routine >= 6) or in debug */
    if (v_debuguse == 0 && obRoutine(&ram[v_player]) >= 6) {
        /* Sonic dying — skip plane scrolling */
    } else {
        DeformLayers();                 /* sonic.asm:3026 */
    }

    BuildSprites();                     /* sonic.asm:3029 */
    ObjPosLoad();                       /* sonic.asm:3030 */
    PaletteCycle();                     /* sonic.asm:3031 */
    RunPLC();                           /* sonic.asm:3032 */
    OscillateNumDo();                   /* sonic.asm:3033 */
    SynchroAnimate();                   /* sonic.asm:3034 */
    SignpostArtLoad();                  /* sonic.asm:3035 */
}
