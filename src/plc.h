#ifndef SONIC1_PLC_H
#define SONIC1_PLC_H

#include "types.h"

/* Pattern Load Cue (PLC) system.
 *
 * The original game queues graphics decompression jobs via a small buffer
 * (v_plc_buffer, 16 slots). RunPLC() processes one slot per frame,
 * decompressing Nemesis-compressed art to VRAM. */

/* Clear all pending PLC entries */
void ClearPLC(void);

/* Add a PLC entry to the queue (from ArtLoadCues table) */
void AddPLC(int id);

/* Start a new PLC (clear + add) */
void NewPLC(int id);

/* Non-zero when the PLC queue is empty (ASM: tst.l (v_plc_buffer).w) */
int PLC_IsEmpty(void);

/* Process one pending PLC entry per frame */
void RunPLC(void);

#endif /* SONIC1_PLC_H */
