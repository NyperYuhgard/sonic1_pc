# sonic1_pc

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
| Sanitizers | `-DSONIC_SANITIZE=ON` | ASan + UBSan (wild-memory-access finder) |

### Assets

Assets are **not** committed. They are staged from the original `disasm/`
tree into `build/assets/` (non-audio only — Music/SoundFX are never staged):

```
python3 tools/stage_assets.py
```

The game loads assets at runtime from `build/assets/`, so run it once before
launching, and again whenever a new `load_asset(...)` path is added to
`src/data.c`.

## Controls

### Player 1

| Action | Key |
|---|---|
| Move | Arrow keys |
| Jump | `Z` (A button) |
| Jump (alt) | `X` (B button) |
| Roll / spin | `C` (C button) |
| Start / confirm | `Enter` |

### Player 2 (partial)

`I / J / K / L` move, `U / Y / O` are the ABC buttons, `P` is Start.

### PC debug keys

| Key | Effect |
|---|---|
| `P` | Toggle the VRAM viewer (tiles + CRAM strip, second SDL window) |
| `O` | Toggle the Object RAM viewer (live object slots) |
| `F` | Toggle free camera |
| `HOME / END / PAGE UP / PAGE DOWN` | Move the free camera; `DELETE` scrolls left |
| `ESC` (window title bar) | Quit |

## Cheat codes

All cheats are enabled by default (`f_*cheat = 1` in `main.c`).

- **Level select**: on the title screen, enter `Up, Down, Left, Right` on the
  D-Pad, then hold **A** (`Z`) and press **Start** (`Enter`). Move with
  `Up/Down`, confirm with any action button.
  - The last level-select row is **Sound Select** (`Left/Right` to browse).
  - Reaching sound `$9E` (with credits cheat) opens the Credits; `$9F` opens the
    Ending.
  - The Special Stage row jumps to the (currently stubbed) special stage state.
- **Debug mode**: `Start` during gameplay with debug enabled spawns the debug
  object palette (ported from `_incObj/DebugMode.asm`).

## Game flow (ported game modes)

The `game_mode_table` in `src/main.c` mirrors `GameModeArray` from `sonic.asm`:

| Mode | ID | Status |
|---|---|---|
| Sega screen | `$00` | ✅ Ported (palette cycle + "SEGA" chant timing) |
| Title screen | `$04` | ✅ Ported (STP, title art, title Sonic, PSBTM, level select, cheats) |
| Demo | `$08` | ⛔ Stub (returns to Sega) |
| Level | `$0C` | ✅ Ported (GHZ1 focus) |
| Special Stage | `$10` | ⛔ Stub (`TODO`) |
| Continue | `$14` | ⛔ Stub (`TODO`) |
| Ending | `$18` | ⛔ Stub (`TODO`) |
| Credits | `$1C` | ⛔ Stub (`TODO`) |

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

## Project layout

```
src/
├── main.c        — main loop, game mode dispatch, title screen, level select
├── ram.h/.c      — global RAM grid (ram[]) + typed accessors + obj macros
├── constants.h   — IDs, collision types, bank/port addresses, PLC ids
├── vdp.c/.h      — Mega Drive VDP model + SDL rendering (PC system layer)
├── input.c/.h    — keyboard → joypad RAM (PC system layer)
├── sound.c/.h    — SDL_mixer music/SFX by bgm/sfx id (PC system layer)
├── level.c/.h    — Level_Enter/Process, ObjPosLoad, scrolling, drawing helpers
├── objects.c/.h  — object dispatch, Sonic, rings, ReactToItem, title cards
├── sprites.c/.h  — BuildSprites / sprite rendering from sprite_queue
├── data.c/.h     — all tables/pointers/asset loading from build/assets/
├── assets.c/.h   — binary file loader (with decompressor slack padding)
├── decomp.c/.h   — Nemesis / Enigma / Kosinski decompressors
├── plc.c/.h      — Pattern Load Cue queue (NewPLC / RunPLC)
├── hud.c/.h      — HUD digit patters + per-frame refresh
├── palette.c/.h  — palettes, PalLoad, fade in/out, PaletteCycle
├── deform.c/.h   — background deformation + camera scroll
├── collision.c/.h— 16x16 collision index for the level charset
├── objview.c/.h  — Object RAM debug viewer window
├── freecamera.c  — free camera debug feature
└── debugmode.c/.h— debug object placement (from _incObj/DebugMode.asm)

disasm/           — original Sega disassembly + uncompressed assets (source of truth)
tools/
└── stage_assets.py — copies referenced disasm assets into build/assets/
```

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

## Debugging

- **Env-var probes** are used so debug output never ships in normal runs, e.g.
  `SONIC_DEBUG_REACT` (ring/collision diagnostics in `ReactToItem`).
- **gdb without `-g`** (default build): `ram` has no type — cast raw addresses
  (`p/x *(char*)ADDR`). With ASLR disabled the image base is
  `0x555555554000`; `nm build/sonic1` gives absolute global addresses.
- **Don't force `v_gamemode = GM_Level` to "test gameplay"**: it skips the
  title-card Phase G loop + PLC drain, so it does not reproduce a full level
  boot (object placement, palettes, PLC state).
- Sanitizer build (`-DSONIC_SANITIZE=ON`) is the first stop for wild memory
  access.

## Roadmap / known stubs

- Special Stage, Continue, Ending and Credits screens (state machines only).
- Demo mode.
- Zones other than Green Hill — data structures/tables are ported where the ASM
  requires them; asset pointers may be `NULL`.
- Software "Delay"-style VBlank features and the Z80 sound driver (the PC sound
  system replaces the Z80).

## License / disclaimer

This is **not** a SEGA product. *Sonic the Hedgehog* and related assets are the
property of SEGA. The disassembly (`disasm/`) is available for educational/
archival purposes; this project is a programming-exercise port of it.