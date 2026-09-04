#ifndef SONIC1_DECOMP_H
#define SONIC1_DECOMP_H

#include "types.h"

/* Nemesis decompression - decompress to VRAM */
void NemDecToVRAM(const uint8_t *source, uint32_t vram_addr);

/* Enigma decompression - decompress to RAM word buffer */
void EniDec(const uint8_t *source, uint16_t *dest, uint16_t starting_art_tile);

#endif /* SONIC1_DECOMP_H */
