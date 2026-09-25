#ifndef SONIC1_INPUT_H
#define SONIC1_INPUT_H

#include "types.h"

void Input_Init(void);
void Input_Read(void);

/* Current joypad states (matching ASM v_jpadhold1/v_jpadpress1) */
extern uint8_t joypad_hold[2];    /* held buttons, per joypad */
extern uint8_t joypad_press[2];   /* newly pressed buttons, per joypad */

void Input_PollTasKeys(void);

/* Prev-state getter/setter (para savestates). */
void Input_GetPrevState(uint8_t out[2]);
void Input_SetPrevState(const uint8_t in[2]);
void Input_PollTasEditorKeys(void);

#endif /* SONIC1_INPUT_H */
