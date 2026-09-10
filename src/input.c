#include "input.h"
#include "ram.h"
#include "constants.h"
#include "vdp.h"
#include "objview.h"
#include "freecamera.h"
#include <SDL2/SDL.h>
#include <string.h>

uint8_t joypad_hold[2]  = {0, 0};
uint8_t joypad_press[2] = {0, 0};

static uint8_t prev_state[2] = {0, 0};
static int prev_vram_key = 0;
static int prev_objview_key = 0;
static int prev_f_key = 0;

void Input_Init(void) {
    memset(joypad_hold, 0, sizeof(joypad_hold));
    memset(joypad_press, 0, sizeof(joypad_press));
    memset(prev_state, 0, sizeof(prev_state));
}

void Input_Read(void) {
    /* Process SDL events */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            extern int running;
            running = 0;
        }
        /* Closing debug viewer windows via the WM */
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE) {
            if ((int)event.window.windowID == VDP_ViewerWindowID())
                VDP_ToggleVRAMViewer();
            else if ((int)event.window.windowID == ObjView_WindowID())
                ObjView_Toggle();
        }
    }

    /* Read keyboard state directly (more reliable than event-based for games) */
    const uint8_t *keys = SDL_GetKeyboardState(NULL);

    /* Debug: P toggles the real-time VRAM viewer window (down-edge only) */
    if (keys[SDL_SCANCODE_P] && !prev_vram_key) {
        VDP_ToggleVRAMViewer();
    }
    prev_vram_key = keys[SDL_SCANCODE_P];

    /* Debug: O toggles the Object RAM viewer window (down-edge only) */
    if (keys[SDL_SCANCODE_O] && !prev_objview_key) {
        ObjView_Toggle();
    }
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
