
#include "input.h"
#include "tas_editor.h"
#include "tas.h"
#include "ram.h"
#include "constants.h"
#include "vdp.h"
#include "objview.h"
#include "ramview.h"
#include "planeview.h"
#include "freecamera.h"
#include <SDL2/SDL.h>
#include <string.h>

uint8_t joypad_hold[2]  = {0, 0};
uint8_t joypad_press[2] = {0, 0};

static uint8_t prev_state[2] = {0, 0};
static int prev_vram_key = 0;
static int prev_objview_key = 0;
static int prev_ramview_key = 0;
static int prev_planeview_key = 0;
static int prev_planeview_wrap_key = 0;
static int prev_f_key = 0;

void Input_Init(void) {
    memset(joypad_hold, 0, sizeof(joypad_hold));
    memset(joypad_press, 0, sizeof(joypad_press));
    memset(prev_state, 0, sizeof(prev_state));
}

void Input_Read(void) {
    extern int running;

    /* Process SDL events */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = 0;
        }
        /* Closing debug viewer windows via the WM */
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE) {
            if ((int)event.window.windowID == VDP_ViewerWindowID())
                VDP_ToggleVRAMViewer();
            else if ((int)event.window.windowID == ObjView_WindowID())
                ObjView_Toggle();
            else if ((int)event.window.windowID == RamView_WindowID())   /* <-- añadir */
                RamView_Toggle();
            else if ((int)event.window.windowID == PlaneView_WindowID())
                PlaneView_Toggle();
            else
                running = 0;   /* main game window closed via the WM */
        }
        /* Plane viewer: mouse wheel scrolls, left-drag pans (only when the
           pointer is inside that window) */
        if (event.type == SDL_MOUSEWHEEL) {
            if ((int)event.wheel.windowID == PlaneView_WindowID()) {
                PlaneView_Scroll(event.wheel.x * 40, -event.wheel.y * 40);
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if ((int)event.button.windowID == PlaneView_WindowID() &&
                event.button.button == SDL_BUTTON_LEFT) {
                PlaneView_DragStart(event.button.x, event.button.y);
            }
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            if ((int)event.button.windowID == PlaneView_WindowID() &&
                event.button.button == SDL_BUTTON_LEFT) {
                PlaneView_DragEnd();
            }
        } else if (event.type == SDL_MOUSEMOTION) {
            if ((int)event.motion.windowID == PlaneView_WindowID()) {
                PlaneView_DragMove(event.motion.x, event.motion.y);
            }
        }
        TASEditor_HandleEvent(&event);
    }

    /* Read keyboard state directly (more reliable than event-based for games) */
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    static int prev_tas_editor_key = 0;

    if (keys[SDL_SCANCODE_T] && !prev_tas_editor_key) {
        TASEditor_Toggle();
    }
    prev_tas_editor_key = keys[SDL_SCANCODE_T];

    /* Debug: P toggles the real-time VRAM viewer window (down-edge only) */
    if (keys[SDL_SCANCODE_P] && !prev_vram_key) {
        VDP_ToggleVRAMViewer();
    }
    prev_vram_key = keys[SDL_SCANCODE_P];

    /* Debug: O toggles the Object RAM viewer window (down-edge only) */
    if (keys[SDL_SCANCODE_O] && !prev_objview_key) {
        ObjView_Toggle();
    }

    /* Debug: R toggles the RAM viewer window (down-edge only) */
    if (keys[SDL_SCANCODE_R] && !prev_ramview_key) {
        RamView_Toggle();
    }

    /* Debug: G toggles the Plane A/B viewer window (down-edge only) */
    if (keys[SDL_SCANCODE_G] && !prev_planeview_key) {
        PlaneView_Toggle();
    }
    prev_planeview_key = keys[SDL_SCANCODE_G];

    /* Debug: V toggles the plane viewer 128-row wrap mode (BlastEm-style) */
    if (keys[SDL_SCANCODE_V] && !prev_planeview_wrap_key) {
        PlaneView_ToggleWrap();
    }
    prev_planeview_wrap_key = keys[SDL_SCANCODE_V];

    prev_ramview_key = keys[SDL_SCANCODE_R];
    prev_objview_key = keys[SDL_SCANCODE_O];

    /* Debug: F toggles free camera mode (down-edge only) */
    if (keys[SDL_SCANCODE_F] && !prev_f_key) {
        FreeCamera_Toggle();
    }
    prev_f_key = keys[SDL_SCANCODE_F];

    uint8_t new_state[2] = {0, 0};

    /* Player 1 */
    if (keys[SDL_SCANCODE_UP])    new_state[0] |= btnUp;
    if (keys[SDL_SCANCODE_DOWN])  new_state[0] |= btnDn;
    if (keys[SDL_SCANCODE_LEFT])  new_state[0] |= btnL;
    if (keys[SDL_SCANCODE_RIGHT]) new_state[0] |= btnR;
    if (keys[SDL_SCANCODE_Z])     new_state[0] |= btnA;
    if (keys[SDL_SCANCODE_X])     new_state[0] |= btnB;
    if (keys[SDL_SCANCODE_C])     new_state[0] |= btnC;
    if (keys[SDL_SCANCODE_RETURN])new_state[0] |= btnStart;

    /* Player 2 */
    if (keys[SDL_SCANCODE_I]) new_state[1] |= btnUp;
    if (keys[SDL_SCANCODE_K]) new_state[1] |= btnDn;
    if (keys[SDL_SCANCODE_J]) new_state[1] |= btnL;
    if (keys[SDL_SCANCODE_L]) new_state[1] |= btnR;
    if (keys[SDL_SCANCODE_U]) new_state[1] |= btnA;
    if (keys[SDL_SCANCODE_Y]) new_state[1] |= btnB;
    if (keys[SDL_SCANCODE_O]) new_state[1] |= btnC;
    if (keys[SDL_SCANCODE_P]) new_state[1] |= btnStart;

    new_state[0] = TAS_OverridePlayer1(new_state[0]);
    new_state[1] = TAS_OverridePlayer2(new_state[1]);
    if (TASEditor_HasFocus()) {
        new_state[0] = 0;
        new_state[1] = 0;
    }

    /* Compute held/pressed (matching ReadJoypads logic) */
    for (int i = 0; i < 2; i++) {
        uint8_t changed = prev_state[i] ^ new_state[i];   /* buttons that changed */
        joypad_press[i] = changed & new_state[i];          /* newly pressed */
        joypad_hold[i]  = new_state[i];                    /* currently held */
        prev_state[i]   = new_state[i];
    }

    /* Write to RAM (matching ASM v_jpadhold1/v_jpadpress1) */
    v_jpadhold1  = joypad_hold[0];
    v_jpadpress1 = joypad_press[0];
    v_jpadhold2  = joypad_hold[1];
    v_jpadpress2 = joypad_press[1];

    /* Debug free camera: read keys after joypads are written */
    FreeCamera_Update(keys);
}

/* ------------------------------------------------------------------ */
/* TAS key handling (F1–F8). Reutiliza el patrón de prev_*_key ya    */
/* presente en Input_Read, pero en su propia función para poder      */
/* llamarla también desde el bucle de pausa sin re-leer joypads.     */
/* ------------------------------------------------------------------ */

void Input_PollTasKeys(void) {
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    static int prevF1=0, prevF2=0, prevF3=0, prevF4=0;
    static int prevF5=0, prevF6=0, prevF7=0, prevF8=0;

    if (TASEditor_HasFocus()) return;

    int f1 = keys[SDL_SCANCODE_F1];
    int f2 = keys[SDL_SCANCODE_F2];
    int f3 = keys[SDL_SCANCODE_F3];
    int f4 = keys[SDL_SCANCODE_F4];
    int f5 = keys[SDL_SCANCODE_F5];
    int f6 = keys[SDL_SCANCODE_F6];
    int f7 = keys[SDL_SCANCODE_F7];
    int f8 = keys[SDL_SCANCODE_F8];

    if (f1 && !prevF1) TAS_ToggleRecord();
    if (f2 && !prevF2) TAS_ReplayFromStart();
    if (f3 && !prevF3) TAS_SaveFile_Action();
    if (f4 && !prevF4) TAS_LoadFile_Action();
    if (f5 && !prevF5) TAS_SaveNextState();
    if (f6 && !prevF6) TAS_LoadPrevState();
    if (f7 && !prevF7) TAS_SetPaused(!TAS_IsPaused());
    if (f8 && !prevF8) TAS_RequestStep();

    prevF1=f1; prevF2=f2; prevF3=f3; prevF4=f4;
    prevF5=f5; prevF6=f6; prevF7=f7; prevF8=f8;
}

void Input_GetPrevState(uint8_t out[2]) {
    out[0] = prev_state[0];
    out[1] = prev_state[1];
}

void Input_SetPrevState(const uint8_t in[2]) {
    prev_state[0] = in[0];
    prev_state[1] = in[1];
}

void Input_PollTasEditorKeys(void) { TASEditor_PollKeys(); }