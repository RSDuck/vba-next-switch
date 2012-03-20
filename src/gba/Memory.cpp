/*============================================================
	FLASH
============================================================ */

#include <string.h>
#include "GBA.h"
#include "EEprom.h"
#include "Globals.h"
#include "Flash.h"
#include "Sram.h"
#include "../Util.h"

#define FLASH_READ_ARRAY         0
#define FLASH_CMD_1              1
#define FLASH_CMD_2              2
#define FLASH_AUTOSELECT         3
#define FLASH_CMD_3              4
#define FLASH_CMD_4              5
#define FLASH_CMD_5              6
#define FLASH_ERASE_COMPLETE     7
#define FLASH_PROGRAM            8
#define FLASH_SETBANK            9

#ifdef __LIBSNES__
extern uint8_t libsnes_save_buf[0x20000 + 0x2000];
uint8_t *flashSaveMemory = libsnes_save_buf;
#else
uint8_t flashSaveMemory[FLASH_128K_SZ];
#endif

int flashState = FLASH_READ_ARRAY;
int flashReadState = FLASH_READ_ARRAY;
int flashSize = 0x10000;
int flashDeviceID = 0x1b;
int flashManufacturerID = 0x32;
int flashBank = 0;

static variable_desc flashSaveData[] = {
  { &flashState, sizeof(int) },
  { &flashReadState, sizeof(int) },
  { &flashSaveMemory[0], 0x10000 },
  { NULL, 0 }
};

static variable_desc flashSaveData2[] = {
  { &flashState, sizeof(int) },
  { &flashReadState, sizeof(int) },
  { &flashSize, sizeof(int) },
  { &flashSaveMemory[0], 0x20000 },
  { NULL, 0 }
};

static variable_desc flashSaveData3[] = {
  { &flashState, sizeof(int) },
  { &flashReadState, sizeof(int) },
  { &flashSize, sizeof(int) },
  { &flashBank, sizeof(int) },
  { &flashSaveMemory[0], 0x20000 },
  { NULL, 0 }
};

void flashInit()
{
#ifdef __LIBSNES__
	memset(flashSaveMemory, 0xff, 0x20000);
#else
	memset(flashSaveMemory, 0xff, sizeof(flashSaveMemory));
#endif
}

void flashReset()
{
	flashState = FLASH_READ_ARRAY;
	flashReadState = FLASH_READ_ARRAY;
	flashBank = 0;
}

void flashSaveGameMem(uint8_t *& data)
{
	utilWriteDataMem(data, flashSaveData3);
}

void flashReadGameMem(const uint8_t *& data, int)
{
	utilReadDataMem(data, flashSaveData3);
}

void flashSetSize(int size)
{
	if(size == 0x10000) {
		flashDeviceID = 0x1b;
		flashManufacturerID = 0x32;
	} else {
		flashDeviceID = 0x13; //0x09;
		flashManufacturerID = 0x62; //0xc2;
	}
	// Added to make 64k saves compatible with 128k ones
	// (allow wrongfuly set 64k saves to work for Pokemon games)
	if ((size == 0x20000) && (flashSize == 0x10000))
		memcpy((uint8_t *)(flashSaveMemory+0x10000), (uint8_t *)(flashSaveMemory), 0x10000);
	flashSize = size;
}

uint8_t flashRead(uint32_t address)
{
	address &= 0xFFFF;

	switch(flashReadState) {
		case FLASH_READ_ARRAY:
			return flashSaveMemory[(flashBank << 16) + address];
		case FLASH_AUTOSELECT:
			switch(address & 0xFF)
			{
				case 0:
					// manufacturer ID
					return flashManufacturerID;
				case 1:
					// device ID
					return flashDeviceID;
			}
			break;
		case FLASH_ERASE_COMPLETE:
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
			return 0xFF;
	};
	return 0;
}

void flashSaveDecide(uint32_t address, uint8_t byte)
{
	if(address == 0x0e005555) {
		saveType = 2;
		cpuSaveGameFunc = flashWrite;
	} else {
		saveType = 1;
		cpuSaveGameFunc = sramWrite;
	}

	(*cpuSaveGameFunc)(address, byte);
}

void flashDelayedWrite(uint32_t address, uint8_t byte)
{
  saveType = 2;
  cpuSaveGameFunc = flashWrite;
  flashWrite(address, byte);
}

void flashWrite(uint32_t address, uint8_t byte)
{
	address &= 0xFFFF;
	switch(flashState) {
		case FLASH_READ_ARRAY:
			if(address == 0x5555 && byte == 0xAA)
				flashState = FLASH_CMD_1;
			break;
		case FLASH_CMD_1:
			if(address == 0x2AAA && byte == 0x55)
				flashState = FLASH_CMD_2;
			else
				flashState = FLASH_READ_ARRAY;
			break;
		case FLASH_CMD_2:
			if(address == 0x5555) {
				if(byte == 0x90) {
					flashState = FLASH_AUTOSELECT;
					flashReadState = FLASH_AUTOSELECT;
				} else if(byte == 0x80) {
					flashState = FLASH_CMD_3;
				} else if(byte == 0xF0) {
					flashState = FLASH_READ_ARRAY;
					flashReadState = FLASH_READ_ARRAY;
				} else if(byte == 0xA0) {
					flashState = FLASH_PROGRAM;
				} else if(byte == 0xB0 && flashSize == 0x20000) {
					flashState = FLASH_SETBANK;
				} else {
					flashState = FLASH_READ_ARRAY;
					flashReadState = FLASH_READ_ARRAY;
				}
			} else {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			}
			break;
		case FLASH_CMD_3:
			if(address == 0x5555 && byte == 0xAA) {
				flashState = FLASH_CMD_4;
			} else {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			}
			break;
		case FLASH_CMD_4:
			if(address == 0x2AAA && byte == 0x55) {
				flashState = FLASH_CMD_5;
			} else {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			}
			break;
		case FLASH_CMD_5:
			if(byte == 0x30) {
				// SECTOR ERASE
				memset(&flashSaveMemory[(flashBank << 16) + (address & 0xF000)],
						0,
						0x1000);
				systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
				flashReadState = FLASH_ERASE_COMPLETE;
			} else if(byte == 0x10) {
				// CHIP ERASE
				memset(flashSaveMemory, 0, flashSize);
				systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
				flashReadState = FLASH_ERASE_COMPLETE;
			} else {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			}
			break;
		case FLASH_AUTOSELECT:
			if(byte == 0xF0) {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			} else if(address == 0x5555 && byte == 0xAA)
				flashState = FLASH_CMD_1;
			else {
				flashState = FLASH_READ_ARRAY;
				flashReadState = FLASH_READ_ARRAY;
			}
			break;
		case FLASH_PROGRAM:
			flashSaveMemory[(flashBank<<16)+address] = byte;
			systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
			break;
		case FLASH_SETBANK:
			if(address == 0) {
				flashBank = (byte & 1);
			}
			flashState = FLASH_READ_ARRAY;
			flashReadState = FLASH_READ_ARRAY;
			break;
	}
}

/*============================================================
	EEPROM
============================================================ */
extern int cpuDmaCount;

int eepromMode = EEPROM_IDLE;
int eepromByte = 0;
int eepromBits = 0;
int eepromAddress = 0;

#ifdef __LIBSNES__
// Workaround for broken-by-design GBA save semantics.
extern u8 libsnes_save_buf[0x20000 + 0x2000];
u8 *eepromData = libsnes_save_buf + 0x20000;
#else
u8 eepromData[0x2000];
#endif

u8 eepromBuffer[16];
bool eepromInUse = false;
int eepromSize = 512;

variable_desc eepromSaveData[] = {
  { &eepromMode, sizeof(int) },
  { &eepromByte, sizeof(int) },
  { &eepromBits , sizeof(int) },
  { &eepromAddress , sizeof(int) },
  { &eepromInUse, sizeof(bool) },
  { &eepromData[0], 512 },
  { &eepromBuffer[0], 16 },
  { NULL, 0 }
};

void eepromInit()
{
#ifdef __LIBSNES__
	memset(eepromData, 255, 0x2000);
#else
	memset(eepromData, 255, sizeof(eepromData));
#endif
}

void eepromReset (void)
{
	eepromMode = EEPROM_IDLE;
	eepromByte = 0;
	eepromBits = 0;
	eepromAddress = 0;
	eepromInUse = false;
	eepromSize = 512;
}

void eepromSaveGameMem(uint8_t *& data)
{
   utilWriteDataMem(data, eepromSaveData);
   utilWriteIntMem(data, eepromSize);
   utilWriteMem(data, eepromData, 0x2000);
}

void eepromReadGameMem(const uint8_t *& data, int version)
{
	utilReadDataMem(data, eepromSaveData);
	if (version >= SAVE_GAME_VERSION_3) {
		eepromSize = utilReadIntMem(data);
		utilReadMem(eepromData, data, 0x2000);
	} else {
		// prior to 0.7.1, only 4K EEPROM was supported
		eepromSize = 512;
	}
}

int eepromRead (void)
{
	switch(eepromMode)
	{
		case EEPROM_IDLE:
		case EEPROM_READADDRESS:
		case EEPROM_WRITEDATA:
			return 1;
		case EEPROM_READDATA:
			{
				eepromBits++;
				if(eepromBits == 4) {
					eepromMode = EEPROM_READDATA2;
					eepromBits = 0;
					eepromByte = 0;
				}
				return 0;
			}
		case EEPROM_READDATA2:
			{
				int data = 0;
				int address = eepromAddress << 3;
				int mask = 1 << (7 - (eepromBits & 7));
				data = (eepromData[address+eepromByte] & mask) ? 1 : 0;
				eepromBits++;
				if((eepromBits & 7) == 0)
					eepromByte++;
				if(eepromBits == 0x40)
					eepromMode = EEPROM_IDLE;
				return data;
			}
		default:
			return 0;
	}
	return 1;
}

void eepromWrite(u8 value)
{
	if(cpuDmaCount == 0)
		return;
	int bit = value & 1;
	switch(eepromMode) {
		case EEPROM_IDLE:
			eepromByte = 0;
			eepromBits = 1;
			eepromBuffer[eepromByte] = bit;
			eepromMode = EEPROM_READADDRESS;
			break;
		case EEPROM_READADDRESS:
			eepromBuffer[eepromByte] <<= 1;
			eepromBuffer[eepromByte] |= bit;
			eepromBits++;
			if((eepromBits & 7) == 0) {
				eepromByte++;
			}
			if(cpuDmaCount == 0x11 || cpuDmaCount == 0x51) {
				if(eepromBits == 0x11) {
					eepromInUse = true;
					eepromSize = 0x2000;
					eepromAddress = ((eepromBuffer[0] & 0x3F) << 8) |
						((eepromBuffer[1] & 0xFF));
					if(!(eepromBuffer[0] & 0x40)) {
						eepromBuffer[0] = bit;
						eepromBits = 1;
						eepromByte = 0;
						eepromMode = EEPROM_WRITEDATA;
					} else {
						eepromMode = EEPROM_READDATA;
						eepromByte = 0;
						eepromBits = 0;
					}
				}
			} else {
				if(eepromBits == 9) {
					eepromInUse = true;
					eepromAddress = (eepromBuffer[0] & 0x3F);
					if(!(eepromBuffer[0] & 0x40)) {
						eepromBuffer[0] = bit;
						eepromBits = 1;
						eepromByte = 0;
						eepromMode = EEPROM_WRITEDATA;
					} else {
						eepromMode = EEPROM_READDATA;
						eepromByte = 0;
						eepromBits = 0;
					}
				}
			}
			break;
		case EEPROM_READDATA:
		case EEPROM_READDATA2:
			// should we reset here?
			eepromMode = EEPROM_IDLE;
			break;
		case EEPROM_WRITEDATA:
			eepromBuffer[eepromByte] <<= 1;
			eepromBuffer[eepromByte] |= bit;
			eepromBits++;
			if((eepromBits & 7) == 0)
				eepromByte++;
			if(eepromBits == 0x40)
			{
				eepromInUse = true;
				// write data;
				for(int i = 0; i < 8; i++)
					eepromData[(eepromAddress << 3) + i] = eepromBuffer[i];
				systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
			}
			else if(eepromBits == 0x41)
			{
				eepromMode = EEPROM_IDLE;
				eepromByte = 0;
				eepromBits = 0;
			}
			break;
	}
}

/*============================================================
	SRAM
============================================================ */

u8 sramRead(u32 address)
{
	return flashSaveMemory[address & 0xFFFF];
}

void sramDelayedWrite(u32 address, u8 byte)
{
	saveType = 1;
	cpuSaveGameFunc = sramWrite;
	sramWrite(address, byte);
}

void sramWrite(u32 address, u8 byte)
{
	flashSaveMemory[address & 0xFFFF] = byte;
	systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
}
