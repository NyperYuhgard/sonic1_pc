/* config.h */
#ifndef SONIC1_CONFIG_H
#define SONIC1_CONFIG_H
#include "types.h"

typedef struct {
    int  widescreen;      /* 0 = 320, si no: ancho deseado (398, 424, 480) */
    int  fps_interp;      /* 0 = off, 1 = 60→120 Hz interpolado */
    int  scanlines;       /* 0/1 efecto CRT */
    int  fullscreen;      /* 0/1 */
    int  ss_alt_anim;
} Settings;

extern Settings g_settings;

void Settings_Load(void);
void Settings_Save(void);

/* Ancho de render efectivo (siempre par, mínimo 320). */
int  Settings_RenderWidth(void);
#endif