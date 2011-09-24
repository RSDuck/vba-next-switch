#ifndef GBACPU_H
#define GBACPU_H

#if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) \
|| defined (__x86_64__) || defined (__ia64__) || defined (__i386__)
# define INSN_REGPARM __attribute__((regparm(1)))
#else
# define INSN_REGPARM
//# define INSN_REGPARM __declspec(passinreg)
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
 
// Waitstates when accessing data
static inline uint32_t dataTicksAccess16(u32 address) // DATA 8/16bits NON SEQ
{
	int addr = (address>>24)&15;
	uint32_t value =  memoryWait[addr];

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

static inline uint32_t dataTicksAccess32(u32 address) // DATA 32bits NON SEQ
{
	int addr = (address>>24)&15;
	uint32_t value = memoryWait32[addr];

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

static inline uint32_t dataTicksAccessSeq16(u32 address)// DATA 8/16bits SEQ
{
	int addr = (address>>24)&15;
	uint32_t value = memoryWaitSeq[addr];

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

static inline uint32_t dataTicksAccessSeq32(u32 address)// DATA 32bits SEQ
{
	int addr = (address>>24)&15;
	uint32_t value =  memoryWaitSeq32[addr];

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
static int codeTicksAccess16(u32 address) // THUMB NON SEQ
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
	}
	busPrefetchCount = 0;
	return memoryWait[addr];
}

static int codeTicksAccess32(u32 address) // ARM NON SEQ
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
	}
	busPrefetchCount = 0;
	return memoryWait32[addr];
}

static uint32_t codeTicksAccessSeq16(u32 address) // THUMB SEQ
{
	int addr = (address>>24)&15;

	if ((addr>=0x08) && (addr<=0x0D))
	{
		if (busPrefetchCount&0x1)
		{
			busPrefetchCount = ((busPrefetchCount&0xFF)>>1) | (busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		else if (busPrefetchCount>0xFF)
		{
			busPrefetchCount=0;
			return memoryWait[addr];
		}
	}
	else
		busPrefetchCount = 0;
	return memoryWaitSeq[addr];
}

static uint32_t codeTicksAccessSeq32(u32 address) // ARM SEQ
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
		else if (busPrefetchCount>0xFF)
		{
			busPrefetchCount=0;
			return memoryWait32[addr];
		}
	}
	return memoryWaitSeq32[addr];
}


// Emulates the Cheat System (m) code
#ifdef USE_CHEATS
static void cpuMasterCodeCheck(void)
{
	if((mastercode) && (mastercode == armNextPC))
	{
		u32 joy = 0;
		joy = systemReadJoypad(-1);
		u32 ext = (joy >> 10);
		cpuTotalTicks += cheatsCheckKeys(P1^0x3FF, ext);
	}
}
#endif

#endif // GBACPU_H
