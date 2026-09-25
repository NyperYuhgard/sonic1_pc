#include "tas.h"
#include "ram.h"
#include "vdp.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAS_MAX_FRAMES  (60u * 60u * 60u)   /* 1 hora @ 60Hz */
#define TAS_STATE_SLOTS 8u

typedef struct {
    int      valid;
    uint32_t frame;
    TasMode  mode;
    uint8_t *ram;
    uint8_t *vram;
    uint8_t *cram;
    uint8_t *vsram;
    uint16_t vdp_regs[24];
    uint16_t vdp_status;
    uint32_t vdp_cmd;
    uint8_t  prev_state[2];
} TasSlot;

static struct {
    TasMode  mode;
    uint32_t frame;
    uint32_t length;
    uint8_t *p1;
    uint8_t *p2;
    int      paused;
    int      step_pending;
    int      next_save_slot;
    TasSlot  slots[TAS_STATE_SLOTS];
} tas;

static TasSlot *slot_ptr(int i) {
    return (i >= 0 && i < (int)TAS_STATE_SLOTS) ? &tas.slots[i] : NULL;
}

/* ------------------------------------------------------------------ */

void TAS_Init(void) {
    memset(&tas, 0, sizeof(tas));
    tas.p1 = (uint8_t *)calloc(TAS_MAX_FRAMES, 1);
    tas.p2 = (uint8_t *)calloc(TAS_MAX_FRAMES, 1);
    if (!tas.p1 || !tas.p2) {
        fprintf(stderr, "[TAS] allocation failed\n");
        exit(1);
    }
    tas.mode = TAS_MODE_LIVE;
}

TasMode  TAS_GetMode(void)   { return tas.mode; }
uint32_t TAS_GetFrame(void)  { return tas.frame; }
uint32_t TAS_GetLength(void) { return tas.length; }

uint8_t TAS_OverridePlayer1(uint8_t physical) {
    if (tas.mode == TAS_MODE_PLAYBACK && tas.frame < tas.length)
        return tas.p1[tas.frame];
    return physical;
}
uint8_t TAS_OverridePlayer2(uint8_t physical) {
    if (tas.mode == TAS_MODE_PLAYBACK && tas.frame < tas.length)
        return tas.p2[tas.frame];
    return physical;
}

/* ------------------------------------------------------------------ */

void TAS_EndFrame(void) {
    if (tas.mode == TAS_MODE_RECORD && tas.frame < TAS_MAX_FRAMES) {
        tas.p1[tas.frame] = joypad_hold[0];
        tas.p2[tas.frame] = joypad_hold[1];
        tas.length = tas.frame + 1;
    }
    tas.frame++;
    if (tas.mode == TAS_MODE_PLAYBACK && tas.frame >= tas.length) {
        fprintf(stderr, "[TAS] Playback ended at frame %u\n", tas.frame);
        tas.mode = TAS_MODE_LIVE;
    }
}

/* ------------------------------------------------------------------ */

void TAS_SetPaused(int p) {
    if (p && !tas.paused) {
        tas.paused = 1;
        fprintf(stderr, "[TAS] Paused at frame %u\n", tas.frame);
    } else if (!p && tas.paused) {
        tas.paused = 0;
        tas.step_pending = 0;
        fprintf(stderr, "[TAS] Resumed at frame %u\n", tas.frame);
    }
}
int TAS_IsPaused(void) { return tas.paused; }

int TAS_TryConsumeStep(void) {
    if (tas.step_pending) { tas.step_pending = 0; return 1; }
    return 0;
}
void TAS_RequestStep(void) { if (tas.paused) tas.step_pending = 1; }

/* ------------------------------------------------------------------ */

void TAS_ToggleRecord(void) {
    switch (tas.mode) {
        case TAS_MODE_LIVE:
            tas.mode   = TAS_MODE_RECORD;
            tas.frame  = 0;
            tas.length = 0;
            fprintf(stderr, "[TAS] Recording from frame 0\n");
            break;
        case TAS_MODE_RECORD:
            tas.mode = TAS_MODE_LIVE;
            fprintf(stderr, "[TAS] Recording stopped at frame %u (%u frames)\n",
                    tas.frame, tas.length);
            break;
        case TAS_MODE_PLAYBACK:
            tas.mode = TAS_MODE_LIVE;
            fprintf(stderr, "[TAS] Playback cancelled, now LIVE\n");
            break;
    }
}

void TAS_ReplayFromStart(void) {
    if (tas.length == 0) { fprintf(stderr, "[TAS] Nothing to replay\n"); return; }
    tas.frame = 0;
    tas.mode  = TAS_MODE_PLAYBACK;
    fprintf(stderr, "[TAS] Replaying %u frames from start\n", tas.length);
}

/* ------------------------------------------------------------------ */
/* File I/O                                                            */
/* ------------------------------------------------------------------ */

int TAS_SaveFile(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    uint8_t hdr[16];
    memcpy(hdr, "TASFILE\0", 8);
    hdr[8]  = 1;
    hdr[9]  = (v_megadrive >= 0) ? 1 : 0;
    hdr[10] = 0; hdr[11] = 0;
    hdr[12] = (uint8_t)( tas.length        & 0xFF);
    hdr[13] = (uint8_t)((tas.length >>  8) & 0xFF);
    hdr[14] = (uint8_t)((tas.length >> 16) & 0xFF);
    hdr[15] = (uint8_t)((tas.length >> 24) & 0xFF);
    fwrite(hdr, 1, 16, f);
    for (uint32_t i = 0; i < tas.length; i++) {
        fputc(tas.p1[i], f);
        fputc(tas.p2[i], f);
    }
    fclose(f);
    return 1;
}

int TAS_LoadFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t hdr[16];
    if (fread(hdr, 1, 16, f) != 16 ||
        memcmp(hdr, "TASFILE\0", 8) != 0 || hdr[8] != 1) {
        fclose(f); return 0;
        }
        uint32_t len = (uint32_t)hdr[12]
        | ((uint32_t)hdr[13] <<  8)
        | ((uint32_t)hdr[14] << 16)
        | ((uint32_t)hdr[15] << 24);
    if (len > TAS_MAX_FRAMES) { fclose(f); return 0; }
    for (uint32_t i = 0; i < len; i++) {
        int c1 = fgetc(f), c2 = fgetc(f);
        if (c1 == EOF || c2 == EOF) { fclose(f); return 0; }
        tas.p1[i] = (uint8_t)c1;
        tas.p2[i] = (uint8_t)c2;
    }
    fclose(f);
    tas.length = len;
    tas.frame  = 0;
    tas.mode   = TAS_MODE_PLAYBACK;
    return 1;
}

void TAS_SaveFile_Action(void) {
    if (tas.length == 0) { fprintf(stderr, "[TAS] Nothing to save\n"); return; }
    if (TAS_SaveFile("tas.bin"))
        fprintf(stderr, "[TAS] Saved %u frames to tas.bin\n", tas.length);
    else
        fprintf(stderr, "[TAS] Failed to write tas.bin\n");
}

void TAS_LoadFile_Action(void) {
    if (TAS_LoadFile("tas.bin"))
        fprintf(stderr, "[TAS] Loaded %u frames from tas.bin, replaying\n", tas.length);
    else
        fprintf(stderr, "[TAS] Failed to read tas.bin\n");
}

/* ------------------------------------------------------------------ */
/* Savestates                                                          */
/* ------------------------------------------------------------------ */

int TAS_SaveState(int slot) {
    TasSlot *s = slot_ptr(slot);
    if (!s) return 0;

    if (!s->ram)   s->ram   = malloc(0x10000);
    if (!s->vram)  s->vram  = malloc(VRAM_SIZE);
    if (!s->cram)  s->cram  = malloc(CRAM_SIZE * sizeof(uint16_t));
    if (!s->vsram) s->vsram = malloc(VSRAM_SIZE * sizeof(uint16_t));
    if (!s->ram || !s->vram || !s->cram || !s->vsram) return 0;

    memcpy(s->ram,   ram,       0x10000);
    memcpy(s->vram,  vdp.vram,  VRAM_SIZE);
    memcpy(s->cram,  vdp.cram,  CRAM_SIZE * sizeof(uint16_t));
    memcpy(s->vsram, vdp.vsram, VSRAM_SIZE * sizeof(uint16_t));
    memcpy(s->vdp_regs, vdp.registers, sizeof(s->vdp_regs));
    s->vdp_status = vdp.status;
    s->vdp_cmd    = vdp.vdp_cmd;

    Input_GetPrevState(s->prev_state);
    s->frame = tas.frame;
    s->mode  = tas.mode;
    s->valid = 1;
    return 1;
}

int TAS_LoadState(int slot) {
    TasSlot *s = slot_ptr(slot);
    if (!s || !s->valid) return 0;

    memcpy(ram,          s->ram,   0x10000);
    memcpy(vdp.vram,     s->vram,  VRAM_SIZE);
    memcpy(vdp.cram,     s->cram,  CRAM_SIZE * sizeof(uint16_t));
    memcpy(vdp.vsram,    s->vsram, VSRAM_SIZE * sizeof(uint16_t));
    memcpy(vdp.registers,s->vdp_regs, sizeof(s->vdp_regs));
    vdp.status  = s->vdp_status;
    vdp.vdp_cmd = s->vdp_cmd;

    Input_SetPrevState(s->prev_state);
    tas.frame = s->frame;
    tas.mode  = s->mode;
    return 1;
}

void TAS_SaveNextState(void) {
    int slot = tas.next_save_slot % TAS_STATE_SLOTS;
    if (TAS_SaveState(slot)) {
        fprintf(stderr, "[TAS] State saved to slot %d (frame %u)\n",
                slot, tas.frame);
        tas.next_save_slot = (slot + 1) % TAS_STATE_SLOTS;
    }
}

void TAS_LoadPrevState(void) {
    int slot = (tas.next_save_slot + TAS_STATE_SLOTS - 1) % TAS_STATE_SLOTS;
    uint32_t was = tas.frame;
    if (TAS_LoadState(slot))
        fprintf(stderr, "[TAS] State loaded from slot %d (frame %u -> %u)\n",
                slot, was, tas.frame);
        else
            fprintf(stderr, "[TAS] Slot %d empty\n", slot);
}

/* ------------------------------------------------------------------ */
/* Acceso directo al buffer (para el editor)                          */
/* ------------------------------------------------------------------ */

uint8_t TAS_GetInputP1(uint32_t frame) {
    if (frame >= tas.length) return 0;
    return tas.p1[frame];
}
uint8_t TAS_GetInputP2(uint32_t frame) {
    if (frame >= tas.length) return 0;
    return tas.p2[frame];
}
void TAS_SetInputP1(uint32_t frame, uint8_t value) {
    if (frame >= tas.length) return;
    tas.p1[frame] = value;
}
void TAS_SetInputP2(uint32_t frame, uint8_t value) {
    if (frame >= tas.length) return;
    tas.p2[frame] = value;
}
void TAS_SetLength(uint32_t length) {
    if (length > TAS_MAX_FRAMES) length = TAS_MAX_FRAMES;
    if (length < tas.length) {
        /* truncar */
    }
    tas.length = length;
}

void TAS_DeleteFrame(uint32_t at) {
    if (at >= tas.length) return;
    uint32_t after = tas.length - at - 1;
    if (after > 0) {
        memmove(&tas.p1[at], &tas.p1[at + 1], after);
        memmove(&tas.p2[at], &tas.p2[at + 1], after);
    }
    tas.length--;
}

void TAS_InsertFrame(uint32_t at) {
    if (tas.length >= TAS_MAX_FRAMES) return;
    if (at > tas.length) at = tas.length;
    uint32_t after = tas.length - at;
    if (after > 0) {
        memmove(&tas.p1[at + 1], &tas.p1[at], after);
        memmove(&tas.p2[at + 1], &tas.p2[at], after);
    }
    tas.p1[at] = 0;
    tas.p2[at] = 0;
    tas.length++;
}

void TAS_DeleteRange(uint32_t start, uint32_t end) {
    if (start > end) { uint32_t t = start; start = end; end = t; }
    if (start >= tas.length) return;
    if (end >= tas.length) end = tas.length - 1;
    uint32_t count = end - start + 1;
    uint32_t after = tas.length - end - 1;
    if (after > 0) {
        memmove(&tas.p1[start], &tas.p1[end + 1], after);
        memmove(&tas.p2[start], &tas.p2[end + 1], after);
    }
    tas.length -= count;
}