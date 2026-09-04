#ifndef SONIC1_SPRITES_H
#define SONIC1_SPRITES_H

#include "types.h"

/* VDP sprite format (8 bytes per entry in the sprite table) */
typedef struct {
    uint16_t y_pos;     /* Y position + size/link info */
    uint8_t  size;      /* width (high nibble) | height (low nibble) */
    uint8_t  link;      /* next sprite index */
    uint16_t tile;      /* art tile + attributes */
    uint16_t x_pos;     /* X position */
} VDP_Sprite;

/* Sprite piece from mapping data (Sonic 1 format: 5 bytes) */
typedef struct {
    int8_t   ypos;      /* Y offset */
    uint8_t  size;      /* width/height encoded */
    uint16_t tile;      /* tile + attributes */
    int8_t   xpos;      /* X offset */
} SpritePiece;

/* Build sprites from object list (translated from BuildSprites.asm) */
void BuildSprites(void);

/* Render the sprite table buffer to the framebuffer texture */
void Sprites_RenderToTexture(void *texture_pixels, int pitch);

#endif /* SONIC1_SPRITES_H */
