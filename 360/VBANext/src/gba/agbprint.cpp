#include <stdio.h>
#include <string.h>

#include "GBA.h"
#include "Globals.h"
#include "../common/Port.h"
#include "../System.h"

#define debuggerWriteHalfWord(addr, value) \
  WRITE16LE((u16*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask], (value))

#define debuggerReadHalfWord(addr) \
  READ16LE(((u16*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))

static bool agbPrintEnabled = false;
static bool agbPrintProtect = false;

bool agbPrintWrite(u32 address, u16 value)
{
  
  return false;
}

void agbPrintReset()
{
  agbPrintProtect = false;
}

void agbPrintEnable(bool enable)
{
  agbPrintEnabled = enable;
}

bool agbPrintIsEnabled()
{
  return agbPrintEnabled;
}

void agbPrintFlush()
{
 
}
