# VRAM Bank Switcher

> VRAM extension via independent 64 KB banks for the Sega Mega Drive / Genesis port.

## Index

- [Motivation](#motivation)
- [Concept](#concept)
- [Configuration](#configuration)
- [Why 64 KB banks instead of extended VRAM?](#why-64-kb-banks-instead-of-extended-vram)
- [Current status and compatibility](#current-status-and-compatibility)
- [Usage](#usage)
- [Important note: shared object slots](#important-note-shared-object-slots)
- [API reference](#api-reference)

---

## Motivation

Sega Mega Drive VRAM is limited to 64 KB, and this port faithfully replicates that restriction in order not to break the semantics of the original hardware (nametables, sprite table, tile layout, etc.).

However, replicating the limitation as-is would introduce an artificial restriction on modern development: there would be no room for additional art, HD overlays, or any content the original hardware never had to support.

The **VRAM Bank Switcher** solves this by allowing `N` independent 64 KB banks, each with the same structure as the original VRAM. The code explicitly chooses which bank to use when loading and when drawing, so the hardware limitation is not imposed on the development of new features.

## Concept

VRAM is organized into 64 KB banks, identical to the single block of the original hardware:

| Bank  | Usage                                                              |
| ----- | ------------------------------------------------------------------ |
| `0`   | "Classic" VRAM. All vanilla port code writes and reads here.      |
| `1..N-1` | Extended banks. Only accessible through the Bank Switcher.     |

**Bank 0** is transparent: any existing code path keeps working unchanged. Banks `1..N-1` require explicit intervention to load art and to tell objects where to read from.

Bank selection happens at two distinct moments:

- **When loading art** → the VRAM writer is told which bank to dump the tiles into.
- **When rendering** → the sprite is told which bank to read its tiles from.

If neither is specified, both fall back to bank `0` and the behavior is bit-for-bit identical to the original.

## Configuration

The total number of banks is defined in a single place:

```c
/* vdp.h */
#define VRAM_BANK_SIZE   0x10000   /* 64 KB per bank (identical to MD) */
#define VRAM_BANK_COUNT  4         /* <-- single configuration point    */
```

Changing `VRAM_BANK_COUNT` automatically reconfigures:

- The size of `vdp.vram_ext[]` (allocation of banks `1..N-1`).
- The number of tabs in the VRAM Viewer.
- The viewer layout (auto-shrink of tab width).
- Keyboard and click handling inside the viewer.

There is no other place where the number of banks needs to be touched.

## Why 64 KB banks instead of extended VRAM?

Extending VRAM linearly (e.g. to 1 MB as a single buffer) would have been tempting from a programmer's perspective, but it introduces several problems:

- **Unmanageable VRAM Viewer.** A grid of 32,768 tiles is not inspectable, and the viewer's scroll/zoom becomes a usability nightmare.
- **Harder modding.** Modders would have to remember global offsets; with banks, each "64 KB space" is a self-contained context addressable just like on the original hardware.
- **Risk of misaligning the Genesis structure.** Nametables, the sprite table, and the routines that write to the VDP assume a 17-bit address space (128 KB addressable, 64 KB usable). Extending linearly would break that convention.
- **Chaotic tile management.** Without a clear per-context boundary, art fragmentation becomes a real problem (which tile goes where? which range can be freed?).

64 KB banks preserve structural compatibility with the original hardware, keep tile administration scoped per context, and make the viewer's job trivial.

## Current status and compatibility

Currently, only Nemesis-compressed art is compatible with the Bank Switcher:

```c
void NemDecToVRAM(const uint8_t *source, uint32_t vram_addr);
```

`NemDecToVRAM` honors `g_vdp_write_bank`, so it is enough to change it before the call to redirect the destination.

Planned extensions to support:

- Uncompressed art.
- Other compression formats.

The current priority is Nemesis because it is the format the object engine uses overwhelmingly. The whole point of the Bank Switcher is precisely that: allowing objects to load their art into dedicated spaces without colliding with the rest of the level.

## Usage

### 1. Loading art into a specific bank

```c
VDP_SetWriteBank(1);                  /* Redirect writes to bank 1 */
if (Nem_TitleSonic) {
    NemDecToVRAM(Nem_TitleSonic, ArtTile_Title_Sonic * tile_size);
}
VDP_SetWriteBank(0);                  /* Back to bank 0 (MD-visible) */
```

> **Always restore `VDP_SetWriteBank(0)` after loading.**
> If forgotten, the next call to `AddPLC` or any other writer will end up writing to the wrong bank.

### 2. Telling the object which bank to read from

In the object's `init`, alongside the `obGfx` assignment:

```c
obGfx(o) = (uint16_t)(ArtTile_Title_Sonic | Tile_Pal2);
obSetGfxBank(o, 1);                   /* Read tiles from bank 1 */
```

From that point on, `BuildSprites` will emit each sprite piece with bank `1` attached, and `VDP_RenderFrame` will read that bank's tiles instead of bank `0`.

## Important note: shared object slots

VRAM banks are assigned **per object slot**, not per object identity.

Sonic 1 aggressively reuses `ObjRAM` slots between game modes and between distinct objects. Common examples:

| Slot | Objects that share it                       |
| ---- | ------------------------------------------- |
| `1`  | `TitleSonic` (title screen) ↔ `HUD` (in-game) |
| `2`  | `TitleCard` ↔ `GameOverText`                |
| …    | …                                           |

**Consequence:** if an object with bank ≠ `0` leaves its slot without resetting, the next object inheriting that slot also inherits the bank, and will read its tiles from the wrong bank. The typical symptom is an object that "disappears" (it is actually reading art that does not belong to it).

### How to avoid it

**Option A — automatic reset (implemented, recommended).**

`ExecuteObjects` detects when a slot's `obID` changes compared to the previous frame and resets its bank to `0` before executing the new routine. This covers all normal slot reuse cases via spawn/delete.

**Option B — explicit per-object reset (defensive).**

If an object can inherit a slot without `obID` changing (rare, but possible when `FindFreeObj` reuses a manually cleared slot), it is advisable to force the bank in its own `init`:

```c
if (routine == 0) {
    obRoutine(o) = 2;
    obSetGfxBank(o, 0);            /* Force bank 0 regardless of inherited state */
    ...
}
```

> **Rule of thumb:** always specify the bank in the object's `init`. If you do not specify it, the system falls back to bank `0` — but only if nobody left an inherited bank on that slot. **Do not trust the default.**

## API reference

### Write bank control

```c
void VDP_SetWriteBank(int bank);   /* Changes the destination bank for public writers */
int  VDP_GetWriteBank(void);       /* Queries the current bank */
```

### Direct per-bank access

```c
uint8_t       *VDP_VRAM_BankW(int bank);   /* Writable pointer to the bank */
const uint8_t *VDP_VRAM_Bank (int bank);   /* Read-only pointer */
```

### Explicit-bank writers (ignore `g_vdp_write_bank`)

```c
void VDP_WriteVRAM_Bank       (int bank, const uint8_t *src,
                               uint32_t addr, uint32_t len);
void VDP_FillVRAM_Bank        (int bank, uint8_t byte,
                               uint32_t addr, uint32_t len);
void VDP_CopyTilemapToVRAM_Bank(int bank, const uint16_t *src,
                               uint32_t dest, int width, int height);
```

### Per-object bank assignment

```c
void    obSetGfxBank(void *obj, uint8_t bank);  /* 0..3 */
uint8_t obGetGfxBank(void *obj);                /* Queries the current bank */
```

Called once after setting `obGfx(o)` in the object's `init`. The bank persists per slot until `DeleteObject` or the automatic reset in `ExecuteObjects` clears it.
