# sonic1_pc

**Idioma / Language:** [English](README.md) | [Español](README.es.md)

A **1:1 PC port** of the *Sonic the Hedgehog* (Sega Mega Drive) 68000 disassembly,
written in **C11** and rendered/played through **SDL2** and **SDL2_mixer**.
No retro engine — the game logic is a faithful translation of the original
`disasm/sonic.asm` + `disasm/_inc/*` assembly.

## Porting philosophy

- **Strict 1:1 parity.** The C code mirrors the disassembly's logic, structure,
  widths, order and lookups. If the ASM derives a value through a table or a
  header (e.g. the PLC ID from `LevelHeaders`), the port performs the same lookup
  — it never hardcodes the result.
- **No shortcuts or reinterpretations.** Branches, state machines and quirks
  (FixBugs=0 disasm) are kept as-is.
- **Data structures first.** Tables and mechanisms are ported completely even when
  only Green Hill Zone is currently playable; other zone assets may be stubbed
  or `NULL`.
- **PC-only system layer.** Only the VDP (video), the sound system and the input
  go through SDL/SDL2_mixer (`src/vdp.c`, `src/sound.c`, `src/input.c`).
  Everything else is 1:1 territory.

See `CONTRIBUTING.md` for the full contributing guidelines.

## Dependencies

- A C11 compiler and CMake ≥ 3.10
- SDL2
- SDL2_mixer (with OGG support)

On Debian/Ubuntu:

```
sudo apt install build-essential cmake libsdl2-dev libsdl2-mixer-dev
```

## Build & run

```
cmake -S . -B build
cmake --build build -j$(nproc)
./build/sonic1
```

### Build variants

| Variant | CMake flag | Effect |
|---|---|---|
| Default | — | `-O2`, no `-g` (symbols via `nm`, gdb needs raw casts) |
| Debug | `-DCMAKE_BUILD_TYPE=Debug` | `-O0 -g -DDEBUG` |
| RelWithDebInfo | `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | `-O2 -g -DNDEBUG` |
| Sanitizers | `-DSONIC_SANITIZE=ON` | ASan + UBSan (wild-memory-access finder) |

### Build scripts

`scripts/` wraps the manual commands above into ready-to-use variants that also
stage the assets next to the executable.

**Linux / WSL2 (native build):**

```
scripts/build-linux.sh                 # release  -> build/release/sonic1
scripts/build-linux.sh debug           # debug    -> build/debug/sonic1
scripts/build-linux.sh relwithdebinfo  # optimized + symbols
scripts/build-linux.sh asan            # ASan + UBSan
scripts/build-linux.sh all --clean     # all four variants, from scratch
scripts/build-linux.sh debug --run     # build and launch
```

Run the game from the repository root (`./build/release/sonic1`): `src/sound.c`
loads audio from `./assets` (working-directory relative) while `src/data.c`
resolves the rest against the executable path.

**Windows (cross-compiled with mingw-w64 from Linux/WSL2):**

```
scripts/build-windows.sh                       # release -> dist/win/release/sonic1.exe
scripts/build-windows.sh debug --clean
scripts/build-windows.sh all
scripts/build-windows.bat                      # same, from cmd.exe on Windows (needs WSL2)
```

`dist/win/<variant>/` is self-contained: `sonic1.exe`, the SDL2 DLLs and
`assets/`. Cross-built SDL2 is required, and since mingw-w64 ships no
AddressSanitizer the `asan` variant is Linux/WSL2 only:

```
sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64
scripts/build-windows.sh release --prefix ~/mingw64   # SDL2 cross-built for mingw-w64
```

Known blocker: `src/data.c` maps level data with POSIX `mmap` (`sys/mman.h`),
which mingw-w64 does not provide, so the `.exe` does not link until that is
replaced by a Windows equivalent (`VirtualAlloc`).

Run `scripts/build-linux.sh --help` / `scripts/build-windows.sh --help` for the
full option list.

### Assets

Assets are **not** committed. The engine loads from `build/assets/` first,
falling back to `disasm/`, so you can either:

- Run the game directly from `disasm/` (no extra copies), or
- Copy the needed subfolders from `disasm/` into `build/assets/` if you want
  overrides/custom assets.

`src/data.c` uses the original filenames from `disasm/` (with spaces).

## Controls

The game takes **no command-line arguments** — `main()` discards
`argc`/`argv` explicitly (`src/main.c:1185-1187`) and there is no `getopt`
anywhere in `src/`. Everything is keyboard-driven, and every persistent
setting lives in the in-game options menu (see below).

### Player 1

| Action | Key |
|---|---|
| Move | Arrow keys |
| Jump | `Z` (A button) |
| Jump (alt) | `X` (B button) |
| Roll / spin | `C` (C button) |
| Start / confirm | `Enter` |

### Player 2 (partial)

> **Provisional mapping.** Sonic 1 has no two-player mode, so these bindings
> exist only to feed `v_jpadhold2` / `v_jpadpress2`. They are expected to be
> remapped — treat this table as temporary.

| Action | Key |
|---|---|
| Move | `I` `J` `K` `L` |
| Jump | `U` (A button) |
| Jump (alt) | `Y` (B button) |
| Roll / spin | `O` (C button) |
| Start | `P` |

⚠️ `O` and `P` double as the Object RAM / VRAM viewer toggles, so with player
2 active every `O` or `P` press also opens or closes a viewer window. That
collision is one of the reasons this mapping needs to change.

The game actually *reads* player 2 in one place: the LZ water tunnels let the
second pad steer Sonic up/down while the wind pushes him along
(`src/level.c:926-927`).

### PC debug keys

All of these work at any time, in any game mode, from the main window:

| Key | Effect |
|---|---|
| `P` | Toggle the VRAM viewer (tile sheet with VRAM address labels, OSD of plane bases/scrolls, palette_main + CRAM strips) |
| `O` | Toggle the Object RAM viewer (live object slots) |
| `G` | Toggle the Plane A/B viewer (full nametables, adapts to window size without stretching: side-by-side or stacked, wheel/drag scroll) |
| `V` | Plane viewer: toggle the 128-row wrap mode (BlastEm-style) |
| `R` | Toggle the RAM viewer (live hex + decimal, grouped into labelled sections) |
| `F` | Toggle the free camera |
| `T` | Toggle the TAS editor window |
| `F1`–`F8` | TAS actions (see below) |
| `ESC` | Quit (window title bar) |

Each viewer is a separate SDL window that refreshes at 60 Hz from the end of
`VDP_RenderFrame`, and closing one with the window manager toggles it back off
(`src/input.c:42-53`).

**Viewer navigation** — the Plane viewer scrolls with the mouse wheel and pans
with left-drag, but only while the pointer is inside that window
(`src/input.c:55-75`). The RAM viewer is read-only; its sections are GAME
STATE, BOUNDS & CAMERA, SONIC, LOOP/ROLL/TRACK, FLAGS, SYNC/OSCILLATE and
MISC (`src/ramview.c:219-227`).

### Free camera

`F` toggles it. The camera then pans 8 px per frame, clamped to the level
bounds (`v_limitleft2` / `v_limitright2` / `v_limittop2` / `v_limitbtm2`):

| Key | Direction |
|---|---|
| `Home` | Up |
| `End` | Down |
| `PageUp` | Right |
| `Delete` | Left |

It moves the real 16.16 world camera and forces a full FG redraw so the plane
refreshes while panning (`src/freecamera.c:31-51`).

### TAS keys

| Key | Action |
|---|---|
| `F1` | Toggle recording (LIVE → RECORD → LIVE, or cancel playback) |
| `F2` | Replay from the first frame |
| `F3` | Save `tas.bin` |
| `F4` | Load `tas.bin` |
| `F5` | Save state to the next slot (8 slots, round-robin) |
| `F6` | Load the previous slot |
| `F7` | Pause / unpause |
| `F8` | Advance one frame (only while paused) |

Playback feeds the recorded pad values straight into `v_jpadhold1` /
`v_jpadpress1`, overriding the keyboard (`src/input.c:147-148`).

### TAS editor keys

| Key | Action |
|---|---|
| `Esc` | Close the editor |
| `Space` | Pause / unpause |
| `R` | Pause and replay from frame 0 |
| `←` / `→` | Move the frame cursor |
| `Home` / `End` | Jump to the first / last frame |
| `PageUp` / `PageDown` | Scroll by one page of frames |
| `.` | Single frame step |
| `Delete` / `Backspace` | Delete the selected frame(s) |
| `Insert` | Insert a frame at the cursor |
| `A` | Select all |
| `Ctrl+S` / `Ctrl+O` | Save / load `tas.bin` |
| `F5` / `F6` | Save state / load state |

While the editor window has focus, game input is zeroed so the keyboard cannot
perturb the run (`src/input.c:149-152`).

## In-game options menu

Reached from the title screen: hold **A** and press **Start** (level select),
then press **B** (`src/main.c:635`). `↑`/`↓` pick the row, `←`/`→` change the
value, `A`/`C`/`Start` confirm.

| Row | Values | Effect |
|---|---|---|
| WIDESCREEN | OFF / 398 / 424 / 480 | Render width (`VDP_ApplyWidescreen`, `src/config.c:40-44`) |
| FPS INTERP. | ON / OFF | **Not implemented** — the flag is stored and toggled but never read by the renderer. Listed for parity with the planned 60→120 Hz interpolation |
| SCANLINES | ON / OFF | Scanline overlay (`src/postprocess.c:14`) |
| FULLSCREEN | ON / OFF | `SDL_WINDOW_FULLSCREEN_DESKTOP` |
| SS ALT ANIM | ON / OFF | Alternative Special Stage exit animation (`SonicSS_ExitStage`) |
| SS SMOOTH | ON / OFF | Smooth scrolling in the Special Stage maze |
| CRT | ON / OFF | Barrel curvature + subtle vignette, nearest sampling (`src/postprocess.c:48`) |
| BLUR | ON / OFF | Separable H+V blur (`src/postprocess.c:138`, `:160`) |
| APPLY & SAVE | — | Write the config to disk |
| BACK | — | Return to the level select |

Post-processing runs in a fixed order — scanlines, blur H, blur V, CRT — over
two internal buffers, with a maximum render width of 640
(`src/postprocess.h:8-9`, `src/postprocess.c:205-221`).

Settings are persisted to **`sonic1.cfg`**, a *binary* file in the current
working directory: the 4-byte magic `S1CF`, a `uint32_t` version (currently
`4`), then the raw `Settings` struct (`src/config.c:17-38`). It is loaded once
at startup (`src/main.c:1196`) and written on APPLY & SAVE, on BACK and on
exit. Delete the file to get the defaults back.

## Cheat codes

All four cheats are enabled at boot (`f_*cheat = 1`, `src/main.c:1225-1229`,
mirroring `CheatsEnabled=1` in `sonic.asm:420-425`):

| Flag | RAM | Effect | Default |
|---|---|---|---|
| `f_levselcheat` | `$FFE0` | Level select screen | on |
| `f_slomocheat` | `$FFE1` | Slow motion from the pause screen | on |
| `f_debugcheat` | `$FFE2` | Debug mode in level | on |
| `f_creditscheat` | `$FFE3` | Hidden Japanese credits / ending | on |

- **Level select**: on the title screen, hold **A** (`Z`) and press **Start**
  (`Enter`) — the ASM checks `f_levselcheat` and `A` held while `Start` is
  pressed (`disasm/sonic.asm:2166-2170`). Move with `Up/Down`, confirm with any
  action button.
  - The last level-select row is **Sound Select** (`Left/Right` to browse).
  - Reaching sound `$9E` (with credits cheat) opens the Credits; `$9F` opens the
    Ending.
  - The **Special Stage** row starts a real special stage (3 lives, 0 rings,
    `v_emldlist` cleared) — it is not a stub.
  - Pressing **B** on any row opens the options menu.
- **Slow motion**: while paused, hold **B** (`X`) or tap **C** to enter
  slow-motion; press **A** (`Z`) to quit back to the title screen
  (`src/level.c:1278-1281`).
- **Debug mode**: `Start` during gameplay with debug enabled spawns the debug
  object palette (ported from `_incObj/DebugMode.asm`).
- **Re-arming the cheats** (`Tit_ActivateCheat`, `disasm/sonic.asm:2113-2128`,
  ported at `src/main.c:702-723`): entering `Up, Down, Left, Right` on the
  title-screen D-Pad re-enables one cheat, chosen by how many times **C** was
  pressed (0-1 level select, 2-3 slow motion, 4-5 debug, 6-7 credits). On
  non-Japanese regions (`v_megadrive >= 0`) two or more C presses always force
  slow motion + debug, and the credits cheat stays unreachable — exactly like
  the original. Since the port boots with all four already on, this code is
  only observable if something clears them.

## Game flow (ported game modes)

The `game_mode_table` in `src/main.c` mirrors `GameModeArray` from `sonic.asm`:

| Mode | ID | Status |
|---|---|---|
| Sega screen | `$00` | ✅ 100% (palette cycle + "SEGA" chant timing) |
| Title screen | `$04` | ✅ 98% (STP, title art, title Sonic, PSBTM, level select, cheats) |
| Demo | `$08` | ⛔ Stub (returns to Sega) |
| Level | `$0C` | ✅ 40% (Only Green Hill Zone Complete) |
| Special Stage | `$10` | ✅ 99% (Full flow: fades, results screen, emeralds) |
| Continue | `$14` | ⛔ Stub (`TODO`) |
| Ending | `$18` | ⛔ Stub (`TODO`) |
| Credits | `$1C` | ⛔ Stub (`TODO`) |
| End Demo | `$20` | ✅ Port-only SDL screen ("END DEMO" / "DEVELOPED BY" + logos), not the ASM routine — build-time demo ending only |

## Currently ported gameplay (Green Hill Zone)

- Level entry (`Level_Enter`): fade, title-card Phase G loop, PLC drain, RAM
  clears, VDP setup, layout/chunk/mapping loads.
- Per-frame `Level_Process`: pause, VBlank, `ExecuteObjects`, deform,
  `BuildSprites`, `ObjPosLoad`, palette cycle, PLC, oscillators, synchro anim,
  signpost art.
- Sonic player object (`Sonic_Control`, `Sonic_Animate`, `Sonic_LoadGfx`,
  entity speed/object fall, floor distance).
- `ReactToItem` collision (rings, monitors, badniks, spikes, boss logic
  ported).
- **Rings**: spawn via `ObjPosLoad` → expand/animate → `DisplaySprite` →
  bit-7 rendered → `ReactToItem` collects → sparkle → delete; `CollectRing`
  updates the counter/HUD and awards extra lives at 100/200 rings.
- HUD (score/time/rings/lives) via `HUD_Update`, `Hud_Base`.
- Title cards, game-over card, "Got Through" card, signpost.
- Collision index for the charset (`ColIndexLoad`, `ConvertCollisionArray`).
- Animated level graphics per zone (GHZ) and zone palette cycling.
- **Special Stage**: full 1:1 flow (white fades, maze physics, results screen
  with card elements, ring bonus tally, Chaos Emeralds, exit to next level),
  including `PalCycle_SS` / `PalCycle_SS_2` from `SpecCode.asm`.

## Labyrinth Zone water system

Ported 1:1 from `disasm/_inc/LZWaterFeatures.asm` (the `NUEVO` markers in
`src/level.c` mean "newly ported", not "invented"):

- **Per-act water height** — `WaterHeight[4]` for LZ1/LZ2/LZ3/SBZ3
  (`level.c:597`) applied to `v_waterpos1..3` by `LZ_LevelWaterSetup`
  (`level.c:630`).
- **`LZWindTunnels`** (`level.c:878`) — waterfall wind tunnels, located from
  `LZWind_Data` at offset `8+(act<<3)` (two entries on act 1). Inside a tunnel
  `sfx_Waterfall` plays every `$40` frames, Sonic is pushed along at
  `obVelX = $400` in the `id_Float2` animation, a suction zone near the left
  wall lifts or drops him, and **player 2's** `Up`/`Down` nudges him
  vertically. Sets `f_wtunnelmode` and honours `f_wtunneldisallow`.
- **`LZWaterSlides`** (`level.c:939`) — water slides matched by chunk id
  against `Slide_Chunks[7]`.
- **`LZDynamicWater`** (`level.c:1130`) — the state machine that walks
  `v_waterpos2` towards `v_waterpos3`, including the hardcoded Act 3 targets
  (`$0508`, `$0608`, `$07C0`, `$0128`).
- **Per-scanline palette** — the renderer selects `palette_water_main` for the
  submerged region from `f_wtr_state` (whole screen under water) and
  `v_hblank_line` (water line), refreshed from `v_palette_water` once per frame
  (`vdp.c:677`, `vdp.c:714-729`).
- **Water surface spawn** in `Level_ChkWater` (`level.c:756`) and the active
  underwater palette load before the fade-in (`level.c:745`).
- **Assets** — `palette/Labyrinth Zone Underwater.bin`,
  `palette/Sonic - LZ Underwater.bin`, `artnem/LZ Water Surface.nem`,
  `artnem/LZ Water & Splashes.nem`, `artunc/GHZ Waterfall.unc`
  (`data.c:62-63`, `data.c:673-674`, `data.c:764`).

## Port-only additions

None of this exists in the Mega Drive game — it is the PC layer.

| System | File(s) | What it does |
|---|---|---|
| Options menu + config | `config.c/.h`, `options.h`, `main.c` | 10-row in-game menu persisted to the binary `sonic1.cfg` |
| Post-processing | `postprocess.c/.h` | Scanlines, separable blur, CRT curvature + vignette; widescreen widths 398/424/480 |
| VRAM viewer (`P`) | `vdp.c` | 2048-tile sheet, VRAM address labels, OSD of plane bases + hscroll, palette_main and CRAM strips |
| Object RAM viewer (`O`) | `objview.c` | Live object slots |
| Plane A/B viewer (`G`, `V`) | `planeview.c` | Full nametables with a rectangle over the sampled 320x224 region, 128-row wrap toggle |
| RAM viewer (`R`) | `ramview.c` | Live hex + decimal of `ram[]` in 7 labelled sections |
| Free camera (`F`) | `freecamera.c` | Pans the real 16.16 world camera, clamped to the level bounds |
| Collision overlay | `vdp.c` | `SONIC_DEBUG_COLLISION` paints every 16x16 collision cell, coloured by the surface flags `FindNearestTile` returns |
| TAS | `tas.c`, `tas.h`, `tas_editor.c/.h` | Record/replay up to 1 h @ 60 Hz, `tas.bin` (`TASFILE` v1), 8 full-state savestates (64 KB RAM + VRAM + CRAM + VSRAM + VDP registers), plus a frame editor |
| End Demo screen | `enddemo.c` | SDL screen with a hand-rolled vector pixel font instead of the ASM routine |
| Shared 8x8 font | `font8x8.h` | Bitmap font reused by the three viewers, no external font dependency |

## Project layout

```
src/
├── main.c         — main loop, game mode dispatch, title screen, level select, options menu
├── ram.h/.c       — global RAM grid (ram[]) + typed accessors + obj macros
├── constants.h    — IDs, collision types, bank/port addresses, PLC ids
├── types.h        — shared fixed-width typedefs
├── vdp.c/.h       — Mega Drive VDP model + SDL rendering (PC system layer)
├── input.c/.h     — keyboard → joypad RAM (PC system layer)
├── sound.c/.h     — SDL_mixer music/SFX by bgm/sfx id (PC system layer)
├── postprocess.c/.h — scanlines / blur / CRT / widescreen pipeline
├── config.c/.h    — sonic1.cfg load/save (binary, magic "S1CF")
├── options.h      — options menu row indices + VRAM layout
├── level.c/.h     — Level_Enter/Process, ObjPosLoad, scrolling, LZ water system
├── objects.c/.h   — obj_map[] dispatch table, then every object sorted by ID
├── special.c/.h   — Special Stage state machine + PalCycle_SS
├── sprites.c/.h   — BuildSprites / sprite rendering from sprite_queue
├── data.c/.h      — all tables/pointers/asset loading from build/assets/
├── assets.c/.h    — binary file loader (with decompressor slack padding)
├── decomp.c/.h    — Nemesis / Enigma / Kosinski decompressors
├── plc.c/.h       — Pattern Load Cue queue (NewPLC / RunPLC)
├── hud.c/.h       — HUD digit patters + per-frame refresh
├── palette.c/.h   — palettes, PalLoad, fade in/out, PaletteCycle
├── deform.c/.h    — background deformation + camera scroll
├── collision.c/.h — 16x16 collision index for the level charset
├── enddemo.c/.h   — port-only "END DEMO" screen
├── tas.c/.h       — TAS record / playback / savestates
├── tas_editor.c/.h— TAS frame editor window
├── font8x8.h      — shared 8x8 bitmap font for the viewers
├── freecamera.c/.h— debug free camera
├── objview.c/.h   — Object RAM debug viewer window
├── planeview.c/.h — Plane A/B debug viewer window
├── ramview.c/.h   — RAM debug viewer window
└── debugmode.c/.h — debug object placement (from _incObj/DebugMode.asm)

disasm/            — original Sega disassembly + uncompressed assets (source of truth)
Objects-List.md    — every object ID, its disasm source and its ported status
```

### Reading `src/objects.c`

`src/objects.c` is ~17k lines, so it is laid out to stay navigable:

1. The `obj_map[]` table, one row per implemented object, **sorted by object
   ID**, with the ID and the `disasm/_incObj/` source in the trailing comment.
2. `Objects_Init` / `ExecuteObjects` / `DisplaySprite` / `FindFreeObj` /
   `DeleteObject`.
3. **Part 1** — shared helpers from `disasm/_incObj/sub/` (`CalcSine`,
   `SpeedToPos`, `ObjFloorDist`, `RememberState`, `SolidObject`, `FindFreeObj`,
   `AnimateSprite`, …).
4. **Parts 2-4** — the object implementations, in the **same ascending ID
   order** as `obj_map[]`, split at `$30` and `$60` purely for navigation.

So `$4C` (MZ lava geyser maker) is found the same way in the table and in the
code. Each section banner names the `.asm` file it was translated from, and
**Objects-List.md** lists the rest.

## Architecture notes

- **RAM grid.** `src/ram.h` models the 68k address space as a flat byte array
  (`ram[]`); `RAM_BYTE/RAM_WORD/RAM_LONG(addr)` perform typed accesses at the
  original offsets and `RAM_ADDR(addr)` yields a runtime pointer (e.g.
  `RAM_ADDR(v_lvlobjspace)`). Object fields use macros (`obX`, `obY`,
  `obRoutine`, `obRender`, `obColType`, …).
- **Objects.** 128 slots of 64 bytes; level objects live in
  `v_lvlobjspace..v_lvlobjend` (96 slots). Each object dispatches on its
  `obRoutine` byte via the `obj_dispatch[]` table.
- **Sprite rendering flag.** `DisplaySprite` queues an object;
  `BuildSprites` runs after `ExecuteObjects` each frame and sets
  `obRender |= sprite_rendered` (bit 7) — so collision (`ReactToItem`) only
  ever sees an object that was rendered the previous frame.
- **Collision types.** `col_none` 0x00, `col_item` 0x40 (rings/monitors),
  `col_hurt` 0x80 (damaging), `col_special` 0xC0 (special properties).
- **PLC queue.** Graphics are loaded on demand through a 16-slot pattern load
  cue queue (`NewPLC`/`RunPLC`, one entry per frame) — same as the original.

## RAM modding

`src/ram.h` models the 68k address space as a flat byte array (`ram[]`).
Every RAM address is an **offset** into that array, and `RAM_BYTE/RAM_WORD/
RAM_LONG(addr)` read or write 1/2/4 bytes at that offset — exactly like
`move.b`/`move.w`/`move.l` on the 68000. `RAM_ADDR(addr)` yields a raw
pointer (`&ram[addr]`) for callers that need an address instead of a value.

Because the original ASM freely mixes access widths (e.g. it writes `v_zone`
as a byte and `v_zone_act` as a word — same two bytes, different views), the
port must be able to do the same. That is why the `RAM_*` macros exist at
all, and why some identifiers are declared in two flavours.

### Two declaration styles

Every `v_*` name is one of:

| Style | Example | Meaning |
|---|---|---|
| **Offset** | `#define v_objspace 0xD000` | The name *is* an address. |
| **Lvalue** | `#define v_invinc (RAM_BYTE(0xFE2D))` | The name *is* a variable at that address. |

They are both valid, but **you must match the access style to the declaration**:

| Style | Correct write | Correct read | Address-taking |
|---|---|---|---|
| Offset | `RAM_BYTE(v_x) = n;` | `RAM_BYTE(v_x)` | `RAM_ADDR(v_x)` |
| Offset (derived) | `RAM_WORD(v_y) = n;` | `RAM_WORD(v_y)` | `RAM_ADDR(v_y)` |
| Lvalue | `v_z = n;` | `v_z` | `&(v_z)` (rare) |

Mixing them is the source of the **double-deref bug** below.

### The double-deref pitfall

If `v_x` is an lvalue macro, wrapping it in `RAM_*` compiles but is wrong:

```
#define v_invinc (RAM_BYTE(0xFE2D))    /* lvalue: reads RAM when used */

v_invinc = 1;              /* ✅  ram[0xFE2D] = 1                          */
RAM_BYTE(v_invinc) = 0;    /* ❌  expands to ram[ ram[0xFE2D] ] = 0       */
```

The second form reads the current value at 0xFE2D and uses it as an index,
silently writing to the wrong address (often ram[0] or ram[1], which is why
it can look like nothing happened at all). No compiler warning: the resulting
index is a perfectly valid uint16_t.

Rule of thumb: if v_x = n; compiles, then RAM_BYTE(v_x) = n; is a bug.

### Cross-size access

Sometimes the original ASM writes a wider or narrower value than the natural
width of a variable. Example from the disasm:

```

move.w  #id_GHZ_act1,(v_zone_act).w   ; writes v_zone (byte) + v_act (byte)
move.b  #$04,(v_zone).w                ; writes only v_zone

```

When the port needs the same freedom, use the raw address, not the
lvalue macro. Two common approaches:

1) Take the address directly with a literal (simplest):

```

v_zone = 0x04;                          /* natural-width write */
RAM_WORD(0xFE10) = id_GHZ_act1;         /* cross-size write    */

```
2) Declare both views of the same byte pair:

```

#define v_zone_act_a   0xFE10
#define v_zone_act     (*(uint16_t *)&ram[v_zone_act_a])
#define v_zone         (*(uint8_t  *)&ram[v_zone_act_a + 0])   /* check endianness */
#define v_act          (*(uint8_t  *)&ram[v_zone_act_a + 1])

v_zone = 0x04;                          /* ✅  single byte        */
RAM_WORD(v_zone_act_a) = id_GHZ_act1;   /* ✅  both bytes, swapped */

```

The _a (or _addr) suffix convention marks the raw address; the name without
it is the typed access. This makes the intent obvious at every call site and
lets the RAM viewer (which reads ram[] directly) keep working unchanged.

### Endianness

On the 68k, words and longs are stored big-endian. In this port ram[]
holds the same byte sequence the 68k would produce, but is indexed as a
flat array, so reading a uint16_t from ram[] gives little-endian on
x86-64. That is why writes that cross byte boundaries go through
RAM_SET_U16 / RAM_SET_U32 (or RAM_WORD for the byte-swapped view): they
swap to match the 68k layout.

When in doubt, follow the pattern the disasm uses:

```
    move.b → RAM_BYTE(addr)

    move.w → RAM_WORD(addr) or RAM_SET_U16(addr, v)

    move.l → RAM_LONG(addr) or RAM_SET_U32(addr, v)
    
```

### Quick audit for mixed styles

From the project root:

```
grep -nE 'RAM_(BYTE|WORD|LONG)\(v_[A-Za-z0-9_]+\)' src/*.c

```

Each hit is a bug iff the corresponding v_* is declared as an lvalue
macro in ram.h. The proper fix is to drop the RAM_* wrapper:

```
RAM_BYTE(v_invinc) = 0;   →   v_invinc  = 0;
RAM_BYTE(v_shield) = 0;   →   v_shield  = 0;
RAM_BYTE(f_bigring) = 1;  →   f_bigring = 1;

```
and to keep RAM_* only for literal addresses or arithmetic on addresses:

```
RAM_BYTE(0xFE2D) = 0;             /* ✅ literal              */
RAM_BYTE(v_sslayout_base + d4) = x; /* ✅ arithmetic on address */

```

If a v_* needs both natural-width and cross-size access, apply the _a
suffix convention above rather than mixing styles ad hoc.


## Debugging

### Env-var probes

Debug output never ships in a normal run — every probe is gated behind an
environment variable read once at startup:

| Variable | Effect |
|---|---|
| `SONIC_DEBUG_COLLISION` | Paint every visible 16x16 collision cell. Coloured by the surface flags `FindNearestTile` returns: grey = 3 faces, green = top, blue = left/right, yellow = bottom, dark red = no surface. `=fill` fills the cell instead of only its border |
| `SONIC_DEBUG_REACT` | Ring/collision diagnostics in `ReactToItem` (object range + pointers) |
| `SONIC_LOG_SPEED` | Inertia / angle / velocity of the player, every 4 frames |
| `SONIC_DUMP_VRAM` | Dump VRAM, CRAM, VSRAM and the planes to disk once, when `v_generictimer <= SONIC_DUMP_TDINT` (default 286) |
| `SONIC_DUMP_FRAMES` | Dump frames while `SONIC_DUMP_TMIN <= v_generictimer <= SONIC_DUMP_TMAX` (defaults 300..376) |
| `SONIC_DUMP_TDINT` | Generic timer limit for the VRAM dump |
| `SONIC_DUMP_TMIN` / `SONIC_DUMP_TMAX` | First / last timer value of the frame dump |
| `SONIC_GARBAGE` | Output folder for the dumps |

⚠️ When `SONIC_GARBAGE` is unset, the dump paths fall back to a
**hardcoded absolute path** of the original machine
(`src/main.c:552`, `src/main.c:579`) and the dumps silently go nowhere useful
on anyone else's box. Always set it.

### Other debugging notes

- **gdb without `-g`** (default build): `ram` has no type — cast raw addresses
  (`p/x *(char*)ADDR`). With ASLR disabled the image base is
  `0x555555554000`; `nm build/sonic1` gives absolute global addresses.
- **Don't force `v_gamemode = GM_Level` to "test gameplay"**: it skips the
  title-card Phase G loop + PLC drain, so it does not reproduce a full level
  boot (object placement, palettes, PLC state).
- Sanitizer build (`-DSONIC_SANITIZE=ON`) is the first stop for wild memory
  access.
- The collision overlay + free camera + RAM viewer cover most "why is this
  object in the wrong place" questions without any image inspection.

## Roadmap / known stubs

**Not implemented**

- **FPS interpolation** — the options-menu row and the `fps_interp` setting
  exist, but nothing in the renderer reads them.
- Demo mode (`$08` handler is `NULL`, `GotoDemo()` just returns to the Sega
  screen).
- Continue, Ending and Credits screens (`TODO` state machines only, reached
  through the level-select cheats).
- Software "Delay"-style VBlank features and the Z80 sound driver (the PC
  sound system replaces the Z80). `DACDriverLoad()` is an empty stub.
- **Object coverage**: 72 of the 134 object IDs the game can spawn
  (`$01`-`$8C`) are registered in `obj_map[]` in `src/objects.c`. The other 62
  fall through to `NullObject_Main` → `DeleteObject`, so they never appear in a
  level; 7 of those are flagged *unused* by the disassembly and need no port,
  leaving 55 reachable objects pending, almost all of Labyrinth, Sandopolis and
  Star Light. See **[Objects-List.md](Objects-List.md)** for the full table:
  every ID, its `disasm/_incObj/` source, its zones, and its status.
- `Render the VDP sprite table to an SDL texture` (`sprites.c:187`) — sprites
  are composited in software instead.
- Object 4C's mappings are referenced but its graphics are not loaded
  (`objects.c` Object 4C section).

**Partially ported**

- Zones other than Green Hill — data structures/tables are ported where the
  ASM requires them; asset pointers may be `NULL`, so level select reaches
  those zones but they are not fully playable.
- Level entry is verified end-to-end only on GHZ.


## License / disclaimer

This is **not** a SEGA product. *Sonic the Hedgehog* and related assets are the
property of SEGA. The disassembly (`disasm/`) is available for educational/
archival purposes; this project is a programming-exercise port of it.
