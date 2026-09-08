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
    /* TODO: level art AddPLC queue — decoded inline by LevelDataLoad here */

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
    uint8_t zone = (uint8_t)(v_zone_act >> 8);

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

    /* --- Palette --- */
    switch (zone) {
    default:
    case 0: /* Green Hill (and other zones, once assets are loaded) */
        PalLoad_Fade(palid_GHZ);
        break;
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
   ObjPosLoad — object position manager stub
   Reads object layout data and spawns objects when the camera reaches
   their X position. Called once per frame.
   =================================================================== */
void ObjPosLoad(void) {
    /* TODO: implement object spawning from level layout data */
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
