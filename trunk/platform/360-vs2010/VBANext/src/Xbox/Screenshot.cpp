
#include <xtl.h>
#include <string.h>
#include <fcntl.h>

#include "png.h"
 
#include "screenshot.h"
#include "System.h"

#define DECOMPOSE_PIXEL(PIX,R,G,B) {(R) = (PIX) >> 11; (G) = ((PIX) >> 6) & 0x1f; (B) = (PIX) & 0x1f; }

extern struct EmulatedSystem emulator;
extern u8 *pix;
 
bool DoScreenshot(int width, int height)
{
 
	emulator.emuWritePNG("game:\\previewsnap.png");
	return TRUE;

}


bool DoScreenshot(int width, int height, char *fname)
{
	emulator.emuWritePNG(fname);
    return TRUE;

}

