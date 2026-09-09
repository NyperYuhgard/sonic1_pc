#include "plc.h"
#include "types.h"
#include "constants.h"
#include "ram.h"
#include "data.h"
#include "decomp.h"
#include <string.h>

/* ===================================================================
   Pattern Load Cue (PLC) system
   Ported from _inc/Pattern Load Cues.asm (queue) and the AddPLC/RunPLC
   subroutines in sonic.asm.

   A PLC slot is 6 bytes: 4-byte Nemesis source pointer + 2-byte art
   tile destination (matching the ASM `plcm gfx,vram` = dc.l gfx,
   dc.w (vram)*tile_size).  The port's NemDecToVRAM() decompresses a
   full NEM in one call, so RunPLC() just processes one slot per frame.
   =================================================================== */

typedef struct {
    uint8_t **src;                      /* -> runtime-loaded asset pointer */
    uint16_t  dest;                     /* destination art tile number */
} plc_entry;

#define plc_decl(gfx, vram) { &(gfx), (uint16_t)(vram) }

/* PLC_Main (plcid_Main) — _inc/Pattern Load Cues.asm
   The ASM list also contains Nem_Lamp (lamppost) and Nem_Points (points
   from enemy), neither of which is ported yet. */
static const plc_entry plc_Main[] = {
    plc_decl(Nem_Hud,   ArtTile_HUD),
    plc_decl(Nem_Lives, ArtTile_Lives_Counter),
    plc_decl(Nem_Ring,  ArtTile_Ring),
};

/* PLC_Main2 (plcid_Main2) — _inc/Pattern Load Cues.asm lines 84-88 */
static const plc_entry plc_Main2[] = {
    plc_decl(Nem_Monitors, ArtTile_Monitor),
    plc_decl(Nem_Shield,   ArtTile_Shield),
    plc_decl(Nem_Stars,    ArtTile_Invincibility),
};

/* PLC_GHZ (plcid_GHZ) — _inc/Pattern Load Cues.asm lines 107-120 */
static const plc_entry plc_GHZ[] = {
    plc_decl(Nem_GHZ_1st, ArtTile_Level),
    plc_decl(Nem_GHZ_2nd, ArtTile_Level + 0x1CD),
    plc_decl(Nem_Stalk,   ArtTile_GHZ_Flower_Stalk),
    plc_decl(Nem_PplRock, ArtTile_GHZ_Purple_Rock),
    plc_decl(Nem_Crabmeat, ArtTile_Crabmeat),
    plc_decl(Nem_Buzz,    ArtTile_Buzz_Bomber),
    plc_decl(Nem_Chopper, ArtTile_Chopper),
    plc_decl(Nem_Newtron, ArtTile_Newtron),
    plc_decl(Nem_Motobug, ArtTile_Moto_Bug),
    plc_decl(Nem_Spikes,  ArtTile_Spikes),
    plc_decl(Nem_HSpring, ArtTile_Spring_Horizontal),
    plc_decl(Nem_VSpring, ArtTile_Spring_Vertical),
};

/* Static asset list sentinels (the value of a pointer is not a constant) */
static const plc_entry plc_Empty[] = { { 0, 0 } };

/* ------------------------------------------------------------------
   The ASM queue stores a 32-bit dc.l source address; host pointers are
   64-bit, so the RAM 6-byte slot can only hold the dest.  plc_src[]
   mirrors the RAM queue (same slot index) with the real pointers.
   A NULL entry means the slot is free, matching the ASM tst.l check.
   ------------------------------------------------------------------ */
static const uint8_t *plc_src[plc_slot_count];

/* ArtLoadCues index — must stay in the same order as plcid_* in
   constants.h (see _inc/Pattern Load Cues.asm lines 27-68).  Counts
   mirror the plcheader value ((size)/6)-1; -1 = no list. */
typedef struct {
    const plc_entry *entries;
    int              count;
} plc_list;

#define PLC_NONE { plc_Empty, -1 }

static const plc_list plc_index[] = {
    /* 0: */           { plc_Main,  (int)(sizeof(plc_Main) / sizeof(plc_Main[0])) - 1 },
    /* 1: */           { plc_Main2, (int)(sizeof(plc_Main2) / sizeof(plc_Main2[0])) - 1 },
    /* 2..3: */        PLC_NONE, PLC_NONE,
    /* 4: */           { plc_GHZ,   (int)(sizeof(plc_GHZ) / sizeof(plc_GHZ[0])) - 1 },
    /* 5..7: */        PLC_NONE, PLC_NONE, PLC_NONE,
    /* 8..11: */       PLC_NONE, PLC_NONE, PLC_NONE, PLC_NONE,
    /* 12..15: */      PLC_NONE, PLC_NONE, PLC_NONE, PLC_NONE,
    /* 16..19: */      PLC_NONE, PLC_NONE, PLC_NONE, PLC_NONE,
    /* 20..23: */      PLC_NONE, PLC_NONE, PLC_NONE, PLC_NONE,
    /* 24..27: */      PLC_NONE, PLC_NONE, PLC_NONE, PLC_NONE,
    /* 28..31: */      PLC_NONE, PLC_NONE, PLC_NONE, PLC_NONE,
};

/* Clear all pending PLC entries (from sonic.asm ClearPLC) */
void ClearPLC(void) {
    memset(RAM_ADDR(v_plc_buffer), 0, plc_slot_size * plc_slot_count);
    memset(plc_src, 0, sizeof(plc_src));
}

/* Add a PLC list to the queue (sonic.asm AddPLC)
   Copies the src/dest pairs of the list pointed to by `id` (from the
   ArtLoadCues index into the first free v_plc_buffer slot. */
void AddPLC(int id) {
    const plc_list *list;
    int i, slot;

    if (id < 0 || id >= (int)(sizeof(plc_index) / sizeof(plc_index[0]))) {
        return;
    }
    list = &plc_index[id];
    if (list->count < 0) {
        return;                          /* no list: bmi.s .return */
    }

    /* findspace: first free slot is signalled by a NULL source pointer */
    for (slot = 0; slot < plc_slot_count && plc_src[slot] != NULL; slot++) {
        /* advance to next slot */
    }
    if (slot >= plc_slot_count) {
        return;                          /* queue full */
    }

    /* copytoRAM: entries in list order (first entry first), matching the
       ASM (a1)+ copy loop; asset-failed (NULL) entries are skipped */
    for (i = 0; i <= list->count; i++) {
        const uint8_t *p = *list->entries[i].src;
        if (p == NULL) {
            continue;
        }
        if (slot >= plc_slot_count) {
            return;                      /* queue full */
        }
        plc_src[slot] = p;
        *(uint16_t *)(RAM_ADDR(v_plc_buffer) + slot * plc_slot_size + 4) =
            list->entries[i].dest;
        slot++;
    }
}

/* Start a new PLC (clear + add) */
void NewPLC(int id) {
    ClearPLC();
    AddPLC(id);
}

/* Process one pending PLC entry per frame (sonic.asm RunPLC)
   The ASM runs a fine-grained Nemesis state machine across VBlank
   (ProcessPLC_9Tiles); the port's NemDecToVRAM() is synchronous, so
   decompression happens here and the entry is dropped from the queue. */
void RunPLC(void) {
    uint8_t *slot;
    int i;

    if (plc_src[0] == NULL) {
        return;                          /* nothing queued */
    }

    slot = RAM_ADDR(v_plc_buffer);
    NemDecToVRAM(plc_src[0],
                 (uint32_t)*(uint16_t *)(slot + 4) * tile_size);

    /* drop the processed entry: shift the queue down one slot */
    for (i = 1; i < plc_slot_count; i++) {
        plc_src[i - 1] = plc_src[i];
    }
    plc_src[plc_slot_count - 1] = NULL;

    memmove(slot, slot + plc_slot_size,
            (size_t)(RAM_ADDR(v_plc_buffer_only_end) - slot) - plc_slot_size);
    memset(RAM_ADDR(v_plc_buffer_only_end) - plc_slot_size, 0, plc_slot_size);
}