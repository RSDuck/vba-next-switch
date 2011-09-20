#ifdef USE_AGBPRINT
#ifndef AGBPRINT_H
#define AGBPRINT_H

void agbPrintEnable(bool enable);
bool agbPrintIsEnabled(void);
void agbPrintReset(void);
bool agbPrintWrite(uint32_t address, uint16_t value);
void agbPrintFlush(void);

#endif // AGBPRINT_H
#endif
