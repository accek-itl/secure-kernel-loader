#ifndef __IDT_H__
#define __IDT_H__

#ifdef DEBUG
void setup_idt(void);
#else
static inline void setup_idt(void) {}
#endif

#endif /* __IDT_H__ */
