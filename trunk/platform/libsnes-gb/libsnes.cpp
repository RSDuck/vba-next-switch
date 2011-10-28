#include <string>
#include <string.h>
#include "libsnes.hpp"

#include "../vba/Util.h"
#include "../vba/gba/Sound.h"
#include "../vba/gb/gb.h"
#include "../vba/gb/gbSound.h"
#include "../vba/gb/gbGlobals.h"
#include "../vba/gba/Globals.h"

#include <stdio.h>

static snes_video_refresh_t video_cb;
static snes_audio_sample_t audio_cb;
static snes_input_poll_t poll_cb;
static snes_input_state_t input_cb;

static std::string g_rom_name;
static std::string g_basename;

uint32_t srcWidth;
uint32_t srcHeight;
extern int gbJoymask[4];

unsigned snes_library_revision_major(void)
{
   return 1;
}

unsigned snes_library_revision_minor(void)
{
   return 3;
}

const char *snes_library_id(void)
{
   return "VBANext GB";
}

void snes_set_video_refresh(snes_video_refresh_t cb)
{
   video_cb = cb;
}

void snes_set_audio_sample(snes_audio_sample_t cb)
{
   audio_cb = cb;
}

void snes_set_input_poll(snes_input_poll_t cb)
{
   poll_cb = cb;
}

void snes_set_input_state(snes_input_state_t cb)
{
   input_cb = cb;
}

void snes_set_controller_port_device(bool, unsigned)
{}

void snes_set_cartridge_basename(const char* path_)
{
	char path[1024];
	strncpy(path, path_, sizeof(path));

	const char *split = strrchr(path_, '/');
	if (!split) split = strrchr(path_, '\\');
	if (split)
		g_basename = split + 1;
	else
		g_basename = path_;
	
	g_rom_name = path_;

	fprintf(stderr, "PATH: %s\n", g_rom_name.c_str());
	fprintf(stderr, "BASENAME: %s\n", g_basename.c_str());
}

void snes_init(void) {}

static unsigned serialize_size = 0;

void snes_term(void) {}

void snes_power(void)
{
	gbReset();
}

void snes_reset(void)
{
	gbReset();
}

void systemReadJoypadGB(int n)
{
   poll_cb();

   uint32_t J = 0;

   static const unsigned binds[] = {
      SNES_DEVICE_ID_JOYPAD_A,
      SNES_DEVICE_ID_JOYPAD_B,
      SNES_DEVICE_ID_JOYPAD_SELECT,
      SNES_DEVICE_ID_JOYPAD_START,
      SNES_DEVICE_ID_JOYPAD_RIGHT,
      SNES_DEVICE_ID_JOYPAD_LEFT,
      SNES_DEVICE_ID_JOYPAD_UP,
      SNES_DEVICE_ID_JOYPAD_DOWN,
      SNES_DEVICE_ID_JOYPAD_R,
      SNES_DEVICE_ID_JOYPAD_L
   };

   for (unsigned i = 0; i < 10; i++)
      J |= input_cb(SNES_PORT_1, SNES_DEVICE_JOYPAD, 0, binds[i]) << i;

   gbJoymask[n] = J;
}

void snes_run(void)
{
	gbEmulate();
}


unsigned snes_serialize_size(void)
{
   return serialize_size;
}

bool snes_serialize(uint8_t *data, unsigned size)
{
   	//return CPUWriteState_libgba(data, size);
}

bool snes_unserialize(const uint8_t *data, unsigned size)
{
   	//return CPUReadState_libgba(data, size);
}

void snes_cheat_reset(void)
{}

void snes_cheat_set(unsigned, bool, const char*)
{}

static void gb_settings()
{
	srcWidth = 160;
	srcHeight = 144;
	gbBorderLineSkip = 160;
	gbBorderColumnSkip = 0;
	gbBorderRowSkip = 0;
	gbBorderOn = 0;
}

static void sgb_settings()
{
	srcWidth = 256;
	srcHeight = 224;
	gbBorderLineSkip = 256;
	gbBorderColumnSkip = 48;
	gbBorderRowSkip = 40;
	gbBorderOn = 1;
}

static void gb_system_init(void)
{
	for(int i = 0; i < 24; )
	{
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}

	utilUpdateSystemColorMaps();
}

static void gb_init(void)
{
	gb_settings();
	gb_system_init();

	soundInit();
	gbGetHardwareType();

	gbSoundReset();
	gbSoundSetDeclicking(false);
	gbReset();

	uint8_t *state_buf = new uint8_t[2000000];
	#if 0
	serialize_size = CPUWriteState_libgba(state_buf, 2000000);
	#endif
	delete[] state_buf;
}

bool snes_load_cartridge_normal(const char*, const uint8_t *rom_data, unsigned rom_size)
{
	const char *tmppath = tmpnam(NULL);
	if (!tmppath)
		return false;

	FILE *file = fopen(tmppath, "wb");
	if (!file)
		return false;

	fwrite(rom_data, 1, rom_size, file);
	fclose(file);

	unsigned ret = gbLoadRom(tmppath);
	unlink(tmppath);
	gb_init();

	return ret;
}

bool snes_load_cartridge_bsx_slotted(
  const char*, const uint8_t*, unsigned,
  const char*, const uint8_t*, unsigned
)
{ return false; }

bool snes_load_cartridge_bsx(
  const char*, const uint8_t *, unsigned,
  const char*, const uint8_t *, unsigned
)
{ return false; }

bool snes_load_cartridge_sufami_turbo(
  const char*, const uint8_t*, unsigned,
  const char*, const uint8_t*, unsigned,
  const char*, const uint8_t*, unsigned
)
{ return false; }

bool snes_load_cartridge_super_game_boy(
  const char*, const uint8_t*, unsigned,
  const char*, const uint8_t*, unsigned
)
{ return false; }

void snes_unload_cartridge(void)
{}

bool snes_get_region(void)
{
   return SNES_REGION_NTSC;
}

// Workaround for broken-by-design GBA save semantics.
uint8_t libsnes_save_buf[0x20000 + 0x2000];
uint8_t *snes_get_memory_data(unsigned id)
{
   if (id != SNES_MEMORY_CARTRIDGE_RAM)
      return 0;

   return libsnes_save_buf;
}

unsigned snes_get_memory_size(unsigned id)
{
   if (id != SNES_MEMORY_CARTRIDGE_RAM)
      return 0;

   return sizeof(libsnes_save_buf);
}


void systemOnSoundShutdown()
{}

void systemSoundNonblock(bool)
{}

void systemSoundSetThrottle(uint16_t)
{}

bool systemSoundInitDriver(long)
{
   return true;
}

void systemSoundPause()
{}

void systemSoundReset()
{}

void systemSoundResume()
{}

void systemOnWriteDataToSoundBuffer(int16_t *finalWave, int length)
{
   for (int i = 0; i < length; i += 2)
      audio_cb(finalWave[i + 0], finalWave[i + 1]);
}

static uint16_t pix_buf[160 * 1024];
extern uint8_t * pix;

void systemDrawScreen()
{
   for (unsigned y = 0; y < srcHeight; y++)
   {
      uint16_t *dst = pix_buf + y * 1024;
      const uint32_t *src = (const uint32_t*)pix + (gbBorderLineSkip+1) * (register_LY + gbBorderRowSkip+1) + gbBorderColumnSkip;
      for (unsigned x = 0; x < srcWidth; x++)
         dst[x] = (uint16_t)(src[x] & 0x7fff);
   }

   video_cb(pix_buf, srcWidth, srcHeight);
}

// Stubs
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
uint16_t systemGbPalette[24];
int systemColorDepth = 32;
int systemDebug = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int systemSpeed = 0;
int systemRedShift = 10;
int systemGreenShift = 5;
int systemBlueShift = 0;

void systemMessage(int, const char*, ...)
{}


bool systemSoundInit()
{
   return true;
}

bool systemCanChangeSoundQuality()
{
   return true;
}

void systemFrame()
{}

void systemGbPrint(unsigned char*, int, int, int, int)
{}

void systemGbBorderOn()
{}
