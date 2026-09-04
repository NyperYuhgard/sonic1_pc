#ifndef SONIC1_SOUND_H
#define SONIC1_SOUND_H

#include "types.h"

/* Sound system stub - Z80 driver is a separate subsystem.
   For now, all sound functions are no-ops. */

void Sound_Init(void);
void Sound_Update(void);   /* Called once per frame (equivalent to UpdateMusic) */
void Sound_Queue(int id);  /* Queue a sound/music by ID */

#endif /* SONIC1_SOUND_H */
