#ifndef SONIC1_SPECIAL_H
#define SONIC1_SPECIAL_H

#include "types.h"

/* GM_Special — punto de entrada bloqueante (portado de GM_Special).
   Llamado desde MainGameLoop cuando v_gamemode == GM_Special. */
void GM_Special_Stage_Main(void);

/* Inicializa toda la RAM del SS (layout, matriz, cola de animaciones). */
void SS_InitVars(void);

/* Carga el layout del SS indicado por v_lastspecial a v_sslayout_actual
   y los SS_MapIndex a v_ss_spritesettings. */
void SS_Load(void);

/* Carga los tilemaps del fondo (birds/fish, clouds/bubbles) a VRAM. */
void SS_BGLoad(void);

/* Ciclea la paleta del SS y cambia el canvas del fondo cada N frames. */
void PalCycle_SS(void);

/* Anima el fondo (bubbles/clouds) y actualiza v_hscrolltablebuffer. */
void SS_BGAnimate(void);

/* Genera la matriz de rotación y emite todos los bloques visibles. */
void SS_ShowLayout(void);

/* Anima bloques (walls, rings, glass) modificando v_ss_spritesettings. */
void SS_AnimateBlocks(void);

/* Procesa la cola de animaciones de bloques tocados por Sonic. */
void SS_ExecuteAnimationQueue(void);

uint8_t *SS_FindFreeAnimationSlot(void);

/* IDs de animación (ss_ani_id) de la cola de bloques */
#define SS_ANI_ID_RINGSPARKS    1
#define SS_ANI_ID_BUMPER        2
#define SS_ANI_ID_1UP           3
#define SS_ANI_ID_REVERSE       4
#define SS_ANI_ID_EMERALDSPARKS 5
#define SS_ANI_ID_GLASSBLOCK    6

/* Offsets dentro de un slot de animación (8 bytes) */
#define ss_ani_id(p)    ((p)[0])
#define ss_ani_delay(p) ((p)[2])
#define ss_ani_frame(p) ((p)[3])
#define ss_ani_block(p) (*(uint32_t *)((p) + 4))

#endif /* SONIC1_SPECIAL_H */
