#ifndef EEPROM_H
#define EEPROM_H

#define EEPROM_IDLE           0
#define EEPROM_READADDRESS    1
#define EEPROM_READDATA       2
#define EEPROM_READDATA2      3
#define EEPROM_WRITEDATA      4

extern bool eepromInUse;
extern int eepromSize;
#ifdef __LIBSNES__
extern u8 *eepromData;
#else
extern u8 eepromData[0x2000];
#endif

extern void eepromReadGameMem(const uint8_t *&data, int version);
extern void eepromSaveGameMem(uint8_t *&data);

extern int eepromRead(void);
extern void eepromWrite(u8 value);
extern void eepromInit(void);
extern void eepromReset(void);

#endif // EEPROM_H
