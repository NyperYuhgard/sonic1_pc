#include "postprocess.h"
#include "ram.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

/* Dos buffers de trabajo persistentes. Se reutilizan entre frames, así no
   hay malloc en el hot path. Tamaño máximo = widescreen más ancho × 224. */
static uint32_t g_buf_a[PP_MAX_W * PP_MAX_H];
static uint32_t g_buf_b[PP_MAX_W * PP_MAX_H];

/* --- Scanlines: oscurece filas impares al 75% --- */
/* --- Scanlines con respuesta a la luminancia --- */
static void filter_scanlines(const uint32_t *src, uint32_t *dst, int w, int h) {
    for (int y = 0; y < h; y++) {
        const uint32_t *s = src + (size_t)y * w;
        uint32_t *d = dst + (size_t)y * w;
        
        if (y & 1) {
            for (int x = 0; x < w; x++) {
                uint32_t c = s[x];
                uint32_t r = (c >> 16) & 0xFF;
                uint32_t g = (c >> 8)  & 0xFF;
                uint32_t b =  c        & 0xFF;

                // Aproximación rápida de luminancia percibida (Y = 0.25R + 0.65G + 0.1B)
                uint32_t lum = (r >> 2) + (g >> 1) + (g >> 3) + (b >> 3);

                // Factor base de oscuridad (ej. 85% de brillo mínimo en lugar de 75%)
                // Los píxeles más brillantes conservan más luz (simula la expansión del haz)
                uint32_t factor = 217 + (lum * 38 / 255); // Rango de ~85% a ~100%

                r = (r * factor) >> 8;
                g = (g * factor) >> 8;
                b = (b * factor) >> 8;

                d[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        } else {
            memcpy(d, s, (size_t)w * 4);
        }
    }
}
static inline uint8_t lerp8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)(a + t * (b - a));
}
/* --- CRT: curvatura barrel + viñeteado sutil, muestreo nearest --- */
static void filter_crt(const uint32_t *src, uint32_t *dst, int w, int h) {
    const float curve_x = 0.020f;
    const float curve_y = 0.040f;
    const float fx = 2.0f / (float)w;
    const float fy = 2.0f / (float)h;

    for (int y = 0; y < h; y++) {
        float yc = (float)y * fy - 1.0f;
        float yc2 = yc * yc;
        uint32_t *d = dst + (size_t)y * w;

        for (int x = 0; x < w; x++) {
            float xc = (float)x * fx - 1.0f;
            float r2 = xc * xc + yc2;

            float sx = xc * (1.0f + curve_x * r2);
            float sy = yc * (1.0f + curve_y * r2);

            float u = (sx + 1.0f) * 0.5f * (float)w;
            float v = (sy + 1.0f) * 0.5f * (float)h;

            int px = (int)u;
            int py = (int)v;

            float dist_x = fabsf(sx);
            float dist_y = fabsf(sy);

            if (px < 0 || px >= w || py < 0 || py >= h || dist_x >= 1.0f || dist_y >= 1.0f) {
                d[x] = 0xFF000000u; // Marco negro
            } else {
                // 1. Muestreo del píxel actual y vecino izquierdo (para sangrado y difuminado)
                size_t idx = (size_t)py * w + px;
                uint32_t col_curr = src[idx];
                
                // Vecino izquierdo (si existe)
                uint32_t col_left = (px > 0) ? src[idx - 1] : col_curr;
                // Vecino derecho (si existe)
                uint32_t col_right = (px < w - 1) ? src[idx + 1] : col_curr;

                // Descomponer canales del píxel central
                uint32_t cr = (col_curr >> 16) & 0xFF;
                uint32_t cg = (col_curr >> 8)  & 0xFF;
                uint32_t cb =  col_curr        & 0xFF;

                // Descomponer canales del vecino izquierdo
                uint32_t lr = (col_left >> 16) & 0xFF;
                uint32_t lg = (col_left >> 8)  & 0xFF;
                uint32_t lb =  col_left        & 0xFF;

                // Descomponer canales del vecino derecho
                uint32_t rr = (col_right >> 16) & 0xFF;
                uint32_t rg = (col_right >> 8)  & 0xFF;
                uint32_t rb =  col_right        & 0xFF;

                // 2. Sangrado de croma (Rojo y Azul "chorrean" a la derecha) + Leve difuminado de fósforo [1 2 1]/4
                // Mezclamos croma analógico + difuminado horizontal en un solo cálculo
                uint32_t r = ((cr * 2 + lr + rr) >> 2); 
                uint32_t g = ((cg * 2 + lg + rg) >> 2);
                uint32_t b = ((cb * 2 + lb + rb) >> 2);

                // Aplicamos un sangrado extra en los rojos/azules hacia la derecha
                r = (r * 3 + lr) >> 2; // ~25% arrastre
                b = (b * 4 + lb) / 5;  // ~20% arrastre

                // Clampeo rápido por si las dudas
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;

                // 3. Anti-aliasing del borde del marco
                float edge_x = (1.0f - dist_x) * (float)w * 0.5f;
                float edge_y = (1.0f - dist_y) * (float)h * 0.5f;
                float edge = edge_x < edge_y ? edge_x : edge_y;

                if (edge < 1.5f) {
                    float alpha = edge / 1.5f;
                    if (alpha < 0.0f) alpha = 0.0f;

                    r = (uint32_t)(r * alpha);
                    g = (uint32_t)(g * alpha);
                    b = (uint32_t)(b * alpha);
                }

                d[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/* --- Blur separable 3-tap [1 2 1]/4, horizontal --- */
static void filter_blur_h(const uint32_t *src, uint32_t *dst, int w, int h) {
    for (int y = 0; y < h; y++) {
        const uint32_t *s = src + (size_t)y * w;
        uint32_t *d = dst + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            int x0 = x > 0     ? x - 1 : 0;
            int x2 = x < w - 1 ? x + 1 : w - 1;
            uint32_t c0 = s[x0], c1 = s[x], c2 = s[x2];
            uint32_t r = ((c0 >> 16) & 0xFF) + 2 * ((c1 >> 16) & 0xFF)
                       + ((c2 >> 16) & 0xFF);
            uint32_t g = ((c0 >> 8)  & 0xFF) + 2 * ((c1 >> 8)  & 0xFF)
                       + ((c2 >> 8)  & 0xFF);
            uint32_t b = ( c0        & 0xFF) + 2 * ( c1        & 0xFF)
                       + ( c2        & 0xFF);
            d[x] = 0xFF000000u | ((r >> 2) << 16)
                                | ((g >> 2) << 8)
                                |  (b >> 2);
        }
    }
}

/* --- Blur vertical (misma operación sobre columnas) --- */
static void filter_blur_v(const uint32_t *src, uint32_t *dst, int w, int h) {
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            int y0 = y > 0     ? y - 1 : 0;
            int y2 = y < h - 1 ? y + 1 : h - 1;
            uint32_t c0 = src[(size_t)y0 * w + x];
            uint32_t c1 = src[(size_t)y  * w + x];
            uint32_t c2 = src[(size_t)y2 * w + x];
            uint32_t r = ((c0 >> 16) & 0xFF) + 2 * ((c1 >> 16) & 0xFF)
                       + ((c2 >> 16) & 0xFF);
            uint32_t g = ((c0 >> 8)  & 0xFF) + 2 * ((c1 >> 8)  & 0xFF)
                       + ((c2 >> 8)  & 0xFF);
            uint32_t b = ( c0        & 0xFF) + 2 * ( c1        & 0xFF)
                       + ( c2        & 0xFF);
            dst[(size_t)y * w + x] =
                0xFF000000u | ((r >> 2) << 16) | ((g >> 2) << 8) | (b >> 2);
        }
    }
}

void PP_Reset(void) {
    /* Nada que resetear por ahora; los buffers son estáticos. Este hook
       existe por si en el futuro añades estado (p. ej. history para
       temporal blur). */
}

const uint32_t *PP_Apply(const uint32_t *src, int w, int h,
                        const Settings *s) {
    if (w > PP_MAX_W || h > PP_MAX_H) return src;   /* fuera de rango, no-op */

    /* Sin filtros activos: devuelve el src tal cual (cero coste). */
    if (!s->scanlines && !s->crt && !s->blur) return src;

    const uint32_t *cur = src;
    uint32_t *a = g_buf_a;
    uint32_t *b = g_buf_b;

    /* Para no escribir en el buffer original cuando aplicamos el primer
       filtro (que podría querer leer del original en su segunda pasada),
       siempre escribimos al buffer A la primera vez. Luego ping-pong. */
    uint32_t *write_to = a;
    uint32_t *read_from = NULL;

    // 1. Scanlines primero sobre los píxeles planos
    if (s->scanlines) {
        filter_scanlines(cur, write_to, w, h);
        cur = write_to;
        write_to = (write_to == a) ? b : a;
    }

    // 2. Blur para suavizar el sangrado entre píxeles y scanlines
    if (s->blur) {
        read_from = (write_to == a) ? b : a;
        filter_blur_h(cur, write_to, w, h);
        filter_blur_v(write_to, read_from, w, h);
        cur = read_from;
        write_to = (read_from == a) ? b : a;
    }

    // 3. Deformación CRT al final
    if (s->crt) {
        filter_crt(cur, write_to, w, h);
        cur = write_to;
    }

    return cur;
}