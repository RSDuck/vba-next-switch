#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "gba.h"
#include "globals.h"
#include "memory.h"
#include "port.h"
#include "system.h"

extern "C" {
#include "switch/localtime.h"
}

/*============================================================
	UTIL
============================================================ */

extern bool cpuIsMultiBoot;

bool utilIsGBAImage(const char * file)
{
	cpuIsMultiBoot = false;
	if(strlen(file) > 4)
	{
		const char * p = strrchr(file,'.');

		if(p != NULL)
      {
		char p_lower[strlen(p)];
		for(int i = 0; i < strlen(p); i++)
			p_lower[i] = tolower(p[i]);
		p = p_lower;

         if(
               !strcasecmp(p, ".agb") ||
               !strcasecmp(p, ".gba") ||
               !strcasecmp(p, ".bin") ||
               !strcasecmp(p, ".elf")
           )
            return true;

         if(!strcasecmp(p, ".mb"))
         {
            cpuIsMultiBoot = true;
            return true;
         }
      }
	}

	return false;
}

static int utilGetSize(int size)
{
	int res = 1;

	while(res < size)
		res <<= 1;

	return res;
}

uint8_t *utilLoad(const char *file, bool (*accept)(const char *), uint8_t *data, int &size)
{
    uint8_t *image = NULL;
	FILE *fp       = fopen(file,"rb");
    if(!fp) return NULL;

	fseek(fp, 0, SEEK_END); /*go to end*/
	size = ftell(fp); /* get position at end (length)*/
	rewind(fp);

	image = data;

	if(image == NULL)
	{
		/*allocate buffer memory if none was passed to the function*/
		image = (uint8_t *)malloc(utilGetSize(size));
		if(image == NULL)
		{
			systemMessage("Failed to allocate memory for data");
			return NULL;
		}
	}

	fread(image, 1, size, fp); /* read into buffer*/
	fclose(fp);
	return image;
}

/* Not endian safe, but VBA itself doesn't seem to care */
void utilWriteIntMem(uint8_t *& data, int val)
{
	memcpy(data, &val, sizeof(int));
	data += sizeof(int);
}

void utilWriteMem(uint8_t *& data, const void *in_data, unsigned size)
{
	memcpy(data, in_data, size);
	data += size;
}

void utilWriteDataMem(uint8_t *& data, variable_desc *desc)
{
	while (desc->address) 
	{
		utilWriteMem(data, desc->address, desc->size);
		desc++;
	}
}

int utilReadIntMem(const uint8_t *& data)
{
	int res;

	memcpy(&res, data, sizeof(int));
	data += sizeof(int);
	return res;
}

void utilReadMem(void *buf, const uint8_t *& data, unsigned size)
{
	memcpy(buf, data, size);
	data += size;
}

void utilReadDataMem(const uint8_t *& data, variable_desc *desc)
{
	while (desc->address)
	{
		utilReadMem(desc->address, data, desc->size);
		desc++;
	}
}

/*============================================================
	FLASH
============================================================ */


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

extern uint8_t libretro_save_buf[0x20000 + 0x2000];
uint8_t *flashSaveMemory = libretro_save_buf;

int flashState = FLASH_READ_ARRAY;
int flashReadState = FLASH_READ_ARRAY;
int flashSize = 0x10000;
int flashDeviceID = 0x1b;
int flashManufacturerID = 0x32;
int flashBank = 0;

int autosaveCountdown = 0;
u32 rtcOffset = 0;

static variable_desc flashSaveData3[] = {
  { &flashState, sizeof(int) },
  { &flashReadState, sizeof(int) },
  { &flashSize, sizeof(int) },
  { &flashBank, sizeof(int) },
  { &flashSaveMemory[0], 0x20000 },
  { NULL, 0 }
};

void flashInit (void)
{
	memset(flashSaveMemory, 0xff, 0x20000);
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
  autosaveCountdown = BAT_UNUSED_FRAMES_BEFORE_SAVE;
  flashWrite(address, byte);
}

void flashWrite(uint32_t address, uint8_t byte)
{
	if(cpuSaveGameFunc == flashWrite)
		autosaveCountdown = BAT_UNUSED_FRAMES_BEFORE_SAVE;

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
				flashReadState = FLASH_ERASE_COMPLETE;
			} else if(byte == 0x10) {
				// CHIP ERASE
				memset(flashSaveMemory, 0, flashSize);
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
int eepromMode = EEPROM_IDLE;
int eepromByte = 0;
int eepromBits = 0;
int eepromAddress = 0;

// Workaround for broken-by-design GBA save semantics.
extern u8 libretro_save_buf[0x20000 + 0x2000];
u8 *eepromData = libretro_save_buf + 0x20000;

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

void eepromInit (void)
{
	memset(eepromData, 255, 0x2000);
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
	eepromSize = utilReadIntMem(data);
	utilReadMem(eepromData, data, 0x2000);
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
         break;
   }

   return 0;
}

void eepromWrite(u8 value)
{
	if(cpuDmaCount == 0)
		return;
	if(eepromInUse)
		autosaveCountdown = BAT_UNUSED_FRAMES_BEFORE_SAVE;

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
	if(cpuSaveGameFunc == sramWrite)
		autosaveCountdown = BAT_UNUSED_FRAMES_BEFORE_SAVE;
	flashSaveMemory[address & 0xFFFF] = byte;
}

/*============================================================
	GYRO
============================================================ */

#if USE_MOTION_SENSOR
static u16 gyro_data[3];

static void gyroWritePins(unsigned pins) {
	if (!hardware.readWrite) return;

	uint16_t& t = gyro_data[0];
	t &= hardware.direction;
	hardware.pinState = t | (pins & ~hardware.direction & 0xF);
	t = hardware.pinState;
}

static void gyroReadPins() {
	if (hardware.pinState & 1) {
		// Normalize to ~12 bits, focused on 0x6C0
		hardware.gyroSample = (systemGetGyroZ() >> 21) + 0x6C0; // Crop off an extra bit so that we can't go negative
	}

	if (hardware.gyroEdge && !(hardware.pinState & 2)) {
		// Write bit on falling edge
		unsigned bit = hardware.gyroSample >> 15;
		hardware.gyroSample <<= 1;
		gyroWritePins(bit << 2);
	}

	hardware.gyroEdge = !!(hardware.pinState & 2);
}

bool gyroWrite(u32 address, u16 value) {
	switch (address) {
	case 0x80000c4:
		hardware.pinState &= ~hardware.direction;
		hardware.pinState |= value;
		gyroReadPins();
		break;
	case 0x80000c6:
		hardware.direction = value;
		break;
	case 0x80000c8:
		hardware.readWrite = value;
		break;
	default:
		break;
	}
	if (hardware.readWrite) {
		uint16_t& t = gyro_data[0];
		t &= ~hardware.direction;
		t |= hardware.pinState;
	} else {
		gyro_data[0] = 0;
	}
	return true;
}

u16 gyroRead(u32 address) {
	return gyro_data[(address - 0x80000c4) >> 1];
}
#endif

/*============================================================
	RTC
============================================================ */

#define IDLE		0
#define COMMAND		1
#define DATA		2
#define READDATA	3

typedef struct
{
	u8 byte0;
	u8 byte1;
	u8 byte2;
	u8 command;
	int dataLen;
	int bits;
	int state;
	u8 data[12];
	// reserved variables for future
	u8 reserved[12];
	bool reserved2;
	u32 reserved3;
} RTCCLOCKDATA;

struct tm gba_time;

static RTCCLOCKDATA rtcClockData;
static bool rtcEnabled = true;

void SetGBATime()
{
    gba_time = *getRealLocalTime();
	if(rtcOffset <= 13)
		gba_time.tm_hour += rtcOffset;
	else 
		gba_time.tm_hour -= rtcOffset - 13;
	mktime(&gba_time);
}

void rtcEnable(bool e)
{
	rtcEnabled = e;
}

bool rtcIsEnabled (void)
{
	return rtcEnabled;
}

u16 rtcRead(u32 address)
{
	if(rtcEnabled)
	{
		switch(address)
		{
			case 0x80000c8:
				return rtcClockData.byte2;
			case 0x80000c6:
				return rtcClockData.byte1;
			case 0x80000c4:
				return rtcClockData.byte0;
		}
	}

	return READ16LE((&rom[address & 0x1FFFFFE]));
}

static u8 toBCD(u8 value)
{
	value = value % 100;
	int l = value % 10;
	int h = value / 10;
	return h * 16 + l;
};


bool rtcWrite(uint32_t address, uint16_t value)
{
    if (address == 0x80000c8) {
        rtcClockData.byte2 = (uint8_t)value; // bit 0 = enable reading from 0x80000c4 c6 and c8
    } else if (address == 0x80000c6) {
        rtcClockData.byte1 = (uint8_t)value; // 0=read/1=write (for each of 4 low bits)

    } else if (address == 0x80000c4) // 4 bits of I/O Port Data (upper bits not used)
    {

        // Boktai solar sensor
        if (rtcClockData.byte1 == 0x07) {
            if (value & 2) {
                // reset counter to 0
                rtcClockData.reserved[11] = 0;
            }

            if ((value & 1) && !(rtcClockData.reserved[10] & 1)) {
                // increase counter, ready to do another read
                if (rtcClockData.reserved[11] < 255) {
                    rtcClockData.reserved[11]++;
                } else {
                    rtcClockData.reserved[11] = 0;
                }
            }

            rtcClockData.reserved[10] = value & rtcClockData.byte1;
        }

        // WarioWare Twisted rotation sensor
        if (rtcClockData.byte1 == 0x0b) {
            if (value & 2) {
                // clock goes high in preperation for reading a bit
                rtcClockData.reserved[11]--;
            }

            if (value & 1) {
                // start ADC conversion
                rtcClockData.reserved[11] = 15;
            }

            rtcClockData.byte0 = value & rtcClockData.byte1;
        }

        // Real Time Clock
        if (rtcClockData.byte1 & 4) {
            if (rtcClockData.state == IDLE && rtcClockData.byte0 == 1 && value == 5) {
                rtcClockData.state = COMMAND;
                rtcClockData.bits = 0;
                rtcClockData.command = 0;
            } else if (!(rtcClockData.byte0 & 1) && (value & 1)) // bit transfer
            {
                rtcClockData.byte0 = (uint8_t)value;

                switch (rtcClockData.state) {
                case COMMAND:
                    rtcClockData.command |= ((value & 2) >> 1) << (7 - rtcClockData.bits);
                    rtcClockData.bits++;

                    if (rtcClockData.bits == 8) {
                        rtcClockData.bits = 0;

                        switch (rtcClockData.command) {
                        case 0x60:
                            // not sure what this command does but it doesn't take parameters
                            // maybe it is a reset or stop
                            rtcClockData.state = IDLE;
                            rtcClockData.bits = 0;
                            break;

                        case 0x62:
                            // this sets the control state but not sure what those values are
                            rtcClockData.state = READDATA;
                            rtcClockData.dataLen = 1;
                            break;

                        case 0x63:
                            rtcClockData.dataLen = 1;
                            rtcClockData.data[0] = 0x40;
                            rtcClockData.state = DATA;
                            break;

                        case 0x64:
                            break;

                        case 0x65: {
                            if (rtcEnabled)
                                SetGBATime();

                            rtcClockData.dataLen = 7;
                            rtcClockData.data[0] = toBCD(gba_time.tm_year);
                            rtcClockData.data[1] = toBCD(gba_time.tm_mon + 1);
                            rtcClockData.data[2] = toBCD(gba_time.tm_mday);
                            rtcClockData.data[3] = toBCD(gba_time.tm_wday);
                            rtcClockData.data[4] = toBCD(gba_time.tm_hour);
                            rtcClockData.data[5] = toBCD(gba_time.tm_min);
                            rtcClockData.data[6] = toBCD(gba_time.tm_sec);
                            rtcClockData.state = DATA;
                        } break;

                        case 0x67: {
                            if (rtcEnabled)
                                SetGBATime();

                            rtcClockData.dataLen = 3;
                            rtcClockData.data[0] = toBCD(gba_time.tm_hour);
                            rtcClockData.data[1] = toBCD(gba_time.tm_min);
                            rtcClockData.data[2] = toBCD(gba_time.tm_sec);
                            rtcClockData.state = DATA;
                        } break;

                        default:
                            rtcClockData.state = IDLE;
                            break;
                        }
                    }

                    break;

                case DATA:
                    if (rtcClockData.byte1 & 2) {
                    } else if (rtcClockData.byte1 & 4) {
                        rtcClockData.byte0 = (rtcClockData.byte0 & ~2) | ((rtcClockData.data[rtcClockData.bits >> 3] >> (rtcClockData.bits & 7)) & 1) * 2;
                        rtcClockData.bits++;

                        if (rtcClockData.bits == 8 * rtcClockData.dataLen) {
                            rtcClockData.bits = 0;
                            rtcClockData.state = IDLE;
                        }
                    }

                    break;

                case READDATA:
                    if (!(rtcClockData.byte1 & 2)) {
                    } else {
                        rtcClockData.data[rtcClockData.bits >> 3] = (rtcClockData.data[rtcClockData.bits >> 3] >> 1) | ((value << 6) & 128);
                        rtcClockData.bits++;

                        if (rtcClockData.bits == 8 * rtcClockData.dataLen) {
                            rtcClockData.bits = 0;
                            rtcClockData.state = IDLE;
                        }
                    }

                    break;

                default:
                    break;
                }
            } else
                rtcClockData.byte0 = (uint8_t)value;
        }
    }

    return true;
}

void rtcReset (void)
{
	memset(&rtcClockData, 0, sizeof(rtcClockData));

	rtcClockData.byte0 = 0;
	rtcClockData.byte1 = 0;
	rtcClockData.byte2 = 0;
	rtcClockData.command = 0;
	rtcClockData.dataLen = 0;
	rtcClockData.bits = 0;
	rtcClockData.state = IDLE;
}

void rtcSaveGameMem(uint8_t *& data)
{
	utilWriteMem(data, &rtcClockData, sizeof(rtcClockData));
}

void rtcReadGameMem(const uint8_t *& data)
{
	utilReadMem(&rtcClockData, data, sizeof(rtcClockData));
}

