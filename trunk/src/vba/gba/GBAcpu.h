#ifndef GBACPU_H
#define GBACPU_H

extern int armExecute();
extern int thumbExecute();

#if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) \
|| defined (__x86_64__) || defined (__ia64__) || defined (__i386__)
# define INSN_REGPARM __attribute__((regparm(1)))
#else
# define INSN_REGPARM
//# define INSN_REGPARM __declspec(passinreg)
#endif

#ifdef __GNUC__
# define LIKELY(x) __builtin_expect(!!(x),1)
# define UNLIKELY(x) __builtin_expect(!!(x),0)
#else
# define LIKELY(x) (x)
# define UNLIKELY(x) (x)
#endif

#define UPDATE_REG(address, value)\
  {\
    WRITE16LE(((u16 *)&ioMem[address]),value);\
  }\

#define ARM_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadMemoryQuick(armNextPC);\
    cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC+4);\
  }

#define THUMB_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadHalfWordQuick(armNextPC);\
    cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC+2);\
  }

#define ARM_PREFETCH_NEXT \
  cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC+4);

#define THUMB_PREFETCH_NEXT\
  cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC+2);
 
#ifdef USE_SWITICKS
extern int SWITicks;
#endif
extern u32 mastercode;
extern bool busPrefetch;
extern bool busPrefetchEnable;
extern u32 busPrefetchCount;
extern int cpuNextEvent;
extern bool holdState;
extern u32 cpuPrefetch[2];
extern int cpuTotalTicks;
extern u8 memoryWait[16];
extern u8 memoryWait32[16];
extern u8 memoryWaitSeq[16];
extern u8 memoryWaitSeq32[16];
extern u8 cpuBitsSet[256];
extern u8 cpuLowestBitSet[256];
extern void CPUSwitchMode(int mode, bool saveState, bool breakLoop);
extern void CPUSwitchMode(int mode, bool saveState);
extern void CPUUpdateCPSR();
extern void CPUUpdateFlags(bool breakLoop);
extern void CPUUpdateFlags();
//extern void CPUUndefinedException();
extern void CPUSoftwareInterrupt();
extern void CPUSoftwareInterrupt(int comment);


// Waitstates when accessing data
inline int dataTicksAccess16(u32 address) // DATA 8/16bits NON SEQ
{
  int addr = (address>>24)&15;
  int value =  memoryWait[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    waitState = (1 & ~waitState) | (waitState & waitState);
	busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1; 
  }

  return value;
}

inline int dataTicksAccess32(u32 address) // DATA 32bits NON SEQ
{
  int addr = (address>>24)&15;
  int value = memoryWait32[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    waitState = (1 & ~waitState) | (waitState & waitState);
    busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
  }

  return value;
}

inline int dataTicksAccessSeq16(u32 address)// DATA 8/16bits SEQ
{
  int addr = (address>>24)&15;
  int value = memoryWaitSeq[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    waitState = (1 & ~waitState) | (waitState & waitState);
    busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
  }

  return value;
}

inline int dataTicksAccessSeq32(u32 address)// DATA 32bits SEQ
{
  int addr = (address>>24)&15;
  int value =  memoryWaitSeq32[addr];

  if ((addr>=0x08) || (addr < 0x02))
  {
    busPrefetchCount=0;
    busPrefetch=false;
  }
  else if (busPrefetch)
  {
    int waitState = value;
    waitState = (1 & ~waitState) | (waitState & waitState);
    busPrefetchCount = ((busPrefetchCount+1)<<waitState) - 1;
  }

  return value;
}


// Waitstates when executing opcode
static int __attribute__((always_inline)) codeTicksAccess16(u32 address) // THUMB NON SEQ
{
  int addr = (address>>24)&15;

  if ((addr>=0x08) && (addr<=0x0D))
  {
    if (busPrefetchCount&0x1)
    {
      if (busPrefetchCount&0x2)
      {
        busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
        return 0;
      }
      busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
      return memoryWaitSeq[addr]-1;
    }
    else
    {
      busPrefetchCount=0;
      return memoryWait[addr];
    }
  }
  else
  {
    busPrefetchCount = 0;
    return memoryWait[addr];
  }
}

static int __attribute__((always_inline)) codeTicksAccess32(u32 address) // ARM NON SEQ
{
  int addr = (address>>24)&15;

  if ((addr>=0x08) && (addr<=0x0D))
  {
    if (busPrefetchCount&0x1)
    {
      if (busPrefetchCount&0x2)
      {
        busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
        return 0;
      }
      busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
      return memoryWaitSeq[addr] - 1;
    }
    else
    {
      busPrefetchCount = 0;
      return memoryWait32[addr];
    }
  }
  else
  {
    busPrefetchCount = 0;
    return memoryWait32[addr];
  }
}

static int __attribute__((always_inline)) codeTicksAccessSeq16(u32 address) // THUMB SEQ
{
  int addr = (address>>24)&15;

  if ((addr>=0x08) && (addr<=0x0D))
  {
    if (busPrefetchCount&0x1)
    {
      busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
      return 0;
    }
    else
    if (busPrefetchCount>0xFF)
    {
      busPrefetchCount=0;
      return memoryWait[addr];
    }
    else
      return memoryWaitSeq[addr];
  }
  else
  {
    busPrefetchCount = 0;
    return memoryWaitSeq[addr];
  }
}

static int __attribute__((always_inline)) codeTicksAccessSeq32(u32 address) // ARM SEQ
{
  int addr = (address>>24)&15;

  if ((addr>=0x08) && (addr<=0x0D))
  {
    if (busPrefetchCount&0x1)
    {
      if (busPrefetchCount&0x2)
      {
        busPrefetchCount = ((busPrefetchCount&0xFF)>>2) | (busPrefetchCount&0xFFFFFF00);
        return 0;
      }
      busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
      return memoryWaitSeq[addr];
    }
    else
    if (busPrefetchCount>0xFF)
    {
      busPrefetchCount=0;
      return memoryWait32[addr];
    }
    else
      return memoryWaitSeq32[addr];
  }
  else
  {
    return memoryWaitSeq32[addr];
  }
}


// Emulates the Cheat System (m) code
#ifdef USE_CHEATS
static void __attribute__((always_inline)) cpuMasterCodeCheck()
{
  if((mastercode) && (mastercode == armNextPC))
  {
    u32 joy = 0;
    //if(systemReadJoypads())
      joy = systemReadJoypad(-1);
    u32 ext = (joy >> 10);
    cpuTotalTicks += cheatsCheckKeys(P1^0x3FF, ext);
  }
}
#endif

#endif // GBACPU_H
