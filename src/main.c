#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

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

/* ===================================================================
   Game Mode IDs (from sonic.asm GameModeArray)
   =================================================================== */
#define GM_Sega      0x00
#define GM_Title     0x04
#define GM_Level     0x0C
#define GM_Special   0x10
#define GM_Continue  0x14
#define GM_Ending    0x18
#define GM_Credits   0x1C

/* VBlank routine IDs (from sonic.asm) */
#define id_VBlank_Lag          0x00
#define id_VBlank_Sega         0x02
#define id_VBlank_Title        0x04
#define id_VBlank_Levels       0x08
#define id_VBlank_SpecialStage 0x0A
#define id_VBlank_PaletteFade  0x12
#define id_VBlank_SegaPCM      0x14

/* ===================================================================
   Display scaling
   =================================================================== */
#define SCALE 3

/* ===================================================================
   Game state
   =================================================================== */
static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
int running = 1;

/* ===================================================================
    Forward declarations for game mode functions
    =================================================================== */
static void GM_Sega_Screen(void);
static void GM_Title_Screen(void);
static void GM_Level_Process(void);
static void GM_Special_Stage(void);
static void GM_Continue_Screen(void);
static void GM_Ending_Screen(void);
static void GM_Credits_Screen(void);

/* Level select */
static void LevSelTextLoad(void);
static void LevSelControls(void);

/* ===================================================================
   RAM initialization (from GameInit in sonic.asm)
   =================================================================== */
static void ClearRAM(void) {
    memset(ram, 0, 0xFE00);
}

static void ClearCrossResetRAM(void) {
    memset(&ram[v_crossresetram], 0, 0x200);
}

/* ===================================================================
   VDP setup (from VDPSetupGame in sonic.asm)
   =================================================================== */
static void VDPSetupGame(void) {
    VDP_Reset();

    /* Clear CRAM */
    memset(vdp.cram, 0, sizeof(vdp.cram));

    /* Clear scroll buffers */
    v_scrposy_vdp = 0;
    v_scrposx_vdp = 0;

    /* Clear entire VRAM */
    VDP_FillVRAM(0, 0, VRAM_SIZE);

    /* Set VDP buffer */
    v_vdp_buffer1 = vdp.registers[1];
}

/* ===================================================================
   Joypad init
   =================================================================== */
static void JoypadInit(void) {
    Input_Init();
}

/* ===================================================================
   DAC driver load stub
   =================================================================== */
static void DACDriverLoad(void) {
    /* Stub - would load Z80 DAC driver */
}

/* ===================================================================
   VBlank handlers (from sonic.asm VBlank_Index table)
   =================================================================== */

/* VBlank_StandardTransfers: palette, sprites, hscroll to VDP */
static void VBlank_StandardTransfers(void) {
    /* Transfer palette to rendering buffer (palette -> CRAM equivalent) */
    Palette_Update();
    /* Sprite and hscroll transfers happen during VDP_RenderFrame */
}

/* VBlank_SegaPCM ($14): Sega Screen PCM - just decrement timer */
static void VBlank_SegaPCM(void) {
    if (v_generictimer != 0) {
        v_generictimer--;
    }
}

/* VBlank_Sega ($02): Sega Screen - standard transfers + timer decrement */
static void VBlank_Sega(void) {
    VBlank_StandardTransfers();
    VBlank_SegaPCM(); /* ASM: VBlank_Sega falls through to VBlank_SegaPCM */
}

/* VBlank_PaletteFade ($12): Palette fade - standard transfers */
static void VBlank_PaletteFade(void) {
    VBlank_StandardTransfers();
}

/* ===================================================================
   WaitForVBlank (from sonic.asm)
   Blocks until next frame. Handles frame timing, input, events,
   dispatches VBlank handler, and updates sound.
   =================================================================== */
static void ProcessSDLEvents(void) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            fprintf(stderr, "[Main] SDL_QUIT received, exiting\n");
            running = 0;
        }
    }
}

void WaitForVBlank(void) {
    /* Read input */
    Input_Read();

    /* Process SDL events */
    ProcessSDLEvents();

    /* Frame timing: wait for ~16.67ms (60 FPS) */
    static uint32_t last_frame_time = 0;
    uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - last_frame_time;
    if (elapsed < 16) {
        SDL_Delay(16 - elapsed);
    }
    last_frame_time = SDL_GetTicks();

    /* Dispatch VBlank handler based on v_vblank_routine */
    uint8_t routine = v_vblank_routine;
    v_vblank_routine = id_VBlank_Lag; /* reset to lag (like the real VBlank ISR) */

    switch (routine & 0x3E) {
    case id_VBlank_Sega:         VBlank_Sega();         break;
    case id_VBlank_PaletteFade:  VBlank_PaletteFade();  break;
    case id_VBlank_SegaPCM:      VBlank_SegaPCM();      break;
    case id_VBlank_Title:        VBlank_StandardTransfers(); break;
    case id_VBlank_Levels:       VBlank_StandardTransfers(); break;
    default:                     VBlank_StandardTransfers(); break;
    }

    /* Update sound */
    Sound_Update();

    /* Render VDP frame */
    VDP_RenderFrame(renderer);

    /* Increment frame counters */
    v_framecount = v_framecount + 1;
    v_vblank_count = v_vblank_count + 1;
}

/* ===================================================================
   GM_Sega_Screen (from sonic.asm GM_Sega, lines 1808-1885)
   This is a BLOCKING function - runs the entire Sega screen sequence.
   =================================================================== */
static void GM_Sega_Screen(void) {
    /* Stop music and fade out from previous game mode */
    Sound_Queue(bgm_Stop, false);
    Palette_FadeOut();

    /* Disable display for screen setup */
    v_vdp_buffer1 &= ~0x0040;

    /* Clear screen */
    VDP_ClearScreen();

    /* Set VDP registers for Sega screen (from GM_Sega) */
    VDP_SetRegister(0, 0x04); /* mode 1: 8-colour mode */
    VDP_SetRegister(1, v_vdp_buffer1 | 0x34);
    VDP_SetRegister(2, 0x30);    /* FG nametable at $C000 */
    VDP_SetRegister(3, 0x28);    /* Window nametable at $A000 */
    VDP_SetRegister(4, 0x07);    /* BG nametable at $E000 */
    VDP_SetRegister(7, 0x00);    /* background colour */
    VDP_SetRegister(0x0B, 0x00); /* full-screen vertical scrolling */

    /* Decompress Sega logo tiles directly to VRAM */
    NemDecToVRAM(Nem_SegaLogo, ArtTile_Sega_Tiles * 32);

    /* Decompress Sega logo tilemap to RAM buffer */
    static uint16_t eni_buffer[24 * 8 + 40 * 28 + 16];
    EniDec(Eni_SegaLogo, eni_buffer, ArtTile_Sega_Tiles);

    /* copyTilemap: BG plane light scan effect (24x8 tiles at vram_bg+$510) */
    VDP_CopyTilemapToVRAM(eni_buffer, vram_bg + 0x510, 24, 8);

    /* copyTilemap: FG plane Sega logo cutout (40x28 tiles at vram_fg) */
    VDP_CopyTilemapToVRAM(eni_buffer + 24 * 8, vram_fg, 40, 28);

    /* Load Sega screen palette directly to active palette */
    PalLoad(palid_SegaBG);
    Palette_Update();

    /* Initialize palette cycle variables */
    v_pcyc_num  = (uint16_t)(int16_t)(-10);
    v_pcyc_time = 0;

    /* Enable display */
    v_vdp_buffer1 |= 0x0040;

    /* --- Light scanning palette cycle effect --- */
    do {
        v_vblank_routine = id_VBlank_Sega; /* set every frame (matches ASM) */
        WaitForVBlank();
    } while (PalCycle_Sega());

    /* --- Wait for "SEGA" sound ---
       ASM: queue the sound, set routine to $14, ONE WaitForVBlank.
       Sound driver is stubbed, so this is just one frame. */
       Sound_Queue(sfx_Sega, false);
    v_vblank_routine = id_VBlank_SegaPCM;
    WaitForVBlank();

    /* --- Post-chant wait (30 frames or until Start pressed) --- */
    v_generictimer = 120;
    do {
        v_vblank_routine = id_VBlank_Sega; /* set every frame (matches ASM loop) */
        WaitForVBlank();
        if (v_generictimer == 0)
            break;
    } while (!(v_jpadpress1 & 0x80)); /* btnStart */

    /* Go to title screen */
    v_gamemode = GM_Title;
}

/* ===================================================================
    Game mode stubs and helpers
    =================================================================== */

/* Stub: Clear any pending PLC (Pattern Load Cue) */
static void ClearPLC(void) {
    memset(RAM_ADDR(v_plc_buffer), 0, 0x20);
}

/* Stub: Queue sound/music */
static void QueueSound2(int id, bool loop) {
    Sound_Queue(id, loop);
}

/* Stub: Run PLC (load pending patterns) */
static void RunPLC(void) {
    /* TODO: Implement PLC runner */
}

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

static void LevelSizeLoad(void) {
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

static void LevelLayoutLoad(void) {
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
static void DrawChunks(void) {
    draw_chunks_plane(vram_bg,
                      (int16_t)RAM_WORD(0xF708),   /* v_bgscreenposx */
                      (int16_t)RAM_WORD(0xF70C),   /* v_bgscreenposy */
                      RAM_ADDR(v_lvllayout_bg));
}

/* Stub: Start a new PLC */
static void NewPLC(int id) {
    /* TODO: Implement PLC */
}

/* Stub: Goto demo mode */
static void GotoDemo(void) {
    v_gamemode = GM_Sega;
}

/* Stub: Start level from title screen */
static void PlayLevel(void) {
    v_gamemode = GM_Level;
}

/* Clear screen alias */
static void ClearScreen(void) {
    VDP_ClearScreen();
}

/* ===================================================================
   GM_Title_Screen (from sonic.asm GM_Title)
   =================================================================== */
static void GM_Title_Screen(void) {
    static int init_done = 0;

    if (!init_done) {
        /* Stop music and clear PLC */
        QueueSound2(bgm_Stop, false);
        ClearPLC();

        /* Fade out from previous game mode */
        Palette_FadeOut();

        /* Disable display for screen setup */
        v_vdp_buffer1 &= ~0x0040;

        /* Screen setup */
        VDP_SetRegister(0, 0x04); /* 8-colour mode */
        VDP_SetRegister(1, v_vdp_buffer1 | 0x34);
        VDP_SetRegister(2, 0x30);    /* FG nametable at $C000 */
        VDP_SetRegister(3, 0x28);    /* Window nametable at $A000 */
        VDP_SetRegister(4, 0x07);    /* BG nametable at $E000 */
        VDP_SetRegister(7, 0x00);    /* background colour */
        VDP_SetRegister(0x0B, 0x00); /* full-screen vertical scrolling */
        VDP_SetRegister(0x0C, 0x81); /* 40-cell display */
        VDP_SetRegister(0x0D, 0x37); /* H-scroll table at $DC00 */

        /* Clear screen and object RAM */
        VDP_ClearScreen();
        memset(RAM_ADDR(v_objspace), 0, 0x2000);

        /* Load hidden Japanese credits patterns */
        if (Nem_JapNames) {
            NemDecToVRAM(Nem_JapNames, ArtTile_Title_Japanese_Text * tile_size);
        }

        /* Load SONIC TEAM PRESENTS font */
        if (Nem_CreditText) {
            NemDecToVRAM(Nem_CreditText, ArtTile_Sonic_Team_Font * tile_size);
        }

        /* Decompress and load hidden Japanese credits tilemap */
        if (Eni_JapNames) {
            static uint16_t eni_buffer[40 * 28];
            EniDec(Eni_JapNames, eni_buffer, ArtTile_Title_Japanese_Text);
            VDP_CopyTilemapToVRAM(eni_buffer, vram_fg, 40, 28);
        }

        /* Clear palette fade buffer and load Sonic palette for fade-in */
        memset(RAM_ADDR(v_palette_fading), 0, sizeof(palette_fading));
        PalLoad_Fade(palid_Sonic);

        /* Create SONIC TEAM PRESENTS object */
        RAM_BYTE(v_sonicteam) = id_CreditsText;

        /* Execute objects and build sprites for STP */
        ExecuteObjects();
        BuildSprites();

        /* Fade in STP screen */
        Palette_FadeIn();

        /* -------------------------------------------------------------------
           Load main title screen patterns while STP is shown
           ------------------------------------------------------------------- */
        if (Nem_TitleFg) {
            NemDecToVRAM(Nem_TitleFg, ArtTile_Title_Foreground * tile_size);
        }
        if (Nem_TitleSonic) {
            NemDecToVRAM(Nem_TitleSonic, ArtTile_Title_Sonic * tile_size);
        }
        if (Nem_TitleTM) {
            NemDecToVRAM(Nem_TitleTM, ArtTile_Title_Trademark * tile_size);
        }

        /* Load level select font (uncompressed) */
        if (Art_Text) {
            uint16_t vram_addr = ArtTile_Level_Select_Font * tile_size;
            uint16_t words = (uint16_t)(Art_Text_len / 2 - 1);
            for (uint16_t i = 0; i <= words; i++) {
                uint16_t word = ((uint16_t)Art_Text[i * 2] << 8) | Art_Text[i * 2 + 1];
                vdp.vram[vram_addr + i * 2]     = (uint8_t)(word >> 8);
                vdp.vram[vram_addr + i * 2 + 1] = (uint8_t)(word & 0xFF);
            }
        }

        /* Miscellaneous initializations */
        RAM_WORD(v_lastlamp) = 0;
        v_debuguse = 0;
        f_demo = 0;
        v_unused2 = 0;
        v_zone_act = id_GHZ_act1;
        v_pcyc_time = 0;

        LevelSizeLoad();
        DeformLayers();

        /* Load GHZ 16x16 block mappings */
        if (Blk16_GHZ) {
            uint16_t *buf = (uint16_t *)RAM_ADDR(v_16x16);
            EniDec(Blk16_GHZ, buf, ArtTile_Level);
        }

        /* Load GHZ 256x256 chunk mappings */
        if (Blk256_GHZ) {
            uint8_t *buf = RAM_ADDR(v_256x256);
            KosDec(Blk256_GHZ, buf);
        }

        LevelLayoutLoad();

        /* Fade out STP screen */
        Palette_FadeOut();

        /* -------------------------------------------------------------------
           Main title screen setup
           ------------------------------------------------------------------- */
        VDP_ClearScreen();

        /* Draw initial background layer (GHZ chunks) */
        DrawChunks();

        /* Decompress and load title emblem tilemap */
        if (Eni_Title) {
            static uint16_t eni_buffer[34 * 22];
            EniDec(Eni_Title, eni_buffer, ArtTile_Level);
            VDP_CopyTilemapToVRAM(eni_buffer, vram_fg + 0x206, 34, 22);
        }

        /* Load GHZ patterns */
        if (Nem_GHZ_1st) {
            NemDecToVRAM(Nem_GHZ_1st, ArtTile_Level * tile_size);
        }

        /* Load title palette to fade buffer */
        PalLoad_Fade(palid_Title);

        /* Start title music */
        QueueSound2(bgm_Title, false);

        /* Disable debug mode */
        f_debugmode = 0;

        /* Title screen timer */
        v_generictimer = 376;

        /* Clear SONIC TEAM PRESENTS object RAM (partial clear, matches original bug) */
        memset(RAM_ADDR(v_sonicteam), 0, object_size / 2);

        /* Create title screen objects */
        RAM_BYTE(v_titlesonic) = id_TitleSonic;
        RAM_BYTE(v_pressstart) = id_PSBTM;

        /* On non-Japanese consoles, load TM object */
        if (v_megadrive >= 0) {
            RAM_BYTE(v_titletm) = id_PSBTM;
            obFrame(&ram[v_titletm]) = 3; /* TM frame */
        }

        /* Masking sprites object */
        RAM_BYTE(v_ttlsonichide) = id_PSBTM;
        obFrame(&ram[v_ttlsonichide]) = 2; /* hide-torso frame */

        /* Position v_player for background scroll (matches LevSz_StartLoc for title) */
        obX(&ram[v_player]) = (int16_t)0x50;
        obY(&ram[v_player]) = (int16_t)0x3B0;

        /* Execute objects and build sprites before fade-in */
        ExecuteObjects();
        DeformLayers();
        BuildSprites();

        /* Queue main PLC (rings, etc.) */
        NewPLC(plcid_Main);

        /* Clear cheat counters */
        v_title_dcount = 0;
        v_title_ccount = 0;

        /* Enable display and fade in */
        v_vdp_buffer1 |= 0x0040;
        VDP_SetRegister(1, v_vdp_buffer1 | 0x34);
        Palette_FadeIn();

        init_done = 1;
    }

    /* ==================================================================
       Title screen main loop
       ================================================================== */
    v_vblank_routine = id_VBlank_Title;
    WaitForVBlank();
    ExecuteObjects();
    DeformLayers();
    BuildSprites();

    /* One-shot VRAM/CRAM/RAM dump after title sprites are built
       (diagnostics; set SONIC_DUMP_VRAM=1) */
    static int vram_dumped = 0;
    long tdint = getenv("SONIC_DUMP_TDINT") ? atol(getenv("SONIC_DUMP_TDINT")) : 286;
    if (!vram_dumped && getenv("SONIC_DUMP_VRAM") && v_generictimer <= tdint) {
        vram_dumped = 1;
        const char *gd = getenv("SONIC_GARBAGE");
        if (!gd) gd = "/media/nyper/FuckYouMS/Github/GensToPC/Sonic1/garbage/";
        char p[512];
        struct {
            const char *name;
            void *data;
            size_t len;
        } blobs[] = {
            {"vram.bin",  vdp.vram, 0x10000},
            {"cram.bin",  vdp.cram, 0x80},
            {"ram.bin",   &ram[0],  0x10000},
        };
        for (size_t i = 0; i < 3; i++) {
            snprintf(p, sizeof(p), "%s/%s", gd, blobs[i].name);
            FILE *f = fopen(p, "wb");
            if (f) { fwrite(blobs[i].data, 1, blobs[i].len, f); fclose(f); }
        }
    }

    /* Headless frame captures: with SONIC_DUMP_FRAMES=1, save the finished
       frame (PPM) + raw sprite table each title frame while v_generictimer is
       inside SONIC_DUMP_TMIN..SONIC_DUMP_TMAX (default 300..376). Used to
       verify the 20-sprites-per-scanline drop behaviour. */
    if (getenv("SONIC_DUMP_FRAMES")) {
        long tmin = getenv("SONIC_DUMP_TMIN") ? atol(getenv("SONIC_DUMP_TMIN")) : 300;
        long tmax = getenv("SONIC_DUMP_TMAX") ? atol(getenv("SONIC_DUMP_TMAX")) : 376;
        if (v_generictimer >= tmin && v_generictimer <= tmax) {
            const char *gd = getenv("SONIC_GARBAGE");
            if (!gd) gd = "/media/nyper/FuckYouMS/Github/GensToPC/Sonic1/garbage/";
            char p[512];
            static int iter = 0;
            iter++;
            uint8_t *po = &ram[v_pressstart];
            uint8_t *so = &ram[v_titlesonic];
            fprintf(stderr,
                    "DBG iter=%d timer=%d | son:r=%d f=%d an=%d y=%d dly=%d | press:r=%d f=%d an=%d y=%d\n",
                    iter, (int)v_generictimer,
                    (int)obRoutine(so), (int)obFrame(so), (int)obAnim(so), (int)obScreenY(so), (int)obDelayAni(so),
                    (int)obRoutine(po), (int)obFrame(po), (int)obAnim(po), (int)obScreenY(po));
            snprintf(p, sizeof(p), "%s/frame_%04ld.ppm", gd, (long)v_generictimer);
            VDP_RenderFrame(renderer);
            VDP_SaveScreenshot(p);
            snprintf(p, sizeof(p), "%s/sprites_%04ld.bin", gd, (long)v_generictimer);
            FILE *f = fopen(p, "wb");
            if (f) { fwrite(&ram[v_spritetablebuffer], 1, 80 * 8, f); fclose(f); }
        }
    }
    PalCycle_Title();
    RunPLC();

    /* Move title Sonic right 2px per frame (ASM uses v_player+obX) */
    {
        uint16_t x = (uint16_t)obX(&ram[v_player]);
        x += 2;
        obX(&ram[v_player]) = (int16_t)x;
    }

    /* --- Timer / Start / Demo --- */
    if (v_generictimer == 0) {
        GotoDemo();
        init_done = 0;
        init_done = 0;
        return;
    }

    if (v_jpadpress1 & btnStart) {
        if (f_levselcheat && (v_jpadhold1 & btnA)) {
            v_vblank_routine = id_VBlank_Title;
            WaitForVBlank();
            PalLoad(palid_LevelSel);   /* load level select palette (ASM: moveq #palid_LevelSel, bsr PalLoad) */
            Palette_Update();
            memset(RAM_ADDR(v_hscrolltablebuffer), 0, 0x400);
            v_scrposy_vdp = 0;
            v_scrposx_vdp = 0;

            VDP_FillVRAM(0, vram_bg, plane_size_64x32);

            LevSelTextLoad();

            for (;;) {
                v_vblank_routine = id_VBlank_Title;
                WaitForVBlank();
                LevSelControls();
                RunPLC();

                if (v_jpadpress1 & (btnA | btnB | btnC | btnStart)) {
                    if (v_levselitem == levsel_sndtest_row) {
                        int sound = v_levselsound + 0x80;
                        if (f_creditscheat && sound == 0x9F) {
                            v_gamemode = GM_Ending;
                            v_zone_act = id_EndZ_good;
                            init_done = 0;
                            return;
                        }
                        if (f_creditscheat && sound == 0x9E) {
                            v_gamemode = GM_Credits;
                            QueueSound2(bgm_Credits, false);
                            v_creditsnum = 0;
                            init_done = 0;
                            return;
                        }
                        Sound_Queue(sound, false);
                    } else {
                        uint16_t sel = v_levselitem;
                        if (sel < LevSel_Ptrs_len / 2) {
                            uint16_t ptr = LevSel_Ptrs[sel];
                            if (ptr == (0x8000 | id_SS)) {
                                v_gamemode = GM_Special;
                                v_zone_act = 0;
                                v_lives = 3;
                                v_rings = 0;
                                v_time = 0;
                                v_score = 0;
                                init_done = 0;
                                return;
                            }
                            v_zone_act = ptr & 0x3FFF;
                            init_done = 0;
                            break;
                        }
                    }
                }
            }
        }

        PlayLevel();
        v_lives = 3;
        v_rings = 0;
        v_time = 0;
        v_score = 0;
        v_lastspecial = 0;
        v_emeralds = 0;
        RAM_SET_U32((uint32_t)v_emldlist, 0);
        RAM_SET_U32((uint32_t)(v_emldlist + 4), 0);
        v_continues = 0;
        QueueSound2(bgm_Fade, false);
        init_done = 0;
        init_done = 0;
        return;
    }

    /* Continue looping */
    v_generictimer--;

    /* --- Level select cheat code (D-Pad) --- */
    {
        const uint8_t *code = (v_megadrive >= 0) ? LevSelCode_US : LevSelCode_J;
        uint16_t dcount = v_title_dcount;
        uint8_t pressed = v_jpadpress1 & btnDir;
        if (pressed == code[dcount]) {
            dcount++;
            if (code[dcount] == 0xFF) {
                f_levselcheat = 1;
                dcount = 0;
                Sound_Queue(sfx_Ring, false);
            }
            v_title_dcount = dcount;
        } else if (pressed != 0) {
            v_title_dcount = 0;
        }
    }
}

static void GM_Level_Process(void) {
    /* TODO: Implement Level processing */
    ExecuteObjects();
    BuildSprites();
    WaitForVBlank();
}

static void GM_Special_Stage(void) {
    /* TODO: Implement Special Stage */
}

static void GM_Continue_Screen(void) {
    /* TODO: Implement Continue Screen */
}

static void GM_Ending_Screen(void) {
    /* TODO: Implement Ending Sequence */
}

static void GM_Credits_Screen(void) {
    /* TODO: Implement Credits */
}

/* ===================================================================
    Level Select helpers (from sonic.asm GM_Title)
    =================================================================== */

static void LevSel_ChgSnd(uint16_t vram_addr, uint16_t tile_attr, uint16_t d0) {
    d0 &= 0xF;
    if (d0 >= 0xA) {
        d0 += 7;
    }
    uint16_t tile = d0 | tile_attr;
    vdp.vram[vram_addr] = (uint8_t)(tile >> 8);
    vdp.vram[vram_addr + 1] = (uint8_t)(tile & 0xFF);
}

static uint8_t ascii_to_tile(char c) {
    if (c == ' ') return 0xFF;
    if (c >= '0' && c <= '9') return c - '0';
    if (c == '$') return 0x0A;
    if (c == '-') return 0x0B;
    if (c == '=') return 0x0C;
    if (c == '>') return 0x0D;
    if (c == 'Y') return 0x0F;
    if (c == 'Z') return 0x10;
    if (c >= 'A' && c <= 'X') return c - 'A' + 0x11;

    return 0xFF;
}

static void LevSel_ChgLine(const char *text, uint16_t vram_base, uint16_t tile_attr) {
    for (int i = 0; i < levsel_line_length; i++) {
        uint16_t vram_addr = vram_base + i * 2;
        char ch = text[i];
        uint8_t tile_code;
        if (ch == '\0') {

            tile_code = 0xFF;
        } else {
            tile_code = ascii_to_tile(ch);
        }

        uint16_t tile = tile_attr + tile_code;

        vdp.vram[vram_addr]     = (uint8_t)(tile >> 8);
        vdp.vram[vram_addr + 1] = (uint8_t)(tile & 0xFF);
    }
}

static const char *level_texts[levsel_line_count] = {
    "GREEN HILL ZONE  STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "LABYRINTH ZONE   STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "MARBLE ZONE      STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "STAR LIGHT ZONE  STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "SPRING YARD ZONE STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "SCRAP BRAIN ZONE STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "FINAL ZONE              ",
    "SPECIAL STAGE           ",
    "SOUND SELECT            "
};

static void LevSelTextLoad(void) {
    uint16_t vram_base = levsel_vram_main;


    for (int line = 0; line < levsel_line_count; line++) {
        LevSel_ChgLine(level_texts[line], vram_base + line * 128, levsel_white);
    }


    uint16_t selected = v_levselitem;
    if (selected < levsel_line_count) {
        LevSel_ChgLine(level_texts[selected], vram_base + selected * 128, levsel_yellow);
    }


    uint16_t tile_attr = (v_levselitem == levsel_sndtest_row) ? levsel_yellow : levsel_white;
    uint16_t vram_num = levsel_vram_sndtestnum;
    uint16_t sound = v_levselsound + 0x80;
    LevSel_ChgSnd(vram_num, tile_attr, sound >> 4);
    LevSel_ChgSnd(vram_num + 2, tile_attr, sound & 0xF);
}

static void LevSelControls(void) {
    uint8_t pressed = v_jpadpress1 & (btnUp | btnDn);
    if (pressed) {
        v_levseldelay = 11;
        uint8_t held = v_jpadhold1 & (btnUp | btnDn);
        if (held) {
            uint16_t sel = v_levselitem;
            if (v_jpadhold1 & btnUp) {
                sel--;
                if (sel >= levsel_line_count) sel = levsel_line_count - 1;
            }
            if (v_jpadhold1 & btnDn) {
                sel++;
                if (sel >= levsel_line_count) sel = 0;
            }
            v_levselitem = sel;
            LevSelTextLoad();
        }
        return;
    }

    if (v_levseldelay > 0) {
        v_levseldelay--;
        if (v_levseldelay > 0) {
            return;
        }
    }

    if (v_levselitem == levsel_sndtest_row) {
        uint8_t lr = v_jpadpress1 & (btnL | btnR);
        if (lr) {
            uint16_t sound = v_levselsound;
            if (v_jpadpress1 & btnL) {
                sound--;
                if (sound > sfx__Last - 0x80) sound = sfx__Last - 0x80;
            }
            if (v_jpadpress1 & btnR) {
                sound++;
                if (sound > sfx__Last - 0x80) sound = 0;
            }
            v_levselsound = sound;
            LevSelTextLoad();
        }
    }
}

/* ===================================================================
    Game mode array (matches GameModeArray in sonic.asm)
    =================================================================== */
typedef void (*GameModeFunc)(void);

static const GameModeFunc game_mode_table[] = {
    GM_Sega_Screen,      /* $00 */
    GM_Title_Screen,     /* $04 */
    NULL,                /* $08 - demo (uses GM_Level) */
    GM_Level_Process,    /* $0C */
    GM_Special_Stage,    /* $10 */
    GM_Continue_Screen,  /* $14 */
    GM_Ending_Screen,    /* $18 */
    GM_Credits_Screen,   /* $1C */
};

#define GAME_MODE_TABLE_SIZE (sizeof(game_mode_table) / sizeof(game_mode_table[0]))

/* ===================================================================
   Main game loop (from MainGameLoop in sonic.asm)
   =================================================================== */
static void MainGameLoop(void) {
    while (running) {
        /* Get current game mode */
        uint8_t mode = v_gamemode;
        int index = (mode & 0x1C) >> 2;

        if (index < (int)GAME_MODE_TABLE_SIZE && game_mode_table[index]) {
            game_mode_table[index]();
        }

        if (!running) break;

        /* Small delay to prevent CPU spin when game mode is instant */
        SDL_Delay(16);
    }
}

/* ===================================================================
   SDL initialization
   =================================================================== */
static int InitSDL(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    window = SDL_CreateWindow(
        "Sonic the Hedgehog (PC Port)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        /* Headless/dummy runs: fall back to a software renderer. */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    return 1;
}

static void CleanupSDL(void) {
    Data_Quit();
    if (vdp.framebuffer) {
        SDL_DestroyTexture(vdp.framebuffer);
        vdp.framebuffer = NULL;
    }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();
}

/* ===================================================================
   Entry point (from EntryPoint in sonic.asm)
   =================================================================== */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("Sonic the Hedgehog - PC Port\n");
    printf("Based on the Sega Mega Drive disassembly\n\n");

    if (!InitSDL()) {
        return 1;
    }

    if (Data_Init() != 0) {
        fprintf(stderr, "Warning: some assets failed to load, using fallbacks\n");
    }

    ClearCrossResetRAM();

    /* Detect console version (set to NTSC US for now) */
    v_megadrive = 0;

    /* Set init flag */
    v_init = 0x696E6974; /* 'init' */

    ClearRAM();

    /* Initialize subsystems */
    VDPSetupGame();
    DACDriverLoad();
    JoypadInit();
    Objects_Init();
    Sound_Init();

    /* Set first game mode */
    v_gamemode = GM_Sega;

    printf("Starting main game loop...\n");
    printf("Controls:\n");
    printf("  Player 1: Arrow keys + Z/X/C + Enter\n");
    printf("  Player 2: I/J/K/L + U/Y/O + P\n");
    printf("  ESC: Quit\n\n");

    MainGameLoop();

    CleanupSDL();

    printf("Goodbye!\n");
    return 0;
}
