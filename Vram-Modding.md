# VRAM Bank Switcher

> Extensión de VRAM por bancos independientes de 64 KB para el port de Sega Mega Drive / Genesis.

## Índice

- [Motivación](#motivación)
- [Concepto](#concepto)
- [Configuración](#configuración)
- [¿Por qué bancos de 64 KB y no VRAM extendida?](#por-qué-bancos-de-64-kb-y-no-vram-extendida)
- [Estado actual y compatibilidad](#estado-actual-y-compatibilidad)
- [Uso](#uso)
- [Nota importante: slots de objeto compartidos](#nota-importante-slots-de-objeto-compartidos)
- [Referencia de API](#referencia-de-api)

---

## Motivación

La VRAM de la Sega Mega Drive está limitada a 64 KB, y este port replica fielmente dicha restricción para no romper la semántica del hardware original (nametables, sprite table, layout de tiles, etc.).

Sin embargo, replicar la limitación tal cual introduciría una restricción artificial en el desarrollo moderno: no habría sitio para arte adicional, overlays HD, ni contenido que el hardware original nunca tuvo que soportar.

El **VRAM Bank Switcher** resuelve esto permitiendo disponer de `N` bancos independientes de 64 KB, cada uno con la misma estructura que la VRAM original. El código elige explícitamente qué banco usar al cargar y al dibujar, de modo que la limitación del hardware no se impone al desarrollo de nuevas características.

## Concepto

La VRAM se organiza en bancos de 64 KB, idénticos al bloque único del hardware original:

| Banco  | Uso                                                                 |
| ------ | ------------------------------------------------------------------- |
| `0`    | VRAM "clásica". Todo el código vanilla del port escribe y lee aquí. |
| `1..N-1` | Bancos extendidos. Solo se acceden a través del Bank Switcher.    |

El **Banco 0** es transparente: cualquier ruta de código existente sigue funcionando sin cambios. Los bancos `1..N-1` requieren intervención explícita para cargar arte y para indicar a los objetos de dónde leer.

La elección de banco se realiza en dos momentos bien diferenciados:

- **Al cargar arte** → se indica al escritor de VRAM en qué banco volcar los tiles.
- **Al renderizar** → se indica al sprite de qué banco leer sus tiles.

Si ninguno de los dos se especifica, ambos caen a banco `0` y el comportamiento es bit a bit idéntico al original.

## Configuración

El número total de bancos se define en un único punto:

```c
/* vdp.h */
#define VRAM_BANK_SIZE   0x10000   /* 64 KB por banco (idéntico al MD) */
#define VRAM_BANK_COUNT  4         /* <-- único punto de configuración  */
```

Cambiar `VRAM_BANK_COUNT` reconfigura automáticamente:

- El tamaño de `vdp.vram_ext[]` (reserva de los bancos `1..N-1`).
- El número de pestañas del VRAM Viewer.
- El layout del viewer (auto-shrink del ancho de pestaña).
- El manejo de teclado y click dentro del viewer.

No hay ningún otro sitio donde tocar el número de bancos.

## ¿Por qué bancos de 64 KB y no VRAM extendida?

Extender la VRAM linealmente (por ejemplo a 1 MB como un buffer único) hubiera sido tentador desde el punto de vista del programador, pero introduce varios problemas:

- **VRAM Viewer inmanejable.** Un grid de 32.768 tiles no es inspeccionable, y el scroll/zoom del viewer se vuelve un infierno de usabilidad.
- **Modding más difícil.** El modder tendría que recordar offsets globales; con bancos, cada "espacio de 64 KB" es un contexto autocontenido y direccionable igual que en hardware original.
- **Riesgo de desalinear la estructura de Genesis.** Los nametables, la sprite table y las rutinas que escriben al VDP asumen un espacio de direcciones de 17 bits (128 KB direccionables, 64 KB útiles). Extender linealmente rompería esa convención.
- **Administración de tiles caótica.** Sin un límite claro por contexto, la fragmentación de arte se vuelve un problema real (¿qué tile va dónde? ¿qué rango se puede liberar?).

Los bancos de 64 KB preservan compatibilidad estructural con el hardware original, mantienen la administración de tiles acotada por contexto y hacen trivial el trabajo del viewer.

## Estado actual y compatibilidad

Actualmente, solo el arte comprimido en Nemesis es compatible con el Bank Switcher:

```c
void NemDecToVRAM(const uint8_t *source, uint32_t vram_addr);
```

`NemDecToVRAM` respeta `g_vdp_write_bank`, por lo que basta con cambiarlo antes de la llamada para redirigir el destino.

Está planificado extender la compatibilidad a:

- Arte sin comprimir.
- Otros formatos de compresión.

La prioridad actual es Nemesis porque es el formato que el motor de objetos usa de forma abrumadora. El objetivo del Bank Switcher es precisamente ese: permitir que los objetos carguen su arte en espacios dedicados sin colisionar con el resto del nivel.

## Uso

### 1. Cargar arte en un banco concreto

```c
VDP_SetWriteBank(1);                  /* Redirige las escrituras al banco 1 */
if (Nem_TitleSonic) {
    NemDecToVRAM(Nem_TitleSonic, ArtTile_Title_Sonic * tile_size);
}
VDP_SetWriteBank(0);                  /* Vuelve al banco 0 (MD-visible) */
```

> **Restaurar siempre `VDP_SetWriteBank(0)` tras la carga.**
> Si se olvida, la siguiente llamada a `AddPLC` o cualquier otro writer acabará escribiendo en el banco equivocado.

### 2. Indicar al objeto de qué banco leer

En el `init` del objeto, junto a la asignación de `obGfx`:

```c
obGfx(o) = (uint16_t)(ArtTile_Title_Sonic | Tile_Pal2);
obSetGfxBank(o, 1);                   /* Lee los tiles desde el banco 1 */
```

A partir de ese momento, `BuildSprites` emitirá cada pieza del sprite con el banco `1` asociado, y `VDP_RenderFrame` leerá los tiles de ese banco en lugar del `0`.

## Nota importante: slots de objeto compartidos

Los bancos de VRAM se asignan **por slot de objeto**, no por identidad de objeto.

Sonic 1 reutiliza agresivamente los slots de `ObjRAM` entre game modes y entre objetos distintos. Ejemplos habituales:

| Slot | Objetos que lo comparten                  |
| ---- | ----------------------------------------- |
| `1`  | `TitleSonic` (title screen) ↔ `HUD` (in-game) |
| `2`  | `TitleCard` ↔ `GameOverText`              |
| …    | …                                         |

**Consecuencia:** si un objeto con banco ≠ `0` deja su slot sin resetear, el siguiente objeto que herede ese slot hereda también el banco, y leerá sus tiles desde el banco equivocado. El síntoma típico es un objeto que "desaparece" (en realidad está leyendo arte que no le corresponde).

### Cómo evitarlo

**Opción A — reset automático (implementado, recomendado).**

`ExecuteObjects` detecta cuándo un slot cambia de `obID` respecto al frame anterior y resetea su banco a `0` antes de ejecutar la nueva rutina. Esto cubre todos los casos de reutilización de slots por spawn/delete normales.

**Opción B — reset explícito por objeto (defensivo).**

Si un objeto puede heredar un slot sin que cambie `obID` (raro, pero posible con `FindFreeObj` reutilizando un slot borrado a mano), conviene forzar el banco en su propio `init`:

```c
if (routine == 0) {
    obRoutine(o) = 2;
    obSetGfxBank(o, 0);            /* Fuerza banco 0 sin importar lo heredado */
    ...
}
```

> **Regla práctica:** especifica siempre el banco en el `init` del objeto. Si no lo especificas, el sistema cae a banco `0` — pero solo si nadie dejó un banco heredado en ese slot. **No confíes en el default.**

## Referencia de API

### Control del banco de escritura

```c
void VDP_SetWriteBank(int bank);   /* Cambia el banco destino de los writers públicos */
int  VDP_GetWriteBank(void);       /* Consulta el banco actual */
```

### Acceso directo por banco

```c
uint8_t       *VDP_VRAM_BankW(int bank);   /* Puntero escribible al banco */
const uint8_t *VDP_VRAM_Bank (int bank);   /* Puntero de solo lectura */
```

### Writers con banco explícito (ignoran `g_vdp_write_bank`)

```c
void VDP_WriteVRAM_Bank       (int bank, const uint8_t *src,
                               uint32_t addr, uint32_t len);
void VDP_FillVRAM_Bank        (int bank, uint8_t byte,
                               uint32_t addr, uint32_t len);
void VDP_CopyTilemapToVRAM_Bank(int bank, const uint16_t *src,
                               uint32_t dest, int width, int height);
```

### Asignación de banco a un objeto

```c
void    obSetGfxBank(void *obj, uint8_t bank);  /* 0..3 */
uint8_t obGetGfxBank(void *obj);                /* Consulta el banco actual */
```

Se llama una sola vez tras fijar `obGfx(o)` en el `init` del objeto. El banco persiste por slot hasta que `DeleteObject` o el reset automático de `ExecuteObjects` lo limpien.
