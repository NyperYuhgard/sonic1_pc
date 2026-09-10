#ifndef SONIC1_OBJVIEW_H
#define SONIC1_OBJVIEW_H

/* Debug Object RAM viewer: toggle a second SDL window that lists every
   slot in v_objspace with live fields (ID, routine, X/Y, velocity, etc).
   Call ObjView_Toggle on key-down edge. */

void ObjView_Toggle(void);
int  ObjView_WindowID(void);
void ObjView_Render(void);

#endif /* SONIC1_OBJVIEW_H */
