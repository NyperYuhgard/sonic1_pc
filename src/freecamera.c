#include "freecamera.h"
#include "ram.h"

#include <SDL2/SDL.h>

/* ---------------------------------------------------------------------------
   Debug free camera. Moves the world-space camera (v_screenposx/y, which are
   16.16 fixed point with integer word at [+0], fraction at [+2]) and forces
   the FG redraw flags so the plane refreshes like normal scrolling.
   ------------------------------------------------------------------------- */

#define CAM_SPEED_PX  8   /* integer pixels moved per frame per held key */

static int free_cam_enabled = 0;

int FreeCamera_IsActive(void) {
    return free_cam_enabled;
}

void FreeCamera_Toggle(void) {
    free_cam_enabled = !free_cam_enabled;
}

static void freecam_set_cam(uint16_t addr, int32_t v, int16_t lo_lim, int16_t hi_lim) {
    if (v < lo_lim)  v = lo_lim;
    if (v > hi_lim)  v = hi_lim;
    RAM_WORD(addr)     = (uint16_t)((uint32_t)v >> 16);
    RAM_WORD(addr + 2) = (uint16_t)(uint32_t)v;
}

void FreeCamera_Update(const uint8_t *keys) {
    if (!free_cam_enabled) return;

    /* integer pixel position (truncate the 16.16 long) */
    int32_t ix = (int32_t)(int16_t)RAM_WORD(0xF700);
    int32_t iy = (int32_t)(int16_t)RAM_WORD(0xF704);

    if (keys[SDL_SCANCODE_HOME])    iy -= CAM_SPEED_PX;   /* up */
    if (keys[SDL_SCANCODE_END])     iy += CAM_SPEED_PX;   /* down */
    if (keys[SDL_SCANCODE_DELETE])  ix -= CAM_SPEED_PX;   /* left */
    if (keys[SDL_SCANCODE_PAGEUP])  ix += CAM_SPEED_PX;   /* right */

    freecam_set_cam(0xF700, ix,
                    RAM_WORD(0xF728),            /* v_limitleft2 */
                    RAM_WORD(0xF72A) + 320);     /* v_limitright2 + screen width */
    freecam_set_cam(0xF704, iy,
                    RAM_WORD(0xF72C),            /* v_limittop2 */
                    RAM_WORD(0xF72E));           /* v_limitbtm2 */

    /* signal a full FG redraw block so the plane refreshes while panning */
    RAM_WORD(0xF754) |= (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
}