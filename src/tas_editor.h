#ifndef SONIC1_TAS_EDITOR_H
#define SONIC1_TAS_EDITOR_H

#include <SDL2/SDL.h>

void TASEditor_Toggle(void);
int  TASEditor_WindowID(void);
int  TASEditor_IsOpen(void);
int  TASEditor_HasFocus(void);
void TASEditor_HandleEvent(SDL_Event *e);
void TASEditor_PollKeys(void);
void TASEditor_Render(void);

#endif