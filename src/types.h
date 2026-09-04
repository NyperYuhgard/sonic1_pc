#ifndef SONIC1_TYPES_H
#define SONIC1_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Tipo base - implicito en el 68000 */
typedef uint8_t  u8;     /* byte */
typedef int8_t   s8;     /* signed byte */
typedef uint16_t u16;    /* word */
typedef int16_t  s16;    /* signed word */
typedef uint32_t u32;    /* long */
typedef int32_t  s32;    /* signed long */

/* Helpers para leer datos big-endian del ROM (el Mega Drive es big-endian) */

static inline u16 BE16(const u8 *p) {
    return (u16)((p[0] << 8) | p[1]);
}

static inline u32 BE32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static inline s16 BE16S(const u8 *p) {
    u16 v = BE16(p);
    return (s16)v;
}

static inline s32 BE32S(const u8 *p) {
    u32 v = BE32(p);
    return (s32)v;
}

static inline void SET_BE16(u8 *p, u16 v) {
    p[0] = (u8)(v >> 8);
    p[1] = (u8)(v & 0xFF);
}

static inline void SET_BE32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)((v >> 16) & 0xFF);
    p[2] = (u8)((v >> 8) & 0xFF);
    p[3] = (u8)(v & 0xFF);
}

/* Tamaños fijos del juego */
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#endif /* SONIC1_TYPES_H */