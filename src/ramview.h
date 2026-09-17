#ifndef SONIC1_RAMVIEW_H
#define SONIC1_RAMVIEW_H

/* Debug RAM viewer: second SDL window listing named RAM variables with
   live hex + decimal values. Call RamView_Toggle on key-down edge. */
void RamView_Toggle(void);
int  RamView_WindowID(void);
void RamView_Render(void);

#endif /* SONIC1_RAMVIEW_H */