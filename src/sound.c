#include "sound.h"
#include "constants.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>

/* Paths are relative to the working directory (data.c uses "./assets/%s"). */

/* ---------------------------------------------------------------------------
 *  Music table: bgm id -> assets/Music ogg files
 *  ------------------------------------------------------------------------ */

typedef struct { int id; const char *path; } snd_map_t;

static const snd_map_t bgm_map[] = {
    { bgm_GHZ,         "assets/Music/GreenHill.ogg" },
    { bgm_LZ,          "assets/Music/Labyrinth.ogg" },
    { bgm_MZ,          "assets/Music/Marble.ogg" },
    { bgm_SLZ,         "assets/Music/StarLight.ogg" },
    { bgm_SYZ,         "assets/Music/SpringYard.ogg" },
    { bgm_SBZ,         "assets/Music/ScrapBrain.ogg" },
    { bgm_Invincible,  "assets/Music/Invincibility.ogg" },
    { bgm_ExtraLife,   "assets/SoundFX/Global/1Up.wav" },
    { bgm_SS,          "assets/Music/SpecialStage.ogg" },
    { bgm_Title,       "assets/Music/Titlescreen.ogg" },
    { bgm_Ending,      "assets/Music/Ending.ogg" },
    { bgm_Boss,        "assets/Music/Boss.ogg" },
    { bgm_FZ,          "assets/Music/Final.ogg" },
    { bgm_GotThrough,  "assets/Music/ActComplete.ogg" },
    { bgm_GameOver,    "assets/Music/GameOver.ogg" },
    { bgm_Continue,    "assets/Music/Continue.ogg" },
    { bgm_Credits,     "assets/Music/Credits.ogg" },
    { bgm_Drowning,    "assets/Music/Drowning.ogg" },
    { bgm_Emerald,    "assets/SoundFX/Stage/Emerald.wav" },
    /* bgm_Emerald has no dedicated file; keep it silent (SS music stays). */
};

/* ---------------------------------------------------------------------------
 *  SFX table: sfx id -> assets/SoundFX/...wav
 *  ------------------------------------------------------------------------ */

static const snd_map_t sfx_map[] = {
    { sfx_Jump,         "assets/SoundFX/Global/Jump.wav" },
    { sfx_Lamppost,     "assets/SoundFX/Global/LampPost.wav" },
    { sfx_Death,        "assets/SoundFX/Global/Hurt.wav" },
    { sfx_Skid,         "assets/SoundFX/Global/Skidding.wav" },
    { sfx_HitSpikes,    "assets/SoundFX/Global/Spike.wav" },
    { sfx_Push,         "assets/SoundFX/Stage/PushBlock.wav" },
    { sfx_SSGoal,       "assets/SoundFX/Stage/Exit_SS.wav" },
    { sfx_SSItem,       "assets/SoundFX/Stage/RotateSS.wav" },
    { sfx_Splash,       "assets/SoundFX/Stage/WaterSplash.wav" },
    { sfx_HitBoss,      "assets/SoundFX/Stage/BossHit.wav" },
    { sfx_Bubble,       "assets/SoundFX/Global/BubbleBounce.wav" },
    { sfx_Fireball,     "assets/SoundFX/Stage/SmallFireball.wav" },
    { sfx_Shield,       "assets/SoundFX/Global/BlueShield.wav" },
    { sfx_Saw,          "assets/SoundFX/Stage/Buzzsaw.wav" },
    { sfx_Electric,     "assets/SoundFX/Stage/ElectricArc.wav" },
    { sfx_Drown,        "assets/SoundFX/Stage/DrownAlert.wav" },
    { sfx_Flamethrower, "assets/SoundFX/Stage/FlameThrower.wav" },
    { sfx_Bumper,       "assets/SoundFX/Stage/Bumper.wav" },
    { sfx_Ring,         "assets/SoundFX/Global/Ring.wav" },
    { sfx_SpikesMove,   "assets/SoundFX/Global/SpikesMove.wav" },
    { sfx_Rumbling,     "assets/SoundFX/Stage/Crusher.wav" },
    { sfx_Collapse,     "assets/SoundFX/Stage/LedgeBreak.wav" },
    { sfx_SSGlass,      "assets/SoundFX/Stage/GemBlockSS.wav" },
    { sfx_Door,         "assets/SoundFX/Stage/FlappingDoor.wav" },
    { sfx_Teleport,     "assets/SoundFX/Stage/Exit_SS.wav" },
    { sfx_ChainStomp,   "assets/SoundFX/Stage/ChainPull.wav" },
    { sfx_Roll,         "assets/SoundFX/Global/Rolling.wav" },
    { sfx_Continue,     "assets/SoundFX/Stage/Continue.wav" },
    { sfx_Basaran,      "assets/SoundFX/Stage/BatbrainFly.wav" },
    { sfx_BreakItem,    "assets/SoundFX/Global/Destroy.wav" },
    { sfx_Warning,      "assets/SoundFX/Stage/DrownAlert.wav" },
    { sfx_GiantRing,    "assets/SoundFX/Global/SpecialRing.wav" },
    { sfx_Bomb,         "assets/SoundFX/Global/Explosion.wav" },
    { sfx_Cash,         "assets/SoundFX/Global/ScoreAdd.wav" },
    { sfx_RingLoss,     "assets/SoundFX/Global/LoseRings.wav" },
    { sfx_ChainRise,    "assets/SoundFX/Stage/ChainPull.wav" },
    { sfx_Burning,      "assets/SoundFX/Stage/FireBurn.wav" },
    { sfx_Bonus,        "assets/SoundFX/Global/BonusPoints.wav" },
    { sfx_EnterSS,      "assets/SoundFX/Global/SpecialWarp.wav" },
    { sfx_WallSmash,    "assets/SoundFX/Stage/BlockBreak.wav" },
    { sfx_Spring,       "assets/SoundFX/Global/Spring.wav" },
    { sfx_Switch,       "assets/SoundFX/Stage/ButtonPress.wav" },
    { sfx_RingLeft,     "assets/SoundFX/Global/Ring.wav" },
    { sfx_Signpost,     "assets/SoundFX/Global/SignPost.wav" },
    /* Special (spec__): continuous/looping sounds */
    { sfx_Waterfall,    "assets/SoundFX/Stage/Waterfall.wav" },
    { sfx_Sega,         "assets/SoundFX/Stage/Sega.wav" },
};

/* Ambient SFX that loop while playing (skipped if already active). */
static const int snd_looping_sfx[] = { sfx_Waterfall, sfx_Rumbling };

/* ---------------------------------------------------------------------------
 *  State
 *  ------------------------------------------------------------------------ */

#define SND_SFX_TABLE_MAX  0x100   /* one slot per sfx id (0xA0-0xE1) */
#define SND_CHANNELS       16

static int        snd_inited    = 0;
static Mix_Music *snd_music     = NULL;
static char       snd_music_path[256];
static int        snd_music_bgm = 0;   /* bgm id of current track (0=none) */
static int        snd_music_loop = 1;  /* loop flag of current track */

/* Custom loop points read from <music>.txt (milisegundos -> segundos). */
static double snd_music_loop_start = 0.0;
static double snd_music_loop_end   = 0.0;
static int    snd_music_loop_enabled = 0;

/* Flags de control del loop personalizado:
 *    snd_music_fading        = 1 mientras Mix_FadeOutMusic está en curso.
 *                              Bloquea el hook finished (si no, el fade
 *                              dispara el callback al terminar y revive
 *                              la música que acabamos de apagar).
 *    snd_music_needs_restart = el hook finished lo levanta, Sound_Update
 *                              lo consume desde el main thread. Evita
 *                              llamar a Mix_PlayMusic dentro del propio
 *                              callback de SDL_mixer (riesgo de deadlock). */
static volatile int snd_music_fading        = 0;
static volatile int snd_music_needs_restart = 0;
static int snd_skip_poll_frames = 0;
static Mix_Chunk *snd_chunk[SND_SFX_TABLE_MAX];
static int        snd_ambient_channel = -1;

/* ---------------------------------------------------------------------------
 *  Lookup helpers
 *  ------------------------------------------------------------------------ */

static const char *snd_map_find(const snd_map_t *map, size_t n, int id) {
    for (size_t i = 0; i < n; i++) {
        if (map[i].id == id) return map[i].path;
    }
    return NULL;
}

static const char *snd_bgm_path(int id) {
    return snd_map_find(bgm_map, sizeof(bgm_map) / sizeof(bgm_map[0]), id);
}

static const char *snd_sfx_path(int id) {
    return snd_map_find(sfx_map, sizeof(sfx_map) / sizeof(sfx_map[0]), id);
}

/* Build the speed-shoes variant of a music path: "X.ogg" -> "X_F.ogg". */
static void snd_make_fast_path(char *out, size_t n, const char *path) {
    const char *dot = strrchr(path, '.');
    size_t base = dot ? (size_t)(dot - path) : strlen(path);
    const char *ext = dot ? dot : ".ogg";
    snprintf(out, n, "%.*s_F%s", (int)base, path, ext);
}

/* ---------------------------------------------------------------------------
 *  Music — custom loop points
 *  ------------------------------------------------------------------------ */

/* Deriva la ruta del .txt a partir de la del .ogg/.wav. */
static void snd_derive_txt_path(char *out, size_t n, const char *music_path) {
    snprintf(out, n, "%s", music_path);
    char *dot = strrchr(out, '.');
    if (dot) snprintf(dot, n - (size_t)(dot - out), ".txt");
    else     snprintf(out + strlen(out), n - strlen(out), ".txt");
}

/* Lee loop_start_ms / loop_end_ms de <music_path>.txt.
 *  Si no existe o está mal, deja el loop personalizado desactivado
 *  y la música cae al loop nativo de SDL_mixer (archivo completo). */
static void snd_load_loop_points(const char *music_path) {
    snd_music_loop_start   = 0.0;
    snd_music_loop_end     = 0.0;
    snd_music_loop_enabled = 0;

    char txt_path[512];
    snd_derive_txt_path(txt_path, sizeof(txt_path), music_path);

    FILE *f = fopen(txt_path, "r");
    if (!f) return;

    long long start_ms = -1, end_ms = -1;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        long long v;
        if (sscanf(line, "loop_start_ms=%lld", &v) == 1)      start_ms = v;
        else if (sscanf(line, "loop_end_ms=%lld", &v) == 1)   end_ms   = v;
    }
    fclose(f);

    if (start_ms >= 0 && end_ms > start_ms) {
        snd_music_loop_start   = (double)start_ms / 1000.0;
        snd_music_loop_end     = (double)end_ms   / 1000.0;
        snd_music_loop_enabled = 1;
        fprintf(stderr, "[Sound] Loop points for '%s': %.3f .. %.3f s\n",
                music_path, snd_music_loop_start, snd_music_loop_end);
    }
}

/* Hook de SDL_mixer: la música llegó al final natural del archivo.
 *  Sólo levantamos un flag; el restart real lo hace Sound_Update desde
 *  el main thread. Llamar a Mix_PlayMusic aquí dentro puede provocar
 *  deadlocks en algunas versiones de SDL_mixer. */
static void snd_music_finished_cb(void) {
    if (snd_music_fading)        return;   /* fade en curso: no revivir */
        if (!snd_music)              return;
        if (!snd_music_loop)         return;
        if (!snd_music_loop_enabled) return;

        snd_music_needs_restart = 1;
}

static int snd_play_music_file(const char *path, bool loop) {
    Mix_Music *m;

    if (!path || !path[0]) return -1;


    if (Mix_PlayingMusic() && Mix_FadingMusic() == MIX_NO_FADING
        && strcmp(snd_music_path, path) == 0) {
        return 0;
        }

        m = Mix_LoadMUS(path);
    if (!m) {
        fprintf(stderr, "[Sound] Failed to load '%s': %s\n", path, Mix_GetError());
        return -1;
    }


    snd_load_loop_points(path);


    int play_loops = 0;
    if (loop && !snd_music_loop_enabled) {
        play_loops = -1;
    }

    if (Mix_PlayMusic(m, play_loops) == -1) {
        fprintf(stderr, "[Sound] Failed to play '%s': %s\n", path, Mix_GetError());
        Mix_FreeMusic(m);
        return -1;
    }


    snd_music_fading        = 0;
    snd_music_needs_restart = 0;


    Mix_SetMusicPosition(0.001);


    snd_skip_poll_frames = 6;

    if (snd_music) Mix_FreeMusic(snd_music);
    snd_music = m;
    snprintf(snd_music_path, sizeof(snd_music_path), "%s", path);
    snd_music_loop = loop;
    return 0;
}

static void snd_play_bgm(int id, bool loop) {
    const char *path = snd_bgm_path(id);
    if (snd_play_music_file(path, loop) == 0) {
        snd_music_bgm = id;
    }
}

static void snd_bgm_speedup(void) {
    char fast[256];
    const char *path;

    if (snd_music_bgm == 0) return;
    path = snd_bgm_path(snd_music_bgm);
    if (!path) return;

    snd_make_fast_path(fast, sizeof(fast), path);
    /* If there is no _F variant, stay on the normal track. */
    if (snd_play_music_file(fast, snd_music_loop) != 0) {
        snd_play_music_file(path, snd_music_loop);
    }
}

static void snd_bgm_slowdown(void) {
    const char *path;

    if (snd_music_bgm == 0) return;
    path = snd_bgm_path(snd_music_bgm);
    if (!path) return;

    snd_play_music_file(path, snd_music_loop);
}

/* ---------------------------------------------------------------------------
 *  SFX
 *  ------------------------------------------------------------------------ */

static void snd_play_sfx(int id, int loop) {
    Mix_Chunk *ch;
    const char *path;
    int channel;

    path = snd_sfx_path(id);
    if (!path) return;

    /* Ambient SFX: skip if the looping instance is already active. */
    for (size_t i = 0; i < sizeof(snd_looping_sfx) / sizeof(snd_looping_sfx[0]); i++) {
        if (snd_looping_sfx[i] == id) {
            if (snd_ambient_channel >= 0 && Mix_Playing(snd_ambient_channel)) return;
            loop = 1;
            break;
        }
    }

    if (id >= 0 && id < SND_SFX_TABLE_MAX) {
        ch = snd_chunk[id];
    } else {
        ch = NULL;
    }

    if (!ch) {
        ch = Mix_LoadWAV(path);
        if (!ch) {
            fprintf(stderr, "[Sound] Failed to load '%s': %s\n", path, Mix_GetError());
            return;
        }
        if (id >= 0 && id < SND_SFX_TABLE_MAX) snd_chunk[id] = ch;
    }

    Mix_VolumeChunk(ch, MIX_MAX_VOLUME);
    channel = Mix_PlayChannel(-1, ch, loop ? -1 : 0);
    if (loop) snd_ambient_channel = channel;
}

/* ---------------------------------------------------------------------------
 *  Public API
 *  ------------------------------------------------------------------------ */

void Sound_Init(void) {
    if (snd_inited) return;

    Mix_Init(MIX_INIT_OGG);

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        fprintf(stderr, "[Sound] Failed to open audio: %s\n", Mix_GetError());
        return;
    }

    Mix_AllocateChannels(SND_CHANNELS);
    Mix_HookMusicFinished(snd_music_finished_cb);
    memset(snd_chunk, 0, sizeof(snd_chunk));
    snd_inited = 1;
}

void Sound_Quit(void) {
    if (!snd_inited) return;

    Mix_HaltMusic();
    Mix_HaltChannel(-1);

    if (snd_music) {
        Mix_FreeMusic(snd_music);
        snd_music = NULL;
    }
    for (int i = 0; i < SND_SFX_TABLE_MAX; i++) {
        if (snd_chunk[i]) {
            Mix_FreeChunk(snd_chunk[i]);
            snd_chunk[i] = NULL;
        }
    }

    /* Must stop the mixer thread before SDL_Quit() tears down audio. */
    Mix_CloseAudio();
    Mix_Quit();
    snd_inited = 0;
}

void Sound_Update(void) {
    if (!snd_inited) return;

    /* Saltar el polling durante los primeros frames tras Play: la
     *      posición que reporta SDL_mixer aún puede ser la heredada del
     *      tema anterior. */
    if (snd_skip_poll_frames > 0) {
        snd_skip_poll_frames--;
        /* Aun así, procesa el restart diferido del hook finished por
         *          si la pista anterior acabó justo antes del cambio. */
    } else {
        /* 2) Loop por polling: ... (tu código actual) */
        if (snd_music && snd_music_loop && snd_music_loop_enabled &&
            Mix_PlayingMusic() && Mix_FadingMusic() == MIX_NO_FADING &&
            snd_music_loop_end > 0.0) {
            double pos = Mix_GetMusicPosition(snd_music);

        if (pos >= 0.0) {
            double margin = 4096.0 / 44100.0;
            if (pos >= snd_music_loop_end - margin) {
                Mix_SetMusicPosition(snd_music_loop_start);
            }
        }
            }
    }

    /* 1) Consumir el flag diferido del hook finished (sin tocar). */
    if (snd_music_needs_restart && snd_music && snd_music_loop &&
        snd_music_loop_enabled) {
        snd_music_needs_restart = 0;
    if (Mix_PlayMusic(snd_music, 0) == 0) {
        if (snd_music_loop_start > 0.001) {
            Mix_SetMusicPosition(snd_music_loop_start);
        }
    }
        }
}

void Sound_Queue(int id, bool loop) {
    if (!snd_inited) return;

    switch (id) {
        case bgm_Fade:

            snd_music_fading = 1;
            Mix_FadeOutMusic(2000);


            snd_music_path[0] = 0;
            return;

        case bgm_Speedup:
            snd_bgm_speedup();
            return;

        case bgm_Slowdown:
            snd_bgm_slowdown();
            return;

        case bgm_Stop:
            Mix_HaltMusic();
            snd_music_bgm = 0;
            snd_music_path[0] = 0;
            snd_music_loop_enabled = 0;
            snd_music_loop_start = 0.0;
            snd_music_loop_end   = 0.0;
            snd_music_needs_restart = 0;
            snd_music_fading = 0;
            return;

        default:
            break;
    }

    if (id >= bgm__First && id <= bgm__Last) {
        snd_play_bgm(id, loop);
    } else if ((id >= sfx__First && id <= sfx__Last) || id == sfx_Waterfall
        || id == sfx_Sega) {
        snd_play_sfx(id, loop ? 1 : 0);
        } else {
            /* 0x80, 0x94-0x9F, etc. = unused slots */
        }
}
