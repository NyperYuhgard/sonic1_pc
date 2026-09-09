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
   by v_zone*$10). Byte offsets match the ASM layout; the gfx/map16/
   map256 pointer fields are unused here (asset pointers come from
   data.c), so only plc1/plc2/pal are looked up.
   =================================================================== */
typedef struct {
    uint8_t  plc1;      /* +0:   first level PLC id */
    uint8_t  gfx[3];    /* +1..3: level gfx pointer (unused) */
    uint8_t  plc2;      /* +4:   second level PLC id */
    uint8_t  map16[3];  /* +5..7: 16x16 block data pointer (unused) */
    uint8_t  map256[4]; /* +8..B: 256x256 chunk data pointer (unused) */
    uint8_t  reserved;  /* +C:   0 */
    uint8_t  music;     /* +D:   music (unused; MusicList used instead) */
    uint8_t  pal;       /* +E:   palette id */
    uint8_t  pal2;      /* +F:   palette id (duplicate) */
} level_header;

#define LHEAD(plc1, plc2, music, pal) \
    { plc1, {0,0,0}, plc2, {0,0,0}, {0,0,0,0}, 0, music, pal, pal }

static const level_header level_headers[] = {
    /*                                         music     palette      */
    LHEAD(plcid_GHZ,  plcid_GHZ2, bgm_GHZ, palid_GHZ),    /* 0: Green Hill */
    LHEAD(plcid_LZ,   plcid_LZ2,   bgm_LZ,  palid_LZ),    /* 1: Labyrinth */
    LHEAD(plcid_MZ,   plcid_MZ2,   bgm_MZ,  palid_MZ),    /* 2: Marble */
    LHEAD(plcid_SLZ,  plcid_SLZ2,  bgm_SLZ, palid_SLZ),   /* 3: Star Light */
    LHEAD(plcid_SYZ,  plcid_SYZ2,  bgm_SYZ, palid_SYZ),   /* 4: Spring Yard */
    LHEAD(plcid_SBZ,  plcid_SBZ2,  bgm_SBZ, palid_SBZ1),  /* 5: Scrap Brain */
    LHEAD(0,          0,           bgm_SBZ, palid_Ending),/* 6: Ending */
};

/* ===================================================================
   Level size loading (from _inc/LevelSizeLoad & BgScrollSpeed.asm)
   =================================================================== */

/* Level size array (from _inc/LevelSizeArray.asm): one 6-word entry per act:
   <unused=$0004> <left> <right> <top> <bottom> <lookshift=$0060> */
static const uint16_t level_size_array[][6] = {
    /*                                                  GHZ1 */
    { 0x0004, 0x0000, 0x24BF, 0x0000, 0x0300, 0x0060 },
    /*                                                  GHZ2 */
    { 0x0004, 0x0000, 0x1EBF, 0x0000, 0x0300, 0x0060 },
    /*                                                  GHZ3 */
    { 0x0004, 0x0000, 0x2960, 0x0000, 0x0300, 0x0060 },
    /*                                                  GHZ4 (unused) */
    { 0x0004, 0x0000, 0x2ABF, 0x0000, 0x0300, 0x0060 },
};

void LevelSizeLoad(void) {
    uint8_t zone = (uint8_t)(v_zone_act >> 8);
    uint8_t act  = (uint8_t)(v_zone_act & 0xFF);

    /* Clear level-change variables */
    v_unused7 = 0;
    v_unused8 = 0;
    v_unused9 = 0;
    v_unused10 = 0;
    v_dle_routine = 0;
    f_nobgscroll = 0;

    /* LevelSizeArray entry index = zone*4 + act */
    uint32_t idx = (uint32_t)zone * 4 + act;
    if (idx < sizeof(level_size_array) / sizeof(level_size_array[0])) {
        const uint16_t *e = level_size_array[idx];

        v_unused11      = e[0];                 /* always $0004 */
        v_limitleft2    = v_limitleft1  = e[1];
        v_limitright2   = v_limitright1 = e[2];
        v_limittop2     = v_limittop1   = e[3];
        v_limitbtm2     = v_limitbtm1   = e[4];
        v_lookshift     = e[5];                 /* always $0060 */
        v_limitleft3    = v_limitleft2 + 0x240;

        /* Trigger drawing of a whole column on next frame */
        v_fg_xblock = 0x10;
        v_fg_yblock = 0x10;
    }

    /* Start location. The title screen (FixBugs) uses a fixed spot to avoid
       conflicts with GHZ1's start location. */
    int16_t startX = 0x0050;
    int16_t startY = 0x03B0;
    obX(&ram[v_player]) = startX;
    obY(&ram[v_player]) = startY;

    /* LevSz_InitCameraPositions */
    int32_t camX = startX - 160;                /* center Sonic horizontally */
    if (camX < 0) camX = 0;
    if (camX >= (int32_t)v_limitright2) camX = v_limitright2;

    int32_t camY = startY - 96;                 /* center Sonic vertically */
    if (camY < 0) camY = 0;
    if (camY >= (int32_t)v_limitbtm2) camY = v_limitbtm2;

    RAM_WORD(0xF700) = (uint16_t)camX;          /* v_screenposx */
    RAM_WORD(0xF704) = (uint16_t)camY;          /* v_screenposy */
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

void LevelLayoutLoad(void) {
    uint8_t zone = (uint8_t)(v_zone_act >> 8);
    uint8_t act  = (uint8_t)(v_zone_act & 0xFF);

    /* Clear the entire layout buffer (FixBugs) */
    memset(RAM_ADDR(v_lvllayout), 0, v_lvllayout_end - v_lvllayout);

    if (zone != 0) return;   /* only GHZ loaded so far */

    /* Level_Index (sonic.asm): all GHZ acts share the GHZ1 FG; GHZ4 (unused)
       falls back to the background blob. */
    const uint8_t *fg;
    const uint8_t *bg;
    switch (act) {
    case 3:  fg = Level_GHZbg;  bg = Level_GHZbg;  break;
    default: fg = Level_GHZ1;   bg = Level_GHZbg;  break;
    }
    if (!fg || !bg) return;

    level_layout_load2(fg, RAM_ADDR(v_lvllayout_fg));
    level_layout_load2(bg, RAM_ADDR(v_lvllayout_bg));
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
    if (chunk_id == 0) return;                  /* empty chunk, skip */

    /* Chunk RAM address: (chunk_id-1) * $200 */
    uint32_t chunk_off = (uint32_t)((chunk_id - 1) & 0x7F) * chunk_size;

    /* Block cell within the chunk: 2 bytes per 16x16 block */
    int cell_y = (ly * 2) & 0x1E0;
    int cell_x = ((lx >> 3) & 0x1E);
    const uint8_t *cell = RAM_ADDR(v_256x256) + chunk_off + cell_y + cell_x;

    /* Cell word: block ID (low byte + low 2 bits of flag byte) */
    uint16_t cell_word = data_be16(cell);
    uint16_t block_id  = cell_word & 0x3FF;

    /* Block data: 4 words (TL, TR, BL, BR) in RAM, native order from EniDec */
    const uint16_t *blk = (const uint16_t *)RAM_ADDR(v_16x16) + block_id * 4;
    uint16_t t[4];
    for (int i = 0; i < 4; i++) t[i] = blk[i];

    /* Flipping */
    int flip_x = (cell[0] >> 3) & 1;
    int flip_y = (cell[0] >> 4) & 1;

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
   Level entry (once per level, before first frame)
   Ported from sonic.asm GM_Level lines 2702-2957
   =================================================================== */
static void Level_Enter(void) {
    /* ------------------------------------------------------------------
       Phase A: Fade out + clear PLC (sonic.asm:2702-2712)
       ------------------------------------------------------------------ */
    /* bset #7, v_gamemode — mark as "in pre-level sequence" */
    v_gamemode = 0x8C;  /* GM_Level | 0x80 */

    /* Fade out music */
    Sound_Queue(bgm_Fade, false);

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
    VDP_SetRegister(0x0B, 0x00);  /* full-screen vertical scrolling */
    VDP_SetRegister(0x02, (vram_fg >> 10) & 0x38);  /* FG nametable at $C000 */
    VDP_SetRegister(0x04, (vram_bg >> 13) & 0x07);  /* BG nametable at $E000 */
    VDP_SetRegister(0x05, (vram_sprites >> 9) & 0x7F); /* sprite table */
    VDP_SetRegister(0x10, 0x01);  /* 64-cell hscroll size */
    VDP_SetRegister(0x00, 0x04);  /* 8-colour mode */
    VDP_SetRegister(0x07, 0x20);  /* background colour (line 2, colour 0) */
    VDP_SetRegister(0x0A, 0x81);  /* HBlank rate (for water) */

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
    if ((int16_t)RAM_WORD(f_demo) >= 0) {
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
    int settle_frames = 0;
    do {
        WaitForVBlank();
        ExecuteObjects();        /* first call spawns the four card elements */
        BuildSprites();
        RunPLC();                /* ASM processes the level PLCs each VBlank */
        settle_frames++;
    } while (settle_frames < 120 && !TitleCardsSettled());

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

    LevelDataLoad();               /* load block mappings, layout and palette */
    LoadTilesFromStart();          /* draw FG + BG once before fade-in */

    ConvertCollisionArray();       /* no-op stub */
    ColIndexLoad();                /* stub — sets v_collindex */
    LZWaterFeatures();             /* stub — no-op for GHZ */

    /* Spawn player and HUD */
    LevelSpawnPlayer();
    LevelSpawnHUD();

    /* Initialize object position manager */
    ObjPosLoad();

    /* Execute objects once to initialize everything */
    ExecuteObjects();
    BuildSprites();

    /* ------------------------------------------------------------------
       Phase J: Clear gameplay counters (sonic.asm:2900-2919)
       ------------------------------------------------------------------ */
    v_rings = 0;
    v_time = 0;
    v_lifecount = 0;
    f_timeover = 0;
    v_shield = 0;
    v_invinc = 0;
    v_shoes = 0;
    v_debuguse = 0;
    f_restart = 0;
    v_framecount = 0;

    OscillateNumInit();

    f_scorecount = 1;
    f_ringcount  = 1;
    f_timecount  = 1;

    /* ------------------------------------------------------------------
       Phase K: Fade in (sonic.asm:2957)
       ------------------------------------------------------------------ */
    v_vblank_routine = id_VBlank_Levels;
    Palette_FadeIn();

    /* Level has faded in: make the title cards start moving (sonic.asm:2972-2975) */
    obRoutine(&ram[v_titlecard])                     += 2;  /* name  -> wait */
    obRoutine(&ram[v_titlecard + OBJECT_SIZE * 1])   += 4;  /* ZONE  -> wait */
    obRoutine(&ram[v_titlecard + OBJECT_SIZE * 2])   += 4;  /* ACT   -> wait */
    obRoutine(&ram[v_titlecard + OBJECT_SIZE * 3])   += 4;  /* oval  -> wait */
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
    /* --- 16x16 Block Mappings --- */
    if (Blk16_GHZ) {
        uint16_t *buf = (uint16_t *)RAM_ADDR(v_16x16);
        EniDec(Blk16_GHZ, buf, ArtTile_Level);
    }

    /* --- 256x256 Chunk Mappings --- */
    if (Blk256_GHZ) {
        uint8_t *buf = RAM_ADDR(v_256x256);
        KosDec(Blk256_GHZ, buf);
    }

    /* --- Level Layout (FG/BG) --- */
    LevelLayoutLoad();

    /* --- Palette (from the current zone's LevelHeaders entry) ---
       Non-GHZ palettes no-op until their palette assets are wired in
       Palette_Init (PalLoad guards on a NULL source). */
    {
        uint8_t zone = (uint8_t)(v_zone_act >> 8);
        if (zone < (uint8_t)(sizeof(level_headers) / sizeof(level_headers[0]))) {
            PalLoad_Fade(level_headers[zone].pal);
        }
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
   ColIndexLoad — stub
   Sets v_collindex to point at the collision index for the current zone.
   =================================================================== */
void ColIndexLoad(void) {
    /* TODO: set v_collindex based on v_zone */
}

/* ===================================================================
   OscillateNumInit — stub
   Initializes oscillation data used by swings, platforms, etc.
   =================================================================== */
void OscillateNumInit(void) {
    /* TODO: fill v_oscillate table with initial values */
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
    memset(player_slot, 0, OBJECT_SIZE);
    obID(player_slot) = 0x01;  /* id_SonicPlayer = $01 */
}

/* ===================================================================
   LevelSpawnHUD — create HUD object
   =================================================================== */
void LevelSpawnHUD(void) {
    uint8_t *hud_slot = RAM_ADDR(v_hud);
    memset(hud_slot, 0, OBJECT_SIZE);
    obID(hud_slot) = 0x21;  /* id_HUD = $21 */
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
   AnimateLevelAct — per-frame animated tile updates (stub)
   =================================================================== */
void AnimateLevelAct(void) {
    /* TODO: per-zone animated tile cycling */
}

/* ===================================================================
   Level_Process — called once per frame from MainGameLoop
   =================================================================== */
void Level_Process(void) {
    /* One-time init */
    if (!level_init_done) {
        Level_Enter();
        level_init_done = 1;
        return;
    }

    /* --- Per-frame gameplay (from GM_Level_main, sonic.asm:2960+) --- */

    /* TODO: PauseGame — uncomment when ready */

    /* VBlank: palette transfer + sprite DMA + hscroll */
    v_vblank_routine = id_VBlank_Levels;
    WaitForVBlank();

    v_framecount = v_framecount + 1;

    /* TODO: MoveSonicInDemo — demo playback (no-op in normal play) */

    ExecuteObjects();       /* run all active objects */
    DeformLayers();         /* deform BG layers */
    BuildSprites();         /* build sprite table for VDP */
    ObjPosLoad();           /* spawn new objects as camera scrolls */

    /* TODO: PaletteCycle — zone palette cycling */
    RunPLC();               /* process pending graphics decompression */
    /* TODO: OscillateNumDo — update oscillation values */
    AnimateLevelAct();      /* per-zone animated tiles */

    /* Check for level restart (death, Act complete) */
    if (f_restart) {
        /* TODO: handle death/act complete */
        level_init_done = 0;
    }
}
