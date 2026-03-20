#ifndef __DEBUGFB_H__
#define __DEBUGFB_H__

#ifdef DEBUGFB_BASE
void debugfb_init(void);
void debugfb_putchar(char c);
void debugfb_teardown(void);
#else
static inline void debugfb_init(void) {}
static inline void debugfb_teardown(void) {}
#endif

#endif /* __DEBUGFB_H__ */
