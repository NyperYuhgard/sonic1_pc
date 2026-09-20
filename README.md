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
| Sanitizers | `-DSONIC_SANITIZE=ON` | ASan + UBSan (wild-memory-access finder) |

### Assets

Assets are **not** committed. The engine loads from `build/assets/` first,
falling back to `disasm/`, so you can either:

- Run the game directly from `disasm/` (no extra copies), or
- Copy the needed subfolders from `disasm/` into `build/assets/` if you want
  overrides/custom assets.

`src/data.c` uses the original filenames from `disasm/` (with spaces).

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
| `P` | Toggle the VRAM viewer (tile sheet with VRAM address labels, OSD of plane bases/scrolls, palette_main + CRAM strips) |
| `O` | Toggle the Object RAM viewer (live object slots) |
| `G` | Toggle the Plane A/B viewer (full nametables, adapts to window size without stretching: side-by-side or stacked, wheel/drag scroll) |
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
| Sega screen | `$00` | ✅ 100% (palette cycle + "SEGA" chant timing) |
| Title screen | `$04` | ✅ 98% (STP, title art, title Sonic, PSBTM, level select, cheats) |
| Demo | `$08` | ⛔ Stub (returns to Sega) |
| Level | `$0C` | ✅ 40% (Only Green Hill Zone Complete) |
| Special Stage | `$10` | ✅ 99% (Full flow: fades, results screen, emeralds) |
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
- **Special Stage**: full 1:1 flow (white fades, maze physics, results screen
  with card elements, ring bonus tally, Chaos Emeralds, exit to next level).

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
└── debugmode.c/.h— debug object placement (from _incObj/DebugMode.asm)

disasm/           — original Sega disassembly + uncompressed assets (source of truth)
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

- Demo mode.
- Continue, Ending and Credits screens (state machines only).
- Zones other than Green Hill — data structures/tables are ported where the ASM
  requires them; asset pointers may be `NULL`.
- Software "Delay"-style VBlank features and the Z80 sound driver (the PC sound
  system replaces the Z80).

## License / disclaimer

This is **not** a SEGA product. *Sonic the Hedgehog* and related assets are the
property of SEGA. The disassembly (`disasm/`) is available for educational/
archival purposes; this project is a programming-exercise port of it.
