/* tools/looptool.c — editor de loop points para archivos OGG
 *
 * Build:
 *   gcc -O2 -o looptool looptool.c \
 *       $(pkg-config --cflags --libs sdl2 vorbisfile)
 *
 * Uso:
 *   ./looptool ruta/al/GreenHill.ogg
 *
 * Genera automáticamente GreenHill.txt junto al OGG.
 *
 * Controles:
 *   Espacio      play / pause
 *   L            toggle loop mode (usa los markers)
 *   1            marcar loop_start en la posición actual
 *   2            marcar loop_end en la posición actual
 *   Home / End   ir al inicio / final
 *   ← / →        seek 1 segundo (Shift: 5 segundos)
 *   Click        seek
 *   Shift+Click  marcar loop_start
 *   Ctrl+Click   marcar loop_end
 *   S            guardar .txt
 *   Q / Esc      salir (auto-guarda si hay cambios)
 */

#include <SDL2/SDL.h>
#include <vorbis/vorbisfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define WIN_W   1200
#define WIN_H   500
#define WAVE_Y  100
#define WAVE_H  300

static int16_t  *pcm        = NULL;   /* muestras decodificadas */
static int64_t   pcm_len    = 0;      /* en frames (samples por canal) */
static int       pcm_rate   = 44100;
static int       pcm_chans  = 2;

static int64_t   loop_start = 0;
static int64_t   loop_end   = 0;
static int64_t   play_pos   = 0;

static volatile bool playing   = false;
static volatile bool loop_mode = false;
static volatile bool dirty     = false;

static SDL_AudioDeviceID dev = 0;
static char out_path[1024];
static char title[256];

/* ------------------------------------------------------------------ */
/*  Decodificación                                                     */
/* ------------------------------------------------------------------ */

static int decode_ogg(const char *path) {
    OggVorbis_File vf;
    if (ov_fopen(path, &vf) != 0) return -1;

    vorbis_info *vi = ov_info(&vf, -1);
    pcm_rate  = vi->rate;
    pcm_chans = vi->channels;

    int64_t cap = 0, len = 0;
    int16_t *buf = NULL;
    int16_t tmp[4096];
    int bs;

    for (;;) {
        long r = ov_read(&vf, (char *)tmp, sizeof(tmp), 0, 2, 1, &bs);
        if (r <= 0) break;
        int64_t got = r / 2;
        if (len + got > cap) {
            cap = cap ? cap * 2 : (1 << 20);
            while (cap < len + got) cap *= 2;
            buf = realloc(buf, cap * sizeof(int16_t));
        }
        memcpy(buf + len, tmp, got * sizeof(int16_t));
        len += got;
    }
    ov_clear(&vf);

    pcm     = buf;
    pcm_len = len / pcm_chans;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Audio (callback propio)                                            */
/* ------------------------------------------------------------------ */

static void audio_cb(void *u, Uint8 *stream, int bytes) {
    (void)u;
    int16_t *out = (int16_t *)stream;
    int frames = bytes / (sizeof(int16_t) * pcm_chans);

    for (int i = 0; i < frames; i++) {
        if (!playing) {
            for (int c = 0; c < pcm_chans; c++) *out++ = 0;
            continue;
        }
        /* Wrap al loop_start si hemos superado loop_end */
        if (loop_mode && loop_end > loop_start && play_pos >= loop_end) {
            play_pos = loop_start;
        }
        /* Final del archivo */
        if (play_pos >= pcm_len) {
            if (loop_mode && loop_end > loop_start) {
                play_pos = loop_start;
            } else {
                playing = false;
                for (int c = 0; c < pcm_chans; c++) *out++ = 0;
                continue;
            }
        }
        for (int c = 0; c < pcm_chans; c++)
            *out++ = pcm[play_pos * pcm_chans + c];
        play_pos++;
    }
}

/* ------------------------------------------------------------------ */
/*  Utilidades de tiempo                                               */
/* ------------------------------------------------------------------ */

static void sample_to_ms(int64_t s, char *out, size_t n) {
    int64_t ms = s * 1000 / pcm_rate;
    int64_t min = ms / 60000; ms %= 60000;
    int64_t sec = ms / 1000;  ms %= 1000;
    snprintf(out, n, "%02lld:%02lld.%03lld",
             (long long)min, (long long)sec, (long long)ms);
}

static void log_markers(void) {
    char a[32], b[32];
    sample_to_ms(loop_start, a, sizeof(a));
    sample_to_ms(loop_end,   b, sizeof(b));
    fprintf(stderr, "[looptool] loop = %s .. %s\n", a, b);
}

static int64_t x_to_sample(int x) {
    int64_t s = (int64_t)x * pcm_len / WIN_W;
    if (s < 0) s = 0;
    if (s >= pcm_len) s = pcm_len - 1;
    return s;
}

static int sample_to_x(int64_t s) {
    return (int)((int64_t)s * WIN_W / pcm_len);
}

/* ------------------------------------------------------------------ */
/*  Render                                                             */
/* ------------------------------------------------------------------ */

static void render(SDL_Renderer *r) {
    SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
    SDL_RenderClear(r);

    /* Waveform: min/max por columna, submuestreado */
    SDL_SetRenderDrawColor(r, 80, 200, 80, 255);
    for (int x = 0; x < WIN_W; x++) {
        int64_t s0 = (int64_t)x       * pcm_len / WIN_W;
        int64_t s1 = (int64_t)(x + 1) * pcm_len / WIN_W;
        if (s1 <= s0) s1 = s0 + 1;
        int16_t mn = 32767, mx = -32768;
        int64_t step = (s1 - s0) / 256;
        if (step < 1) step = 1;
        for (int64_t i = s0; i < s1; i += step) {
            int16_t v = pcm[i * pcm_chans];   /* canal izquierdo */
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        int y0 = WAVE_Y + WAVE_H / 2 - (int)((int32_t)mx * WAVE_H / 2 / 32768);
        int y1 = WAVE_Y + WAVE_H / 2 - (int)((int32_t)mn * WAVE_H / 2 / 32768);
        SDL_RenderDrawLine(r, x, y0, x, y1);
    }

    /* Línea de cero */
    SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
    SDL_RenderDrawLine(r, 0, WAVE_Y + WAVE_H / 2, WIN_W, WAVE_Y + WAVE_H / 2);

    /* Región de loop */
    if (loop_end > loop_start) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 80, 80, 200, 60);
        SDL_Rect rc = {
            sample_to_x(loop_start), WAVE_Y,
            sample_to_x(loop_end) - sample_to_x(loop_start), WAVE_H
        };
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    /* Marker loop_start (verde) */
    SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
    int x_start = sample_to_x(loop_start);
    SDL_RenderDrawLine(r, x_start, WAVE_Y - 30, x_start, WAVE_Y + WAVE_H + 30);

    /* Marker loop_end (rojo) */
    SDL_SetRenderDrawColor(r, 255, 60, 60, 255);
    int x_end = sample_to_x(loop_end);
    SDL_RenderDrawLine(r, x_end, WAVE_Y - 30, x_end, WAVE_Y + WAVE_H + 30);

    /* Playhead (amarillo) */
    SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
    int xp = sample_to_x(play_pos);
    SDL_RenderDrawLine(r, xp, WAVE_Y - 20, xp, WAVE_Y + WAVE_H + 20);

    SDL_RenderPresent(r);
}

/* ------------------------------------------------------------------ */
/*  Guardar                                                            */
/* ------------------------------------------------------------------ */

static void save_loop_txt(void) {
    FILE *f = fopen(out_path, "w");
    if (!f) { perror(out_path); return; }
    fprintf(f, "# Loop points for %s\n", title);
    fprintf(f, "# Generated by looptool\n");
    fprintf(f, "loop_start_ms=%lld\n", (long long)(loop_start * 1000 / pcm_rate));
    fprintf(f, "loop_end_ms=%lld\n",   (long long)(loop_end   * 1000 / pcm_rate));
    fclose(f);
    dirty = false;
    fprintf(stderr, "[looptool] Saved %s\n", out_path);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.ogg>\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1];

    /* Ruta de salida: mismo nombre, extensión .txt */
    snprintf(out_path, sizeof(out_path), "%s", in_path);
    char *dot = strrchr(out_path, '.');
    if (dot) snprintf(dot, sizeof(out_path) - (size_t)(dot - out_path), ".txt");
    else     snprintf(out_path + strlen(out_path),
                      sizeof(out_path) - strlen(out_path), ".txt");

    const char *base = strrchr(in_path, '/');
    snprintf(title, sizeof(title), "%s", base ? base + 1 : in_path);

    if (decode_ogg(in_path) != 0) {
        fprintf(stderr, "Failed to decode %s\n", in_path);
        return 1;
    }
    fprintf(stderr, "[looptool] %s: %lld frames, %d Hz, %d ch\n",
            title, (long long)pcm_len, pcm_rate, pcm_chans);

    /* Loop por defecto = archivo completo */
    loop_start = 0;
    loop_end   = pcm_len;

    /* Si ya existe el .txt, cargarlo */
    FILE *f = fopen(out_path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            long long v;
            if (sscanf(line, "loop_start_ms=%lld", &v) == 1)
                loop_start = v * pcm_rate / 1000;
            else if (sscanf(line, "loop_end_ms=%lld", &v) == 1)
                loop_end = v * pcm_rate / 1000;
        }
        fclose(f);
        if (loop_end <= 0) loop_end = pcm_len;
        fprintf(stderr, "[looptool] Loaded existing loop points\n");
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window   *win = SDL_CreateWindow("looptool",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H,
        SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_AudioSpec want = {0}, have;
    want.freq     = pcm_rate;
    want.format   = AUDIO_S16SYS;
    want.channels = pcm_chans;
    want.samples  = 2048;
    want.callback = audio_cb;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(dev, 0);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                SDL_Keymod  m = SDL_GetModState();
                switch (k) {
                    case SDLK_ESCAPE:
                    case SDLK_q:     running = false; break;
                    case SDLK_SPACE:
                        if (playing) playing = false;
                        else {
                            if (play_pos >= pcm_len) play_pos = 0;
                            playing = true;
                        }
                        break;
                    case SDLK_l:     loop_mode = !loop_mode;
                                     fprintf(stderr, "[looptool] loop %s\n",
                                             loop_mode ? "ON" : "OFF");
                                     break;
                    case SDLK_s:     save_loop_txt(); break;
                    case SDLK_1:     loop_start = play_pos; dirty = true;
                                     log_markers(); break;
                    case SDLK_2:     loop_end   = play_pos; dirty = true;
                                     log_markers(); break;
                    case SDLK_HOME:  play_pos = 0; break;
                    case SDLK_END:   play_pos = pcm_len - 1; break;
                    case SDLK_LEFT: {
                        int64_t step = (m & KMOD_SHIFT) ? pcm_rate * 5 : pcm_rate;
                        play_pos -= step;
                        if (play_pos < 0) play_pos = 0;
                    } break;
                    case SDLK_RIGHT: {
                        int64_t step = (m & KMOD_SHIFT) ? pcm_rate * 5 : pcm_rate;
                        play_pos += step;
                        if (play_pos >= pcm_len) play_pos = pcm_len - 1;
                    } break;
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                SDL_Keymod m = SDL_GetModState();
                int64_t s = x_to_sample(e.button.x);
                if (m & KMOD_SHIFT) {
                    loop_start = s; dirty = true; log_markers();
                } else if (m & KMOD_CTRL) {
                    loop_end = s; dirty = true; log_markers();
                } else {
                    play_pos = s;
                }
            }
        }
        render(ren);
        SDL_Delay(16);
    }

    if (dirty) {
        fprintf(stderr, "[looptool] Unsaved changes, saving...\n");
        save_loop_txt();
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    free(pcm);
    return 0;
}