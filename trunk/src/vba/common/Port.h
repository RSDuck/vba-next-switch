#ifndef PORT_H
#define PORT_H

#include "Types.h"

// swaps a 16-bit value
#if 0
static inline u16 swap16(u16 v)
{
  return (v<<8)|(v>>8);
}
#endif

// swaps a 32-bit value
#if 0
static inline u32 swap32(u32 v)
{
  return (v<<24)|((v<<8)&0xff0000)|((v>>8)&0xff00)|(v>>24);
}
#endif

// if a >= 0 return x else y
static __inline int isel( int a, int x, int y )
{
	int mask = (a >> 31); // arithmetic shift right, splat out the sign bit
	// mask is 0xFFFFFFFF if (a < 0) and 0x00 otherwise.
	return (x & (~mask)) + (y & mask);
}


#ifdef WORDS_BIGENDIAN
#if defined(__GNUC__) && defined(__ppc__)

#define READ16LE(base) \
  ({ unsigned short lhbrxResult;       \
     __asm__ ("lhbrx %0, 0, %1" : "=r" (lhbrxResult) : "r" (base) : "memory"); \
      lhbrxResult; })

#define READ32LE(base) \
  ({ unsigned long lwbrxResult; \
     __asm__ ("lwbrx %0, 0, %1" : "=r" (lwbrxResult) : "r" (base) : "memory"); \
      lwbrxResult; })

#define WRITE16LE(base, value) \
  __asm__ ("sthbrx %0, 0, %1" : : "r" (value), "r" (base) : "memory")

#define WRITE32LE(base, value) \
  __asm__ ("stwbrx %0, 0, %1" : : "r" (value), "r" (base) : "memory")

#else
#define READ16LE(x) \
  (*((u16 *)(x))<<8)|(*((u16 *)(x))>>8);
#define READ32LE(x) \
  (*((u32 *)(x))<<24)|((*((u32 *)(x))<<8)&0xff0000)|((((*((u32 *)(x))x>>8)&0xff00)|(*((u32 *)(x))>>24);
#define WRITE16LE(x,v) \
  *((u16 *)x) = (*((u16 *)(v))<<8)|(*((u16 *)(v))>>8);
#define WRITE32LE(x,v) \
  *((u32 *)x) = (v<<24)|((v<<8)&0xff0000)|((v>>8)&0xff00)|(v>>24);
#endif
#else
#define READ16LE(x) \
  *((u16 *)x)
#define READ32LE(x) \
  *((u32 *)x)
#define WRITE16LE(x,v) \
  *((u16 *)x) = (v)
#define WRITE32LE(x,v) \
  *((u32 *)x) = (v)
#endif

#endif // PORT_H
