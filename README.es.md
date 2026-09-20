# sonic1_pc

**Idioma / Language:** [English](README.md) | [Español](README.es.md)

Un **port 1:1 a PC** del desensamblado 68000 de *Sonic the Hedgehog* (Sega Mega Drive),
escrito en **C11** y renderizado/reproducido a través de **SDL2** y **SDL2_mixer**.
No es un motor retro — la lógica del juego es una traducción fiel del ASM original
`disasm/sonic.asm` + `disasm/_inc/*`.

## Filosofía de porting

- **Paridad 1:1 estricta.** El código C refleja la lógica, estructura, anchos,
  orden y búsquedas del desensamblado. Si el ASM deriva un valor mediante una
  tabla o cabecera (p. ej. el PLC ID desde `LevelHeaders`), el port realiza la
  misma búsqueda — **nunca** hardcodea el resultado.
- **Atajos ni reinterpretaciones.** Ramas, máquinas de estado y rarezas
  (disasm FixBugs=0) se conservan tal cual.
- **Estructuras de datos primero.** Tablas y mecanismos se portan completos aunque
  solo Green Hill Zone sea jugable; los assets de otras zonas pueden ser stubs
  o `NULL`.
- **Capa de sistema solo PC.** Solo el VDP (video), el sistema de sonido y la
  entrada pasan por SDL/SDL2_mixer (`src/vdp.c`, `src/sound.c`, `src/input.c`).
  Todo lo demás es territorio 1:1.

Ver `CONTRIBUTING.md` para las guías completas de contribución.

## Dependencias

- Compilador C11 y CMake ≥ 3.10
- SDL2
- SDL2_mixer (con soporte OGG)

En Debian/Ubuntu:

```
sudo apt install build-essential cmake libsdl2-dev libsdl2-mixer-dev
```

## Compilar y ejecutar

```
cmake -S . -B build
cmake --build build -j$(nproc)
./build/sonic1
```

### Variantes de build

| Variante | Flag CMake | Efecto |
|---|---|---|
| Default | — | `-O2`, sin `-g` (símbolos via `nm`, gdb necesita casts raw) |
| Debug | `-DCMAKE_BUILD_TYPE=Debug` | `-O0 -g -DDEBUG` |
| Sanitizers | `-DSONIC_SANITIZE=ON` | ASan + UBSan (detector de accesos a memoria salvaje) |

### Assets

Los assets **no se comiten**. El motor carga primero desde `build/assets/`,
recayendo en `disasm/`, así que puedes:

- Ejecutar el juego directamente desde `disasm/` (sin copias extra), o
- Copiar las subcarpetas necesarias de `disasm/` a `build/assets/` si quieres
  overrides/custom assets.

`src/data.c` usa los nombres de archivo originales de `disasm/` (con espacios).

## Controles

### Jugador 1

| Acción | Tecla |
|---|---|
| Mover | Flechas |
| Saltar | `Z` (botón A) |
| Saltar (alt) | `X` (botón B) |
| Rodar / spin | `C` (botón C) |
| Start / confirmar | `Enter` |

### Jugador 2 (parcial)

`I / J / K / L` mueven, `U / Y / O` son los botones ABC, `P` es Start.

### Teclas de debug PC

| Tecla | Efecto |
|---|---|
| `P` | Alterna el visor VRAM (hoja de tiles con etiquetas de direcciones VRAM, OSD de bases de plano/scrolls, tiras de palette_main + CRAM) |
| `O` | Alterna el visor Object RAM (slots de objetos en vivo) |
| `G` | Alterna el visor Plano A/B (nametables completas, se adapta al tamaño de ventana sin estirar: lado a lado o apilado, scroll con rueda/arrastre) |
| `ESC` (barra de título) | Salir |

## Códigos de trucos

Todos los trucos están habilitados por defecto (`f_*cheat = 1` en `main.c`).

- **Level select**: en la pantalla de título, introduce `Arriba, Abajo, Izquierda, Derecha`
  en el D-Pad, luego mantén **A** (`Z`) y pulsa **Start** (`Enter`). Navega con
  `Arriba/Abajo`, confirma con cualquier botón de acción.
  - La última fila del level select es **Sound Select** (`Izquierda/Derecha` para navegar).
  - Al llegar al sonido `$9E` (con cheat de créditos) se abren los Créditos; `$9F` abre el
    Ending.
  - La fila Special Stage salta al estado de Special Stage (actualmente stub).
- **Debug mode**: `Start` durante el juego con debug activado invoca la paleta
  de objetos debug (portada desde `_incObj/DebugMode.asm`).

## Flujo de juego (game modes portados)

La `game_mode_table` en `src/main.c` refleja `GameModeArray` de `sonic.asm`:

| Modo | ID | Estado |
|---|---|---|
| Sega screen | `$00` | ✅ 100% (ciclo de paleta + timing del canto "SEGA") |
| Title screen | `$04` | ✅ 98% (STP, arte del título, title Sonic, PSBTM, level select, cheats) |
| Demo | `$08` | ⛔ Stub (vuelve a Sega) |
| Level | `$0C` | ✅ 40% (Solo Green Hill Zone completa) |
| Special Stage | `$10` | ✅ 99% (Flujo completo: fades, pantalla de resultados, esmeraldas) |
| Continue | `$14` | ⛔ Stub (`TODO`) |
| Ending | `$18` | ⛔ Stub (`TODO`) |
| Credits | `$1C` | ⛔ Stub (`TODO`) |

## Gameplay actualmente portado (Green Hill Zone)

- Entrada de nivel (`Level_Enter`): fade, bucle Phase G de title-card, drenaje
  PLC, limpiezas de RAM, setup VDP, cargas de layout/chunk/mappings.
- `Level_Process` por frame: pause, VBlank, `ExecuteObjects`, deform,
  `BuildSprites`, `ObjPosLoad`, ciclo de paleta, PLC, osciladores, anim sincronizada,
  arte del signpost.
- Objeto jugador Sonic (`Sonic_Control`, `Sonic_Animate`, `Sonic_LoadGfx`,
  velocidad de entidad/caída de objeto, distancia al suelo).
- Colisión `ReactToItem` (anillos, monitores, badniks, spikes, lógica de boss
  portada).
- **Anillos**: spawn vía `ObjPosLoad` → expandir/animar → `DisplaySprite` →
  bit-7 rendered → `ReactToItem` recoge → sparkle → delete; `CollectRing`
  actualiza contador/HUD y da vidas extra en 100/200 anillos.
- HUD (puntuación/tiempo/anillos/vidas) vía `HUD_Update`, `Hud_Base`.
- Title cards, game-over card, "Got Through" card, signpost.
- Índice de colisión para el charset (`ColIndexLoad`, `ConvertCollisionArray`).
- Gráficos animados por zona (GHZ) y ciclo de paletas de zona.
- **Special Stage**: flujo 1:1 completo (fades blancos, física del laberinto,
  pantalla de resultados con elementos de card, conteo de bonus de anillos,
  Chaos Emeralds, salida al siguiente nivel).

## Estructura del proyecto

```
src/
├── main.c        — bucle principal, dispatch de game mode, title screen, level select
├── ram.h/.c      — grilla RAM global (ram[]) + accessors tipados + macros de objeto
├── constants.h   — IDs, tipos de colisión, direcciones bank/port, PLC ids
├── vdp.c/.h      — modelo VDP Mega Drive + renderizado SDL (capa de sistema PC)
├── input.c/.h    — teclado → joypad RAM (capa de sistema PC)
├── sound.c/.h    — SDL_mixer música/SFX por id bgm/sfx (capa de sistema PC)
├── level.c/.h    — Level_Enter/Process, ObjPosLoad, scrolling, helpers de dibujo
├── objects.c/.h  — dispatch de objetos, Sonic, anillos, ReactToItem, title cards
├── sprites.c/.h  — BuildSprites / renderizado de sprites desde sprite_queue
├── data.c/.h     — todas las tablas/punteros/carga de assets desde build/assets/
├── assets.c/.h   — cargador de archivos binarios (con padding slack de descompresores)
├── decomp.c/.h   — descompresores Nemesis / Enigma / Kosinski
├── plc.c/.h      — cola Pattern Load Cue (NewPLC / RunPLC)
├── hud.c/.h      — patrones de dígitos HUD + refresco por frame
├── palette.c/.h  — paletas, PalLoad, fade in/out, PaletteCycle
├── deform.c/.h   — deformación de fondo + scroll de cámara
├── collision.c/.h— índice de colisión 16x16 para el charset del nivel
├── objview.c/.h  — visor de Object RAM para debug
└── debugmode.c/.h— colocación de objeto debug (desde _incObj/DebugMode.asm)

disasm/           — desensamblado original Sega + assets sin comprimir (fuente de verdad)
```

## Notas de arquitectura

- **Grilla RAM.** `src/ram.h` modela el espacio de direcciones 68k como un array
  plano de bytes (`ram[]`); `RAM_BYTE/RAM_WORD/RAM_LONG(addr)` realizan accesos
  tipados en los offsets originales y `RAM_ADDR(addr)` produce un puntero en
  tiempo de ejecución (p. ej. `RAM_ADDR(v_lvlobjspace)`). Los campos de objeto
  usan macros (`obX`, `obY`, `obRoutine`, `obRender`, `obColType`, …).
- **Objetos.** 128 slots de 64 bytes; los objetos de nivel viven en
  `v_lvlobjspace..v_lvlobjend` (96 slots). Cada objeto despacha según su
  byte `obRoutine` mediante la tabla `obj_dispatch[]`.
- **Flag de render de sprites.** `DisplaySprite` encola un objeto;
  `BuildSprites` corre tras `ExecuteObjects` cada frame y pone
  `obRender |= sprite_rendered` (bit 7) — así la colisión (`ReactToItem`) solo
  ve objetos que fueron renderizados el frame anterior.
- **Tipos de colisión.** `col_none` 0x00, `col_item` 0x40 (anillos/monitores),
  `col_hurt` 0x80 (dañinos), `col_special` 0xC0 (propiedades especiales).
- **Cola PLC.** Los gráficos se cargan bajo demanda mediante una cola de 16
  slots pattern load cue (`NewPLC`/`RunPLC`, una entrada por frame) — igual que
  el original.

## Modding de RAM

`src/ram.h` modela el espacio de direcciones 68k como un array plano de bytes
(`ram[]`). Cada dirección RAM es un **offset** dentro de ese array, y
`RAM_BYTE/RAM_WORD/RAM_LONG(addr)` leen o escriben 1/2/4 bytes en ese offset —
exactamente como `move.b`/`move.w`/`move.l` en el 68000. `RAM_ADDR(addr)`
produce un puntero crudo (`&ram[addr]`) para llamadores que necesiten una
dirección en vez de un valor.

Como el ASM original mezcla libremente anchos de acceso (p. ej. escribe `v_zone`
como byte y `v_zone_act` como word — los mismos dos bytes, vistas distintas),
el port debe poder hacer lo mismo. Por eso existen las macros `RAM_*` y por eso
algunos identificadores se declaran en dos sabores.

### Dos estilos de declaración

Todo nombre `v_*` es uno de:

| Estilo | Ejemplo | Significado |
|---|---|---|
| **Offset** | `#define v_objspace 0xD000` | El nombre *es* una dirección. |
| **Lvalue** | `#define v_invinc (RAM_BYTE(0xFE2D))` | El nombre *es* una variable en esa dirección. |

Ambos son válidos, pero **debes hacer coincidir el estilo de acceso con la
declaración**:

| Estilo | Escritura correcta | Lectura correcta | Tomar dirección |
|---|---|---|---|
| Offset | `RAM_BYTE(v_x) = n;` | `RAM_BYTE(v_x)` | `RAM_ADDR(v_x)` |
| Offset (derivado) | `RAM_WORD(v_y) = n;` | `RAM_WORD(v_y)` | `RAM_ADDR(v_y)` |
| Lvalue | `v_z = n;` | `v_z` | `&(v_z)` (raro) |

Mezclarlos es la fuente del **bug de doble desreferencia** más abajo.

### El problema del doble desreferencia

Si `v_x` es un macro lvalue, envolverlo en `RAM_*` compila pero es erróneo:

```c
#define v_invinc (RAM_BYTE(0xFE2D))    /* lvalue: lee RAM al usarse */

v_invinc = 1;              /* ✅  ram[0xFE2D] = 1                          */
RAM_BYTE(v_invinc) = 0;    /* ❌  expande a ram[ ram[0xFE2D] ] = 0       */
```

La segunda forma lee el valor actual en 0xFE2D y lo usa como índice,
escribiendo silenciosamente en la dirección equivocada (a menudo ram[0] o
ram[1], por lo que puede parecer que no pasó nada). Sin warning del
compilador: el índice resultante es un uint16_t perfectamente válido.

Regla práctica: si `v_x = n;` compila, entonces `RAM_BYTE(v_x) = n;` es un bug.

### Acceso cross-size

A veces el ASM original escribe un valor más ancho o estrecho que el ancho
natural de una variable. Ejemplo del desensamblado:

```asm
move.w  #id_GHZ_act1,(v_zone_act).w   ; escribe v_zone (byte) + v_act (byte)
move.b  #$04,(v_zone).w                ; escribe solo v_zone
```

Cuando el port necesita la misma libertad, usa la dirección cruda, no el
macro lvalue. Dos enfoques comunes:

1) Tomar la dirección directamente con un literal (más simple):

```c
v_zone = 0x04;                          /* escritura ancho natural */
RAM_WORD(0xFE10) = id_GHZ_act1;         /* escritura cross-size    */
```

2) Declarar ambas vistas del mismo par de bytes:

```c
#define v_zone_act_a   0xFE10
#define v_zone_act     (*(uint16_t *)&ram[v_zone_act_a])
#define v_zone         (*(uint8_t  *)&ram[v_zone_act_a + 0])   /* verificar endianness */
#define v_act          (*(uint8_t  *)&ram[v_zone_act_a + 1])

v_zone = 0x04;                          /* ✅  byte individual        */
RAM_WORD(v_zone_act_a) = id_GHZ_act1;   /* ✅  ambos bytes, swapped  */
```

El sufijo `_a` (o `_addr`) marca la dirección cruda; el nombre sin él es el
acceso tipado. Esto hace la intención obvia en cada punto de llamada y permite
que el visor RAM (que lee `ram[]` directamente) siga funcionando sin cambios.

### Endianness

En el 68k, words y longs se almacenan big-endian. En este port `ram[]`
contiene la misma secuencia de bytes que el 68k produciría, pero se indexa
como array plano, así que leer un `uint16_t` desde `ram[]` da little-endian
en x86-64. Por eso las escrituras que cruzan límites de byte pasan por
`RAM_SET_U16` / `RAM_SET_U32` (o `RAM_WORD` para la vista byte-swapped):
hacen swap para coincidir con el layout 68k.

Cuando haya duda, sigue el patrón que usa el desensamblado:

```c
    move.b → RAM_BYTE(addr)

    move.w → RAM_WORD(addr) o RAM_SET_U16(addr, v)

    move.l → RAM_LONG(addr) o RAM_SET_U32(addr, v)
```

### Auditoría rápida de estilos mezclados

Desde la raíz del proyecto:

```bash
grep -nE 'RAM_(BYTE|WORD|LONG)\(v_[A-Za-z0-9_]+\)' src/*.c
```

Cada coincidencia es un bug **sí** el correspondiente `v_*` está declarado
como macro lvalue en `ram.h`. La corrección adecuada es quitar el wrapper
`RAM_*`:

```c
RAM_BYTE(v_invinc) = 0;   →   v_invinc  = 0;
RAM_BYTE(v_shield) = 0;   →   v_shield  = 0;
RAM_BYTE(f_bigring) = 1;  →   f_bigring = 1;
```

y mantener `RAM_*` solo para direcciones literales o aritmética sobre
direcciones:

```c
RAM_BYTE(0xFE2D) = 0;             /* ✅ literal              */
RAM_BYTE(v_sslayout_base + d4) = x; /* ✅ aritmética en address */
```

Si un `v_*` necesita acceso tanto natural como cross-size, aplica la
convención de sufijo `_a` arriba en vez de mezclar estilos ad hoc.

## Debugging

- **Sondas por variable de entorno** se usan para que la salida de debug nunca
  vaya en runs normales, p. ej. `SONIC_DEBUG_REACT` (diagnósticos de
  anillos/colisión en `ReactToItem`).
- **gdb sin `-g`** (build default): `ram` no tiene tipo — castear direcciones
  crudas (`p/x *(char*)ADDR`). Con ASLR desactivado la base de imagen es
  `0x555555554000`; `nm build/sonic1` da direcciones absolutas globales.
- **No fuerces `v_gamemode = GM_Level` para "testear gameplay"**: salta el
  bucle Phase G de title-card + drenaje PLC, por lo que no reproduce un boot
  completo de nivel (colocación de objetos, paletas, estado PLC).
- Build con sanitizer (`-DSONIC_SANITIZE=ON`) es la primera parada para
  accesos a memoria salvaje.

## Roadmap / stubs conocidos

- Demo mode.
- Pantallas Continue, Ending y Credits (solo máquinas de estado).
- Zonas distintas a Green Hill — estructuras de datos/tablas portadas donde el
  ASM las requiere; punteros de assets pueden ser `NULL`.
- Features VBlank estilo "Delay" por software y el driver de sonido Z80 (el
  sistema de sonido PC reemplaza al Z80).

## Licencia / descargo

Esto **no** es un producto de SEGA. *Sonic the Hedgehog* y assets relacionados
son propiedad de SEGA. El desensamblado (`disasm/`) está disponible para
fines educativos/archivísticos; este proyecto es un port de ejercicio de
programación del mismo.