#ifndef SONIC1_SOUND_H
#define SONIC1_SOUND_H

#include "types.h"

/* Sound system driven by SDL_mixer.
   Music is loaded by bgm id (0x81-0x93) from assets/Music (ogg files),
   sound effects by sfx id (0xA0-0xCF, 0xE1) from assets/SoundFX (wav files).
   Sound command bytes (bgm_Fade/Speedup/Slowdown/Stop) are handled here. */

void Sound_Init(void);
void Sound_Quit(void);     /* Free mixer resources and stop the audio thread */
void Sound_Update(void);   /* Called once per frame (no-op with SDL_mixer) */
void Sound_Queue(int id, bool loop);  /* Queue a music/sfx id or sound command; loop only matters for music and looping sfx */

#endif /* SONIC1_SOUND_H */