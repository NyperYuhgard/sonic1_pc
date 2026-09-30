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
#include <stdlib.h>
#include <stdio.h>

/* Forward declarations for main.c helpers we call */
extern void ClearScreen(void);
extern void WaitForVBlank(void);

/* VBlank routine IDs (from constants.h / main.c) */
#define id_VBlank_Lag          0x00
#define id_VBlank_Levels       0x08
#define id_VBlank_Paused       0x10

/* Has Level_Process done its one-time init? */
static int level_init_done = 0;

/* ===================================================================
   Level Headers (from _inc/LevelHeaders.asm)
   =================================================================== */
typedef struct {
    uint8_t          plc1;
    const uint8_t   *gfx;
    uint8_t          plc2;
    const uint8_t   *map16;
    const uint8_t   *map256;
    uint8_t          reserved;
    uint8_t          music;
    uint8_t          pal;
    uint8_t          pal2;
} level_header;

#define LHEAD(plc1, gfx, plc2, map16, map256, music, pal) \
    { plc1, gfx, plc2, map16, map256, 0, music, pal, pal }

static level_header level_headers[] = {
    LHEAD(plcid_GHZ, NULL, plcid_GHZ2,  NULL,    NULL,    bgm_GHZ, palid_GHZ),
    LHEAD(plcid_LZ,  NULL, plcid_LZ2,   NULL,    NULL,    bgm_LZ,  palid_LZ),
    LHEAD(plcid_MZ,  NULL, plcid_MZ2,   NULL,    NULL,    bgm_MZ,  palid_MZ),
    LHEAD(plcid_SLZ, NULL, plcid_SLZ2,  NULL,    NULL,    bgm_SLZ, palid_SLZ),
    LHEAD(plcid_SYZ, NULL, plcid_SYZ2,  NULL,    NULL,    bgm_SYZ, palid_SYZ),
    LHEAD(plcid_SBZ, NULL, plcid_SBZ2,  NULL,    NULL,    bgm_SBZ, palid_SBZ1),
    LHEAD(0,         NULL, 0,           NULL,    NULL,    bgm_SBZ, palid_Ending),
};

typedef struct {
    const uint8_t *fg;
    const uint8_t *bg;
    const uint8_t *left;
} level_index_entry;

typedef struct {
    const uint8_t *main;
    const uint8_t *null;
} objpos_index_entry;

static objpos_index_entry objpos_index[28];
static level_index_entry level_index[28];

void LevelHeaders_Init(void) {
    level_headers[0].gfx    = Nem_GHZ_2nd;
    level_headers[0].map16  = Blk16_GHZ;
    level_headers[0].map256 = Blk256_GHZ;

    level_headers[1].gfx    = Nem_LZ;
    level_headers[1].map16  = Blk16_LZ;
    level_headers[1].map256 = Blk256_LZ;

    level_headers[2].gfx    = Nem_MZ;
    level_headers[2].map16  = Blk16_MZ;
    level_headers[2].map256 = Blk256_MZ;

    level_headers[3].gfx    = Nem_SLZ;
    level_headers[3].map16  = Blk16_SLZ;
    level_headers[3].map256 = Blk256_SLZ;

    level_headers[4].gfx    = Nem_SYZ;
    level_headers[4].map16  = Blk16_SYZ;
    level_headers[4].map256 = Blk256_SYZ;

    level_headers[5].gfx    = Nem_SBZ;
    level_headers[5].map16  = Blk16_SBZ;
    level_headers[5].map256 = Blk256_SBZ;

    level_headers[6].gfx    = Nem_GHZ_2nd;
    level_headers[6].map16  = Blk16_GHZ;
    level_headers[6].map256 = Blk256_GHZ;

    /* GHZ */
    level_index[0].fg = Level_GHZ1;   level_index[0].bg = Level_GHZbg;
    level_index[1].fg = Level_GHZ2;   level_index[1].bg = Level_GHZbg;
    level_index[2].fg = Level_GHZ3;   level_index[2].bg = Level_GHZbg;
    /* LZ */
    level_index[4].fg = Level_LZ1;    level_index[4].bg = Level_LZbg;
    level_index[5].fg = Level_LZ2;    level_index[5].bg = Level_LZbg;
    level_index[6].fg = Level_LZ3;    level_index[6].bg = Level_LZbg;
    level_index[7].fg = Level_SBZ3;   level_index[7].bg = Level_LZbg;
    /* MZ */
    level_index[8].fg = Level_MZ1;    level_index[8].bg = Level_MZ1bg;
    level_index[9].fg = Level_MZ2;    level_index[9].bg = Level_MZ2bg;
    level_index[10].fg = Level_MZ3;   level_index[10].bg = Level_MZ3bg;
    /* SLZ */
    level_index[12].fg = Level_SLZ1;  level_index[12].bg = Level_SLZbg;
    level_index[13].fg = Level_SLZ2;  level_index[13].bg = Level_SLZbg;
    level_index[14].fg = Level_SLZ3;  level_index[14].bg = Level_SLZbg;
    /* SYZ */
    level_index[16].fg = Level_SYZ1;  level_index[16].bg = Level_SYZbg;
    level_index[17].fg = Level_SYZ2;  level_index[17].bg = Level_SYZbg;
    level_index[18].fg = Level_SYZ3;  level_index[18].bg = Level_SYZbg;
    /* SBZ */
    level_index[20].fg = Level_SBZ1;  level_index[20].bg = Level_SBZ1bg;
    level_index[21].fg = Level_SBZ2;  level_index[21].bg = Level_SBZ2bg;
    level_index[22].fg = Level_SBZ2;  level_index[22].bg = Level_SBZ2bg;
    /* Ending */
    level_index[24].fg = Level_End;   level_index[24].bg = Level_GHZbg;
    level_index[25].fg = Level_End;   level_index[25].bg = Level_GHZbg;

    /* ObjPos_Index */
    objpos_index[0].main  = ObjPos_GHZ1;
    objpos_index[1].main  = ObjPos_GHZ2;
    objpos_index[2].main  = ObjPos_GHZ3;
    objpos_index[3].main  = ObjPos_GHZ1;
    objpos_index[4].main  = ObjPos_LZ1;
    objpos_index[5].main  = ObjPos_LZ2;
    objpos_index[6].main  = ObjPos_LZ3;
    objpos_index[7].main  = ObjPos_SBZ3;
    objpos_index[8].main  = ObjPos_MZ1;
    objpos_index[9].main  = ObjPos_MZ2;
    objpos_index[10].main = ObjPos_MZ3;
    objpos_index[11].main = ObjPos_MZ1;
    objpos_index[12].main = ObjPos_SLZ1;
    objpos_index[13].main = ObjPos_SLZ2;
    objpos_index[14].main = ObjPos_SLZ3;
    objpos_index[15].main = ObjPos_SLZ1;
    objpos_index[16].main = ObjPos_SYZ1;
    objpos_index[17].main = ObjPos_SYZ2;
    objpos_index[18].main = ObjPos_SYZ3;
    objpos_index[19].main = ObjPos_SYZ1;
    objpos_index[20].main = ObjPos_SBZ1;
    objpos_index[21].main = ObjPos_SBZ2;
    objpos_index[22].main = ObjPos_FZ;
    objpos_index[23].main = ObjPos_SBZ1;
    objpos_index[24].main = ObjPos_End;
    objpos_index[25].main = ObjPos_End;
    objpos_index[26].main = ObjPos_End;
    objpos_index[27].main = ObjPos_End;
}

/* ===================================================================
   Level size loading (from _inc/LevelSizeLoad & BgScrollSpeed.asm)
   =================================================================== */
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

static const uint8_t loop_chunk_nums[] = {
    0xB5, 0x7F, 0x1F, 0x20,
    0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
    0xAA, 0xB4, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F,
};

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

static void BgScrollSpeed(int16_t y, int16_t x);
static void Lamp_LoadInfo(void) {}

void LevelSizeLoad(void) {
    uint8_t zone = (uint8_t)v_zone;
    uint8_t act  = (uint8_t)v_act;
    uint16_t d0, d1;
    const uint16_t *a0;
    const uint8_t *a1;

    v_unused7 = 0;
    v_unused8 = 0;
    v_unused9 = 0;
    v_unused10 = 0;
    v_dle_routine = 0;

    uint16_t idx = (uint16_t)zone * 4 + (uint16_t)act;
    if (idx >= sizeof(level_size_array) / sizeof(level_size_array[0])) idx = 0;
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
        if (StartLocArray && si < 28) a1 = StartLocArray + si * 4;
        if ((int16_t)f_demo >= 0) {
            if (StartLocArray && si < 28) {
                d1 = (uint16_t)((a1[0] << 8) | a1[1]);
                obX(&ram[v_player]) = (int16_t)d1;
                d0 = (uint16_t)((a1[2] << 8) | a1[3]);
                obY(&ram[v_player]) = (int16_t)d0;
            } else {
                d1 = 0x0050; obX(&ram[v_player]) = 0x0050;
                d0 = 0x03B0; obY(&ram[v_player]) = 0x03B0;
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
                d1 = 0x0050; obX(&ram[v_player]) = 0x0050;
                d0 = 0x03B0; obY(&ram[v_player]) = 0x03B0;
            }
        }
    }

    {
        int16_t camX = (int16_t)d1 - (320 / 2);
        if (camX < 0) camX = 0;
        if (camX >= (int16_t)v_limitright2) camX = (int16_t)v_limitright2;
        RAM_WORD(0xF700) = (uint16_t)camX;

        int16_t camY = (int16_t)d0 - ((224 / 2) - 16);
        if (camY < 0) camY = 0;
        if (camY >= (int16_t)v_limitbtm2) camY = (int16_t)v_limitbtm2;
        RAM_WORD(0xF704) = (uint16_t)camY;
    }

    BgScrollSpeed(d0, d1);

    if (zone < sizeof(loop_chunk_nums) / 4) {
        uint8_t *p = RAM_ADDR(0xF7AC);
        p[0] = loop_chunk_nums[zone * 4 + 0];
        p[1] = loop_chunk_nums[zone * 4 + 1];
        p[2] = loop_chunk_nums[zone * 4 + 2];
        p[3] = loop_chunk_nums[zone * 4 + 3];
    }

    if (zone < 7) {
        const uint16_t *src = &bg_scroll_block_sizes[zone * 4];
        v_scroll_block_1_size = src[0];
        v_scroll_block_2_size = src[1];
        v_scroll_block_3_size = src[2];
        v_scroll_block_4_size = src[3];
    }
}

static void BgScrollSpeed(int16_t y, int16_t x) {
    if (RAM_BYTE(v_lastlamp) == 0) {
        RAM_WORD(0xF70C) = (uint16_t)y;
        RAM_WORD(0xF714) = (uint16_t)y;
        RAM_WORD(0xF708) = (uint16_t)x;
        RAM_WORD(0xF710) = (uint16_t)x;
        RAM_WORD(0xF718) = (uint16_t)x;
    }

    switch (v_zone) {
    case 0:
        RAM_LONG(0xF708) = 0;
        RAM_LONG(0xF70C) = 0;
        RAM_LONG(0xF714) = 0;
        RAM_LONG(0xF71C) = 0;
        memset(RAM_ADDR(v_bgscroll_buffer), 0, 12);
        break;
    case 1:
        RAM_WORD(0xF70C) = (int16_t)(y >> 1);
        break;
    case 2:
        break;
    case 3:
        RAM_WORD(0xF70C) = (int16_t)((y >> 1) + 0xC0);
        RAM_LONG(0xF708) = 0;
        break;
    case 4: {
        int32_t d0 = (int32_t)y << 4;
        int32_t d2 = d0;
        d0 = (d0 << 1) + d2;
        d0 >>= 8;
        d0 += 1;
        RAM_WORD(0xF70C) = (int16_t)d0;
        RAM_LONG(0xF708) = 0;
        break;
    }
    case 5: {
        int16_t d0 = (int16_t)((uint16_t)y & 0x7F8);
        d0 >>= 3;
        d0 += 1;
        RAM_WORD(0xF70C) = (int16_t)d0;
        break;
    }
    case 6: {
        int16_t d0 = (int16_t)RAM_WORD(0xF700);
        d0 >>= 1;
        RAM_WORD(0xF708) = (uint16_t)d0;
        RAM_WORD(0xF710) = (uint16_t)d0;
        int16_t d1 = d0;
        d0 >>= 2;
        d1 = d0;
        d0 += d0;
        d0 += d1;
        RAM_WORD(0xF718) = (uint16_t)d0;
        RAM_LONG(0xF70C) = 0;
        RAM_LONG(0xF714) = 0;
        RAM_LONG(0xF71C) = 0;
        memset(RAM_ADDR(v_bgscroll_buffer), 0, 12);
        break;
    }
    }
}

/* ===================================================================
   Level layout loading
   =================================================================== */
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

void LevelLayoutLoad(void) {
    uint8_t zone = (uint8_t)v_zone;
    uint8_t act  = (uint8_t)v_act;

    memset(RAM_ADDR(v_lvllayout), 0, v_lvllayout_end - v_lvllayout);

    unsigned row = zone * 4 + act;
    if (row >= sizeof(level_index) / sizeof(level_index[0])) return;

    const level_index_entry *lr = &level_index[row];
    if (lr->fg) level_layout_load2(lr->fg, RAM_ADDR(v_lvllayout_fg));
    if (lr->bg) level_layout_load2(lr->bg, RAM_ADDR(v_lvllayout_bg));
}

/* ===================================================================
   Level drawing
   =================================================================== */
static uint16_t data_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void draw_chunks_block(const uint8_t *layout, int cam_x, int cam_y,
                              int sx, int sy, uint32_t vram) {
    int ly = cam_y + sy;
    int lx = cam_x + sx;
    int row_off = ((ly >> 1) & 0x380);
    int col_off = ((lx >> 8) & 0x7F);
    uint8_t chunk_id = layout[row_off + col_off];

    uint16_t block_id;
    int flip_x, flip_y;
    if (chunk_id == 0) {
        block_id = 0; flip_x = 0; flip_y = 0;
    } else {
        uint32_t chunk_off = (uint32_t)((chunk_id - 1) & 0x7F) * chunk_size;
        int cell_y = (ly * 2) & 0x1E0;
        int cell_x = ((lx >> 3) & 0x1E);
        const uint8_t *cell = RAM_ADDR(v_256x256) + chunk_off + cell_y + cell_x;
        uint16_t cell_word = data_be16(cell);
        block_id = cell_word & 0x3FF;
        flip_x = (cell[0] >> 3) & 1;
        flip_y = (cell[0] >> 4) & 1;
    }

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

    vdp.vram[vram]           = (uint8_t)(r0a >> 8);
    vdp.vram[vram + 1]       = (uint8_t)r0a;
    vdp.vram[vram + 2]       = (uint8_t)(r0b >> 8);
    vdp.vram[vram + 3]       = (uint8_t)r0b;
    vdp.vram[vram + 0x80]    = (uint8_t)(r1a >> 8);
    vdp.vram[vram + 0x81]    = (uint8_t)r1a;
    vdp.vram[vram + 0x82]    = (uint8_t)(r1b >> 8);
    vdp.vram[vram + 0x83]    = (uint8_t)r1b;
}

static void draw_chunks_plane(uint32_t plane_base, int cam_x, int cam_y,
                              const uint8_t *layout) {
    for (int strip = 0; strip < 16; strip++) {
        int sy = -16 + strip * 16;
        int block_row = ((cam_y + sy) & 0xF0) >> 4;
        for (int bx = 0; bx < 32; bx++) {
            int sx = bx * 16;
            int col = ((cam_x + sx) & 0x1F0) >> 4;
            uint32_t vram = plane_base + block_row * 0x100 + col * 4;
            draw_chunks_block(layout, cam_x, cam_y, sx, sy, vram);
        }
    }
}

void DrawChunks(void) {
    draw_chunks_plane(vram_bg,
                      (int16_t)RAM_WORD(0xF708),
                      (int16_t)RAM_WORD(0xF70C),
                      RAM_ADDR(v_lvllayout_bg));
}

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

static void draw_bg_top(uint16_t *flags, int cam_x, int cam_y,
                         uint32_t plane_base, const uint8_t *layout) {
    if (!(*flags & 0xFF)) return;

    if (*flags & 0x01) {
        *flags &= ~0x01;
        draw_strip_lr(layout, cam_x, cam_y, plane_base, -16, -16, (512 / 16) - 1);
    }
    if (*flags & 0x02) {
        *flags &= ~0x02;
        draw_strip_lr(layout, cam_x, cam_y, plane_base, 224, -16, (512 / 16) - 1);
    }
    if (*flags & 0x04) {
        *flags &= ~0x04;
        int d6 = (int)(int16_t)v_scroll_block_1_size - (cam_y & 0xFFF0);
        if (d6 >= 0) {
            d6 >>= 4;
            if (d6 > ((224 + 16 + 16) / 16) - 1) d6 = (224 + 16 + 16) / 16 - 1;
            draw_strip_tb(layout, cam_x, cam_y, plane_base, -16, -g_render_left - 16, d6);
        }
    }
    if (*flags & 0x08) {
        *flags &= ~0x08;
        int d6 = (int)(int16_t)v_scroll_block_1_size - (cam_y & 0xFFF0);
        if (d6 >= 0) {
            d6 >>= 4;
            if (d6 > ((224 + 16 + 16) / 16) - 1) d6 = (224 + 16 + 16) / 16 - 1;
            draw_strip_tb(layout, cam_x, cam_y, plane_base, -16, 320 + g_render_left, d6);
        }
    }
}

static void draw_bg_bottom(uint16_t *flags, int cam_x, int cam_y,
                            uint32_t plane_base, const uint8_t *layout) {
    if (!(*flags & 0xFF)) return;
    int scroll_a = (int)(int16_t)v_scroll_block_1_size;

    if (*flags & 0x04) {
        *flags &= ~0x04;
        if ((uint16_t)cam_x >= 16) {
            int d4 = scroll_a - (cam_y & 0xFFF0);
            if (d4 >= 0) {
                int d6 = (d4 >> 4) - ((224 + 16) / 16 - 1);
                if (d6 < 0) {
                    d6 = -d6;
                    draw_strip_tb(layout, cam_x, cam_y, plane_base, d4, -g_render_left - 16, d6);
                }
            }
        }
    }
    if (*flags & 0x08) {
        *flags &= ~0x08;
        int d4 = scroll_a - (cam_y & 0xFFF0);
        if (d4 >= 0) {
            int d6 = (d4 >> 4) - ((224 + 16) / 16 - 1);
            if (d6 < 0) {
                d6 = -d6;
                draw_strip_tb(layout, cam_x, cam_y, plane_base, d4, 320 + g_render_left, d6);
            }
        }
    }
}

void LoadTilesAsYouMove(void) {
    int bg1x = (int16_t)(uint16_t)v_bgscreenposx_dup;
    int bg1y = (int16_t)(uint16_t)v_bgscreenposy_dup;
    draw_bg_top(&v_bg1_scroll_flags_dup, bg1x, bg1y, vram_bg, RAM_ADDR(v_lvllayout_bg));

    int bg2x = (int16_t)(uint16_t)v_bg2screenposx_dup;
    int bg2y = (int16_t)(uint16_t)v_bg2screenposy_dup;
    draw_bg_bottom(&v_bg2_scroll_flags_dup, bg2x, bg2y, vram_bg, RAM_ADDR(v_lvllayout_bg));

    uint16_t fgf = v_fg_scroll_flags_dup;
    if (!(fgf & 0xFF)) return;

    int fgx = (int16_t)(uint16_t)v_screenposx_dup;
    int fgy = (int16_t)(uint16_t)v_screenposy_dup;
    const uint8_t *fg_layout = RAM_ADDR(v_lvllayout_fg);

    const int strip_start = -g_render_left - 16;
    const int strip_end   = 320 + g_render_left;
    const int strip_count = (g_render_w + 32) / 16 - 1;

    if (fgf & 0x01) { fgf &= ~0x01; draw_strip_lr(fg_layout, fgx, fgy, vram_fg, -16, strip_start, strip_count); }
    if (fgf & 0x02) { fgf &= ~0x02; draw_strip_lr(fg_layout, fgx, fgy, vram_fg, 224, strip_start, strip_count); }
    if (fgf & 0x04) { fgf &= ~0x04; draw_strip_tb(fg_layout, fgx, fgy, vram_fg, -16, strip_start, ((224 + 16 + 16) / 16) - 1); }
    if (fgf & 0x08) { fgf &= ~0x08; draw_strip_tb(fg_layout, fgx, fgy, vram_fg, -16, strip_end, ((224 + 16 + 16) / 16) - 1); }

    v_fg_scroll_flags_dup = fgf;
}

/* ===================================================================
   NUEVO: Tablas del sistema de agua LZ
   =================================================================== */

/* WaterHeight: altura inicial del agua por acto (LZ1, LZ2, LZ3, SBZ3) */
static const uint16_t WaterHeight[4] = {
    0x00B8,  /* LZ act 1 */
    0x0328,  /* LZ act 2 */
    0x0900,  /* LZ act 3 */
    0x0228,  /* SBZ3 (LZ act 4) */
};

/* LZWind_Data: {left, top, right, bottom} para cada túnel de viento.
   Se leen en pares de words; el offset en bytes es base*2 + i*2. */
static const int16_t LZWind_Data[20] = {
    0x0A80, 0x0300, 0x0C10, 0x0380,   /* LZ act 1 - set 1 */
    0x0F80, 0x0100, 0x1410, 0x0180,   /* LZ act 1 - set 2 */
    0x0460, 0x0400, 0x0710, 0x0480,   /* LZ act 2 */
    0x0A20, 0x0600, 0x1610, 0x06E0,   /* LZ act 3 */
    0x0C80, 0x0600, 0x13D0, 0x0680,   /* SBZ3 */
};

/* Velocidades de tobogán de agua (en 8.8 fixed; el byte es la parte entera) */
static const int8_t Slide_Speeds[7] = {
    0x0A, -0x0B, 0x0A, -0x0A, -0x0B, -0x0C, 0x0B
};

/* Chunks que son toboganes de agua */
static const uint8_t Slide_Chunks[7] = {
    2, 7, 3, 0x4C, 0x4B, 8, 4
};

/* ===================================================================
   NUEVO: Setup inicial de agua (Level_LoadPal / Level_WaterPal /
   Level_ChkWaterPal del ASM)
   =================================================================== */

static void LZ_LevelWaterSetup(void) {
    if (v_zone != id_LZ) return;

    uint8_t act = (uint8_t)v_act;
    if (act > 3) act = 0;
    uint16_t wh = WaterHeight[act];
    v_waterpos1 = wh;
    v_waterpos2 = wh;
    v_waterpos3 = wh;
    v_wtr_routine = 0;
    f_wtr_state   = 0;
    f_water       = 1;

    /* Paleta submarina de Sonic: LZ1-3 usan palid_LZSonWater, SBZ3 usa
       palid_SBZ3SonWat. */
    int sonpal = (act == act4) ? palid_SBZ3SonWat : palid_LZSonWater;
    PalLoad_Fade_Water(sonpal);

    if (RAM_BYTE(v_lastlamp) != 0) {
        f_wtr_state = RAM_BYTE(v_lamp_wtrstat);
    }
}

static void LZ_LevelWaterPalLoad(void) {
    if (v_zone != id_LZ) return;
    int pal = ((uint8_t)v_act == act4) ? palid_SBZ3Water : palid_LZWater;
    PalLoad_Water(pal);
}

/* ===================================================================
   Level_Enter (con las llamadas de agua integradas)
   =================================================================== */
static void Level_Enter(void) {
    v_hblank_line = 223;
    f_wtr_state   = 0;
    v_gamemode = 0x8C;

    if ((int16_t)f_demo >= 0) {
        Sound_Queue(bgm_Fade, false);
    }

    ClearPLC();
    Palette_FadeOut();

    if (Nem_TitleCard) {
        NemDecToVRAM(Nem_TitleCard, ArtTile_Title_Card * tile_size);
    }
    {
        uint8_t zone = (uint8_t)v_zone;
        if (zone < (uint8_t)(sizeof(level_headers) / sizeof(level_headers[0]))) {
            uint8_t plc = level_headers[zone].plc1;
            if (plc != 0) AddPLC(plc);
        }
    }
    AddPLC(plcid_Main2);

    memset(RAM_ADDR(v_objspace), 0, 0x2000);
    memset(RAM_ADDR(0xF628), 0, 0x58);
    memset(RAM_ADDR(0xF700), 0, 0x100);
    memset(RAM_ADDR(0xFE60), 0, 0xB0);

    v_vdp_buffer1 &= ~0x0040;
    ClearScreen();

    VDP_SetRegister(0x0B, 0x03);
    VDP_SetRegister(0x02, (vram_fg >> 10) & 0x38);
    VDP_SetRegister(0x04, (vram_bg >> 13) & 0x07);
    VDP_SetRegister(0x05, (vram_sprites >> 9) & 0x7F);
    VDP_SetRegister(0x10, 0x01);
    VDP_SetRegister(0x00, 0x04);
    VDP_SetRegister(0x07, 0x20);
    VDP_SetRegister(0x0A, 223);

    v_air = 30;
    v_vdp_buffer1 |= 0x0040;

    PalLoad(palid_Sonic);

    /* NUEVO: setup inicial de agua LZ (height + fade-water palette). */
    LZ_LevelWaterSetup();

    if ((int16_t)f_demo >= 0) {
        static const uint8_t music_list[] = {
            bgm_GHZ, bgm_LZ, bgm_MZ, bgm_SLZ, bgm_SYZ, bgm_SBZ, bgm_FZ,
        };
        int d0 = v_zone;
        if (RAM_U16(0xFE10) == id_LZ_act4) d0 = 5;
        else if (RAM_U16(0xFE10) == id_FZ) d0 = 6;
        Sound_Queue(music_list[d0], true);
    }

    memset(RAM_ADDR(v_titlecard), 0, 4 * OBJECT_SIZE);
    obID(&ram[v_titlecard]) = id_TitleCard;

    v_vblank_routine = id_VBlank_Levels;
    do {
        WaitForVBlank();
        ExecuteObjects();
        BuildSprites();
        RunPLC();
    } while (!TitleCardsSettled() || RAM_LONG(v_plc_buffer) != 0);

    Hud_Base();

    PalLoad_Fade(palid_Sonic);
    LevelSizeLoad();
    DeformLayers();
    v_fg_scroll_flags |= 0x0C;

    LevelDataLoad();
    LoadTilesFromStart();

    ConvertCollisionArray();
    ColIndexLoad();

    /* NUEVO: cargar la paleta submarina activa antes del fade-in. */
    LZ_LevelWaterPalLoad();

    LZWaterFeatures();

    LevelSpawnPlayer();

    if ((int16_t)f_demo >= 0) {
        LevelSpawnHUD();
    }

    /* NUEVO: spawn de las superficies de agua (Level_ChkWater, LZ only). */
    if (v_zone == id_LZ) {
        RAM_BYTE(v_watersurface1) = id_WaterSurface;
        obX(RAM_ADDR(v_watersurface1)) = 0x60;
        RAM_BYTE(v_watersurface2) = id_WaterSurface;
        obX(RAM_ADDR(v_watersurface2)) = 0x120;
    }

    if (f_debugcheat && (v_jpadhold1 & btnA)) {
        f_debugmode = 1;
    }

    v_jpadhold2 = 0;
    v_jpadpress2 = 0;
    v_jpadhold1 = 0;
    v_jpadpress1 = 0;

    ObjPosLoad();
    ExecuteObjects();
    BuildSprites();

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

    v_btnpushtime1 = 0;
    v_generictimer = 1800;
    if ((int16_t)f_demo < 0) {
        v_generictimer = 540;
        if (v_creditsnum == 4) v_generictimer = 510;
    }

    for (int i = 0; i < 4; i++) {
        v_vblank_routine = id_VBlank_Levels;
        WaitForVBlank();
    }
    Palette_FadeIn();

    if ((int16_t)f_demo >= 0) {
        obRoutine(&ram[v_titlecard])                     += 2;
        obRoutine(&ram[v_titlecard + OBJECT_SIZE * 1])   += 4;
        obRoutine(&ram[v_titlecard + OBJECT_SIZE * 2])   += 4;
        obRoutine(&ram[v_titlecard + OBJECT_SIZE * 3])   += 4;
    } else {
        AddPLC(plcid_Explode);
        int d0 = (uint8_t)v_zone + plcid_GHZAnimals;
        AddPLC(d0);
    }

    v_gamemode = 0x0C;

        /* [DBG] dump one-shot de punteros y del objpos */
    static int dbg_opl = -1;
    if (dbg_opl < 0) dbg_opl = getenv("SONIC_LOG_OPL") ? 1 : 0;
    if (dbg_opl) {
        fprintf(stderr, "\n=== LEVEL ENTER zone=%d act=%d ===\n",
                (int)v_zone, (int)v_act);
        fprintf(stderr,
            "LZ platform ptrs: LZ1pf1=%p LZ1pf2=%p LZ2pf1=%p LZ2pf2=%p LZ3pf1=%p LZ3pf2=%p\n",
            (void*)ObjPos_LZ1pf1, (void*)ObjPos_LZ1pf2,
            (void*)ObjPos_LZ2pf1, (void*)ObjPos_LZ2pf2,
            (void*)ObjPos_LZ3pf1, (void*)ObjPos_LZ3pf2);
        fprintf(stderr,
            "LZ1 objpos ptr=%p len=%zu\n",
            (void*)ObjPos_LZ1, ObjPos_LZ1_len);
        /* Dump de los primeros 12 objetos del objpos LZ actual */
        uint8_t z = (uint8_t)v_zone, a = (uint8_t)v_act;
        unsigned row = z * 4 + a;
        if (row < sizeof(objpos_index)/sizeof(objpos_index[0])) {
            const uint8_t *m = objpos_index[row].main;
            fprintf(stderr, "objpos_index[%u] ptr=%p, primeros objetos:\n",
                    row, (void*)m);
            if (m) {
                for (int i = 0; i < 12; i++) {
                    uint16_t x = ((uint16_t)m[i*6+0]<<8) | m[i*6+1];
                    uint16_t y = ((uint16_t)m[i*6+2]<<8) | m[i*6+3];
                    uint8_t id = m[i*6+4];
                    uint8_t sub = m[i*6+5];
                    fprintf(stderr, "  [%2d] X=$%04X Y=$%04X id=$%02X sub=$%02X",
                            i, x, y, id & 0x7F, sub);
                    if ((id & 0x7F) == 0x63) fprintf(stderr, "  <-- OBJ 63!");
                    fprintf(stderr, "\n");
                    if (x == 0xFFFF && y == 0xFFFF) break; /* objpos terminator */
                }
            }
        }
        fprintf(stderr, "=== /LEVEL ENTER ===\n\n");
    }
    /* [/DBG] */
}

/* ===================================================================
   LoadTilesFromStart
   =================================================================== */
void LoadTilesFromStart(void) {
    draw_chunks_plane(vram_fg,
                      (int16_t)RAM_WORD(0xF700),
                      (int16_t)RAM_WORD(0xF704),
                      RAM_ADDR(v_lvllayout_fg));
    draw_chunks_plane(vram_bg,
                      (int16_t)RAM_WORD(0xF708),
                      (int16_t)RAM_WORD(0xF70C),
                      RAM_ADDR(v_lvllayout_bg));
}

/* ===================================================================
   LevelDataLoad
   =================================================================== */
void LevelDataLoad(void) {
    uint8_t zone = (uint8_t)v_zone;

    if (zone >= sizeof(level_headers) / sizeof(level_headers[0])) return;
    const level_header *lp = &level_headers[zone];

    if (lp->map16) {
        uint16_t *buf = (uint16_t *)RAM_ADDR(v_16x16);
        EniDec(lp->map16, buf, ArtTile_Level);
    }
    if (lp->map256) {
        uint8_t *buf = RAM_ADDR(v_256x256);
        KosDec(lp->map256, buf);
    }

    LevelLayoutLoad();

    {
        uint16_t pal = lp->pal & 0xFF;
        if (RAM_U16(0xFE10) == id_LZ_act4) {
            pal = palid_SBZ3;
        } else if (RAM_U16(0xFE10) == id_SBZ_act2 || RAM_U16(0xFE10) == id_FZ) {
            pal = palid_SBZ2;
        }
        PalLoad_Fade(pal);
    }

    if (lp->plc2 != 0) {
        AddPLC(lp->plc2);
    }
}

/* ===================================================================
   NUEVO: Sistema de agua LZ (LZWaterFeatures + helpers)
   =================================================================== */

/* --- LZWindTunnels --- */
static void LZWindTunnels(void) {
    if (v_debuguse != 0) return;

    uint8_t *player = RAM_ADDR(v_player);
    uint8_t act = (uint8_t)v_act;

    int base = 8 + (act << 3);   /* byte offset en LZWind_Data */
    int count = 1;
    if (act == 0) { base = 0; count = 2; }

    for (int s = 0; s < count; s++) {
        /* a2 apunta a {left, top, right, bottom} en words */
        int word_off = (base >> 1) + s * 4;
        const int16_t *a2 = &LZWind_Data[word_off];

        int16_t son_x = obX(player);
        if ((uint16_t)son_x < (uint16_t)a2[0]) continue;
        if ((uint16_t)son_x >= (uint16_t)a2[2]) continue;

        int16_t son_y = obY(player);
        if ((uint16_t)son_y < (uint16_t)a2[1]) continue;
        if ((uint16_t)son_y >= (uint16_t)a2[3]) continue;

        /* Dentro del túnel: sonido cada $40 frames */
        if ((v_vblank_byte & 0x3F) == 0) {
            Sound_Queue(sfx_Waterfall, false);
        }

        if (f_wtunneldisallow) return;
        if (obRoutine(player) >= 4) {
            f_wtunnelmode = 0;
            return;
        }
        f_wtunnelmode = 1;

        int16_t d0 = (int16_t)(son_x - 128);
        if ((uint16_t)d0 < (uint16_t)a2[0]) {   /* zona de succión */
            int16_t dy = 2;
            if (act == act2) dy = -2;
            obY(player) = (int16_t)(obY(player) + dy);
        }

        obX(player)    = (int16_t)(obX(player) + 4);
        obVelX(player) = 0x400;
        obVelY(player) = 0;
        obAnim(player) = 0x0F;                 /* id_Float2 */
        obStatus(player) |= (1 << 1);

        if (v_jpadhold2 & btnUp) obY(player) -= 1;
        if (v_jpadhold2 & btnDn) obY(player) += 1;
        return;
    }

    /* .notInTunnel */
    if (f_wtunnelmode) {
        obAnim(player) = 0x00;                 /* id_Walk */
        f_wtunnelmode = 0;
    }
}

/* --- LZWaterSlides --- */
static void LZWaterSlides(void) {
    uint8_t *player = RAM_ADDR(v_player);

    if (obStatus(player) & (1 << 1)) goto exit_slide;

    {
        uint16_t d0 = (uint16_t)obY(player);
        d0 = (uint16_t)(d0 >> 1);
        d0 &= 0x380;
        uint8_t d1 = (uint8_t)((uint16_t)obX(player) >> 8);
        d1 &= 0x7F;
        d0 = (uint16_t)(d0 + d1);

        const uint8_t *layout = RAM_ADDR(v_lvllayout_fg);
        uint8_t chunk_id = layout[d0];

        int found = -1;
        for (int i = 6; i >= 0; i--) {
            if (Slide_Chunks[i] == chunk_id) { found = i; break; }
        }
        if (found < 0) goto exit_slide;

        /* LZSlide_Move */
        obStatus(player) &= (uint8_t)~(1 << 0);
        int8_t speed = Slide_Speeds[found];
        obInertia(player) = (int16_t)((uint8_t)speed << 8);
        if (speed < 0) obStatus(player) |= (1 << 0);

        obAnim(player) = 0x1B;                 /* id_Slide */
        f_slidemode = 1;

        if ((v_vblank_byte & 0x1F) == 0) {
            Sound_Queue(sfx_Waterfall, false);
        }
        return;
    }

exit_slide:
    if (f_slidemode) {
        locktime(player) = 5;
        f_slidemode = 0;
    }
}

/* --- LZDynamicWater --- */

static void DynWater_LZ1(void) {
    int16_t d0 = (int16_t)v_screenposx;
    uint8_t r = v_wtr_routine;
    uint8_t *player = RAM_ADDR(v_player);
    int16_t d1;

    if (r != 0) {
        if (r != 1) return;
        if ((int16_t)obY(player) >= 0x2E0) return;
        d1 = 0x03A8;
        if (d0 >= 0x1300) {
            d1 = 0x0108;
            v_wtr_routine = 2;
        }
        v_waterpos3 = (uint16_t)d1;
        return;
    }

    d1 = 0x00B8;
    if (d0 < 0x600) goto set_target;
    d1 = 0x0108;
    if ((int16_t)obY(player) < 0x200) goto secret_top;
    if (d0 < 0xC00) goto set_target;
    d1 = 0x0318;
    if (d0 < 0x1080) goto set_target;
    RAM_BYTE(f_switch + 5) = 0x80;
    d1 = 0x05C8;
    if (d0 < 0x1380) goto set_target;
    d1 = 0x03A8;
    if (v_waterpos2 == (uint16_t)d1) v_wtr_routine = 1;

set_target:
    v_waterpos3 = (uint16_t)d1;
    return;

secret_top:
    if (d0 < 0xC80)  goto set_target;
    d1 = 0x00E8;
    if (d0 < 0x1500) goto set_target;
    d1 = 0x0108;
    goto set_target;
}

static void DynWater_LZ2(void) {
    int16_t d0 = (int16_t)v_screenposx;
    int16_t d1;

    d1 = 0x0328;
    if (d0 < 0x500) goto set_target;
    d1 = 0x03C8;
    if (d0 < 0xB00) goto set_target;
    d1 = 0x0428;

set_target:
    v_waterpos3 = (uint16_t)d1;
}

static void DynWater_LZ3(void) {
    int16_t d0 = (int16_t)v_screenposx;
    uint8_t r = v_wtr_routine;
    uint8_t *player = RAM_ADDR(v_player);
    int16_t d1;

    if (r == 0) {
        d1 = 0x0900;
        if (d0 < 0x600) goto set_target_instant_r0;
        if ((int16_t)obY(player) < 0x3C0) goto set_target_instant_r0;
        if ((int16_t)obY(player) >= 0x600) goto set_target_instant_r0;
        d1 = 0x04C8;
        RAM_BYTE(v_lvllayout_fg + (layout_row * 2) + 6) = 0x4B;
        v_wtr_routine = 1;
        Sound_Queue(sfx_Rumbling, false);

set_target_instant_r0:
        v_waterpos3 = (uint16_t)d1;
        v_waterpos2 = (uint16_t)d1;
        return;
    }

    if (r == 1) {
        d1 = 0x04C8;
        if (d0 < 0x770) goto set_target;
        d1 = 0x0308;
        if (d0 < 0x1400) goto set_target;

        if (v_waterpos3 != 0x0508) {
            if ((int16_t)obY(player) >= 0x600) goto check_end;
            if ((int16_t)obY(player) <  0x280) goto set_target;
        }
check_end:
        d1 = 0x0508;
        v_waterpos2 = (uint16_t)d1;
        if (d0 < 0x1770) goto set_target;
        v_wtr_routine = 2;
        goto set_target;
    }

    if (r == 2) {
        d1 = 0x0508;
        if (d0 < 0x1860) goto set_target;
        d1 = 0x0188;
        if (d0 >= 0x1AF0) { v_wtr_routine = 3; goto set_target; }
        if (v_waterpos2 != (uint16_t)d1) goto set_target;
        v_wtr_routine = 3;
        goto set_target;
    }

    if (r == 3) {
        d1 = 0x0188;
        if (d0 < 0x1AF0) goto set_target_instant_r3;
        d1 = 0x0900;
        if (d0 < 0x1BC0) goto set_target_instant_r3;
        v_wtr_routine = 4;
        v_waterpos3 = 0x0608;
        v_waterpos2 = 0x07C0;
        RAM_BYTE(f_switch + 8) = 1;
        return;

set_target_instant_r3:
        v_waterpos3 = (uint16_t)d1;
        v_waterpos2 = (uint16_t)d1;
        return;
    }

    /* r >= 4: FixBugs=0 -> check right side of tunnel */
    if (d0 < 0x1E00) return;
    v_waterpos3 = 0x0128;
    return;

set_target:
    v_waterpos3 = (uint16_t)d1;
}

static void DynWater_SBZ3(void) {
    int16_t d0 = (int16_t)v_screenposx;
    int16_t d1;

    d1 = 0x0228;
    if (d0 < 0xF00) goto set_target;
    d1 = 0x04C8;

set_target:
    v_waterpos3 = (uint16_t)d1;
}

static void LZDynamicWater(void) {
    switch ((uint8_t)v_act) {
        case 0: DynWater_LZ1();  break;
        case 1: DynWater_LZ2();  break;
        case 2: DynWater_LZ3();  break;
        case 3: DynWater_SBZ3(); break;
    }

    int16_t diff = (int16_t)(v_waterpos3 - v_waterpos2);
    if (diff == 0) return;
    int16_t step = f_water ? 1 : 0;
    if (diff < 0) step = (int16_t)(-step);
    v_waterpos2 = (uint16_t)(v_waterpos2 + step);
}

/* --- LZWaterFeatures: dispatcher per-frame --- */
void LZWaterFeatures(void) {
    if (v_zone != id_LZ) return;

    /* Revision 1: tst.b (f_nobgscroll).w / bne.s .setWaterHeight
       (set while Sonic is drowning by $0A Drown_Countdown). */
    if (f_nobgscroll) goto set_water_height;

    if (obRoutine(RAM_ADDR(v_player)) >= 6) goto set_water_height;

    LZWindTunnels();
    LZWaterSlides();
    LZDynamicWater();

set_water_height:
    f_wtr_state = 0;
    {
        int16_t d0 = (int16_t)(uint8_t)RAM_BYTE(v_oscillate + 2);
        d0 = (int16_t)(d0 >> 1);
        d0 = (int16_t)(d0 + (int16_t)v_waterpos2);
        v_waterpos1 = (uint16_t)d0;

        d0 = (int16_t)(d0 - (int16_t)v_screenposy);

        if (d0 < 0) {
            v_hblank_line = 223;
            f_wtr_state   = 1;
        }
        if ((uint16_t)d0 >= 223) d0 = 223;
        v_hblank_line = (uint8_t)d0;
    }
}

/* ===================================================================
   ConvertCollisionArray
   =================================================================== */
void ConvertCollisionArray(void) {
    /* no-op */
}

/* ===================================================================
   Collision index
   =================================================================== */
const uint8_t *col_index_ptr = NULL;

const uint8_t *GetColIndex(void) {
    return col_index_ptr;
}

void ColIndexLoad(void) {
    const uint8_t *const col_pointers[] = {
        Col_GHZ, Col_LZ, Col_MZ, Col_SLZ, Col_SYZ, Col_SBZ,
    };

    uint8_t zone = (uint8_t)v_zone;
    if (zone >= (uint8_t)(sizeof(col_pointers) / sizeof(col_pointers[0]))) return;
    col_index_ptr = col_pointers[zone];
}

/* ===================================================================
   OscillateNumInit
   =================================================================== */
void OscillateNumInit(void) {
    static const uint16_t baselines[] = {
        0x007C,
        0x0080, 0x0000, 0x0080, 0x0000, 0x0080, 0x0000, 0x0080, 0x0000,
        0x0080, 0x0000, 0x0080, 0x0000, 0x0080, 0x0000, 0x0080, 0x0000,
        0x0080, 0x0000, 0x0080, 0x0000, 0x50F0, 0x011E, 0x2080, 0x00B4,
        0x3080, 0x010E, 0x5080, 0x01C2, 0x7080, 0x0276, 0x0080, 0x0000,
    };   /* 33 words: $FF5E bitfield + $FF60..$FF9F (16 value/rate pairs) */

    for (size_t i = 0; i < sizeof(baselines) / sizeof(baselines[0]); i++) {
        RAM_SET_U16(0xFE5E + (uint32_t)i * 2, baselines[i]);
    }
}

/* ===================================================================
   OscillateNumDo
   =================================================================== */
void OscillateNumDo(void) {
    if (obRoutine(RAM_ADDR(v_player)) >= 6) return;

    static const uint16_t settings[][2] = {
        {2, 0x10}, {2, 0x18}, {2, 0x20}, {2, 0x30},
        {4, 0x20}, {8, 0x08}, {8, 0x40}, {4, 0x40},
        {2, 0x50}, {2, 0x50}, {2, 0x20}, {3, 0x30},
        {5, 0x50}, {7, 0x70}, {2, 0x10}, {2, 0x10},
    };

    uint16_t d3 = RAM_U16(0xFE5E);

    for (int i = 0; i < 16; i++) {
        int bit = 15 - i;
        uint32_t addr = 0xFE5E + 2 + (uint32_t)i * 4;
        uint16_t d2 = settings[i][0];
        uint16_t d4 = settings[i][1];
        int16_t  rate = (int16_t)RAM_U16(addr + 2);
        uint16_t value = RAM_U16(addr);

        if (d3 & (1u << bit)) {
            rate  = (int16_t)(rate - (int16_t)d2);
            value = (uint16_t)(value + (uint16_t)rate);
            RAM_SET_U16(addr + 2, (uint16_t)rate);
            RAM_SET_U16(addr, value);
            uint16_t msb = value >> 8;
            if (d4 <= msb) continue;
            d3 &= (uint16_t)~(1u << bit);
        } else {
            rate  = (int16_t)(rate + (int16_t)d2);
            value = (uint16_t)(value + (uint16_t)rate);
            RAM_SET_U16(addr + 2, (uint16_t)rate);
            RAM_SET_U16(addr, value);
            uint16_t msb = value >> 8;
            if (d4 > msb) continue;
            d3 |= (1u << bit);
        }
    }

    RAM_SET_U16(0xFE5E, d3);
}

/* ===================================================================
   PauseGame
   =================================================================== */
void PauseGame(void) {
    if (v_lives == 0) goto unpauseGame;
    if (f_pause != 0) goto startPause;
    if (!(v_jpadpress1 & btnStart)) return;

startPause:
    f_pause = 1;

pauseLoop:
    v_vblank_routine = id_VBlank_Paused;
    WaitForVBlank();

    if (!f_slomocheat) goto checkUnpausing;
    if (v_jpadpress1 & btnA) { v_gamemode = 0x04; goto unpauseMusic; }
    if (v_jpadhold1 & btnB) goto slowMotion;
    if (v_jpadpress1 & btnC) goto slowMotion;

checkUnpausing:
    if (!(v_jpadpress1 & btnStart)) goto pauseLoop;

unpauseMusic:
unpauseGame:
    f_pause = 0;
    return;

slowMotion:
    f_pause = 1;
    return;
}

/* ===================================================================
   Spawn player / HUD
   =================================================================== */
void LevelSpawnPlayer(void) {
    uint8_t *player_slot = RAM_ADDR(v_player);
    obID(player_slot) = 0x01;
}

void LevelSpawnHUD(void) {
    uint8_t *hud_slot = RAM_ADDR(v_hud);
    obID(hud_slot) = 0x21;
}

/* ===================================================================
   ObjPosLoad (sin cambios)
   =================================================================== */
static uint8_t *opl_ptr_right;
static uint8_t *opl_ptr_left;
static uint8_t *opl_ptr_sec;

static uint16_t opl_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static int OPL_SpawnObj(uint8_t **a0p, uint8_t *a2, uint8_t d2) {
    uint8_t *a0 = *a0p;
    uint8_t *a1;
    uint16_t d0;

    if (a0[4] & 0x80) {
        uint8_t old = a2[2 + d2];
        a2[2 + d2] = (uint8_t)(old | 0x80);
        if (old & 0x80) { a0 += 6; *a0p = a0; return 0; }
    }

    a1 = (uint8_t *)FindFreeObj();
    if (!a1) {
        /* [DBG] */
        fprintf(stderr, "[OPL_Spawn] NO FREE SLOT for id=$%02X\n", a0[4] & 0x7F);
        /* [/DBG] */
        return 1;
    }

    obX(a1) = (int16_t)opl_be16(a0); a0 += 2;
    d0 = opl_be16(a0); a0 += 2;
    obY(a1) = (int16_t)(d0 & 0x0FFF);
    obRender(a1) = (uint8_t)((d0 & 0x4000) ? sprite_xflip : 0)
                 | (uint8_t)((d0 & 0x8000) ? sprite_yflip : 0);
    obStatus(a1) = obRender(a1);
    d0 = a0[0]; a0 += 1;
    if (d0 & 0x80) obRespawnNo(a1) = d2;
    obID(a1)       = (uint8_t)(d0 & 0x7F);
    obSubtype(a1)  = a0[0]; a0 += 1;
    *a0p = a0;

    /* [DBG] */
    fprintf(stderr, "[OPL_Spawn] slot=%d id=$%02X sub=$%02X X=$%04X Y=$%04X d2=%d\n",
            Object_GetIndex(a1), obID(a1), obSubtype(a1),
            obX(a1), obY(a1), d2);
    /* [/DBG] */
    return 0;
}

static void OPL_Next(void);

static void OPL_Main(void) {
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t *a0, *start;
    uint16_t d6;
    uint8_t zone = (uint8_t)v_zone;
    uint8_t act  = (uint8_t)v_act;

    unsigned row = zone * 4 + act;
    if (row >= sizeof(objpos_index) / sizeof(objpos_index[0]) ||
        objpos_index[row].main == NULL) {
        v_opl_routine = 0;
        return;
    }

    v_opl_routine = (uint8_t)(v_opl_routine + 2);

    a0 = (uint8_t *)objpos_index[row].main;
    opl_ptr_right = a0;
    opl_ptr_left  = a0;
    opl_ptr_sec   = NULL;

    /* [DBG] */
    fprintf(stderr, "[OPL_Main] row=%u objpos=%p firstX=$%04X\n",
            row, (void*)a0, (uint16_t)((a0[0]<<8)|a0[1]));
    /* [/DBG] */

    *a2 = 0x01;
    *(a2 + 1) = 0x01;
    a2 += 2;
    for (int i = 0x5E; i >= 0; i--) { *(uint32_t *)a2 = 0; a2 += 4; }

    /* ═══════════════════════════════════════════════════════════════
       FIX: el ASM reinicia a2 a v_objstate tras limpiar la lista.
       Sin esto, los contadores de respawn del primer objeto se
       corrompen y se queda como 1 permanente.
       ═══════════════════════════════════════════════════════════════ */
    a2 = RAM_ADDR(v_objstate);

    d6 = (uint16_t)v_screenposx;
    if (d6 >= 128) d6 -= 128;
    else           d6 = 0;
    d6 &= 0xFF80;

    a0   = opl_ptr_right;
    start = a0;
    int count_right = 0;
    while (opl_be16(a0) < d6) {
        if (a0[4] & 0x80) (*a2)++;
        a0 += 6;
        count_right++;
    }
    opl_ptr_right = a0;
    /* [DBG] */
    fprintf(stderr, "[OPL_Main] d6=$%04X right scan: %d objs, counter[0]=%d\n",
            d6, count_right, *a2);
    /* [/DBG] */

    a0 = start;
    if (d6 >= 128) {
        d6 -= 128;
        while (opl_be16(a0) < d6) {
            if (a0[4] & 0x80) (*(a2 + 1))++;
            a0 += 6;
        }
    }
    opl_ptr_left = a0;

    /* [DBG] */
    fprintf(stderr, "[OPL_Main] counter[1]=%d, opl_screen will start at -1\n",
            *(a2 + 1));
    /* [/DBG] */

    v_opl_screen = 0xFFFF;
    OPL_Next();
}

static void OPL_MovedLeft(uint16_t d6) {
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t *a0;
    uint8_t d2 = 0;
    int16_t d6s;

    v_opl_screen = d6;
    a0 = opl_ptr_left;
    d6s = (int16_t)d6 - 128;
    if (d6s >= 0) {
        while (1) {
            uint16_t cx = opl_be16(a0 - 6);
            if ((int16_t)cx <= d6s) break;
            a0 -= 6;
            if (a0[4] & 0x80) { (*(a2 + 1))--; d2 = *(a2 + 1); }
            if (OPL_SpawnObj(&a0, a2, d2)) {
                if (a0[4] & 0x80) (*(a2 + 1))++;
                a0 += 6;
                break;
            }
            a0 -= 6;
        }
    }
    opl_ptr_left = a0;

    a0 = opl_ptr_right;
    d6s += 128 + 320 + 320;
    while (1) {
        uint16_t cx = opl_be16(a0 - 6);
        if (d6s > (int16_t)cx) break;
        if (a0[-2] & 0x80) (*a2)--;
        a0 -= 6;
    }
    opl_ptr_right = a0;
}

static void OPL_MovedRight(uint16_t d6) {
    uint8_t *a2 = RAM_ADDR(v_objstate);
    uint8_t *a0;
    uint8_t d2 = 0;
    uint16_t d6u;

    v_opl_screen = d6;
    a0 = opl_ptr_right;
    d6u = d6 + 320 + 320;

    while (1) {
        if (opl_be16(a0) >= d6u) break;
        if (a0[4] & 0x80) { d2 = *a2; (*a2)++; }
        if (OPL_SpawnObj(&a0, a2, d2)) break;
    }
    opl_ptr_right = a0;

    a0 = opl_ptr_left;
    if (d6u >= 768) {
        d6u -= 320 + 320 + 128;
        while (1) {
            if (opl_be16(a0) >= d6u) break;
            if (a0[4] & 0x80) (*(a2 + 1))++;
            a0 += 6;
        }
    }
    opl_ptr_left = a0;
}

static void OPL_Next(void) {
    uint16_t d6;
    int16_t prev;

    if (opl_ptr_right == NULL) return;
    d6 = (uint16_t)v_screenposx & 0xFF80;
    prev = (int16_t)v_opl_screen;
    if ((int16_t)d6 == prev) return;
    if ((int16_t)d6 >= prev) OPL_MovedRight(d6);
    else                     OPL_MovedLeft(d6);
}

void ObjPosLoad(void) {
    if (v_opl_routine == 0) OPL_Main();
    else                    OPL_Next();
}

/* ===================================================================
   AnimateLevelAct (sin cambios)
   =================================================================== */
static void load_tiles_vram(const uint8_t *src, uint32_t vram_byte, int count) {
    VDP_WriteVRAM(src, vram_byte, (uint32_t)count * tile_size);
}

static void anis_giant_ring(void) {
    if (v_gfxbigring == 0) return;
    v_gfxbigring = (uint16_t)(v_gfxbigring - 14 * tile_size);
    if ((int16_t)v_gfxbigring < 0) { v_gfxbigring = 0; return; }
    const uint8_t *a1 = Art_BigRing + v_gfxbigring;
    if ((uint32_t)v_gfxbigring + 14 * tile_size > Art_BigRing_len) return;
    load_tiles_vram(a1, ArtTile_Giant_Ring * tile_size + v_gfxbigring, 14);
}

static const uint8_t flower_seq[4] = { 0, 1, 2, 1 };

static void anis_ghz(void) {
    if (!Art_GhzWater || !Art_GhzFlower1 || !Art_GhzFlower2) return;

    if ((int8_t)(--v_lani0_time) < 0) {
        v_lani0_time = 6 - 1;
        const uint8_t *a1 = Art_GhzWater;
        uint8_t d0 = v_lani0_frame;
        v_lani0_frame++;
        if (d0 & 1) a1 += 8 * tile_size;
        load_tiles_vram(a1, ArtTile_GHZ_Waterfall * tile_size, 8);
    }

    if ((int8_t)(--v_lani1_time) < 0) {
        v_lani1_time = 16 - 1;
        const uint8_t *a1 = Art_GhzFlower1;
        uint8_t d0 = v_lani1_frame;
        v_lani1_frame++;
        if (d0 & 1) a1 += 16 * tile_size;
        load_tiles_vram(a1, ArtTile_GHZ_Big_Flower_1 * tile_size, 16);
    }

    if ((int8_t)(--v_lani2_time) >= 0) return;
    v_lani2_time = 8 - 1;
    uint8_t d0 = v_lani2_frame;
    v_lani2_frame++;
    d0 &= 3;
    d0 = flower_seq[d0];
    if (!(d0 & 1)) v_lani2_time = 128 - 1;
    uint32_t off = (uint32_t)d0 * 3 * 0x80;
    load_tiles_vram(Art_GhzFlower2 + off, ArtTile_GHZ_Small_Flower * tile_size, 12);
}

static const uint8_t magma_col_start[16] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
};

static void anis_mz(void) {
    if (!Art_MzLava1 || !Art_MzLava2 || !Art_MzTorch) return;

    if ((int8_t)(--v_lani0_time) < 0) {
        v_lani0_time = 20 - 1;
        uint8_t d0 = v_lani0_frame;
        v_lani0_frame = (uint8_t)((d0 + 1) % 3);
        d0 = v_lani0_frame;
        load_tiles_vram(Art_MzLava1 + (uint32_t)d0 * (8 * tile_size),
                        ArtTile_MZ_Animated_Lava * tile_size, 8);
    }

    if ((int8_t)(--v_lani1_time) < 0) {
        v_lani1_time = 2 - 1;
        v_lani1_frame++;
        const uint8_t *a4 = Art_MzLava2 + ((uint32_t)v_lani0_frame << 9);
        uint16_t d3 = (uint16_t)ram[0xFE5E + 0xA];
        uint8_t *dst = &vdp.vram[ArtTile_MZ_Animated_Magma * tile_size];
        for (int iter = 0; iter < 4; iter++) {
            int j = ((d3 * 2) & 0x1E) >> 1;
            j = magma_col_start[j];
            const uint8_t *a1 = a4;
            for (int row = 0; row < 0x20; row++) {
                for (int b = 0; b < 4; b++) dst[b] = a1[(j + b) & 15];
                dst += 4;
                a1 += 0x10;
            }
            d3 += 4;
        }
    }

    if ((int8_t)(--v_lani2_time) < 0) {
        v_lani2_time = 8 - 1;
        uint8_t d0 = v_lani3_frame;
        v_lani3_frame++;
        v_lani3_frame &= 3;
        if ((uint32_t)d0 * (6 * tile_size) + 6 * tile_size <= Art_MzTorch_len) {
            load_tiles_vram(Art_MzTorch + (uint32_t)d0 * (6 * tile_size),
                            ArtTile_MZ_Torch * tile_size, 6);
        }
    }
}

static void anis_sbz(void) {
    if (!Art_SbzSmoke) return;

    if (v_lani2_frame != 0) {
        v_lani2_frame--;
        goto check_smokePuff2;
    }
    if ((int8_t)(--v_lani0_time) >= 0) goto check_smokePuff2;
    v_lani0_time = 8 - 1;
    {
        uint8_t d0 = v_lani0_frame;
        v_lani0_frame++;
        d0 &= 7;
        if (d0 != 0) {
            d0--;
            load_tiles_vram(Art_SbzSmoke + (uint32_t)d0 * (12 * tile_size),
                            ArtTile_SBZ_Smoke_Puff_1 * tile_size, 12);
            return;
        }
        v_lani2_frame = 3 * 60;
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_1 * tile_size, 6);
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_1 * tile_size + 6 * tile_size, 6);
    }

check_smokePuff2:
    if (v_lani2_time != 0) { v_lani2_time--; return; }
    if ((int8_t)(--v_lani1_time) >= 0) return;
    v_lani1_time = 8 - 1;
    {
        uint8_t d0 = v_lani1_frame;
        v_lani1_frame++;
        d0 &= 7;
        if (d0 != 0) {
            d0--;
            load_tiles_vram(Art_SbzSmoke + (uint32_t)d0 * (12 * tile_size),
                            ArtTile_SBZ_Smoke_Puff_2 * tile_size, 12);
            return;
        }
        v_lani2_time = 2 * 60;
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_2 * tile_size, 6);
        load_tiles_vram(Art_SbzSmoke, ArtTile_SBZ_Smoke_Puff_2 * tile_size + 6 * tile_size, 6);
    }
}

static const uint8_t ending_flower2_seq[8] = { 0, 0, 0, 1, 2, 2, 2, 1 };
static const uint8_t ending_flower34_seq[4] = { 0, 1, 2, 1 };

static void anis_ending(void) {
    if (!Art_GhzFlower1 || !Art_GhzFlower2) return;

    if ((int8_t)(--v_lani1_time) < 0) {
        v_lani1_time = 8 - 1;
        const uint8_t *a1 = Art_GhzFlower1;
        uint8_t *a2 = RAM_ADDR(v_256x256 + 0x4A * chunk_size);
        uint8_t d0 = v_lani1_frame;
        v_lani1_frame++;
        if (d0 & 1) { a1 += 16 * tile_size; a2 += 16 * tile_size; }
        load_tiles_vram(a1, ArtTile_GHZ_Big_Flower_1 * tile_size, 16);
        load_tiles_vram(a2, ArtTile_GHZ_Big_Flower_2 * tile_size, 16);
    }

    if ((int8_t)(--v_lani2_time) < 0) {
        v_lani2_time = 8 - 1;
        uint8_t d0 = v_lani2_frame;
        v_lani2_frame++;
        d0 &= 7;
        d0 = ending_flower2_seq[d0];
        load_tiles_vram(Art_GhzFlower2 + (uint32_t)d0 * 3 * 0x80,
                        ArtTile_GHZ_Small_Flower * tile_size, 12);
    }

    if ((int8_t)(--v_lani4_time) < 0) {
        v_lani4_time = 15 - 1;
        uint8_t d0 = v_lani4_frame;
        v_lani4_frame++;
        d0 &= 3;
        d0 = ending_flower34_seq[d0];
        uint32_t off = (uint32_t)d0 * 2 * 0x100;
        load_tiles_vram(RAM_ADDR(v_256x256 + 0x4C * chunk_size) + off,
                        ArtTile_GHZ_Flower_3 * tile_size, 16);
    }

    if ((int8_t)(--v_lani5_time) >= 0) return;
    v_lani5_time = 12 - 1;
    uint8_t d0 = v_lani5_frame;
    v_lani5_frame++;
    d0 &= 3;
    d0 = ending_flower34_seq[d0];
    uint32_t off = (uint32_t)d0 * 2 * 0x100;
    load_tiles_vram(RAM_ADDR(v_256x256 + 0x4F * chunk_size) + off,
                    ArtTile_GHZ_Flower_4 * tile_size, 16);
}

static void anis_none(void) {}

static const void (*const aniart_index[])(void) = {
    anis_ghz, anis_none, anis_mz, anis_none, anis_none, anis_sbz, anis_ending,
};

void AnimateLevelAct(void) {
    if (f_pause != 0) return;
    anis_giant_ring();
    uint8_t zone = (uint8_t)v_zone;
    if (zone >= (uint8_t)(sizeof(aniart_index) / sizeof(aniart_index[0]))) return;
    aniart_index[zone]();
}

/* ===================================================================
   PaletteCycle (sin cambios)
   =================================================================== */
static void palcycle_ghz(void) {
    const uint8_t *tab = Pal_GHZCycWater;
    if (!tab || Pal_GHZCycWater_len < 32) return;

    v_pcyc_time = (uint16_t)(v_pcyc_time - 1);
    if ((v_pcyc_time & 0x8000) == 0) return;
    v_pcyc_time = 6 - 1;

    uint16_t d0 = v_pcyc_num & 3;
    v_pcyc_num = (uint16_t)(v_pcyc_num + 1);
    int base = (int)d0 * 4;

    uint16_t *a1 = (uint16_t *)RAM_ADDR(v_palette_line_3 + (8 * 2));
    for (int i = 0; i < 4; i++) {
        uint16_t color = ((uint16_t)tab[(base + i) * 2] << 8) | tab[(base + i) * 2 + 1];
        a1[i] = color;
        vdp.cram[40 + i] = color;
    }
}

static void palcycle_none(void) {}
#define palcycle_todo palcycle_none

static const void (*const palcycle_index[])(void) = {
    palcycle_ghz, palcycle_todo, palcycle_none, palcycle_todo,
    palcycle_todo, palcycle_todo, palcycle_ghz,
};

void PaletteCycle(void) {
    uint8_t zone = (uint8_t)v_zone;
    if (zone >= (uint8_t)(sizeof(palcycle_index) / sizeof(palcycle_index[0]))) return;
    palcycle_index[zone]();
}

/* ===================================================================
   SignpostArtLoad (sin cambios)
   =================================================================== */
void SignpostArtLoad(void) {
    if (v_debuguse != 0) return;
    if (v_act == act3) return;

    int16_t d0 = (int16_t)RAM_WORD(0xF700);
    int16_t d1 = (int16_t)v_limitright2 - 0x100;
    if (d0 < d1) return;
    if (f_timecount == 0) return;
    if (d1 == (int16_t)v_limitleft2) return;

    v_limitleft2 = (uint16_t)d1;
    NewPLC(plcid_Signpost);
}

void MoveSonicInDemo(void) {}

/* ===================================================================
   Level_Process (sin cambios)
   =================================================================== */
void Level_Process(void) {
    if (g_last_mode != (uint8_t)0x0C || !level_init_done) {
        Level_Enter();
        level_init_done = 1;
        return;
    }

    PauseGame();
    v_vblank_routine = id_VBlank_Levels;
    WaitForVBlank();
    v_framecount = v_framecount + 1;

    MoveSonicInDemo();
    LZWaterFeatures();
    ExecuteObjects();

    if (f_restart) {
        level_init_done = 0;
        return;
    }

    if (v_debuguse == 0 && obRoutine(&ram[v_player]) >= 6) {
        /* Sonic dying — skip */
    } else {
        DeformLayers();
    }

    BuildSprites();
    ObjPosLoad();
    PaletteCycle();
    RunPLC();
    OscillateNumDo();
    SynchroAnimate();
    SignpostArtLoad();
}