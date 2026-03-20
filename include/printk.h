#ifndef __PRINTK_H__
#define __PRINTK_H__

#include <types.h>

#ifdef DEBUG

void print(const char *s);
void print_p(const void *p);
void print_u32(u32 v);
void print_u64(u64 v);
void hexdump(const void *memory, size_t size);

#else

static inline void print(const char *unused) { }
static inline void print_p(const void *unused) { }
static inline void print_u32(u64 unused) { }
static inline void print_u64(u64 unused) { }
static inline void hexdump(const void *unused, size_t unused2) { }

#endif

#endif
