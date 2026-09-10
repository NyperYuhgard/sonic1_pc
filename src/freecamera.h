#ifndef SONIC1_FREECAMERA_H
#define SONIC1_FREECAMERA_H

#include <stdint.h>

/* Debug free camera mode. When a key is held, instead of following Sonic,
   the camera pans freely around the level (navigating since SonicPlayer is
   not yet fully playable).
 *
 * Keys (see input.c):
 *   F            toggle free camera mode
 *   Home  (Inicio)  move up
 *   End   (Fin)     move down
 *   Delete(Supr)    move left
 *   PgUp  (Av Pag)  move right
 */

void FreeCamera_Toggle(void);
int  FreeCamera_IsActive(void);
void FreeCamera_Update(const uint8_t *keys);

#endif /* SONIC1_FREECAMERA_H */