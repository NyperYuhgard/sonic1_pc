#ifndef SONIC1_HUD_H
#define SONIC1_HUD_H

/* ===========================================================================
   HUD (SCORE / TIME / RINGS / lives) digit patterns.

   Ported from _inc/HUD Update.asm. Two entry points are used:
     - Hud_Base:  one-shot level-entry setup (decompress HUD art + write the
                  static "E______0", "0:00", "__0" and lives digits).
     - HUD_Update: called every Level VBlank, refreshes whichever counters
                  have their update flag set (f_scorecount, f_ringcount,
                  f_timecount, f_lifecount, f_endactbonus).
   =========================================================================== */

void Hud_Base(void);   /* graphics + static digits */
void HUD_Update(void); /* per-frame digit refresh (VBlank) */

#endif /* SONIC1_HUD_H */