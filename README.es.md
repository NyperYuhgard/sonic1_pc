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

- Compilador C11 y CMake ≥ 3.13
- SDL2
- SDL2_mixer (con soporte OGG)

En Debian/Ubuntu:

```
sudo apt install build-essential cmake libsdl2-dev libsdl2-mixer-dev
```

El cross-build de Windows necesita mingw-w64 y un prefijo de SDL2 para mingw en
vez de lo anterior; mira [Scripts de compilación](#scripts-de-compilación) más
abajo.

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
| RelWithDebInfo | `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | `-O2 -g -DNDEBUG` |
| Sanitizers | `-DSONIC_SANITIZE=ON` | ASan + UBSan (detector de accesos a memoria salvaje) |

### Scripts de compilación

`scripts/` envuelve los comandos manuales de arriba en variantes listas para
usar que además dejan los assets junto al ejecutable.

**Linux / WSL2 (build nativo):**

```
scripts/build-linux.sh                 # release  -> build/release/sonic1
scripts/build-linux.sh debug           # debug    -> build/debug/sonic1
scripts/build-linux.sh relwithdebinfo  # optimizado con símbolos
scripts/build-linux.sh asan            # ASan + UBSan
scripts/build-linux.sh all --clean     # las cuatro variantes, desde cero
scripts/build-linux.sh debug --run     # compila y arranca
```

Ejecuta el juego desde la raíz del repo (`./build/release/sonic1`):
`src/sound.c` carga el audio desde `./assets` (relativo al directorio de
trabajo) mientras que `src/data.c` resuelve el resto contra la ruta del
ejecutable.

**Windows (cross-compile con mingw-w64 desde Linux/WSL2):**

```
scripts/build-windows.sh                       # release -> dist/win/release/sonic1.exe
scripts/build-windows.sh debug --clean
scripts/build-windows.sh all
scripts/build-windows.bat                      # lo mismo, desde cmd.exe en Windows (necesita WSL2)
```

`dist/win/<variant>/` es autocontenido: `sonic1.exe` más `assets/`. SDL2,
SDL2_mixer y los códecs Ogg/Vorbis se enlazan **estáticos** por defecto, así que
no hay DLLs de runtime que repartir. Como mingw-w64 no incluye AddressSanitizer
la variante `asan` es solo para Linux/WSL2.

Hace falta un prefijo de SDL2 para mingw-w64. Va incluido en el repo en
`Lib-Windows/prefix` (7.7 MB: cabeceras, archivos estáticos y los `.pc`), así que
no hace falta cross-compilar nada:

```
sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64
scripts/build-windows.sh release --prefix Lib-Windows/prefix
```

Para compilar ese prefijo desde cero en vez de usarlo, hay que cross-compilar
libogg, libvorbis, SDL2 (`-DSDL_SHARED=OFF -DSDL_STATIC=ON`) y SDL2_mixer
(`-DSDL2MIXER_VORBIS=VORBISFILE -DSDL2MIXER_DEPS_SHARED=OFF`) al prefijo que
quieras; `scripts/build-windows.sh --help` tiene la receta completa, incluidas
las dos trampas: usa un toolchain mínimo en vez del de `scripts/` (aborta
mientras el prefijo aún no tenga `pkgconfig/`), y no desactives los códecs
Ogg/Vorbis, porque el juego necesita `Mix_SetMusicPosition` y
`Mix_GetMusicPosition`, que dependen de libvorbisfile.

`src/data.c` mantiene su ruta `mmap` de POSIX en Linux y usa `VirtualAlloc` bajo
`_WIN32`. Ojo: `VirtualAlloc(NULL, ...)` devuelve direcciones por encima de 4 GB
en Windows de 64 bits, lo que truncaría el campo `obMap` de 32 bits, así que la
ruta de Windows pide una dirección baja explícitamente.

Ejecuta `scripts/build-linux.sh --help` / `scripts/build-windows.sh --help` para
ver todas las opciones.

### Assets

Los assets **no se comiten**. El motor carga primero desde `build/assets/`,
recayendo en `disasm/`, así que puedes:

- Ejecutar el juego directamente desde `disasm/` (sin copias extra), o
- Copiar las subcarpetas necesarias de `disasm/` a `build/assets/` si quieres
  overrides/custom assets.

`src/data.c` usa los nombres de archivo originales de `disasm/` (con espacios).

## Controles

El juego **no acepta argumentos de línea de comandos** — `main()` descarta
`argc`/`argv` explícitamente (`src/main.c:1185-1187`) y no hay `getopt` en
ningún sitio de `src/`. Todo va por teclado, y todos los ajustes persistentes
viven en el menú de opciones del juego (ver abajo).

### Jugador 1

| Acción | Tecla |
|---|---|
| Mover | Flechas |
| Saltar | `Z` (botón A) |
| Saltar (alt) | `X` (botón B) |
| Rodar / spin | `C` (botón C) |
| Start / confirmar | `Enter` |

### Jugador 2 (parcial)

> **Mapping provisional.** Sonic 1 no tiene modo de 2 jugadores, así que estos
> bindings solo existen para alimentar `v_jpadhold2` / `v_jpadpress2`. Está
> previsto remapearlos: trata esta tabla como temporal.

| Acción | Tecla |
|---|---|
| Mover | `I` `J` `K` `L` |
| Saltar | `U` (botón A) |
| Saltar (alt) | `Y` (botón B) |
| Rodar / spin | `O` (botón C) |
| Start | `P` |

⚠️ `O` y `P` son también los toggles de los visores de Object RAM y VRAM, así
que con el jugador 2 activo cada pulsación de `O` o `P` abre o cierra además
una ventana de visor. Esa colisión es una de las razones por las que este
mapping tiene que cambiar.

El juego *lee* el jugador 2 en un solo sitio: los túneles de agua de LZ dejan
que el segundo pad mueva a Sonic arriba/abajo mientras el viento lo empuja
(`src/level.c:926-927`).

### Teclas de debug PC

Todas funcionan en cualquier momento, en cualquier game mode, desde la ventana
principal:

| Tecla | Efecto |
|---|---|
| `P` | Alterna el visor VRAM (hoja de tiles con etiquetas de direcciones VRAM, OSD de bases de plano/scrolls, tiras de palette_main + CRAM) |
| `O` | Alterna el visor Object RAM (slots de objetos en vivo) |
| `G` | Alterna el visor Plano A/B (nametables completas, se adapta al tamaño de ventana sin estirar: lado a lado o apilado, scroll con rueda/arrastre) |
| `V` | Visor de planos: alterna el modo de wrap de 128 filas (estilo BlastEm) |
| `R` | Alterna el visor RAM (hex + decimal en vivo, agrupado en secciones etiquetadas) |
| `F` | Alterna la free camera |
| `T` | Alterna la ventana del TAS editor |
| `F1`–`F8` | Acciones de TAS (ver abajo) |
| `ESC` | Salir (barra de título de la ventana) |

Cada visor es una ventana SDL aparte que se refresca a 60 Hz desde el final de
`VDP_RenderFrame`, y cerrarla con el gestor de ventanas la vuelve a alternar
(`src/input.c:42-53`).

**Navegación de los visores** — el visor de planos hace scroll con la rueda del
ratón y panea con arrastre del botón izquierdo, pero solo mientras el puntero
está dentro de esa ventana (`src/input.c:55-75`). El visor RAM es de solo
lectura; sus secciones son GAME STATE, BOUNDS & CAMERA, SONIC,
LOOP/ROLL/TRACK, FLAGS, SYNC/OSCILLATE y MISC (`src/ramview.c:219-227`).

### Free camera

`F` la alterna. La cámara entonces se mueve 8 px por frame, clampada a los
límites del nivel (`v_limitleft2` / `v_limitright2` / `v_limittop2` /
`v_limitbtm2`):

| Tecla | Dirección |
|---|---|
| `Home` | Arriba |
| `End` | Abajo |
| `PageUp` | Derecha |
| `Delete` | Izquierda |

Mueve la cámara real del mundo en 16.16 y fuerza un redraw completo del FG
para que el plano se refresque mientras haces panning
(`src/freecamera.c:31-51`).

### Teclas de TAS

| Tecla | Acción |
|---|---|
| `F1` | Alterna grabación (LIVE → RECORD → LIVE, o cancela reproducción) |
| `F2` | Reproduce desde el primer frame |
| `F3` | Guardar `tas.bin` |
| `F4` | Cargar `tas.bin` |
| `F5` | Guardar estado en el siguiente slot (8 slots, round-robin) |
| `F6` | Cargar el slot anterior |
| `F7` | Pausar / reanudar |
| `F8` | Avanzar un frame (solo mientras está pausado) |

La reproducción mete los valores de pad grabados directamente en
`v_jpadhold1` / `v_jpadpress1`, pisando al teclado (`src/input.c:147-148`).

### Teclas del TAS editor

| Tecla | Acción |
|---|---|
| `Esc` | Cerrar el editor |
| `Space` | Pausar / reanudar |
| `R` | Pausa y reproduce desde el frame 0 |
| `←` / `→` | Mover el cursor de frames |
| `Home` / `End` | Ir al primer / último frame |
| `PageUp` / `PageDown` | Scrollear una página de frames |
| `.` | Frame step |
| `Delete` / `Backspace` | Borrar el/los frame(s) seleccionados |
| `Insert` | Insertar un frame en el cursor |
| `A` | Seleccionar todo |
| `Ctrl+S` / `Ctrl+O` | Guardar / cargar `tas.bin` |
| `F5` / `F6` | Guardar estado / cargar estado |

Mientras la ventana del editor tiene el foco, el input del juego se pone a
cero para que el teclado no perturbe la run (`src/input.c:149-152`).

## Menú de opciones del juego

Se alcanza desde la pantalla de título: mantén **A** y pulsa **Start** (level
select), luego pulsa **B** (`src/main.c:635`). `↑`/`↓` eligen fila, `←`/`→`
cambian el valor, `A`/`C`/`Start` confirman.

| Fila | Valores | Efecto |
|---|---|---|
| WIDESCREEN | OFF / 398 / 424 / 480 | Ancho de render (`VDP_ApplyWidescreen`, `src/config.c:40-44`) |
| FPS INTERP. | ON / OFF | **No implementado** — el flag se guarda y se alterna, pero el renderer nunca lo lee. Está en la lista por paridad con la interpolación 60→120 Hz planeada |
| SCANLINES | ON / OFF | Overlay de scanlines (`src/postprocess.c:14`) |
| FULLSCREEN | ON / OFF | `SDL_WINDOW_FULLSCREEN_DESKTOP` |
| SS ALT ANIM | ON / OFF | Animación de salida alternativa del Special Stage (`SonicSS_ExitStage`) |
| SS SMOOTH | ON / OFF | Scroll suave en el laberinto del Special Stage |
| CRT | ON / OFF | Curvatura barrel + viñeteado sutil, muestreo nearest (`src/postprocess.c:48`) |
| BLUR | ON / OFF | Blur separable H+V (`src/postprocess.c:138`, `:160`) |
| APPLY & SAVE | — | Escribe la config a disco |
| BACK | — | Vuelve al level select |

El post-proceso se aplica en orden fijo — scanlines, blur H, blur V, CRT —
sobre dos buffers internos, con un ancho de render máximo de 640
(`src/postprocess.h:8-9`, `src/postprocess.c:205-221`).

Los ajustes se guardan en **`sonic1.cfg`**, un archivo **binario** en el
directorio de trabajo actual: el magic de 4 bytes `S1CF`, un `uint32_t` de
versión (actualmente `4`) y luego el `struct Settings` en crudo
(`src/config.c:17-38`). Se carga una vez al arrancar (`src/main.c:1196`) y se
escribe en APPLY & SAVE, en BACK y al salir. Borra el fichero para volver a los
valores por defecto.

## Códigos de trucos

Los cuatro trucos se habilitan al arrancar (`f_*cheat = 1`,
`src/main.c:1225-1229`, reflejando `CheatsEnabled=1` en `sonic.asm:420-425`):

| Flag | RAM | Efecto | Por defecto |
|---|---|---|---|
| `f_levselcheat` | `$FFE0` | Pantalla de level select | on |
| `f_slomocheat` | `$FFE1` | Slow motion desde la pausa | on |
| `f_debugcheat` | `$FFE2` | Modo debug en nivel | on |
| `f_creditscheat` | `$FFE3` | Créditos/ending japoneses ocultos | on |

- **Level select**: en la pantalla de título, mantén **A** (`Z`) y pulsa
  **Start** (`Enter`) — el ASM comprueba `f_levselcheat` y que A estuviera
  mantenida al pulsar Start (`disasm/sonic.asm:2166-2170`). Navega con
  `Arriba/Abajo`, confirma con cualquier botón de acción.
  - La última fila del level select es **Sound Select** (`Izquierda/Derecha`
    para navegar).
  - Al llegar al sonido `$9E` (con cheat de créditos) se abren los Créditos;
    `$9F` abre el Ending.
  - La fila **Special Stage** arranca un special stage de verdad (3 vidas,
    0 anillos, `v_emldlist` a cero) — no es un stub.
  - Pulsar **B** en cualquier fila abre el menú de opciones.
- **Slow motion**: en la pausa, mantén **B** (`X`) o pulsa **C** para entrar
  en slow motion; pulsa **A** (`Z`) para salir de vuelta a la pantalla de
  título (`src/level.c:1278-1281`).
- **Modo debug**: `Start` durante el juego con debug habilitado invoca la
  paleta de objetos debug (portada desde `_incObj/DebugMode.asm`).
- **Re-armar los trucos** (`Tit_ActivateCheat`, `disasm/sonic.asm:2113-2128`,
  portado en `src/main.c:702-723`): introducir `Arriba, Abajo, Izquierda,
  Derecha` en el D-Pad de la pantalla de título rehabilita un truco, elegido
  según cuántas veces se pulsó **C** (0-1 level select, 2-3 slow motion, 4-5
  debug, 6-7 créditos). En regiones no japonesas (`v_megadrive >= 0`) dos o más
  pulsaciones de C fuerzan siempre slow motion + debug, y el cheat de créditos
  queda inalcanzable — igual que en el original. Como el port arranca con los
  cuatro ya activados, este código solo se observa si algo los limpia.

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
| End Demo | `$20` | ✅ Pantalla SDL propia del port ("END DEMO" / "DEVELOPED BY" + logos), no la rutina del ASM — solo para builds de demo |

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
  Chaos Emeralds, salida al siguiente nivel), incluido `PalCycle_SS` /
  `PalCycle_SS_2` de `SpecCode.asm`.

## Sistema de agua de Labyrinth Zone

Portado 1:1 desde `disasm/_inc/LZWaterFeatures.asm` (los marcadores `NUEVO` en
`src/level.c` significan "recién portado", no "inventado"):

- **Altura de agua por acto** — `WaterHeight[4]` para LZ1/LZ2/LZ3/SBZ3
  (`level.c:597`) aplicada a `v_waterpos1..3` por `LZ_LevelWaterSetup`
  (`level.c:630`).
- **`LZWindTunnels`** (`level.c:878`) — túneles de viento de cascada,
  localizados desde `LZWind_Data` en el offset `8+(act<<3)` (dos entradas en
  el acto 1). Dentro del túnel suena `sfx_Waterfall` cada `$40` frames,
  Sonic es empujado con `obVelX = $400` en la animación `id_Float2`, una zona
  de succión cerca de la pared izquierda lo levanta o lo baja, y el `Arriba`/
  `Abajo` del **jugador 2** lo ajusta verticalmente. Pone `f_wtunnelmode` y
  respeta `f_wtunneldisallow`.
- **`LZWaterSlides`** (`level.c:939`) — slides de agua emparejados por chunk id
  contra `Slide_Chunks[7]`.
- **`LZDynamicWater`** (`level.c:1130`) — la máquina de estados que mueve
  `v_waterpos2` hacia `v_waterpos3`, incluidos los targets fijos del acto 3
  (`$0508`, `$0608`, `$07C0`, `$0128`).
- **Paleta por scanline** — el renderer elige `palette_water_main` para la
  región sumergida a partir de `f_wtr_state` (pantalla entera bajo el agua) y
  `v_hblank_line` (línea del agua), refrescados desde `v_palette_water` una
  vez por frame (`vdp.c:677`, `vdp.c:714-729`).
- **Spawn de la superficie del agua** en `Level_ChkWater` (`level.c:756`) y
  carga de la paleta submarina activa antes del fade-in (`level.c:745`).
- **Assets** — `palette/Labyrinth Zone Underwater.bin`,
  `palette/Sonic - LZ Underwater.bin`, `artnem/LZ Water Surface.nem`,
  `artnem/LZ Water & Splashes.nem`, `artunc/GHZ Waterfall.unc`
  (`data.c:62-63`, `data.c:673-674`, `data.c:764`).

## Adiciones solo del port

Nada de esto existe en el juego de Mega Drive — es la capa PC.

| Sistema | Fichero(s) | Qué hace |
|---|---|---|
| Menú de opciones + config | `config.c/.h`, `options.h`, `main.c` | Menú de 10 filas persistido en el `sonic1.cfg` binario |
| Post-proceso | `postprocess.c/.h` | Scanlines, blur separable, curvatura CRT + viñeteado; anchos widescreen 398/424/480 |
| Visor VRAM (`P`) | `vdp.c` | Hoja de 2048 tiles, etiquetas de dirección VRAM, OSD de bases de plano + hscroll, tiras de palette_main y CRAM |
| Visor Object RAM (`O`) | `objview.c` | Slots de objetos en vivo |
| Visor Plano A/B (`G`, `V`) | `planeview.c` | Nametables completas con un rectángulo sobre la región de 320x224 muestreada, toggle de wrap de 128 filas |
| Visor RAM (`R`) | `ramview.c` | Hex + decimal de `ram[]` en vivo, en 7 secciones etiquetadas |
| Free camera (`F`) | `freecamera.c` | Mueve la cámara real del mundo en 16.16, clampada a los límites del nivel |
| Overlay de colisión | `vdp.c` | `SONIC_DEBUG_COLLISION` pinta cada celda de colisión 16x16, coloreada por los flags de superficie que devuelve `FindNearestTile` |
| TAS | `tas.c`, `tas.h`, `tas_editor.c/.h` | Graba/reproduce hasta 1 h @ 60 Hz, `tas.bin` (`TASFILE` v1), 8 savestates de estado completo (64 KB RAM + VRAM + CRAM + VSRAM + registros VDP), más un editor frame a frame |
| Pantalla End Demo | `enddemo.c` | Pantalla SDL con una fuente de pixel-art vectorial hecha a mano en vez de la rutina del ASM |
| Fuente 8x8 compartida | `font8x8.h` | Fuente bitmap reutilizada por los tres visores, sin dependencia de fuente externa |

## Estructura del proyecto

```
src/
├── main.c         — bucle principal, dispatch de game mode, title screen, level select, menú de opciones
├── ram.h/.c       — grilla RAM global (ram[]) + accessors tipados + macros de objeto
├── constants.h    — IDs, tipos de colisión, direcciones bank/port, PLC ids
├── types.h        — typedefs de ancho fijo compartidos
├── vdp.c/.h       — modelo VDP Mega Drive + renderizado SDL (capa de sistema PC)
├── input.c/.h     — teclado → joypad RAM (capa de sistema PC)
├── sound.c/.h     — SDL_mixer música/SFX por id bgm/sfx (capa de sistema PC)
├── postprocess.c/.h — pipeline scanlines / blur / CRT / widescreen
├── config.c/.h    — carga/guardado de sonic1.cfg (binario, magic "S1CF")
├── options.h      — índices de filas del menú de opciones + layout de VRAM
├── level.c/.h     — Level_Enter/Process, ObjPosLoad, scrolling, sistema de agua LZ
├── objects.c/.h   — tabla de dispatch obj_map[], luego cada objeto ordenado por ID
├── special.c/.h   — máquina de estados del Special Stage + PalCycle_SS
├── sprites.c/.h   — BuildSprites / renderizado de sprites desde sprite_queue
├── data.c/.h      — todas las tablas/punteros/carga de assets desde build/assets/
├── assets.c/.h    — cargador de archivos binarios (con padding slack de descompresores)
├── decomp.c/.h    — descompresores Nemesis / Enigma / Kosinski
├── plc.c/.h       — cola Pattern Load Cue (NewPLC / RunPLC)
├── hud.c/.h       — patrones de dígitos HUD + refresco por frame
├── palette.c/.h   — paletas, PalLoad, fade in/out, PaletteCycle
├── deform.c/.h    — deformación de fondo + scroll de cámara
├── collision.c/.h — índice de colisión 16x16 para el charset del nivel
├── enddemo.c/.h   — pantalla "END DEMO" propia del port
├── tas.c/.h       — grabación / reproducción / savestates del TAS
├── tas_editor.c/.h— editor de frames del TAS
├── font8x8.h      — fuente bitmap 8x8 compartida por los visores
├── freecamera.c/.h— free camera de debug
├── objview.c/.h   — visor de Object RAM para debug
├── planeview.c/.h — visor de Planos A/B para debug
├── ramview.c/.h   — visor de RAM para debug
└── debugmode.c/.h — colocación de objeto debug (desde _incObj/DebugMode.asm)

disasm/            — desensamblado original Sega + assets sin comprimir (fuente de verdad)
Objects-List.md    — cada ID de objeto, su fuente en el desensamblado y su estado
```

### Cómo leer `src/objects.c`

`src/objects.c` tiene ~17k líneas, así que está organizado para que se pueda
navegar:

1. La tabla `obj_map[]`, una fila por objeto implementado, **ordenada por ID de
   objeto**, con el ID y la fuente en `disasm/_incObj/` en el comentario final.
2. `Objects_Init` / `ExecuteObjects` / `DisplaySprite` / `FindFreeObj` /
   `DeleteObject`.
3. **Parte 1** — helpers compartidos de `disasm/_incObj/sub/` (`CalcSine`,
   `SpeedToPos`, `ObjFloorDist`, `RememberState`, `SolidObject`, `FindFreeObj`,
   `AnimateSprite`, …).
4. **Partes 2-4** — las implementaciones de objetos, en el **mismo orden
   ascendente de ID** que `obj_map[]`, divididas en `$30` y `$60` solo por
   navegación.

Así, `$4C` (MZ lava geyser maker) se localiza igual en la tabla y en el código.
Cada banner de sección nombra el fichero `.asm` del que se tradujo, y
**Objects-List.md** lista el resto.

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

### Sondas por variable de entorno

La salida de debug nunca va en un run normal — cada sonda está detrás de una
variable de entorno leída una vez al arrancar:

| Variable | Efecto |
|---|---|
| `SONIC_DEBUG_COLLISION` | Pinta cada celda de colisión 16x16 visible. Coloreada por los flags de superficie que devuelve `FindNearestTile`: gris = 3 caras, verde = top, azul = izquierda/derecha, amarillo = bottom, rojo oscuro = sin superficie. `=fill` rellena la celda en vez de solo el borde |
| `SONIC_DEBUG_REACT` | Diagnósticos de anillos/colisión en `ReactToItem` (rango de objetos + punteros) |
| `SONIC_LOG_SPEED` | Inercia / ángulo / velocidad del jugador, cada 4 frames |
| `SONIC_DUMP_VRAM` | Vuelca VRAM, CRAM, VSRAM y los planos a disco una vez, cuando `v_generictimer <= SONIC_DUMP_TDINT` (por defecto 286) |
| `SONIC_DUMP_FRAMES` | Vuelca frames mientras `SONIC_DUMP_TMIN <= v_generictimer <= SONIC_DUMP_TMAX` (por defecto 300..376) |
| `SONIC_DUMP_TDINT` | Límite de `v_generictimer` para el dump de VRAM |
| `SONIC_DUMP_TMIN` / `SONIC_DUMP_TMAX` | Primer / último valor de timer del dump de frames |
| `SONIC_GARBAGE` | Carpeta de salida de los dumps |

⚠️ Si `SONIC_GARBAGE` no está definida, las rutas de los dumps caen a una
**ruta absoluta hardcodeada** de la máquina original (`src/main.c:552`,
`src/main.c:579`) y los dumps se pierden silenciosamente en el portátil de
cualquier otro. Defínela siempre.

### Otras notas de debug

- **gdb sin `-g`** (build default): `ram` no tiene tipo — castear direcciones
  crudas (`p/x *(char*)ADDR`). Con ASLR desactivado la base de imagen es
  `0x555555554000`; `nm build/sonic1` da direcciones absolutas globales.
- **No fuerces `v_gamemode = GM_Level` para "testear gameplay"**: salta el
  bucle Phase G de title-card + drenaje PLC, por lo que no reproduce un boot
  completo de nivel (colocación de objetos, paletas, estado PLC).
- Build con sanitizer (`-DSONIC_SANITIZE=ON`) es la primera parada para
  accesos a memoria salvaje.
- El overlay de colisión + la free camera + el visor RAM cubren casi todo lo
  que responde a "por qué este objeto está en el sitio equivocado" sin
  necesidad de inspeccionar imágenes.

## Roadmap / stubs conocidos

**No implementado**

- **Interpolación de FPS** — la fila del menú de opciones y el ajuste
  `fps_interp` existen, pero nada en el renderer los lee.
- Demo mode (el handler `$08` es `NULL`, `GotoDemo()` solo vuelve a la
  pantalla Sega).
- Pantallas Continue, Ending y Credits (solo máquinas de estado `TODO`, se
  alcanzan por los cheats del level select).
- Features VBlank estilo "Delay" por software y el driver de sonido Z80 (el
  sistema de sonido PC reemplaza al Z80). `DACDriverLoad()` es un stub vacío.
- **Cobertura de objetos**: hay 72 de los 134 IDs de objeto que el juego puede
  generar (`$01`-`$8C`) registrados en `obj_map[]` en `src/objects.c`. Los
  otros 62 caen en `NullObject_Main` → `DeleteObject`, así que nunca aparecen
  en un nivel; de esos, 7 los marca el desensamblado como *unused* y no
  necesitan port, dejando 55 objetos alcanzables pendientes, casi todos de
  Labyrinth, Scrap Brain y Star Light. Ver
  **[Objects-List.md](Objects-List.md)** para la tabla completa: cada ID, su
  fuente en `disasm/_incObj/`, sus zonas y su estado.
- `Render the VDP sprite table to an SDL texture` (`sprites.c:187`) — los
  sprites se componen por software en su lugar.
- El objeto 4C referencia sus mappings pero sus gráficos no están cargados
  (sección Object 4C en `objects.c`).

**Parcialmente portado**

- Zonas distintas a Green Hill — estructuras de datos/tablas portadas donde el
  ASM las requiere; los punteros de assets pueden ser `NULL`, así que el level
  select llega a esas zonas pero no son jugables de principio a fin.
- La entrada a nivel está verificada de extremo a extremo solo en GHZ.


## Licencia / descargo

Esto **no** es un producto de SEGA. *Sonic the Hedgehog* y assets relacionados
son propiedad de SEGA. El desensamblado (`disasm/`) está disponible para
fines educativos/archivísticos; este proyecto es un port de ejercicio de
programación del mismo.
