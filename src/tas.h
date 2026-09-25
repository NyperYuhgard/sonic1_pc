#ifndef SONIC1_TAS_H
#define SONIC1_TAS_H

#include "types.h"

typedef enum {
    TAS_MODE_LIVE     = 0,   /* keyboard normal */
    TAS_MODE_RECORD   = 1,   /* keyboard + grabar */
    TAS_MODE_PLAYBACK = 2,   /* solo leer del buffer */
} TasMode;

void     TAS_Init(void);

TasMode  TAS_GetMode(void);
uint32_t TAS_GetFrame(void);
uint32_t TAS_GetLength(void);

/* Sustituye el input físico por el grabado en modo PLAYBACK.
   Llamar desde Input_Read antes de calcular press/hold. */
uint8_t  TAS_OverridePlayer1(uint8_t physical);
uint8_t  TAS_OverridePlayer2(uint8_t physical);

/* Llamar UNA VEZ por frame de juego (desde WaitForVBlank). */
void     TAS_EndFrame(void);

/* Pausa / frame-advance (main loop). */
int      TAS_IsPaused(void);
void     TAS_SetPaused(int paused);
int      TAS_TryConsumeStep(void);   /* 1 si el usuario pidió avanzar */

/* Acciones de alto nivel, invocadas desde Input_PollTasKeys(). */
void     TAS_ToggleRecord(void);
void     TAS_ReplayFromStart(void);

int      TAS_SaveFile(const char *path);
int      TAS_LoadFile(const char *path);
void     TAS_SaveFile_Action(void);
void     TAS_LoadFile_Action(void);

void     TAS_SaveNextState(void);
void     TAS_LoadPrevState(void);
void     TAS_RequestStep(void);

/* Acceso directo al buffer para el editor */
uint8_t TAS_GetInputP1(uint32_t frame);
uint8_t TAS_GetInputP2(uint32_t frame);
void    TAS_SetInputP1(uint32_t frame, uint8_t value);
void    TAS_SetInputP2(uint32_t frame, uint8_t value);
void    TAS_SetLength(uint32_t length);
void    TAS_DeleteFrame(uint32_t at);
void    TAS_InsertFrame(uint32_t at);
void    TAS_DeleteRange(uint32_t start, uint32_t end);
#endif
