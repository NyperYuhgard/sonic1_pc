#include "ram.h"

/* Definition of the 64KB RAM array declared as `extern` in ram.h.
   Zero-filled on startup, matching the game's RAM-clear behaviour. */
uint8_t ram[0x10000];
