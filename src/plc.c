#include "plc.h"
#include "types.h"
#include "constants.h"
#include "ram.h"
#include <string.h>

/* ===================================================================
   Pattern Load Cue (PLC) system — stubs
   Ported from _inc/Pattern Load Cue Index.asm and PLC runner
   =================================================================== */

/* Clear all pending PLC entries (from sonic.asm ClearPLC) */
void ClearPLC(void) {
    memset(RAM_ADDR(v_plc_buffer), 0, 0x20);
}

/* Add a PLC entry to the queue (stub)
   In the ASM, this reads from the ArtLoadCues table indexed by `id` and
   writes a 4-byte header + source pointer into v_plc_buffer. */
void AddPLC(int id) {
    /* TODO: read ArtLoadCues[id] and enqueue into v_plc_buffer */
    (void)id;
}

/* Start a new PLC (clear + add) */
void NewPLC(int id) {
    ClearPLC();
    AddPLC(id);
}

/* Process one pending PLC entry per frame (stub)
   In the ASM, this reads the next entry from v_plc_buffer, decompresses
   the Nemesis-compressed data, and writes it to VRAM. */
void RunPLC(void) {
    /* TODO: read v_plc_buffer head, call NemDecToVRAM, advance pointer */
}
