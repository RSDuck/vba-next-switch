#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
# include <unistd.h>
# include <sys/socket.h>
# include <netdb.h>
# ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
# endif // HAVE_NETINET_IN_H
# ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
# else // ! HAVE_ARPA_INET_H
#  define socklen_t int
# endif // ! HAVE_ARPA_INET_H
# define SOCKET int
#else // _WIN32
# include <xtl.h>
# include <io.h>
# define socklen_t int
# define close closesocket
# define read _read
# define write _write
#endif // _WIN32

#include "GBA.h"

extern bool debugger;
extern void CPUUpdateCPSR();
#ifdef SDL
extern void (*dbgMain)();
extern void debuggerMain();
extern void debuggerSignal(int,int);
#endif

int remotePort = 55555;
int remoteSignal = 5;
SOCKET remoteSocket = -1;
SOCKET remoteListenSocket = -1;
bool remoteConnected = false;
bool remoteResumed = false;

int (*remoteSendFnc)(char *, int) = NULL;
int (*remoteRecvFnc)(char *, int) = NULL;
bool (*remoteInitFnc)() = NULL;
void (*remoteCleanUpFnc)() = NULL;

#ifndef SDL
void remoteSetSockets(SOCKET l, SOCKET r)
{
  remoteSocket = r;
  remoteListenSocket = l;
}
#endif

int remoteTcpSend(char *data, int len)
{
  return send(remoteSocket, data, len, 0);
}

int remoteTcpRecv(char *data, int len)
{
  return recv(remoteSocket, data, len, 0);
}

bool remoteTcpInit()
{
  
  return true;
}

void remoteTcpCleanUp()
{
  if(remoteSocket > 0) {
    fprintf(stderr, "Closing remote socket\n");
    close(remoteSocket);
    remoteSocket = -1;
  }
  if(remoteListenSocket > 0) {
    fprintf(stderr, "Closing listen socket\n");
    close(remoteListenSocket);
    remoteListenSocket = -1;
  }
}

int remotePipeSend(char *data, int len)
{
  int res = write(1, data, len);
  return res;
}

int remotePipeRecv(char *data, int len)
{
  int res = read(0, data, len);
  return res;
}

bool remotePipeInit()
{
//  char dummy;
//  if (read(0, &dummy, 1) == 1)
//  {
//    if(dummy != '+') {
//      fprintf(stderr, "ACK not received\n");
//      exit(-1);
//    }
//  }

  return true;
}

void remotePipeCleanUp()
{
}

void remoteSetPort(int port)
{
  remotePort = port;
}

void remoteSetProtocol(int p)
{

}

void remoteInit()
{

}

void remotePutPacket(const char *packet)
{
 
}

#define debuggerReadMemory(addr) \
  (*(u32*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask])

#define debuggerReadHalfWord(addr) \
  (*(u16*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask])

#define debuggerReadByte(addr) \
  map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]

#define debuggerWriteMemory(addr, value) \
  *(u32*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask] = (value)

#define debuggerWriteHalfWord(addr, value) \
  *(u16*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask] = (value)

#define debuggerWriteByte(addr, value) \
  map[(addr)>>24].address[(addr) & map[(addr)>>24].mask] = (value)

void remoteOutput(const char *s, u32 addr)
{
 
}

void remoteSendSignal()
{

}

void remoteSendStatus()
{
 
}

void remoteBinaryWrite(char *p)
{
 
}

void remoteMemoryWrite(char *p)
{
 
}

void remoteMemoryRead(char *p)
{
 
}

void remoteStepOverRange(char *p)
{
 
}

void remoteWriteWatch(char *p, bool active)
{

}

void remoteReadRegisters(char *p)
{
  
}

void remoteWriteRegister(char *p)
{
 
}

extern int emulating;

void remoteStubMain()
{

}

void remoteStubSignal(int sig, int number)
{

}

void remoteCleanUp()
{

}
