#ifndef GBAINLINE_H
#define GBAINLINE_H

#include "RTC.h"
#include "Sound.h"

#define UPDATE_REG(address, value)	WRITE16LE(((u16 *)&ioMem[address]),value);
#define ARM_PREFETCH_NEXT		cpuPrefetch[1] = CPUReadMemoryQuick(bus.armNextPC+4);
#define THUMB_PREFETCH_NEXT		cpuPrefetch[1] = CPUReadHalfWordQuick(bus.armNextPC+2);

#define ARM_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadMemoryQuick(bus.armNextPC);\
    cpuPrefetch[1] = CPUReadMemoryQuick(bus.armNextPC+4);\
  }

#define THUMB_PREFETCH \
  {\
    cpuPrefetch[0] = CPUReadHalfWordQuick(bus.armNextPC);\
    cpuPrefetch[1] = CPUReadHalfWordQuick(bus.armNextPC+2);\
  }
 
#ifdef USE_SWITICKS
extern int SWITicks;
#endif
extern u32 mastercode;
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
extern void CPUUpdateFlags(bool breakLoop);
extern void CPUUpdateFlags();
extern void CPUSoftwareInterrupt();
extern void CPUSoftwareInterrupt(int comment);


// Waitstates when accessing data
INLINE int dataTicksAccess(u32 address, u8 bit32) // DATA 8/16bits NON SEQ
{
	int addr, value, waitState;

	addr = (address>>24)&15;

	if(bit32)	/* DATA 32bits NON SEQ */
		value = memoryWait32[addr];
	else		/* DATA 8/16bits NON SEQ */
		value =  memoryWait[addr];

	if ((addr>=0x08) || (addr < 0x02))
	{
		bus.busPrefetchCount=0;
		bus.busPrefetch=false;
	}
	else if (bus.busPrefetch)
	{
		waitState = value;
		waitState = (1 & ~waitState) | (waitState & waitState);
		bus.busPrefetchCount = ((bus.busPrefetchCount+1)<<waitState) - 1; 
	}

	return value;
}

INLINE int dataTicksAccessSeq(u32 address, u8 bit32)// DATA 8/16bits SEQ
{
	int addr, value, waitState;

	addr = (address>>24)&15;

	if (bit32)		/* DATA 32bits SEQ */
		value = memoryWaitSeq32[addr];
	else			/* DATA 8/16bits SEQ */
		value = memoryWaitSeq[addr];

	if ((addr>=0x08) || (addr < 0x02))
	{
		bus.busPrefetchCount=0;
		bus.busPrefetch=false;
	}
	else if (bus.busPrefetch)
	{
		waitState = value;
		waitState = (1 & ~waitState) | (waitState & waitState);
		bus.busPrefetchCount = ((bus.busPrefetchCount+1)<<waitState) - 1;
	}

	return value;
}

// Waitstates when executing opcode
static INLINE int codeTicksAccess(u32 address, u8 bit32) // THUMB NON SEQ
{
	int addr, ret;

	addr = (address>>24) & 15;

	if (unsigned(addr - 0x08) <= 5)
	{
		if (bus.busPrefetchCount&0x1)
		{
			if (bus.busPrefetchCount&0x2)
			{
				bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>2) | (bus.busPrefetchCount&0xFFFFFF00);
				return 0;
			}
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
			return memoryWaitSeq[addr]-1;
		}
	}
	bus.busPrefetchCount = 0;

	if(bit32)		/* ARM NON SEQ */
		ret = memoryWait32[addr];
	else			/* THUMB NON SEQ */
		ret = memoryWait[addr];

	return ret;
}

static INLINE int codeTicksAccessSeq16(u32 address) // THUMB SEQ
{
	int addr = (address>>24) & 15;

	if (unsigned(addr - 0x08) <= 5)
	{
		if (bus.busPrefetchCount&0x1)
		{
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
			return 0;
		}
		else if (bus.busPrefetchCount>0xFF)
		{
			bus.busPrefetchCount=0;
			return memoryWait[addr];
		}
	}
	else
		bus.busPrefetchCount = 0;

	return memoryWaitSeq[addr];
}

static INLINE int codeTicksAccessSeq32(u32 address) // ARM SEQ
{
	int addr = (address>>24)&15;

	if (unsigned(addr - 0x08) <= 5)
	{
		if (bus.busPrefetchCount&0x1)
		{
			if (bus.busPrefetchCount&0x2)
			{
				bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>2) | (bus.busPrefetchCount&0xFFFFFF00);
				return 0;
			}
			bus.busPrefetchCount = ((bus.busPrefetchCount&0xFF)>>1) | (bus.busPrefetchCount&0xFFFFFF00);
			return memoryWaitSeq[addr];
		}
		else if (bus.busPrefetchCount > 0xFF)
		{
			bus.busPrefetchCount=0;
			return memoryWait32[addr];
		}
	}
	return memoryWaitSeq32[addr];
}

#define CPUReadByteQuick(addr)		map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]
#define CPUReadHalfWordQuick(addr)	READ16LE(((u16*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))
#define CPUReadMemoryQuick(addr)	READ32LE(((u32*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))

extern const u32 objTilesAddress[3];

extern bool stopState;
extern bool holdState;
extern int holdType;
extern int cpuNextEvent;
extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
#ifdef USE_MOTION_SENSOR
extern bool cpuEEPROMSensorEnabled;
#endif
extern bool timer0On;
extern int timer0Ticks;
extern int timer0ClockReload;
extern bool timer1On;
extern int timer1Ticks;
extern int timer1ClockReload;
extern bool timer2On;
extern int timer2Ticks;
extern int timer2ClockReload;
extern bool timer3On;
extern int timer3Ticks;
extern int timer3ClockReload;
extern int cpuTotalTicks;

static INLINE u32 CPUReadMemory(u32 address)
{
	u32 value;
	switch(address >> 24)
	{
		case 0:
			if(bus.reg[15].I >> 24)
			{
				if(address < 0x4000)
					value = READ32LE(((u32 *)&biosProtected));
				else goto unreadable;
			}
			else
				value = READ32LE(((u32 *)&bios[address & 0x3FFC]));
			break;
		case 2:
			value = READ32LE(((u32 *)&workRAM[address & 0x3FFFC]));
			break;
		case 3:
			value = READ32LE(((u32 *)&internalRAM[address & 0x7ffC]));
			break;
		case 4:
			if((address < 0x4000400) && ioReadable[address & 0x3fc])
			{
				if(ioReadable[(address & 0x3fc) + 2])
					value = READ32LE(((u32 *)&ioMem[address & 0x3fC]));
				else
					value = READ16LE(((u16 *)&ioMem[address & 0x3fc]));
			}
			else
				goto unreadable;
			break;
		case 5:
			value = READ32LE(((u32 *)&graphics.paletteRAM[address & 0x3fC]));
			break;
		case 6:
			address = (address & 0x1fffc);
			if (((graphics.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			{
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			value = READ32LE(((u32 *)&vram[address]));
			break;
		case 7:
			value = READ32LE(((u32 *)&oam[address & 0x3FC]));
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			value = READ32LE(((u32 *)&rom[address&0x1FFFFFC]));
			break;
		case 13:
			if(cpuEEPROMEnabled)
				return eepromRead();	// no need to swap this
			goto unreadable;
		case 14:
			if(cpuFlashEnabled | cpuSramEnabled)
				return flashRead(address);	// no need to swap this
		default:
unreadable:

			if(armState)
				value = CPUReadMemoryQuick(bus.reg[15].I);
			else
			{
				value = CPUReadHalfWordQuick(bus.reg[15].I) |
					CPUReadHalfWordQuick(bus.reg[15].I) << 16;
			}
	}

	if(address & 3) {
		int shift = (address & 3) << 3;
		value = (value >> shift) | (value << (32 - shift));
	}
	return value;
}


static INLINE u32 CPUReadHalfWord(u32 address)
{
	u32 value;

	switch(address >> 24)
	{
		case 0:
			if (bus.reg[15].I >> 24)
			{
				if(address < 0x4000)
					value = READ16LE(((u16 *)&biosProtected[address&2]));
				else
					goto unreadable;
			}
			else
				value = READ16LE(((u16 *)&bios[address & 0x3FFE]));
			break;
		case 2:
			value = READ16LE(((u16 *)&workRAM[address & 0x3FFFE]));
			break;
		case 3:
			value = READ16LE(((u16 *)&internalRAM[address & 0x7ffe]));
			break;
		case 4:
			if((address < 0x4000400) && ioReadable[address & 0x3fe])
			{
				value =  READ16LE(((u16 *)&ioMem[address & 0x3fe]));
				if (((address & 0x3fe)>0xFF) && ((address & 0x3fe)<0x10E))
				{
					if (((address & 0x3fe) == 0x100) && timer0On)
						value = 0xFFFF - ((timer0Ticks-cpuTotalTicks) >> timer0ClockReload);
					else
						if (((address & 0x3fe) == 0x104) && timer1On && !(TM1CNT & 4))
							value = 0xFFFF - ((timer1Ticks-cpuTotalTicks) >> timer1ClockReload);
						else
							if (((address & 0x3fe) == 0x108) && timer2On && !(TM2CNT & 4))
								value = 0xFFFF - ((timer2Ticks-cpuTotalTicks) >> timer2ClockReload);
							else
								if (((address & 0x3fe) == 0x10C) && timer3On && !(TM3CNT & 4))
									value = 0xFFFF - ((timer3Ticks-cpuTotalTicks) >> timer3ClockReload);
				}
			}
			else goto unreadable;
			break;
		case 5:
			value = READ16LE(((u16 *)&graphics.paletteRAM[address & 0x3fe]));
			break;
		case 6:
			address = (address & 0x1fffe);
			if (((graphics.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			{
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			value = READ16LE(((u16 *)&vram[address]));
			break;
		case 7:
			value = READ16LE(((u16 *)&oam[address & 0x3fe]));
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
				value = rtcRead(address);
			else
				value = READ16LE(((u16 *)&rom[address & 0x1FFFFFE]));
			break;
		case 13:
			if(cpuEEPROMEnabled)
				return  eepromRead();	// no need to swap this
			goto unreadable;
		case 14:
			if(cpuFlashEnabled | cpuSramEnabled)
				// no need to swap this
				return flashRead(address);
			// default
		default:
unreadable:
			{
				int param = bus.reg[15].I;
				if(armState)
					param += (address & 2);
				value = CPUReadHalfWordQuick(param);
			}
			break;
	}

	if(address & 1)
		value = (value >> 8) | (value << 24);

	return value;
}

static INLINE u16 CPUReadHalfWordSigned(u32 address)
{
	u16 value = CPUReadHalfWord(address);
	if((address & 1))
		value = (s8)value;
	return value;
}

static INLINE u8 CPUReadByte(u32 address)
{
	switch(address >> 24)
	{
		case 0:
			if (bus.reg[15].I >> 24)
			{
				if(address < 0x4000)
					return biosProtected[address & 3];
				else
					goto unreadable;
			}
			return bios[address & 0x3FFF];
		case 2:
			return workRAM[address & 0x3FFFF];
		case 3:
			return internalRAM[address & 0x7fff];
		case 4:
			if((address < 0x4000400) && ioReadable[address & 0x3ff])
				return ioMem[address & 0x3ff];
			else goto unreadable;
		case 5:
			return graphics.paletteRAM[address & 0x3ff];
		case 6:
			address = (address & 0x1ffff);
			if (((graphics.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return 0;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			return vram[address];
		case 7:
			return oam[address & 0x3ff];
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			return rom[address & 0x1FFFFFF];
		case 13:
			if(cpuEEPROMEnabled)
				return eepromRead();
			goto unreadable;
		case 14:
			if(cpuSramEnabled | cpuFlashEnabled)
				return flashRead(address);
#ifdef USE_MOTION_SENSOR
			if(cpuEEPROMSensorEnabled) {
				switch(address & 0x00008f00) {
					case 0x8200:
						return systemGetSensorX() & 255;
					case 0x8300:
						return (systemGetSensorX() >> 8)|0x80;
					case 0x8400:
						return systemGetSensorY() & 255;
					case 0x8500:
						return systemGetSensorY() >> 8;
				}
			}
#endif
			// default
		default:
unreadable:
			if(armState)
				return CPUReadByteQuick(bus.reg[15].I+(address & 3));
			else
				return CPUReadByteQuick(bus.reg[15].I+(address & 1));
	}
}

static INLINE void CPUWriteMemory(u32 address, u32 value)
{
	switch(address >> 24)
	{
		case 0x02:
			WRITE32LE(((u32 *)&workRAM[address & 0x3FFFC]), value);
			break;
		case 0x03:
			WRITE32LE(((u32 *)&internalRAM[address & 0x7ffC]), value);
			break;
		case 0x04:
			if(address < 0x4000400)
			{
				CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
				CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
			}
			break;
		case 0x05:
			WRITE32LE(((u32 *)&graphics.paletteRAM[address & 0x3FC]), value);
			break;
		case 0x06:
			address = (address & 0x1fffc);
			if (((graphics.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;


			WRITE32LE(((u32 *)&vram[address]), value);
			break;
		case 0x07:
			WRITE32LE(((u32 *)&oam[address & 0x3fc]), value);
			break;
		case 0x0D:
			if(cpuEEPROMEnabled) {
				eepromWrite(value);
				break;
			}
			break;
		case 0x0E:
			if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)
				(*cpuSaveGameFunc)(address, (u8)value);
			break;
		default:
			break;
	}
}

static INLINE void CPUWriteHalfWord(u32 address, u16 value)
{
	switch(address >> 24)
	{
		case 2:
			WRITE16LE(((u16 *)&workRAM[address & 0x3FFFE]),value);
			break;
		case 3:
			WRITE16LE(((u16 *)&internalRAM[address & 0x7ffe]), value);
			break;
		case 4:
			if(address < 0x4000400)
				CPUUpdateRegister(address & 0x3fe, value);
			break;
		case 5:
			WRITE16LE(((u16 *)&graphics.paletteRAM[address & 0x3fe]), value);
			break;
		case 6:
			address = (address & 0x1fffe);
			if (((graphics.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			WRITE16LE(((u16 *)&vram[address]), value);
			break;
		case 7:
			WRITE16LE(((u16 *)&oam[address & 0x3fe]), value);
			break;
		case 8:
		case 9:
			if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
				if(!rtcWrite(address, value))
					break;
			break;
		case 13:
			if(cpuEEPROMEnabled)
				eepromWrite((u8)value);
			break;
		case 14:
			if((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)
				(*cpuSaveGameFunc)(address, (u8)value);
			break;
		default:
			break;
	}
}

static INLINE void CPUWriteByte(u32 address, u8 b)
{
	switch(address >> 24)
	{
		case 2:
			workRAM[address & 0x3FFFF] = b;
			break;
		case 3:
			internalRAM[address & 0x7fff] = b;
			break;
		case 4:
			if(address < 0x4000400)
			{
				switch(address & 0x3FF)
				{
					case 0x60:
					case 0x61:
					case 0x62:
					case 0x63:
					case 0x64:
					case 0x65:
					case 0x68:
					case 0x69:
					case 0x6c:
					case 0x6d:
					case 0x70:
					case 0x71:
					case 0x72:
					case 0x73:
					case 0x74:
					case 0x75:
					case 0x78:
					case 0x79:
					case 0x7c:
					case 0x7d:
					case 0x80:
					case 0x81:
					case 0x84:
					case 0x85:
					case 0x90:
					case 0x91:
					case 0x92:
					case 0x93:
					case 0x94:
					case 0x95:
					case 0x96:
					case 0x97:
					case 0x98:
					case 0x99:
					case 0x9a:
					case 0x9b:
					case 0x9c:
					case 0x9d:
					case 0x9e:
					case 0x9f:
						{
							int gb_addr = table[(address & 0xFF) - 0x60];
							soundEvent_u8(gb_addr, address&0xFF, b);
						}
						break;
					case 0x301: // HALTCNT, undocumented
						if(b == 0x80)
							stopState = true;
						holdState = 1;
						holdType = -1;
						cpuNextEvent = cpuTotalTicks;
						break;
					default: // every other register
						{
							u32 lowerBits = address & 0x3fe;
							uint16_t param;
							if(address & 1)
								param = (READ16LE(&ioMem[lowerBits]) & 0x00FF) | (b << 8);
							else
								param = (READ16LE(&ioMem[lowerBits]) & 0xFF00) | b;

							CPUUpdateRegister(lowerBits, param);
						}
					break;
				}
			}
			break;
		case 5:
			// no need to switch
			*((u16 *)&graphics.paletteRAM[address & 0x3FE]) = (b << 8) | b;
			break;
		case 6:
			address = (address & 0x1fffe);
			if (((graphics.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;

			// no need to switch
			// byte writes to OBJ VRAM are ignored
			if ((address) < objTilesAddress[((graphics.DISPCNT&7)+1)>>2])
				*((u16 *)&vram[address]) = (b << 8) | b;
			break;
		case 7:
			// no need to switch
			// byte writes to OAM are ignored
			//    *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
			break;
		case 13:
			if(cpuEEPROMEnabled)
				eepromWrite(b);
			break;
		case 14:
			if ((saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled))
			{
				(*cpuSaveGameFunc)(address, b);
				break;
			}
		default:
			break;
	}
}

#endif // GBAINLINE_H
