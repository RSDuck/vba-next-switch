#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../gba.h"
#include "../globals.h"
#include "../memory.h"
#include "../port.h"
#include "../sound.h"
#include "../system.h"
#include "../types.h"
#include "gbaover.h"

#include "util.h"
#include "zoom.h"

#include "draw.h"
#include "ui.h"

u8 *currentFB;
u32 currentFBWidth;
u32 currentFBHeight;

#include <switch.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SECONDS_PER_TICKS (1.0 / 19200000)
#define TARGET_FRAMETIME (1.0 / 59.8260982880808)

uint8_t libretro_save_buf[0x20000 + 0x2000]; /* Workaround for broken-by-design GBA save semantics. */

enum { filterNearestInteger,
	filterNearest,
	filterBilinear,
	filtersCount,
};

uint32_t scalingFilter = filterNearest;

const char *filterStrNames[] = {"Nearest Integer", "Nearest Fullscreen", "Bilinear Fullscreen(slow!)"};

extern uint64_t joy;
unsigned device_type = 0;

static uint32_t disableAnalogStick = 0;
static uint32_t switchRLButtons = 0;

static u32 *upscaleBuffer = NULL;
static int upscaleBufferSize = 0;

static bool emulationRunning = false;
static bool emulationPaused = false;

static const char *stringsNoYes[] = {"No", "Yes"};

static char currentRomPath[PATH_LENGTH] = {'\0'};

static Mutex videoLock;
static u16 *videoTransferBuffer;
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

uint32_t buttonMap[11] = {
    KEY_A, KEY_B, KEY_MINUS, KEY_PLUS, KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_R, KEY_L,
    KEY_ZR  // Speedhack Button
};

static bool has_video_frame;
static int audio_samples_written;

static unsigned g_audio_frames = 0;
static unsigned g_video_frames = 0;

static unsigned serialize_size = 0;

static bool scan_area(const uint8_t *data, unsigned size) {
	for (unsigned i = 0; i < size; i++)
		if (data[i] != 0xff) return true;

	return false;
}

void systemMessage(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
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

static void load_image_preferences(void) {
	char buffer[5];
	buffer[0] = rom[0xac];
	buffer[1] = rom[0xad];
	buffer[2] = rom[0xae];
	buffer[3] = rom[0xaf];
	buffer[4] = 0;

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
		enableRtc = gbaover[found_no].rtcEnabled;

		if (gbaover[found_no].flashSize != 0)
			flashSize = gbaover[found_no].flashSize;
		else
			flashSize = 65536;

		cpuSaveType = gbaover[found_no].saveType;

		mirroringEnable = gbaover[found_no].mirroringEnabled;
	}
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

void pause_emulation() {
	mutexLock(&emulationLock);
	emulationPaused = true;
	uiPushState(statePaused);
	char saveFilename[PATH_LENGTH];
	romPathWithExt(saveFilename, PATH_LENGTH, "sav");
	if (CPUWriteBatteryFile(saveFilename)) uiStatusMsg("Wrote save file.");
	mutexUnlock(&emulationLock);

	mutexLock(&audioLock);
	memset(audioTransferBuffer, 0, AUDIO_TRANSFERBUF_SIZE * sizeof(u32));
	audioTransferBufferUsed = AUDIO_TRANSFERBUF_SIZE;
	mutexUnlock(&audioLock);
}

void unpause_emulation() {
	mutexLock(&emulationLock);
	emulationPaused = false;
	uiPopState();
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
	if (CPUReadBatteryFile(saveFileName)) uiStatusMsg("Loaded save file.");

	return ret;
}

void retro_unload_game(void) {
	printf("[VBA] Sync stats: Audio frames: %u, Video frames: %u, AF/VF: %.2f\n", g_audio_frames, g_video_frames,
	       (float)g_audio_frames / g_video_frames);
	g_audio_frames = 0;
	g_video_frames = 0;

	char saveFilename[PATH_LENGTH];
	romPathWithExt(saveFilename, PATH_LENGTH, "sav");
	if (CPUWriteBatteryFile(saveFilename)) uiStatusMsg("Wrote save file.");
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

void systemDrawScreen() {
	mutexLock(&videoLock);
	memcpy(videoTransferBuffer, pix, sizeof(u16) * 256 * 160);
	mutexUnlock(&videoLock);

	g_video_frames++;
	has_video_frame = true;
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

void threadFunc(void *args) {
	mutexLock(&emulationLock);
	retro_init();
	mutexUnlock(&emulationLock);
	init_color_lut();

	while (running) {
		double startTime = (double)svcGetSystemTick() * SECONDS_PER_TICKS;

		mutexLock(&emulationLock);

		if (emulationRunning && !emulationPaused) retro_run();

		mutexUnlock(&emulationLock);

		double endTime = (double)svcGetSystemTick() * SECONDS_PER_TICKS;

		if (endTime - startTime < TARGET_FRAMETIME && !(inputTransferKeysHeld & buttonMap[10])) {
			svcSleepThread((u64)fabs((TARGET_FRAMETIME - (endTime - startTime)) * 1000000000 - 100));
		}
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

	if (!switchRLButtons) {
		buttonMap[8] = KEY_R;
		buttonMap[9] = KEY_L;
		buttonMap[10] = KEY_ZR;
	} else {
		buttonMap[8] = KEY_ZR;
		buttonMap[9] = KEY_ZL;
		buttonMap[10] = KEY_R;
	}
	mutexUnlock(&emulationLock);
}

int main(int argc, char *argv[]) {
#ifdef NXLINK_STDIO
	socketInitializeDefault();
	nxlinkStdio();
#endif

	zoomInit(2048, 2048);

	romfsInit();

	gfxInitResolutionDefault();
	gfxInitDefault();
	gfxConfigureAutoResolutionDefault(true);

	plInitialize();
	textInit();
	fontInit();
	timeInitialize();

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

	uiAddSetting("Show average frametime", &showFrametime, 2, stringsNoYes);
	uiAddSetting("Screen scaling method", &scalingFilter, filtersCount, filterStrNames);
	uiAddSetting("Frameskip", &frameSkip, sizeof(frameSkipValues) / sizeof(frameSkipValues[0]), frameSkipNames);
	uiAddSetting("Disable analog stick", &disableAnalogStick, 2, stringsNoYes);
	uiAddSetting("Switch R and L buttons to ZR and ZL (switches speedhack button too)", &switchRLButtons, 2, stringsNoYes);
	uiAddSetting("Use dark theme", &darkTheme, 2, stringsNoYes);
	uiFinaliseAndLoadSettings();
	applyConfig();

	uiPushState(stateFileselect);

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

		currentFB = gfxGetFramebuffer(&currentFBWidth, &currentFBHeight);
		memset(currentFB, 0, 4 * currentFBWidth * currentFBHeight);

		hidScanInput();
		u32 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
		u32 keysUp = hidKeysUp(CONTROLLER_P1_AUTO);

		if (emulationRunning && !emulationPaused) {
			mutexLock(&videoLock);

			for (int i = 0; i < 256 * 160; i++) conversionBuffer[i] = bbgr_555_to_rgb_888(videoTransferBuffer[i]);

			u32 *srcImage = conversionBuffer;

			float scale = (float)currentFBHeight / 160.f;
			int dstWidth = (int)(scale * 240.f);
			int dstHeight = MIN(currentFBHeight, (u32)(scale * 160.f));
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
				dstSurface.h = MIN(currentFBHeight, (u32)(scale * 160.f));
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

		/*if (keysDown & KEY_MINUS && uiGetState() != stateRunning) {  // hack, TODO: improve UI state machine
			if (uiGetState() == stateSettings)
				uiPopState();
			else
				uiPushState(stateSettings);
		}*/

		UIResult result;
		switch ((result = uiLoop(keysDown))) {
			case resultSelectedFile:
				actionStopEmulation = true;
				actionStartEmulation = true;
				break;
			case resultClose:
				uiPopState();
				if (uiGetState() == stateRunning) uiPopState();
				break;
			case resultOpenSettings:
				if (uiGetState() != stateRunning) {
					uiPopState();
					uiPushState(stateSettings);

				}
				break;
			case resultExit:
				actionStopEmulation = true;
				running = false;
				break;
			case resultUnpause:
				unpause_emulation();
				keysDown = 0;
				break;
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
			case resultSaveSettings:
				applyConfig();
				uiSaveSettings();
				uiPopState();
				uiPushState(statePaused);
				break;
			case resultCancelSettings:
				uiPopState();
				uiPushState(statePaused);
				break;
			case resultNone:
			default:
				break;
		}

		mutexLock(&inputLock);
		inputTransferKeysHeld |= keysDown;
		inputTransferKeysHeld &= ~keysUp;
		mutexUnlock(&inputLock);

		if (actionStopEmulation && emulationRunning) {
			mutexLock(&emulationLock);
			retro_unload_game();
			mutexUnlock(&emulationLock);
		}

		if (actionStartEmulation) {
			mutexLock(&emulationLock);

			uiPushState(stateRunning);

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
			CPUWriteBatteryFile(saveFilename);
			mutexUnlock(&emulationLock);
		}

		double endTime = (double)svcGetSystemTick() * SECONDS_PER_TICKS;
		frameTimeSum += endTime - startTime;

		if (emulationRunning && !emulationPaused && showFrametime) {
			char fpsBuffer[64];
			snprintf(fpsBuffer, 64, "Avg: %fms curr: %fms", (float)frameTimeSum / (float)(frameTimeN++) * 1000.f,
				 (endTime - startTime) * 1000.f);
      

			//uiDrawString(currentFB, currentFBWidth, currentFBHeight, fpsBuffer, 0, 8, 255, 255, 255);
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

	fontExit();

	gfxExit();
	plExit();

	timeExit();

	romfsExit();

	zoomDeinit();
	if (upscaleBuffer != NULL) free(upscaleBuffer);

#ifdef NXLINK_STDIO
	socketExit();
#endif

	return 0;
}