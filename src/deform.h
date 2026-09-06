#ifndef DEFORM_H
#define DEFORM_H

/* Background layer deformation & camera scrolling.
 *
 * Faithful C ports of the Sonic 1 disassembly (REV01, FixBugs=0):
 *   - DeformLayers          (_inc/DeformLayers (REV01).asm)
 *   - ScrollHoriz/ScrollVertical (_inc/ScrollHoriz & ScrollVertical.asm)
 *   - DynamicLevelEvents    (_inc/DynamicLevelEvents.asm)
 *
 * Called once per frame, normally from the title/level loops.
 */
void DeformLayers(void);
void DynamicLevelEvents(void);

#endif /* DEFORM_H */