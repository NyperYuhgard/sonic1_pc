/* config.c */
#include "config.h"
#include <stdio.h>
#include <string.h>

Settings g_settings = {
    .widescreen = 0,      /* default: 1:1 con Genesis */
    .fps_interp = 0,
    .scanlines  = 0,
    .fullscreen = 0,
    .ss_alt_anim = 0,
    .ss_smooth = 0,
};

#define CFG_PATH "sonic1.cfg"

void Settings_Load(void) {
    FILE *f = fopen(CFG_PATH, "rb");
    if (!f) return;
    /* Cabecera mágica + versión para no romper si cambia el layout */
    char magic[4]; uint32_t ver;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "S1CF", 4) != 0) { fclose(f); return; }
    if (fread(&ver, 4, 1, f) != 1 || ver != 3) { fclose(f); return; }
    fread(&g_settings, sizeof(g_settings), 1, f);
    fclose(f);
}

void Settings_Save(void) {
    FILE *f = fopen(CFG_PATH, "wb");
    if (!f) return;
    fwrite("S1CF", 1, 4, f);
    uint32_t ver = 3;
    fwrite(&ver, 4, 1, f);
    fwrite(&g_settings, sizeof(g_settings), 1, f);
    fclose(f);
}

int Settings_RenderWidth(void) {
    if (g_settings.widescreen < 320) return 320;
    /* Forzamos par para que el centrado sea exacto */
    return g_settings.widescreen & ~1;
}