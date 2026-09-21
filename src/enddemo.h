#ifndef SONIC1_ENDDEMO_H
#define SONIC1_ENDDEMO_H

/* End Demo screen entry point — only used in demo builds */
void GM_EndDemo_Screen(void);

/* External symbols from main.c */
extern int running;
extern void WaitForVBlank(void);

#endif /* SONIC1_ENDDEMO_H */