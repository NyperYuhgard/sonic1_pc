#ifndef SONIC1_POSTPROCESS_H
#define SONIC1_POSTPROCESS_H

#include "types.h"
#include "config.h"

#define PP_MAX_W 640
#define PP_MAX_H SCREEN_HEIGHT

/* Aplica el pipeline de post-proceso de `settings` sobre `src` y devuelve
   un puntero al buffer que contiene el resultado final. Puede ser `src`
   (si no hay filtros activos) o uno de los dos buffers internos de
   trabajo. NO liberes el resultado. */
const uint32_t *PP_Apply(const uint32_t *src, int w, int h,
                        const Settings *s);

/* Invalida el estado interno (llamar si cambia el ancho). */
void PP_Reset(void);

#endif