uint32_t set_shader = 0;
static uint32_t currently_selected_controller_menu = 0;

#include "menu-entries.h"

static menu menu_filebrowser = {
	"FILE BROWSER |",		// title
	FILE_BROWSER_MENU,		// enum
	0,				// selected item
	0,				// page
	1,				// refreshpage
	NULL				// items
};

static menu menu_generalvideosettings = {
"VIDEO |",			// title
GENERAL_VIDEO_MENU,		// enum
FIRST_VIDEO_SETTING,		// selected item
0,				// page
1,				// refreshpage
FIRST_VIDEO_SETTING,		// first setting
MAX_NO_OF_VIDEO_SETTINGS,	// max no of settings
items_generalsettings		// items
};

static menu menu_generalaudiosettings = {
"AUDIO |",			// title
GENERAL_AUDIO_MENU,		// enum
FIRST_AUDIO_SETTING,		// selected item
0,				// page
1,				// refreshpage
FIRST_AUDIO_SETTING,		// first setting
MAX_NO_OF_AUDIO_SETTINGS,	// max no of settings
items_generalsettings		// items
};

static menu menu_emu_settings = {
"VBA |",			// title
EMU_GENERAL_MENU,		// enum
FIRST_EMU_SETTING,		// selected item
0,				// page
1,				// refreshpage
FIRST_EMU_SETTING,		// first setting
MAX_NO_OF_EMU_SETTINGS,		// max no of settings
items_generalsettings		// items
};

static menu menu_emu_videosettings = {
"VBA VIDEO |",			// title
EMU_VIDEO_MENU,			// enum
FIRST_EMU_VIDEO_SETTING,	// selected item
0,				// page
1,				// refreshpage
FIRST_EMU_VIDEO_SETTING,	// first setting
MAX_NO_OF_EMU_VIDEO_SETTINGS,	// max no of settings
items_generalsettings		// items
};

static menu menu_emu_audiosettings = {
"VBA AUDIO |",			// title
EMU_AUDIO_MENU,			// enum
FIRST_EMU_AUDIO_SETTING,	// selected item
0,				// page
1,				// refreshpage
FIRST_EMU_AUDIO_SETTING,	// first setting
MAX_NO_OF_EMU_AUDIO_SETTINGS,	// max no of settings
items_generalsettings		// items
};

static menu menu_pathsettings = {
"PATH |",			// title
PATH_MENU,			// enum
FIRST_PATH_SETTING,		// selected item
0,				// page
1,				// refreshpage
FIRST_PATH_SETTING,		// first setting
MAX_NO_OF_PATH_SETTINGS,	// max no of settings
items_generalsettings		// items
};

static menu menu_controlssettings = {
	"CONTROLS |",			// title
	CONTROLS_MENU,			// enum
	FIRST_CONTROLS_SETTING_PAGE_1,	// selected item
	0,				// page
	1,				// refreshpage
	FIRST_CONTROLS_SETTING_PAGE_1,	// first setting
	MAX_NO_OF_CONTROLS_SETTINGS,	// max no of path settings
	items_generalsettings		// items
};

static void produce_menubar(uint32_t menu_enum)
{
	cellDbgFontPuts		(0.09f,	0.05f,	Emulator_GetFontSize(),	menu_enum == GENERAL_VIDEO_MENU ? RED : GREEN,		menu_generalvideosettings.title);
	cellDbgFontPuts		(0.19f,	0.05f,  Emulator_GetFontSize(),  menu_enum == GENERAL_AUDIO_MENU ? RED : GREEN,  menu_generalaudiosettings.title);
	cellDbgFontPuts		(0.29f,	0.05f,	Emulator_GetFontSize(),	menu_enum == EMU_GENERAL_MENU ? RED : GREEN,	menu_emu_settings.title);
	cellDbgFontPuts		(0.38f,  0.05f,  Emulator_GetFontSize(),  menu_enum == EMU_VIDEO_MENU ? RED : GREEN,   menu_emu_videosettings.title);
	cellDbgFontPuts		(0.54f,  0.05f,  Emulator_GetFontSize(),  menu_enum == EMU_AUDIO_MENU ? RED : GREEN,   menu_emu_audiosettings.title);
	cellDbgFontPuts		(0.70f,	0.05f,	Emulator_GetFontSize(),	menu_enum == PATH_MENU ? RED : GREEN,			menu_pathsettings.title);
	cellDbgFontPuts		(0.80f, 0.05f,	Emulator_GetFontSize(), menu_enum == CONTROLS_MENU ? RED : GREEN,		menu_controlssettings.title); 
	cellDbgFontDraw();
}
