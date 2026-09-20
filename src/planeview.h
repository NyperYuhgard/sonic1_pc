#ifndef SONIC1_PLANEVIEW_H
#define SONIC1_PLANEVIEW_H

/* Debug Plane A/B viewer: a second SDL window showing the FULL nametables
   of Plane A and Plane B decoded exactly like the renderer (same tile fetch,
   flips, palette_main and plane-height wrapping), so the user can check
   "what is placed where" independently of the final screen. A red rectangle
   marks the 320x224 region the screen currently samples.
   The layout adapts to the window size (side-by-side with a flexible gap,
   stacking vertically on narrow windows) and never stretches the content;
   when it does not fit, it scrolls (mouse wheel + left-drag).
   Call PlaneView_Toggle on key-down edge. */

void PlaneView_Toggle(void);
int  PlaneView_WindowID(void);
void PlaneView_Render(void);

/* Scroll/pan input, forwarded from input.c. Positive dx scrolls right,
   positive dy scrolls down (content moves the opposite way). */
void PlaneView_Scroll(int dx, int dy);
void PlaneView_DragStart(int mx, int my);
void PlaneView_DragMove(int mx, int my);
void PlaneView_DragEnd(void);

#endif /* SONIC1_PLANEVIEW_H */