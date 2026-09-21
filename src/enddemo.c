/* ===========================================================================
 *  End Demo Screen — build-time demo ending
 *  Simple SDL-based screen: shows "END DEMO", "DEVELOPED BY" and logos.
 *  Only used in demo builds; not part of final game flow.
 *  =========================================================================== */

#include "enddemo.h"
#include "constants.h"
#include "ram.h"
#include "input.h"
#include "sound.h"
#include <SDL2/SDL.h>
#include <stdio.h>

/* External renderer from main.c */
extern SDL_Renderer *renderer;

/* ===========================================================================
 * Helper para renderizar caracteres vectoriales estilo pixel-art
 * =========================================================================== */
static void DrawChar(SDL_Renderer *r, char c, int x, int y, int scale) {
    switch (c) {
        case 'A':
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x, y + 1 * scale, 1 * scale, 4 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 1 * scale, 1 * scale, 4 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 2 * scale, 2 * scale, 1 * scale});
            break;
        case 'B':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 3 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 1 * scale, 1 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 3 * scale, 1 * scale, 1 * scale});
            break;
        case 'D':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 4 * scale, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 1 * scale, 1 * scale, 3 * scale});
            break;
        case 'E':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 4 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x, y + 2 * scale, 3 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x, y + 4 * scale, 4 * scale, 1 * scale});
            break;
        case 'L':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x, y + 4 * scale, 4 * scale, 1 * scale});
            break;
        case 'M':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 4 * scale, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 1 * scale, 1 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 2 * scale, y + 2 * scale, 1 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 1 * scale, 1 * scale, 1 * scale});
            break;
        case 'N':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 1 * scale, 1 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 2 * scale, y + 2 * scale, 1 * scale, 2 * scale});
            break;
        case 'O':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y + 1 * scale, 1 * scale, 3 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 1 * scale, 1 * scale, 3 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 4 * scale, 2 * scale, 1 * scale});
            break;
        case 'P':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 2 * scale, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 1 * scale, 1 * scale, 1 * scale});
            break;
        case 'R':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 5 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 2 * scale, 2 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 1 * scale, 1 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y + 3 * scale, 1 * scale, 2 * scale});
            break;
        case 'V':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 3 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 3 * scale, y, 1 * scale, 3 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 3 * scale, 1 * scale, 1 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 2 * scale, y + 4 * scale, 1 * scale, 1 * scale});
            break;
        case 'Y':
            SDL_RenderFillRect(r, &(SDL_Rect){x, y, 1 * scale, 2 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 2 * scale, y, 1 * scale, 2 * scale});
            SDL_RenderFillRect(r, &(SDL_Rect){x + 1 * scale, y + 2 * scale, 1 * scale, 3 * scale});
            break;
        default:
            break;
    }
}

static void DrawString(SDL_Renderer *r, const char *text, int x, int y, int scale) {
    int curr_x = x;
    while (*text) {
        if (*text == ' ') {
            curr_x += 3 * scale;
        } else {
            DrawChar(r, *text, curr_x, y, scale);
            curr_x += 5 * scale;
        }
        text++;
    }
}

/* ===========================================================================
 * Game Mode Routine
 * =========================================================================== */
void GM_EndDemo_Screen(void) {
    SDL_Texture *logo_texture = NULL;
    int logo_w = 0;
    int logo_h = 0;

    SDL_Texture *logo2_texture = NULL;
    int logo2_w = 0;
    int logo2_h = 0;
    
    /* Variables de pulso para el texto */
    uint32_t sondeo_timer = 0;
    uint32_t sondeo_phase = 0;
    const uint32_t sondeo_speed = 4;

    /* Detener música anterior y lanzar la de demo */
    Sound_Queue(bgm_Stop, false);
    Sound_Queue(bgm_Demo, true);

    /* Cargar primer logo BMP */
    SDL_Surface *temp_surface = SDL_LoadBMP("assets/other/Tech.bmp");
    if (temp_surface) {
        logo_w = temp_surface->w;
        logo_h = temp_surface->h;
        logo_texture = SDL_CreateTextureFromSurface(renderer, temp_surface);
        SDL_FreeSurface(temp_surface);
    } else {
        printf("Advertencia: No se pudo cargar Tech.bmp: %s\n", SDL_GetError());
    }

    /* Cargar segundo logo BMP (Cambia la ruta por la de tu archivo) */
    SDL_Surface *temp_surface2 = SDL_LoadBMP("assets/other/End.bmp");
    if (temp_surface2) {
        logo2_w = temp_surface2->w;
        logo2_h = temp_surface2->h;
        logo2_texture = SDL_CreateTextureFromSurface(renderer, temp_surface2);
        SDL_FreeSurface(temp_surface2);
    } else {
        printf("Advertencia: No se pudo cargar Logo2.bmp: %s\n", SDL_GetError());
    }

    while (running) {
        Input_Read();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            }
        }

        /* Mantener resolución lógica interna */
        SDL_RenderSetLogicalSize(renderer, 320, 224);

        /* Limpiar a negro */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        /* Lógica de color pulsante */
        sondeo_timer++;
        if (sondeo_timer >= sondeo_speed) {
            sondeo_timer = 0;
            sondeo_phase = (sondeo_phase + 1) & 0x1F;
        }

        int pulse = (int)((sondeo_phase & 0x0F) - 8);
        int r = 0xE0 + (pulse * 8);
        int g = 0xE0 + (pulse * 8);
        int b = 0xE0 + (pulse * 8);

        /* Formateado limpio para evitar warnings de GCC */
        if (r < 0x40) { r = 0x40; }
        if (r > 0xFF) { r = 0xFF; }
        if (g < 0x40) { g = 0x40; }
        if (g > 0xFF) { g = 0xFF; }
        if (b < 0x40) { b = 0x40; }
        if (b > 0xFF) { b = 0xFF; }

        int center_x = 160;

        /* 1. Dibujar "END DEMO" arriba */
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        DrawString(renderer, "END DEMO", center_x - 40, 20, 2);

        /* 2. Dibujar el primer Logo BMP */
        if (logo_texture) {
            int target_w = 80;
            int target_h = 80;
            int target_x = 10;
            int target_y = 180;
            if (logo_w > 0 && logo_h > 0) {
                target_h = (logo_h * target_w) / logo_w;
            }
            SDL_Rect logo_rect = { target_x, target_y, target_w * 4, target_h * 4 };
            SDL_RenderCopy(renderer, logo_texture, NULL, &logo_rect);
        }

        /* 3. Dibujar "DEVELOPED BY" */
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        DrawString(renderer, "DEVELOPED BY", center_x - 60, 155, 2);

        /* 4. Dibujar el segundo Logo BMP */
        if (logo2_texture) {
            int target_w2 = 60;
            int target_h2 = 60;
            int target_x2 = 120; /* Centrado por defecto */
            int target_y2 = 50;

            if (logo2_w > 0 && logo2_h > 0) {
                target_h2 = (logo2_h * target_w2) / logo2_w;
            }
            SDL_Rect logo2_rect = { target_x2, target_y2, target_w2 * 2, target_h2 * 2 };
            SDL_RenderCopy(renderer, logo2_texture, NULL, &logo2_rect);
        }

        SDL_RenderPresent(renderer);

        /* Presionar Start */
        if (v_jpadpress1 & btnStart) {
            Sound_Queue(sfx_EnterSS, false);
            break;
        }

        SDL_Delay(16);
    }

    /* Limpieza de ambas texturas */
    if (logo_texture) {
        SDL_DestroyTexture(logo_texture);
    }
    if (logo2_texture) {
        SDL_DestroyTexture(logo2_texture);
    }

    Sound_Queue(bgm_Stop, false);
    v_gamemode = GM_Title;
}