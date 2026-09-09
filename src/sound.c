#include "sound.h"
#include "constants.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>

/* Paths are relative to the working directory (data.c uses "./assets/%s"). */

/* ---------------------------------------------------------------------------
   Music table: bgm id -> assets/Music ogg files
   ------------------------------------------------------------------------ */

typedef struct { int id; const char *path; } snd_map_t;

static const snd_map_t bgm_map[] = {
    { bgm_GHZ,         "assets/Music/GreenHill.ogg" },
    { bgm_LZ,          "assets/Music/Labyrinth.ogg" },
    { bgm_MZ,          "assets/Music/Marble.ogg" },
    { bgm_SLZ,         "assets/Music/StarLight.ogg" },
    { bgm_SYZ,         "assets/Music/SpringYard.ogg" },
    { bgm_SBZ,         "assets/Music/ScrapBrain.ogg" },
    { bgm_Invincible,  "assets/Music/Invincibility.ogg" },
    { bgm_ExtraLife,   "assets/Music/ActComplete.ogg" },
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
    /* bgm_Emerald has no dedicated file; keep it silent (SS music stays). */
};

/* ---------------------------------------------------------------------------
   SFX table: sfx id -> assets/SoundFX/...wav
   ------------------------------------------------------------------------ */

static const snd_map_t sfx_map[] = {
    { sfx_Jump,         "assets/SoundFX/Global/Jump.wav" },
    { sfx_Lamppost,     "assets/SoundFX/Global/LampPost.wav" },
    { sfx_Death,        "assets/SoundFX/Global/Hurt.wav" },
    { sfx_Skid,         "assets/SoundFX/Global/Skidding.wav" },
    { sfx_HitSpikes,    "assets/SoundFX/Global/Spike.wav" },
    { sfx_Push,         "assets/SoundFX/Stage/PushBlock.wav" },
    { sfx_SSGoal,       "assets/SoundFX/Global/SpecialWarp.wav" },
    { sfx_SSItem,       "assets/SoundFX/Global/SpecialRing.wav" },
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
    { sfx_Collapse,     "assets/SoundFX/Stage/LargeWall.wav" },
    { sfx_SSGlass,      "assets/SoundFX/Stage/GemBlockSS.wav" },
    { sfx_Door,         "assets/SoundFX/Stage/FlappingDoor.wav" },
    { sfx_Teleport,     "assets/SoundFX/Stage/Exit_SS.wav" },
    { sfx_ChainStomp,   "assets/SoundFX/Stage/ChainPull.wav" },
    { sfx_Roll,         "assets/SoundFX/Global/Rolling.wav" },
    { sfx_Continue,     "assets/SoundFX/Stage/Continue.wav" },
    { sfx_Basaran,      "assets/SoundFX/Stage/BatbrainFly.wav" },
    { sfx_BreakItem,    "assets/SoundFX/Stage/BlockBreak.wav" },
    { sfx_Warning,      "assets/SoundFX/Stage/DrownAlert.wav" },
    { sfx_GiantRing,    "assets/SoundFX/Global/SpecialRing.wav" },
    { sfx_Bomb,         "assets/SoundFX/Global/Explosion.wav" },
    { sfx_Cash,         "assets/SoundFX/Global/ScoreAdd.wav" },
    { sfx_RingLoss,     "assets/SoundFX/Global/LoseRings.wav" },
    { sfx_ChainRise,    "assets/SoundFX/Stage/ChainPull.wav" },
    { sfx_Burning,      "assets/SoundFX/Stage/FireBurn.wav" },
    { sfx_Bonus,        "assets/SoundFX/Global/BonusPoints.wav" },
    { sfx_EnterSS,      "assets/SoundFX/Global/SpecialWarp.wav" },
    { sfx_WallSmash,    "assets/SoundFX/Stage/LargeWall.wav" },
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
   State
   ------------------------------------------------------------------------ */

#define SND_SFX_TABLE_MAX  0x100   /* one slot per sfx id (0xA0-0xE1) */
#define SND_CHANNELS       16

static int        snd_inited    = 0;
static Mix_Music *snd_music     = NULL;
static char       snd_music_path[256];
static int        snd_music_bgm = 0;   /* bgm id of current track (0=none) */
static int        snd_music_loop = 1;  /* loop flag of current track */
static Mix_Chunk *snd_chunk[SND_SFX_TABLE_MAX];
static int        snd_ambient_channel = -1;

/* ---------------------------------------------------------------------------
   Lookup helpers
   ------------------------------------------------------------------------ */

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
   Music
   ------------------------------------------------------------------------ */

/* Start playing a music file. Returns 0 on success, -1 on failure.
   Does not touch the current track on failure; skips if already playing. */
static int snd_play_music_file(const char *path, bool loop) {
    Mix_Music *m;

    if (!path || !path[0]) return -1;

    if (Mix_PlayingMusic() && strcmp(snd_music_path, path) == 0) {
        return 0;   /* already playing this track */
    }

    m = Mix_LoadMUS(path);
    if (!m) {
        fprintf(stderr, "[Sound] Failed to load '%s': %s\n", path, Mix_GetError());
        return -1;
    }

    if (Mix_PlayMusic(m, loop ? -1 : 0) == -1) {
        fprintf(stderr, "[Sound] Failed to play '%s': %s\n", path, Mix_GetError());
        Mix_FreeMusic(m);
        return -1;
    }

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
   SFX
   ------------------------------------------------------------------------ */

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
   Public API
   ------------------------------------------------------------------------ */

void Sound_Init(void) {
    if (snd_inited) return;

    Mix_Init(MIX_INIT_OGG);

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        fprintf(stderr, "[Sound] Failed to open audio: %s\n", Mix_GetError());
        return;
    }

    Mix_AllocateChannels(SND_CHANNELS);
    memset(snd_chunk, 0, sizeof(snd_chunk));
    snd_inited = 1;
}

void Sound_Update(void) {
    /* SDL_mixer runs on its own thread; nothing to do per frame. */
}

void Sound_Queue(int id, bool loop) {
    if (!snd_inited) return;

    switch (id) {
        case bgm_Fade:
            Mix_FadeOutMusic(2000);
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