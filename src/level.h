#ifndef SONIC1_LEVEL_H
#define SONIC1_LEVEL_H

#include "types.h"

/* GM_Level: called once per frame from MainGameLoop.
   Handles one-time init (when level_init_done == 0) and per-frame gameplay. */
void Level_Process(void);

/* Level entry-point helpers (also used internally) */
void LevelSizeLoad(void);
void LevelLayoutLoad(void);
void DrawChunks(void);
void LoadTilesFromStart(void);
void LevelDataLoad(void);
void LevelHeaders_Init(void);

/* Collision */
void ColIndexLoad(void);
void ConvertCollisionArray(void);
const uint8_t *GetColIndex(void);

/* Player / HUD spawning (called during init) */
void LevelSpawnPlayer(void);
void LevelSpawnHUD(void);

/* Object position manager — reads object layout data and spawns objects
   as the camera scrolls right. Called once per frame. */
void ObjPosLoad(void);

/* Level-specific init per zone (water for LZ, etc.) */
void LZWaterFeatures(void);

/* Oscillatory values used by swings, platforms, etc. */
void OscillateNumInit(void);
void OscillateNumDo(void);

/* Zone-specific palette cycling */
void PaletteCycle(void);

/* Sign post art loading at act end */
void SignpostArtLoad(void);

/* Demo playback control simulation (no-op outside demos) */
void MoveSonicInDemo(void);

/* Animate level-specific animated tiles */
void AnimateLevelAct(void);

/* Pause handling */
void PauseGame(void);

#endif /* SONIC1_LEVEL_H */
