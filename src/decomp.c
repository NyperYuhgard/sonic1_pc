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

void EniDec(const uint8_t *source, uint16_t *dest, uint16_t starting_art_tile) {
    const uint8_t *src = source;
    uint16_t *dst = dest;

    int inline_bits = *src++;
    uint8_t flags = *src++;
    uint16_t inc_tile = (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
    src += 2;
    uint16_t static_tile = (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
    src += 2;

    inc_tile = (uint16_t)(inc_tile + starting_art_tile);
    static_tile = (uint16_t)(static_tile + starting_art_tile);

    uint32_t bitbuf = ((uint32_t)src[0] << 8) | src[1];
    src += 2;
    int bits = 16;

#define ENI_NEED_BITS(n)                             \
    do {                                             \
        while (bits < (n)) {                         \
            bitbuf = (bitbuf << 8) | *src++;         \
            bits += 8;                               \
        }                                            \
    } while (0)

#define ENI_PEEK_BITS(n)                             \
    ({                                               \
        ENI_NEED_BITS(n);                            \
        (uint32_t)(bitbuf >> (bits - (n))) & ((1u << (n)) - 1u); \
    })
#define ENI_DROP_BITS(n) do { bits -= (n); } while (0)
#define ENI_READ_BITS(n)                       \
    ({                                         \
        uint32_t v = ENI_PEEK_BITS(n);         \
        ENI_DROP_BITS(n);                      \
        v;                                     \
    })

    for (;;) {
        uint32_t entry = ENI_PEEK_BITS(7);
        int count;
        int mode;

        if (entry < 0x40) {
            ENI_DROP_BITS(6);
            count = (int)((entry >> 1) & 0x0F);
            mode = (int)(entry >> 4);
        } else {
            ENI_DROP_BITS(7);
            count = (int)(entry & 0x0F);
            mode = (int)(entry >> 4);
        }

#define ENI_INLINE_VALUE()                                             \
        ({                                                             \
            uint16_t value = starting_art_tile;                        \
            if ((flags & 0x10) && ENI_READ_BITS(1)) value |= 0x8000;   \
            if ((flags & 0x08) && ENI_READ_BITS(1)) value |= 0x4000;   \
            if ((flags & 0x04) && ENI_READ_BITS(1)) value |= 0x2000;   \
            if ((flags & 0x02) && ENI_READ_BITS(1)) value |= 0x1000;   \
            if ((flags & 0x01) && ENI_READ_BITS(1)) value |= 0x0800;   \
            if (inline_bits > 0)                                       \
                value = (uint16_t)(value | ENI_READ_BITS(inline_bits));\
            value;                                                     \
        })

        if (mode < 2) {
            for (int i = 0; i <= count; i++) *dst++ = inc_tile++;
        } else if (mode < 4) {
            for (int i = 0; i <= count; i++) *dst++ = static_tile;
        } else if (mode == 4) {
            uint16_t v = ENI_INLINE_VALUE();
            for (int i = 0; i <= count; i++) *dst++ = v;
        } else if (mode == 5) {
            uint16_t v = ENI_INLINE_VALUE();
            for (int i = 0; i <= count; i++) *dst++ = v++;
        } else if (mode == 6) {
            uint16_t v = ENI_INLINE_VALUE();
            for (int i = 0; i <= count; i++) *dst++ = v--;
        } else {
            if (count == 0x0F) {
                break;
            }
            for (int i = 0; i <= count; i++) *dst++ = ENI_INLINE_VALUE();
        }

#undef ENI_INLINE_VALUE
    }

#undef ENI_READ_BITS
#undef ENI_DROP_BITS
#undef ENI_PEEK_BITS
#undef ENI_NEED_BITS
}


/* ===========================================================================
   Kosinski Decompression (from _inc/Decompression/Kosinski Decompression.asm)
   Decompresses to a RAM byte buffer. No length header.
   Description field: 16-bit little-endian (first byte = LSB), bits LSB->MSB,
   refilled every 16 bits. Bit 1 = literal copy; bit 0 = RLE.
   =========================================================================== */

void KosDec(const uint8_t *source, uint8_t *dest) {
    const uint8_t *src = source;
    uint8_t *dst = dest;

    /* First description field (little-endian: src[0] is the high byte,
       and since bits are consumed LSB first, src[1] feeds the first bit) */
    uint16_t d5 = (uint16_t)(src[0] | ((uint16_t)src[1] << 8));
    src += 2;
    int d4 = 15;            /* counts down the 16 bits of the description field */
    int bit;

#define KOS_READ_BIT()                                                    \
    ((bit = (d5 & 1)),                                                    \
     (d5 >>= 1),                                                          \
     ((--d4 < 0) ? (d5 = (uint16_t)(src[0] | ((uint16_t)src[1] << 8)),    \
                    src += 2, d4 = 15, 0)                                 \
                 : 0),                                                    \
     (bit))

    for (;;) {
        /* Kos_Loop: literal / RLE decision bit */
        if (KOS_READ_BIT() == 0) {
            /* Kos_RLE */
            int32_t d3;
            int32_t d2;

            if (KOS_READ_BIT() != 0) {
                /* Kos_SeparateRLE: offset from two bytes (+ optional count) */
                int d0 = *src++;
                int d1 = *src++;
                /* d2 = (int16)(0xE000 | (d1<<5) | d0) — negative back-reference */
                d2 = (int16_t)(0xE000 | ((d1 & 0xF8) << 5) | d0);

                if ((d1 & 7) != 0) {
                    d3 = (d1 & 7) + 1;
                } else {
                    /* Kos_SeparateRLE2: read the repeat count separately */
                    d1 = *src++;
                    if (d1 == 0) return;      /* 0 indicates end of data */
                    if (d1 == 1) continue;    /* 1 indicates new description field */
                    d3 = d1;
                }
            } else {
                /* Normal RLE: 2-bit repeat count + 1-byte offset */
                d3 = KOS_READ_BIT();          /* high repeat count bit */
                d3 = (d3 << 1) | KOS_READ_BIT(); /* low repeat count bit */
                d3 += 1;
                /* moveq #-1,d2; move.b (a0)+,d2 → 0xFFxx, always negative */
                d2 = (int16_t)(0xFF00 | *src++);
            }

            /* Kos_RLELoop: copy d3+1 times from (dst + d2) */
            do {
                *dst++ = dst[d2];
            } while (--d3 != -1);
        } else {
            /* Literal: copy byte as-is */
            *dst++ = *src++;
        }
    }

#undef KOS_READ_BIT
}
