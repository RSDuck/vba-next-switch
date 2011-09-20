#ifndef RTC_H
#define RTC_H

uint16_t rtcRead(uint32_t address);

bool rtcWrite(uint32_t address, uint16_t value);
void rtcEnable(bool);
bool rtcIsEnabled(void);
void rtcReset(void);
void rtcReadGame(gzFile gzFile);
void rtcSaveGame(gzFile gzFile);

#endif // RTC_H
