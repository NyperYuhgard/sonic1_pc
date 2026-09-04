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
    Sound_Queue(bgm_Stop);
    Palette_FadeOut();

    /* Disable display for screen setup */
    v_vdp_buffer1 &= ~0x0040;

    /* Clear screen */
    VDP_ClearScreen();

    /* Set VDP registers for Sega screen (from GM_Sega) */
    VDP_SetRegister(0, 0x0400 | 0x04); /* mode 1: 8-colour mode */
    VDP_SetRegister(1, v_vdp_buffer1 | 0x34);
    VDP_SetRegister(2, 0x0300);    /* FG nametable at $C000 */
    VDP_SetRegister(3, 0x003C);    /* Window nametable at $A000 */
    VDP_SetRegister(4, 0x0007);    /* BG nametable at $E000 */
    VDP_SetRegister(7, 0x0000);    /* background colour */
    VDP_SetRegister(0x0B, 0x0000); /* full-screen vertical scrolling */

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
    v_vblank_routine = id_VBlank_SegaPCM;
    WaitForVBlank();
    VDP_SaveScreenshot("/tmp/sega_final.ppm"); /* TEMP debug */

    /* --- Post-chant wait (30 frames or until Start pressed) --- */
    v_generictimer = 30;
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
   Game mode stubs
   =================================================================== */
static void GM_Title_Screen(void) {
    /* TEST MODE: black screen with a counter bar. This is a stand-in for the
       real title screen while we debug the Sega -> Title transition.
       Keeps frames advancing so we can confirm there is no live-lock. */
    static int init = 0;
    static int counter = 0;

    if (!init) {
        init = 1;
        counter = 0;
        /* Black screen */
        VDP_ClearScreen();
        v_vdp_buffer1 &= ~0x0040; /* display off while clearing */
        v_vdp_buffer1 |= 0x0040;  /* display on */
        vdp_test_counter = 0;     /* enable test overlay */
    }

    WaitForVBlank(); /* renders a frame, advances timing/input */
    counter++;
    vdp_test_counter = counter;

    if (counter == 1) {
        fprintf(stderr, "[hit] title running\n");
    }
    if ((counter % 60) == 0) {
        printf("[Title] frame %d\n", counter);
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
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    return 1;
}

static void CleanupSDL(void) {
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
