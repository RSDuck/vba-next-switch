#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gba.h"
#include "../globals.h"
#include "../memory.h"
#include "../port.h"
#include "../sound.h"
#include "../system.h"
#include "../types.h"

#include "util.h"
#include "zoom.h"

#include "ui.h"

#include <switch.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

uint8_t libretro_save_buf[0x20000 + 0x2000]; /* Workaround for broken-by-design GBA save semantics. */

enum { filterNearestInteger,
       filterNearest,
       filterBilinear,
       filtersCount,
};
uint32_t scalingFilter = filterNearest;
const char *filterStrNames[] = {"Nearest Integer", "Nearest Fullscreen", "Bilinear Fullscreen(slow!)"};

extern uint64_t joy;
static bool can_dupe;
unsigned device_type = 0;

static uint32_t disableAnalogStick = 0;

static u32 *upscaleBuffer = NULL;
static int upscaleBufferSize = 0;

static bool emulationRunning = false;
static bool emulationPaused = false;

static const char *stringsNoYes[] = {"No", "Yes"};

static char currentRomPath[PATH_LENGTH] = {'\0'};

static Mutex videoLock;
static u16 *videoTransferBuffer;
static int videoTransferBackbuffer = 0;
static u32 *conversionBuffer;

static Mutex inputLock;
static u32 inputTransferKeysHeld;

static Mutex emulationLock;

static bool running = true;

char filename_bios[0x100] = {0};

#define AUDIO_SAMPLERATE 48000
#define AUDIO_BUFFER_COUNT 6
#define AUDIO_BUFFER_SAMPLES (AUDIO_SAMPLERATE / 20)
#define AUDIO_TRANSFERBUF_SIZE (AUDIO_BUFFER_SAMPLES * 4)

static int audioTransferBufferUsed = 0;
static u32 *audioTransferBuffer;
static Mutex audioLock;

static unsigned libretro_save_size = sizeof(libretro_save_buf);

const char *frameSkipNames[] = {"No Frameskip", "1/3", "1/2", "1", "2", "3", "4"};
const int frameSkipValues[] = {0, 0x13, 0x12, 0x1, 0x2, 0x3, 0x4};

static uint32_t frameSkip = 0;

uint32_t buttonMap[10] = {KEY_A, KEY_B, KEY_MINUS, KEY_PLUS, KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_R, KEY_L};

static bool scan_area(const uint8_t *data, unsigned size) {
	for (unsigned i = 0; i < size; i++)
		if (data[i] != 0xff) return true;

	return false;
}

static void adjust_save_ram() {
	if (scan_area(libretro_save_buf, 512) && !scan_area(libretro_save_buf + 512, sizeof(libretro_save_buf) - 512)) {
		libretro_save_size = 512;
		printf("Detecting EEprom 8kbit\n");
	} else if (scan_area(libretro_save_buf, 0x2000) && !scan_area(libretro_save_buf + 0x2000, sizeof(libretro_save_buf) - 0x2000)) {
		libretro_save_size = 0x2000;
		printf("Detecting EEprom 64kbit\n");
	}

	else if (scan_area(libretro_save_buf, 0x10000) && !scan_area(libretro_save_buf + 0x10000, sizeof(libretro_save_buf) - 0x10000)) {
		libretro_save_size = 0x10000;
		printf("Detecting Flash 512kbit\n");
	} else if (scan_area(libretro_save_buf, 0x20000) && !scan_area(libretro_save_buf + 0x20000, sizeof(libretro_save_buf) - 0x20000)) {
		libretro_save_size = 0x20000;
		printf("Detecting Flash 1Mbit\n");
	} else
		printf("Did not detect any particular SRAM type.\n");

	if (libretro_save_size == 512 || libretro_save_size == 0x2000)
		eepromData = libretro_save_buf;
	else if (libretro_save_size == 0x10000 || libretro_save_size == 0x20000)
		flashSaveMemory = libretro_save_buf;
}

void romPathWithExt(char *out, int outBufferLen, const char *ext) {
	strcpy_safe(out, currentRomPath, outBufferLen);
	int dotLoc = strlen(out);
	while (dotLoc >= 0 && out[dotLoc] != '.') dotLoc--;

	int extLen = strlen(ext);
	for (int i = 0; i < extLen + 1; i++) out[dotLoc + 1 + i] = ext[i];
}

void retro_init(void) {
	memset(libretro_save_buf, 0xff, sizeof(libretro_save_buf));
	adjust_save_ram();
#if HAVE_HLE_BIOS
	const char *dir = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir) {
		strncpy(filename_bios, dir, sizeof(filename_bios));
		strncat(filename_bios, "/gba_bios.bin", sizeof(filename_bios));
	}
#endif

#ifdef FRONTEND_SUPPORTS_RGB565
	enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
	if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
		log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
#endif

#if THREADED_RENDERER
	ThreadedRendererStart();
#endif
}

static unsigned serialize_size = 0;

typedef struct {
	char romtitle[256];
	char romid[5];
	int flashSize;
	int saveType;
	int rtcEnabled;
	int mirroringEnabled;
	int useBios;
} ini_t;

static const ini_t gbaover[256] = {
    // romtitle,							    	romid	flash	save	rtc	mirror	bios
    {"2 Games in 1 - Dragon Ball Z - The Legacy of Goku I & II (USA)", "BLFE", 0, 1, 0, 0, 0},
    {"2 Games in 1 - Dragon Ball Z - Buu's Fury + Dragon Ball GT - Transformation (USA)", "BUFE", 0, 1, 0, 0, 0},
    {"Boktai - The Sun Is in Your Hand (Europe)(En,Fr,De,Es,It)", "U3IP", 0, 0, 1, 0, 0},
    {"Boktai - The Sun Is in Your Hand (USA)", "U3IE", 0, 0, 1, 0, 0},
    {"Boktai 2 - Solar Boy Django (USA)", "U32E", 0, 0, 1, 0, 0},
    {"Boktai 2 - Solar Boy Django (Europe)(En,Fr,De,Es,It)", "U32P", 0, 0, 1, 0, 0},
    {"Bokura no Taiyou - Taiyou Action RPG (Japan)", "U3IJ", 0, 0, 1, 0, 0},
    {"Card e-Reader+ (Japan)", "PSAJ", 131072, 0, 0, 0, 0},
    {"Classic NES Series - Bomberman (USA, Europe)", "FBME", 0, 1, 0, 1, 0},
    {"Classic NES Series - Castlevania (USA, Europe)", "FADE", 0, 1, 0, 1, 0},
    {"Classic NES Series - Donkey Kong (USA, Europe)", "FDKE", 0, 1, 0, 1, 0},
    {"Classic NES Series - Dr. Mario (USA, Europe)", "FDME", 0, 1, 0, 1, 0},
    {"Classic NES Series - Excitebike (USA, Europe)", "FEBE", 0, 1, 0, 1, 0},
    {"Classic NES Series - Legend of Zelda (USA, Europe)", "FZLE", 0, 1, 0, 1, 0},
    {"Classic NES Series - Ice Climber (USA, Europe)", "FICE", 0, 1, 0, 1, 0},
    {"Classic NES Series - Metroid (USA, Europe)", "FMRE", 0, 1, 0, 1, 0},
    {"Classic NES Series - Pac-Man (USA, Europe)", "FP7E", 0, 1, 0, 1, 0},
    {"Classic NES Series - Super Mario Bros. (USA, Europe)", "FSME", 0, 1, 0, 1, 0},
    {"Classic NES Series - Xevious (USA, Europe)", "FXVE", 0, 1, 0, 1, 0},
    {"Classic NES Series - Zelda II - The Adventure of Link (USA, Europe)", "FLBE", 0, 1, 0, 1, 0},
    {"Digi Communication 2 - Datou! Black Gemagema Dan (Japan)", "BDKJ", 0, 1, 0, 0, 0},
    {"e-Reader (USA)", "PSAE", 131072, 0, 0, 0, 0},
    {"Dragon Ball GT - Transformation (USA)", "BT4E", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - Buu's Fury (USA)", "BG3E", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - Taiketsu (Europe)(En,Fr,De,Es,It)", "BDBP", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - Taiketsu (USA)", "BDBE", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - The Legacy of Goku II International (Japan)", "ALFJ", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - The Legacy of Goku II (Europe)(En,Fr,De,Es,It)", "ALFP", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - The Legacy of Goku II (USA)", "ALFE", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - The Legacy Of Goku (Europe)(En,Fr,De,Es,It)", "ALGP", 0, 1, 0, 0, 0},
    {"Dragon Ball Z - The Legacy of Goku (USA)", "ALGE", 131072, 1, 0, 0, 0},
    {"F-Zero - Climax (Japan)", "BFTJ", 131072, 0, 0, 0, 0},
    {"Famicom Mini Vol. 01 - Super Mario Bros. (Japan)", "FMBJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 12 - Clu Clu Land (Japan)", "FCLJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 13 - Balloon Fight (Japan)", "FBFJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 14 - Wrecking Crew (Japan)", "FWCJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 15 - Dr. Mario (Japan)", "FDMJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 16 - Dig Dug (Japan)", "FTBJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 17 - Takahashi Meijin no Boukenjima (Japan)", "FTBJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 18 - Makaimura (Japan)", "FMKJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 19 - Twin Bee (Japan)", "FTWJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 20 - Ganbare Goemon! Karakuri Douchuu (Japan)", "FGGJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 21 - Super Mario Bros. 2 (Japan)", "FM2J", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 22 - Nazo no Murasame Jou (Japan)", "FNMJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 23 - Metroid (Japan)", "FMRJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 24 - Hikari Shinwa - Palthena no Kagami (Japan)", "FPTJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 25 - The Legend of Zelda 2 - Link no Bouken (Japan)", "FLBJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 26 - Famicom Mukashi Banashi - Shin Onigashima - Zen Kou Hen (Japan)", "FFMJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 27 - Famicom Tantei Club - Kieta Koukeisha - Zen Kou Hen (Japan)", "FTKJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 28 - Famicom Tantei Club Part II - Ushiro ni Tatsu Shoujo - Zen Kou Hen (Japan)", "FTUJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 29 - Akumajou Dracula (Japan)", "FADJ", 0, 1, 0, 1, 0},
    {"Famicom Mini Vol. 30 - SD Gundam World - Gachapon Senshi Scramble Wars (Japan)", "FSDJ", 0, 1, 0, 1, 0},
    {"Game Boy Wars Advance 1+2 (Japan)", "BGWJ", 131072, 0, 0, 0, 0},
    {"Golden Sun - The Lost Age (USA)", "AGFE", 65536, 0, 0, 1, 0},
    {"Golden Sun (USA)", "AGSE", 65536, 0, 0, 1, 0},
    {"Iridion II (Europe) (En,Fr,De)", "AI2P", 0, 5, 0, 0, 0},
    {"Iridion II (USA)", "AI2E", 0, 5, 0, 0, 0},
    {"Koro Koro Puzzle - Happy Panechu! (Japan)", "KHPJ", 0, 4, 0, 0, 0},
    {"Mario vs. Donkey Kong (Europe)", "BM5P", 0, 3, 0, 0, 0},
    {"Pocket Monsters - Emerald (Japan)", "BPEJ", 131072, 0, 1, 0, 0},
    {"Pocket Monsters - Fire Red (Japan)", "BPRJ", 131072, 0, 0, 0, 0},
    {"Pocket Monsters - Leaf Green (Japan)", "BPGJ", 131072, 0, 0, 0, 0},
    {"Pocket Monsters - Ruby (Japan)", "AXVJ", 131072, 0, 1, 0, 0},
    {"Pocket Monsters - Sapphire (Japan)", "AXPJ", 131072, 0, 1, 0, 0},
    {"Pokemon Mystery Dungeon - Red Rescue Team (USA, Australia)", "B24E", 131072, 0, 0, 0, 0},
    {"Pokemon Mystery Dungeon - Red Rescue Team (En,Fr,De,Es,It)", "B24P", 131072, 0, 0, 0, 0},
    {"Pokemon - Blattgruene Edition (Germany)", "BPGD", 131072, 0, 0, 0, 0},
    {"Pokemon - Edicion Rubi (Spain)", "AXVS", 131072, 0, 1, 0, 0},
    {"Pokemon - Edicion Esmeralda (Spain)", "BPES", 131072, 0, 1, 0, 0},
    {"Pokemon - Edicion Rojo Fuego (Spain)", "BPRS", 131072, 1, 0, 0, 0},
    {"Pokemon - Edicion Verde Hoja (Spain)", "BPGS", 131072, 1, 0, 0, 0},
    {"Pokemon - Eidicion Zafiro (Spain)", "AXPS", 131072, 0, 1, 0, 0},
    {"Pokemon - Emerald Version (USA, Europe)", "BPEE", 131072, 0, 1, 0, 0},
    {"Pokemon - Feuerrote Edition (Germany)", "BPRD", 131072, 0, 0, 0, 0},
    {"Pokemon - Fire Red Version (USA, Europe)", "BPRE", 131072, 0, 0, 0, 0},
    {"Pokemon - Leaf Green Version (USA, Europe)", "BPGE", 131072, 0, 0, 0, 0},
    {"Pokemon - Rubin Edition (Germany)", "AXVD", 131072, 0, 1, 0, 0},
    {"Pokemon - Ruby Version (USA, Europe)", "AXVE", 131072, 0, 1, 0, 0},
    {"Pokemon - Sapphire Version (USA, Europe)", "AXPE", 131072, 0, 1, 0, 0},
    {"Pokemon - Saphir Edition (Germany)", "AXPD", 131072, 0, 1, 0, 0},
    {"Pokemon - Smaragd Edition (Germany)", "BPED", 131072, 0, 1, 0, 0},
    {"Pokemon - Version Emeraude (France)", "BPEF", 131072, 0, 1, 0, 0},
    {"Pokemon - Version Rouge Feu (France)", "BPRF", 131072, 0, 0, 0, 0},
    {"Pokemon - Version Rubis (France)", "AXVF", 131072, 0, 1, 0, 0},
    {"Pokemon - Version Saphir (France)", "AXPF", 131072, 0, 1, 0, 0},
    {"Pokemon - Version Vert Feuille (France)", "BPGF", 131072, 0, 0, 0, 0},
    {"Pokemon - Versione Rubino (Italy)", "AXVI", 131072, 0, 1, 0, 0},
    {"Pokemon - Versione Rosso Fuoco (Italy)", "BPRI", 131072, 0, 0, 0, 0},
    {"Pokemon - Versione Smeraldo (Italy)", "BPEI", 131072, 0, 1, 0, 0},
    {"Pokemon - Versione Verde Foglia (Italy)", "BPGI", 131072, 0, 0, 0, 0},
    {"Pokemon - Versione Zaffiro (Italy)", "AXPI", 131072, 0, 1, 0, 0},
    {"Rockman EXE 4.5 - Real Operation (Japan)", "BR4J", 0, 0, 1, 0, 0},
    {"Rocky (Europe)(En,Fr,De,Es,It)", "AROP", 0, 1, 0, 0, 0},
    {"Rocky (USA)(En,Fr,De,Es,It)", "AR8e", 0, 1, 0, 0, 0},
    {"Sennen Kazoku (Japan)", "BKAJ", 131072, 0, 1, 0, 0},
    {"Shin Bokura no Taiyou - Gyakushuu no Sabata (Japan)", "U33J", 0, 1, 1, 0, 0},
    {"Super Mario Advance 4 (Japan)", "AX4J", 131072, 0, 0, 0, 0},
    {"Super Mario Advance 4 - Super Mario Bros. 3 (Europe)(En,Fr,De,Es,It)", "AX4P", 131072, 0, 0, 0, 0},
    {"Super Mario Advance 4 - Super Mario Bros 3 - Super Mario Advance 4 v1.1 (USA)", "AX4E", 131072, 0, 0, 0, 0},
    {"Top Gun - Combat Zones (USA)(En,Fr,De,Es,It)", "A2YE", 0, 5, 0, 0, 0},
    {"Yoshi's Universal Gravitation (Europe)(En,Fr,De,Es,It)", "KYGP", 0, 4, 0, 0, 0},
    {"Yoshi no Banyuuinryoku (Japan)", "KYGJ", 0, 4, 0, 0, 0},
    {"Yoshi - Topsy-Turvy (USA)", "KYGE", 0, 1, 0, 0, 0},
    {"Yu-Gi-Oh! GX - Duel Academy (USA)", "BYGE", 0, 2, 0, 0, 1},
    {"Yu-Gi-Oh! - Ultimate Masters - 2006 (Europe)(En,Jp,Fr,De,Es,It)", "BY6P", 0, 2, 0, 0, 0},
    {"Zoku Bokura no Taiyou - Taiyou Shounen Django (Japan)", "U32J", 0, 0, 1, 0, 0}};

static void load_image_preferences(void) {
	char buffer[5];
	buffer[0] = rom[0xac];
	buffer[1] = rom[0xad];
	buffer[2] = rom[0xae];
	buffer[3] = rom[0xaf];
	buffer[4] = 0;
	printf("GameID in ROM is: %s\n", buffer);

	bool found = false;
	int found_no = 0;

	for (int i = 0; i < 256; i++) {
		if (!strcmp(gbaover[i].romid, buffer)) {
			found = true;
			found_no = i;
			break;
		}
	}

	if (found) {
		printf("Found ROM in vba-over list.\n");

		enableRtc = gbaover[found_no].rtcEnabled;

		if (gbaover[found_no].flashSize != 0)
			flashSize = gbaover[found_no].flashSize;
		else
			flashSize = 65536;

		cpuSaveType = gbaover[found_no].saveType;

		mirroringEnable = gbaover[found_no].mirroringEnabled;
	}

	printf("RTC = %d.\n", enableRtc);
	printf("flashSize = %d.\n", flashSize);
	printf("cpuSaveType = %d.\n", cpuSaveType);
	printf("mirroringEnable = %d.\n", mirroringEnable);
}

static void gba_init(void) {
	cpuSaveType = 0;
	flashSize = 0x10000;
	enableRtc = false;
	mirroringEnable = false;

	load_image_preferences();

	if (flashSize == 0x10000 || flashSize == 0x20000) flashSetSize(flashSize);

	if (enableRtc) rtcEnable(enableRtc);

	doMirroring(mirroringEnable);

	soundSetSampleRate(AUDIO_SAMPLERATE);

#if HAVE_HLE_BIOS
	bool usebios = false;

	struct retro_variable var;

	var.key = "vbanext_bios";
	var.value = NULL;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "disabled") == 0)
			usebios = false;
		else if (strcmp(var.value, "enabled") == 0)
			usebios = true;
	}

	if (usebios && filename_bios[0])
		CPUInit(filename_bios, true);
	else
		CPUInit(NULL, false);
#else
	CPUInit(NULL, false);
#endif
	CPUReset();

	soundReset();

	uint8_t *state_buf = (uint8_t *)malloc(2000000);
	serialize_size = CPUWriteState(state_buf, 2000000);
	free(state_buf);
}

void retro_deinit(void) {
#if THREADED_RENDERER
	ThreadedRendererStop();
#endif

	CPUCleanUp();
}

void retro_reset(void) { CPUReset(); }
/*
static const unsigned binds[] = {RETRO_DEVICE_ID_JOYPAD_A,     RETRO_DEVICE_ID_JOYPAD_B,     RETRO_DEVICE_ID_JOYPAD_SELECT,
				 RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT,
				 RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_DOWN,  RETRO_DEVICE_ID_JOYPAD_R,
				 RETRO_DEVICE_ID_JOYPAD_L};

static const unsigned binds2[] = {RETRO_DEVICE_ID_JOYPAD_B,     RETRO_DEVICE_ID_JOYPAD_A,     RETRO_DEVICE_ID_JOYPAD_SELECT,
				  RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT,
				  RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_DOWN,  RETRO_DEVICE_ID_JOYPAD_R,
				  RETRO_DEVICE_ID_JOYPAD_L};*/

static bool has_video_frame;
static int audio_samples_written;

void pause_emulation() {
	mutexLock(&emulationLock);
	emulationPaused = true;
	uiSetState(statePaused);
	mutexUnlock(&emulationLock);

	mutexLock(&audioLock);
	memset(audioTransferBuffer, 0, AUDIO_TRANSFERBUF_SIZE * sizeof(u32));
	audioTransferBufferUsed = AUDIO_TRANSFERBUF_SIZE;
	mutexUnlock(&audioLock);
}
void unpause_emulation() {
	mutexLock(&emulationLock);
	emulationPaused = false;
	uiSetState(stateRunning);
	mutexUnlock(&emulationLock);
}

void retro_run() {
	mutexLock(&inputLock);
	joy = 0;
	for (unsigned i = 0; i < 10; i++) {
		joy |= ((bool)(inputTransferKeysHeld & buttonMap[i])) << i;
	}
	mutexUnlock(&inputLock);

	has_video_frame = false;
	audio_samples_written = 0;
	UpdateJoypad();
	do {
		CPULoop();
	} while (!has_video_frame);
}

bool retro_load_game() {
	int ret = CPULoadRom(currentRomPath);

	gba_init();

	char saveFileName[PATH_LENGTH];
	romPathWithExt(saveFileName, PATH_LENGTH, "sav");
	if (CPUReadBatteryFile(saveFileName)) uiStatusMsg("loaded savefile %s", saveFileName);

	return ret;
}

static unsigned g_audio_frames = 0;
static unsigned g_video_frames = 0;

void retro_unload_game(void) {
	printf("[VBA] Sync stats: Audio frames: %u, Video frames: %u, AF/VF: %.2f\n", g_audio_frames, g_video_frames,
	       (float)g_audio_frames / g_video_frames);
	g_audio_frames = 0;
	g_video_frames = 0;

	char saveFilename[PATH_LENGTH];
	romPathWithExt(saveFilename, PATH_LENGTH, "sav");
	if (CPUWriteBatteryFile(saveFilename)) uiStatusMsg("wrote savefile %s", saveFilename);
}

void audio_thread_main(void *) {
	AudioOutBuffer sources[2];

	u32 raw_data_size = (AUDIO_BUFFER_SAMPLES * sizeof(u32) + 0xfff) & ~0xfff;
	u32 *raw_data[2];
	for (int i = 0; i < 2; i++) {
		raw_data[i] = (u32 *)memalign(0x1000, raw_data_size);
		memset(raw_data[i], 0, raw_data_size);

		sources[i].next = 0;
		sources[i].buffer = raw_data[i];
		sources[i].buffer_size = raw_data_size;
		sources[i].data_size = AUDIO_BUFFER_SAMPLES * sizeof(u32);
		sources[i].data_offset = 0;

		audoutAppendAudioOutBuffer(&sources[i]);
	}

	int offset = 0;
	while (running) {
		u32 count;
		AudioOutBuffer *released;
		audoutWaitPlayFinish(&released, &count, U64_MAX);

		mutexLock(&audioLock);

		u32 size = (audioTransferBufferUsed < AUDIO_BUFFER_SAMPLES ? audioTransferBufferUsed : AUDIO_BUFFER_SAMPLES) * sizeof(u32);
		memcpy(released->buffer, audioTransferBuffer, size);
		released->data_size = size == 0 ? AUDIO_BUFFER_SAMPLES * sizeof(u32) : size;

		audioTransferBufferUsed -= size / sizeof(u32);
		memmove(audioTransferBuffer, audioTransferBuffer + (size / sizeof(u32)), audioTransferBufferUsed * sizeof(u32));

		mutexUnlock(&audioLock);

		audoutAppendAudioOutBuffer(released);
	}
	free(raw_data[0]);
	free(raw_data[1]);
}

void systemOnWriteDataToSoundBuffer(int16_t *finalWave, int length) {
	mutexLock(&audioLock);
	if (audioTransferBufferUsed + length >= AUDIO_TRANSFERBUF_SIZE) {
		mutexUnlock(&audioLock);
		return;
	}

	memcpy(audioTransferBuffer + audioTransferBufferUsed, finalWave, length * sizeof(s16));

	audioTransferBufferUsed += length / 2;
	mutexUnlock(&audioLock);

	g_audio_frames += length / 2;
}

u32 bgr_555_to_rgb_888_table[32 * 32 * 32];
void init_color_lut() {
	for (u8 r5 = 0; r5 < 32; r5++) {
		for (u8 g5 = 0; g5 < 32; g5++) {
			for (u8 b5 = 0; b5 < 32; b5++) {
				u8 r8 = (u8)((r5 * 527 + 23) >> 6);
				u8 g8 = (u8)((g5 * 527 + 23) >> 6);
				u8 b8 = (u8)((b5 * 527 + 23) >> 6);
				u32 bgr555 = (r5 << 10) | (g5 << 5) | b5;
				u32 bgr888 = r8 | (g8 << 8) | (b8 << 16) | (255 << 24);
				bgr_555_to_rgb_888_table[bgr555] = bgr888;
			}
		}
	}
}

static inline u32 bbgr_555_to_rgb_888(u16 in) {
	u8 r = (in >> 10) << 3;
	u8 g = ((in >> 5) & 31) << 3;
	u8 b = (in & 31) << 3;
	return r | (g << 8) | (b << 16) | (255 << 24);
}

void systemDrawScreen() {
	mutexLock(&videoLock);
	memcpy(videoTransferBuffer, pix, sizeof(u16) * 256 * 160);
	mutexUnlock(&videoLock);

	g_video_frames++;
	has_video_frame = true;
}

void systemMessage(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void threadFunc(void *args) {
	mutexLock(&emulationLock);
	retro_init();
	mutexUnlock(&emulationLock);
	init_color_lut();

	while (running) {
#define SECONDS_PER_TICKS (1.0 / 19200000)
#define TARGET_FRAMETIME (1.0 / 59.8260982880808)
		double startTime = (double)svcGetSystemTick() * SECONDS_PER_TICKS;

		mutexLock(&emulationLock);

		if (emulationRunning && !emulationPaused) retro_run();

		mutexUnlock(&emulationLock);

		double endTime = (double)svcGetSystemTick() * SECONDS_PER_TICKS;

		if (endTime - startTime < TARGET_FRAMETIME && !(inputTransferKeysHeld & KEY_ZL)) {
			svcSleepThread((u64)fabs((TARGET_FRAMETIME - (endTime - startTime)) * 1000000000 - 100));
		}

		/*int audioBuffersPlaying = 0;
		for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
			bool contained;
			if (R_SUCCEEDED(audoutContainsAudioOutBuffer(&audioBuffer[i].buffer, &contained))) audioBuffersPlaying += contained;
		}
		if ((audioBuffersPlaying == 0 && audioBufferQueueNext >= 2) || audioBuffersPlaying > 2) {
			printf("audioBuffersPlaying %d\n", audioBuffersPlaying);
			for (int i = 0; i < audioBufferQueueNext; i++) {
				audioBufferQueue[i]->enqueued = false;
				audoutAppendAudioOutBuffer(&audioBufferQueue[i]->buffer);
			}
			audioBufferQueueNext = 0;
		}*/
	}

	mutexLock(&emulationLock);
	retro_deinit();
	mutexUnlock(&emulationLock);
}

static void applyConfig() {
	mutexLock(&emulationLock);
	SetFrameskip(frameSkipValues[frameSkip]);

	if (!disableAnalogStick) {
		buttonMap[4] = KEY_RIGHT;
		buttonMap[5] = KEY_LEFT;
		buttonMap[6] = KEY_UP;
		buttonMap[7] = KEY_DOWN;
	} else {
		buttonMap[4] = KEY_DRIGHT;
		buttonMap[5] = KEY_DLEFT;
		buttonMap[6] = KEY_DUP;
		buttonMap[7] = KEY_DDOWN;
	}
	mutexUnlock(&emulationLock);
}

int main(int argc, char *argv[]) {
#ifdef NXLINK_STDIO
	socketInitializeDefault();
	nxlinkStdio();
#endif

	zoomInit(2048, 2048);

	gfxInitResolutionDefault();
	gfxInitDefault();
	gfxConfigureAutoResolutionDefault(true);

	audioTransferBuffer = (u32 *)malloc(AUDIO_TRANSFERBUF_SIZE * sizeof(u32));
	mutexInit(&audioLock);

	audoutInitialize();
	audoutStartAudioOut();

	Thread audio_thread;
	threadCreate(&audio_thread, audio_thread_main, NULL, 0x10000, 0x2B, 2);
	threadStart(&audio_thread);

	videoTransferBuffer = (u16 *)malloc(256 * 160 * sizeof(u16));
	conversionBuffer = (u32 *)malloc(256 * 160 * sizeof(u32));
	mutexInit(&videoLock);

	uint32_t showFrametime = 0;

	uiInit();

	uiAddSetting("Show avg. frametime", &showFrametime, 2, stringsNoYes);
	uiAddSetting("Screen scaling method", &scalingFilter, filtersCount, filterStrNames);
	uiAddSetting("Frameskip", &frameSkip, sizeof(frameSkipValues) / sizeof(frameSkipValues[0]), frameSkipNames);
	uiAddSetting("Disable analog stick", &disableAnalogStick, 2, stringsNoYes);
	uiFinaliseAndLoadSettings();
	applyConfig();

	uiSetState(stateFileselect);

	mutexInit(&inputLock);
	mutexInit(&emulationLock);

	Thread mainThread;
	threadCreate(&mainThread, threadFunc, NULL, 0x4000, 0x30, 1);
	threadStart(&mainThread);

	char saveFilename[PATH_LENGTH];

	double frameTimeSum = 0;
	int frameTimeN = 0;

	while (appletMainLoop() && running) {
		double startTime = (double)svcGetSystemTick() * SECONDS_PER_TICKS;

		u32 currentFBWidth, currentFBHeight;
		u8 *currentFB = gfxGetFramebuffer(&currentFBWidth, &currentFBHeight);
		memset(currentFB, 0, sizeof(u32) * currentFBWidth * currentFBHeight);

		hidScanInput();
		u32 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
		u32 keysHeld = hidKeysHeld(CONTROLLER_P1_AUTO);

		mutexLock(&inputLock);
		inputTransferKeysHeld = keysHeld;
		mutexUnlock(&inputLock);

		if (emulationRunning && !emulationPaused) {
			mutexLock(&videoLock);

			for (int i = 0; i < 256 * 160; i++) conversionBuffer[i] = bbgr_555_to_rgb_888(videoTransferBuffer[i]);

			u32 *srcImage = conversionBuffer;

			float scale = (float)currentFBHeight / 160.f;
			int dstWidth = (int)(scale * 240.f);
			int dstHeight = MIN(currentFBHeight, (int)(scale * 160.f));
			int offsetX = currentFBWidth / 2 - dstWidth / 2;
			if (scalingFilter == filterBilinear) {
				int desiredSize = dstWidth * dstHeight * sizeof(u32);
				if (upscaleBufferSize < desiredSize) {
					upscaleBuffer = (u32 *)realloc(upscaleBuffer, desiredSize);
					upscaleBufferSize = desiredSize;
				}

				zoomResizeBilinear_RGB8888((u8 *)upscaleBuffer, dstWidth, dstHeight, (uint8_t *)srcImage, 240, 160,
							   256 * sizeof(u32));

				u32 *src = upscaleBuffer;
				u32 *dst = ((u32 *)currentFB) + offsetX;
				for (int i = 0; i < dstHeight; i++) {
					memcpy(dst, src, dstWidth * sizeof(u32));
					src += dstWidth;
					dst += currentFBWidth;
				}
			} else if (scalingFilter == filterNearest) {
				Surface srcSurface, dstSurface;
				srcSurface.w = 240;
				srcSurface.h = 160;
				srcSurface.pixels = srcImage;
				srcSurface.pitch = 256 * sizeof(u32);

				dstSurface.w = (int)(scale * 240.f);
				dstSurface.h = MIN(currentFBHeight, (int)(scale * 160.f));
				dstSurface.pixels = ((u32 *)currentFB) + offsetX;
				dstSurface.pitch = currentFBWidth * sizeof(u32);

				zoomSurfaceRGBA(&srcSurface, &dstSurface, 0, 0, 0);
			} else if (scalingFilter == filterNearestInteger) {
				unsigned intScale = (unsigned)floor(scale);
				unsigned offsetX = currentFBWidth / 2 - (intScale * 240) / 2;
				unsigned offsetY = currentFBHeight / 2 - (intScale * 160) / 2;
				for (int y = 0; y < 160; y++) {
					for (int x = 0; x < 240; x++) {
						int idx0 = x * intScale + offsetX + (y * intScale + offsetY) * currentFBWidth;
						int idx1 = (x + y * 256);
						u32 val = srcImage[idx1];
						for (unsigned j = 0; j < intScale * currentFBWidth; j += currentFBWidth) {
							for (unsigned i = 0; i < intScale; i++) {
								((u32 *)currentFB)[idx0 + i + j] = val;
							}
						}
					}
				}
			}

			mutexUnlock(&videoLock);
		}

		bool actionStopEmulation = false;
		bool actionStartEmulation = false;

		if (keysDown & KEY_MINUS && uiGetState() != stateRunning) {  // hack, TODO: improve UI state machine
			if (uiGetState() == stateSettings)
				uiSetState(emulationRunning ? statePaused : stateFileselect);
			else
				uiSetState(stateSettings);
		}

		UIResult result;
		switch ((result = uiLoop(currentFB, currentFBWidth, currentFBHeight, keysDown))) {
			case resultSelectedFile:
				actionStopEmulation = true;
				actionStartEmulation = true;
				break;
			case resultClose:
				if (uiGetState() == statePaused) {
					actionStopEmulation = true;
					uiSetState(stateFileselect);
				} else {
					uiSetState(emulationRunning ? statePaused : stateFileselect);
				}
				break;
			case resultExit:
				actionStopEmulation = true;
				running = false;
				break;
			case resultUnpause:
				unpause_emulation();
			case resultLoadState:
			case resultSaveState: {
				mutexLock(&emulationLock);
				char stateFilename[PATH_LENGTH];
				romPathWithExt(stateFilename, PATH_LENGTH, "ram");

				u8 *buffer = (u8 *)malloc(serialize_size);

				if (result == resultLoadState) {
					FILE *f = fopen(stateFilename, "rb");
					if (f) {
						if (fread(buffer, 1, serialize_size, f) != serialize_size ||
						    !CPUReadState(buffer, serialize_size))
							uiStatusMsg("Failed to read save state %s", stateFilename);

						fclose(f);
					} else
						printf("Failed to open %s for read", stateFilename);
				} else {
					if (!CPUWriteState(buffer, serialize_size))
						uiStatusMsg("Failed to write save state %s", stateFilename);

					FILE *f = fopen(stateFilename, "wb");
					if (f) {
						if (fwrite(buffer, 1, serialize_size, f) != serialize_size)
							printf("Failed to write to %s", stateFilename);
						fclose(f);
					} else
						printf("Failed to open %s for write", stateFilename);
				}

				free(buffer);
				mutexUnlock(&emulationLock);
			} break;
			case resultSettingsChanged:
				applyConfig();
				break;
			case resultNone:
			default:
				break;
		}

		if (actionStopEmulation && emulationRunning) {
			mutexLock(&emulationLock);
			retro_unload_game();
			mutexUnlock(&emulationLock);
		}
		if (actionStartEmulation) {
			mutexLock(&emulationLock);

			uiSetState(stateRunning);

			uiGetSelectedFile(currentRomPath, PATH_LENGTH);
			romPathWithExt(saveFilename, PATH_LENGTH, "sav");

			retro_load_game();

			SetFrameskip(frameSkipValues[frameSkip]);

			emulationRunning = true;
			emulationPaused = false;

			mutexUnlock(&emulationLock);
		}

		if (emulationRunning && !emulationPaused && keysDown & KEY_X) pause_emulation();

		if (emulationRunning && !emulationPaused && --autosaveCountdown == 0) {
			mutexLock(&emulationLock);
			if (CPUWriteBatteryFile(saveFilename)) uiStatusMsg("wrote savefile %s", saveFilename);
			mutexUnlock(&emulationLock);
		}

		double endTime = (double)svcGetSystemTick() * SECONDS_PER_TICKS;
		frameTimeSum += endTime - startTime;

		if (emulationRunning && !emulationPaused && showFrametime) {
			char fpsBuffer[64];
			snprintf(fpsBuffer, 64, "avg: %fms curr: %fms", (float)frameTimeSum / (float)(frameTimeN++) * 1000.f,
				 (endTime - startTime) * 1000.f);
			uiDrawString(currentFB, currentFBWidth, currentFBHeight, fpsBuffer, 0, 8, 255, 255, 255);
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
		gfxWaitForVsync();
	}

	threadWaitForExit(&mainThread);
	threadClose(&mainThread);

	uiDeinit();

	free(conversionBuffer);
	free(videoTransferBuffer);

	threadWaitForExit(&audio_thread);
	threadClose(&audio_thread);

	audoutStopAudioOut();
	audoutExit();

	free(audioTransferBuffer);

	gfxExit();

	zoomDeinit();
	if (upscaleBuffer != NULL) free(upscaleBuffer);

#ifdef NXLINK_STDIO
	socketExit();
#endif

	return 0;
}