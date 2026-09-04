#include "decomp.h"
#include "vdp.h"
#include <string.h>

/* ===========================================================================
   Nemesis Decompression (from _inc/Decompression/Nemesis Decompression.asm)
   Decompresses directly to VRAM (vdp.vram[])
   =========================================================================== */

void NemDecToVRAM(const uint8_t *source, uint32_t vram_addr) {
    const uint8_t *src = source;
    uint8_t *dst = &vdp.vram[vram_addr];

    /* Read header word (big-endian) */
    uint16_t header = ((uint16_t)src[0] << 8) | src[1];
    src += 2;

    int use_xor = (header & 0x8000) != 0;
    header &= 0x7FFF;

    /* Number of 8-pixel rows = patterns * 8
       (asm: lsl #1 (drop XOR bit) then lsl #2 => *8) */
    uint32_t total_rows = (uint32_t)header << 3;

    /* Build code table in local buffer (v_ngfx_buffer equivalent, 256 words) */
    uint8_t code_table[512];
    memset(code_table, 0, sizeof(code_table));

    /* Build code table */
    {
        uint8_t d0 = *src++;
    bct_chkend:
        if (d0 == 0xFF) goto done_table;

        {
            uint16_t d7 = d0;
        bct_loop:
            d0 = *src++;
            if (d0 & 0x80) {
                d7 = d0;
                goto bct_chkend;
            }

            {
                uint8_t d1 = d0;
                d7 = (d7 & 0x000F) | ((d1 & 0x70) << 0);
                uint8_t code_len = d0 & 0x0F;
                d7 = (d7 & 0xFF00) | ((uint16_t)code_len << 8) | (d7 & 0x00FF);

                uint8_t extra_code = *src++;
                int remaining = 8 - code_len;
                if (remaining == 0) {
                    int idx = extra_code * 2;
                    code_table[idx]     = (uint8_t)(d7 >> 8);
                    code_table[idx + 1] = (uint8_t)(d7 & 0xFF);
                    goto bct_loop;
                }

                /* Short code: fill multiple entries */
                int shifted_code = extra_code << remaining;
                int num_entries = (1 << remaining) - 1;
                int idx = shifted_code * 2;
                uint8_t hi = (uint8_t)(d7 >> 8);
                uint8_t lo = (uint8_t)(d7 & 0xFF);
                for (int i = 0; i <= num_entries; i++) {
                    code_table[idx + i * 2]     = hi;
                    code_table[idx + i * 2 + 1] = lo;
                }
                goto bct_loop;
            }
        }
    }
done_table:

    /* Process compressed data */
    {
        uint32_t row_data = 0;    /* d4 - current 8-pixel row being built */
        int pixel_count = 8;      /* d3 - pixels remaining in current row */
        uint16_t bit_buf = ((uint16_t)*src << 8) | src[1];
        src += 2;
        int shift = 16;           /* d6 */
        uint32_t xor_prev = 0;    /* d2 - for XOR mode */
        uint32_t rows_left = total_rows; /* a5 */

    process:
        {
            int d7 = shift - 8;
            uint16_t d1 = (uint16_t)(bit_buf >> d7);

            /* Check for inline data marker (>= $FC) */
            if ((d1 & 0xFF) >= 0xFC) {
                /* Inline data */
                shift -= 6;
                if (shift < 9) {
                    shift += 8;
                    bit_buf = (uint16_t)(bit_buf << 8) | *src++;
                }

                shift -= 7;
                d1 = (uint16_t)(bit_buf >> shift);
                uint8_t pal = d1 & 0x0F;
                (void)pal;
                uint8_t repeat = (d1 >> 4) & 0x07;
                (void)repeat;

                if (shift < 9) {
                    shift += 8;
                    bit_buf = (uint16_t)(bit_buf << 8) | *src++;
                }

                /* Write pixels */
                goto write_pixels_inline;
            }

            /* Normal code table lookup */
            d1 &= 0xFF;
            int idx = d1 * 2;
            uint8_t code_len = code_table[idx];
            uint8_t pal_repeat = code_table[idx + 1];

            shift -= code_len;
            if (shift < 9) {
                shift += 8;
                bit_buf = (uint16_t)(bit_buf << 8) | *src++;
            }

            {
                uint8_t pal = pal_repeat & 0x0F;
                uint8_t repeat_count = (pal_repeat >> 4) & 0x0F;

                /* Write pixels (repeat_count + 1 times) */
                for (int i = 0; i <= repeat_count; i++) {
                    row_data = (row_data << 4) | pal;
                    pixel_count--;
                    if (pixel_count == 0) {
                        /* Row complete - write it.
                           row_data packs 8 pixels of 4 bits: p0 in bits 31-28,
                           p7 in bits 3-0. The 68k (big-endian) move.l writes
                           [p0p1][p2p3][p4p5][p6p7], i.e. p0 first in memory. */
                        uint32_t w = row_data;
                        if (use_xor) {
                            w = xor_prev ^ row_data;
                            xor_prev = w;
                        }
                        dst[0] = (uint8_t)(w >> 24);
                        dst[1] = (uint8_t)(w >> 16);
                        dst[2] = (uint8_t)(w >> 8);
                        dst[3] = (uint8_t)(w);
                        dst += 4;
                        rows_left--;
                        if (rows_left == 0) return;
                        row_data = 0;
                        pixel_count = 8;
                    }
                }
                goto process;
            }

        write_pixels_inline:
            {
                uint8_t pal = d1 & 0x0F;
                uint8_t repeat_count = (d1 >> 4) & 0x07;

                for (int i = 0; i <= repeat_count; i++) {
                    row_data = (row_data << 4) | pal;
                    pixel_count--;
                    if (pixel_count == 0) {
                        uint32_t w = row_data;
                        if (use_xor) {
                            w = xor_prev ^ row_data;
                            xor_prev = w;
                        }
                        dst[0] = (uint8_t)(w >> 24);
                        dst[1] = (uint8_t)(w >> 16);
                        dst[2] = (uint8_t)(w >> 8);
                        dst[3] = (uint8_t)(w);
                        dst += 4;
                        rows_left--;
                        if (rows_left == 0) return;
                        row_data = 0;
                        pixel_count = 8;
                    }
                }
                goto process;
            }
        }
    }
}


/* ===========================================================================
   Enigma Decompression (from _inc/Decompression/Enigma Decompression.asm)
   Decompresses to a RAM word buffer (uint16_t array)
   =========================================================================== */

/* Bit reader helpers following the 68k Enigma decoder's rol-based model. */
static inline uint16_t eni_rol16(uint16_t x, int n) {
    n &= 15;
    return (uint16_t)((x << n) | (x >> (16 - n)));
}
static inline uint16_t eni_ror16(uint16_t x, int n) {
    n &= 15;
    return (uint16_t)((x >> n) | (x << (16 - n)));
}

void EniDec(const uint8_t *source, uint16_t *dest, uint16_t starting_art_tile) {
    const uint8_t *src = source;
    uint16_t *dst = dest;
    uint16_t a2 = starting_art_tile; /* base tile properties */

    /* Get number of tile bits (unsigned header byte) */
    int d4 = *src++;

    /* Get tile flags (PCCVH << 3) */
    int a3 = (*src++) << 3;

    /* Get incrementing tile and static tile, offset by base properties */
    uint16_t a4 = (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
    src += 2;
    a4 = (uint16_t)(a4 + a2);
    uint16_t a5 = (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
    src += 2;
    a5 = (uint16_t)(a5 + a2);

    /* Get first word */
    uint16_t d5 = (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
    src += 2;
    int d6 = 16; /* bit count remaining in d5 */

    /* Fetch another byte when 8 or fewer bits remain (asm: cmpi #8,bhi) */
    #define ENI_REFILL()                                                  \
        do {                                                             \
            if (d6 <= 8) {                                               \
                int dd = 8 - d6;                                         \
                d5 = eni_ror16(d5, dd);                                  \
                d5 = (uint16_t)((d5 & 0xFF00) | (*src++));               \
                d5 = eni_rol16(d5, dd);                                  \
                d6 += 8;                                                 \
            }                                                            \
        } while (0)

    /* Read one inline tile (GetEnigmaInline). Returns the tile value. */
    #define ENI_INLINE()                                                 \
        ({                                                               \
            uint16_t d3 = a2;                                            \
            int d7 = a3;                                                 \
            if (d7 & 0x80) { d6--; d5 = eni_rol16(d5, 1); if (d5 & 1) d3 |= 0x8000; } d7 = (d7 << 1) & 0xFF; \
            if (d7 & 0x80) { d6--; d5 = eni_rol16(d5, 1); if (d5 & 1) d3 |= 0x4000; } d7 = (d7 << 1) & 0xFF; \
            if (d7 & 0x80) { d6--; d5 = eni_rol16(d5, 1); if (d5 & 1) d3 |= 0x2000; } d7 = (d7 << 1) & 0xFF; \
            if (d7 & 0x80) { d6--; d5 = eni_rol16(d5, 1); if (d5 & 1) d3 |= 0x1000; } d7 = (d7 << 1) & 0xFF; \
            if (d7 & 0x80) { d6--; d5 = eni_rol16(d5, 1); if (d5 & 1) d3 |= 0x800;  } d7 = (d7 << 1) & 0xFF; \
            ENI_REFILL();                                                \
            int d1 = d4;                                                 \
            int d2 = 0;                                                  \
            if (d1 > 8) {                                                \
                d2 = (int)((d5 >> 8) & 0xFF); d5 = eni_rol16(d5, 8);     \
                d1 -= 8; d2 <<= d1;                                      \
                { int x = 16 - d6; d5 = eni_ror16(d5, x);                \
                  d5 = (uint16_t)((d5 & 0xFF00) | (*src++));            \
                  d5 = eni_rol16(d5, x); }                               \
            }                                                            \
            d6 -= d1; d5 = eni_rol16(d5, d1);                            \
            uint16_t val = (uint16_t)((d5 & ((1 << d1) - 1)) | d2 | d3); \
            ENI_REFILL();                                                \
            val;                                                         \
        })

    for (;;) {
        /* GetEnigmaCode: good bit -> static/incrementing, else inline code */
        d6--;
        d5 = eni_rol16(d5, 1);
        if (d5 & 1) {
            /* InlineTileCode */
            d6 -= 2; int code = (int)((d5 >> 14) & 3); d5 = eni_rol16(d5, 2);
            d6 -= 4; int cnt = (int)((d5 >> 12) & 0xF); d5 = eni_rol16(d5, 4);
            ENI_REFILL();

            if (code == 0) {      /* Mode00: constant inline */
                uint16_t v = ENI_INLINE();
                for (int i = 0; i <= cnt; i++) *dst++ = v;
            } else if (code == 1) { /* Mode01: incrementing inline */
                uint16_t v = ENI_INLINE();
                for (int i = 0; i <= cnt; i++) *dst++ = v++;
            } else if (code == 2) { /* Mode10: decrementing inline */
                uint16_t v = ENI_INLINE();
                for (int i = 0; i <= cnt; i++) *dst++ = v--;
            } else {              /* Mode11: inline per entry */
                if (cnt == 0xF) {
                    /* EnigmaDone: end of data */
                    if (d6 >= 16) src--;  /* discard trailing byte(s) */
                    break;
                }
                for (int i = 0; i <= cnt; i++) {
                    *dst++ = ENI_INLINE();
                }
            }
        } else {
            /* static vs incrementing copy */
            d6--;
            d5 = eni_rol16(d5, 1);
            if (d5 & 1) { /* Mode01: static copy */
                d6 -= 4; int cnt = (int)((d5 >> 12) & 0xF); d5 = eni_rol16(d5, 4);
                ENI_REFILL();
                for (int i = 0; i <= cnt; i++) *dst++ = a5;
            } else {        /* Mode00: incrementing copy */
                d6 -= 4; int cnt = (int)((d5 >> 12) & 0xF); d5 = eni_rol16(d5, 4);
                ENI_REFILL();
                for (int i = 0; i <= cnt; i++) *dst++ = a4++;
            }
        }
    }

    #undef ENI_REFILL
    #undef ENI_INLINE
}
