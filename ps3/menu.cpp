/******************************************************************************* 
 * menu.cpp - VBANext PS3
 *
 *  Created on: August 20, 2010
********************************************************************************/

#include  <string.h>

#include "cellframework2/input/pad_input.h"
#include "cellframework2/fileio/file_browser.h"

#include "emu-ps3.hpp"
#include "menu.hpp"
#include "ps3video.hpp"
#include "ps3input.h"
#include "conf/settings.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define NUM_ENTRY_PER_PAGE 19
#define NUM_ENTRY_PER_SETTINGS_PAGE 18

static menu menuStack[25];
int menu_is_running = 0;			// is the menu running
static int menuStackindex = 0;
static bool set_initial_dir_tmpbrowser;
filebrowser_t browser;				// main file browser->for rom browser
filebrowser_t tmpBrowser;			// tmp file browser->for everything else

#include "menu/menu-logic.h"

#define FILEBROWSER_DELAY	100000
#define SETTINGS_DELAY		150000	

#define ROM_EXTENSIONS "gb|gbc|gba|GBA|GB|GBC|zip|ZIP"

static void UpdateBrowser(filebrowser_t * b)
{
	static uint64_t old_state = 0;
	uint64_t state = cell_pad_input_poll_device(0);
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	if(frame_count < special_action_msg_expired)
	{
	}
	else
	{
		if (CTRL_LSTICK_DOWN(state))
		{
			if(b->currently_selected < b->file_count-1)
			{
				FILEBROWSER_INCREMENT_ENTRY_POINTER(b);
				set_text_message("", 4);
			}
		}

		if (CTRL_DOWN(state))
		{
			if(b->currently_selected < b->file_count-1)
			{
				FILEBROWSER_INCREMENT_ENTRY_POINTER(b);
				set_text_message("", 7);
			}
		}

		if (CTRL_LSTICK_UP(state))
		{
			if(b->currently_selected > 0)
			{
				FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
				set_text_message("", 4);
			}
		}

		if (CTRL_UP(state))
		{
			if(b->currently_selected > 0)
			{
				FILEBROWSER_DECREMENT_ENTRY_POINTER(b);
				set_text_message("", 7);
			}
		}

		if (CTRL_RIGHT(state))
		{
			b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));
			set_text_message("", 7);
		}

		if (CTRL_LSTICK_RIGHT(state))
		{
			b->currently_selected = (MIN(b->currently_selected + 5, b->file_count-1));
			set_text_message("", 4);
		}

		if (CTRL_LEFT(state))
		{
			if (b->currently_selected <= 5)
				b->currently_selected = 0;
			else
				b->currently_selected -= 5;

			set_text_message("", 7);
		}

		if (CTRL_LSTICK_LEFT(state))
		{
			if (b->currently_selected <= 5)
				b->currently_selected = 0;
			else
				b->currently_selected -= 5;

			set_text_message("", 4);
		}

		if (CTRL_R1(state))
		{
			b->currently_selected = (MIN(b->currently_selected + NUM_ENTRY_PER_PAGE, b->file_count-1));
			set_text_message("", 7);
		}

		if (CTRL_L1(state))
		{
			if (b->currently_selected <= NUM_ENTRY_PER_PAGE)
				b->currently_selected= 0;
			else
				b->currently_selected -= NUM_ENTRY_PER_PAGE;

			set_text_message("", 7);
		}

		if (CTRL_CIRCLE(button_was_pressed))
		{
			old_state = state;
			filebrowser_pop_directory(b);
		}


		if (CTRL_L3(state) && CTRL_R3(state))
		{
			// if a rom is loaded then resume it
			if (Emulator_IsROMLoaded())
			{
				menu_is_running = 0;
				Emulator_StartROMRunning(1);
				set_text_message("", 15);
			}
		}

		old_state = state;
	}
}

static void RenderBrowser(filebrowser_t * b)
{
	uint32_t file_count = b->file_count;
	int current_index = b->currently_selected;

	int page_number = current_index / NUM_ENTRY_PER_PAGE;
	int page_base = page_number * NUM_ENTRY_PER_PAGE;
	float currentX = 0.09f;
	float currentY = 0.09f;
	float ySpacing = 0.035f;

	for (int i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
	{
		currentY = currentY + ySpacing;
		cellDbgFontPuts(currentX, currentY, Emulator_GetFontSize(), i == current_index ? RED : b->cur[i].d_type == CELL_FS_TYPE_DIRECTORY ? GREEN : WHITE, b->cur[i].d_name);
		cellDbgFontDraw();
	}
	cellDbgFontDraw();
}

#include "menu/menu-choice.h"

#include "menu/menu-helpmessages.h"

#include "menu/settings-logic.h"

static void select_setting(menu * menu_obj)
{
	uint64_t state, diff_state, button_was_pressed, i;
	static uint64_t old_state = 0;

	state = cell_pad_input_poll_device(0);
	diff_state = old_state ^ state;
	button_was_pressed = old_state & diff_state;


	if(frame_count < special_action_msg_expired)
	{
	}
	else
	{
		/* back to ROM menu if CIRCLE is pressed */
		if (CTRL_L1(button_was_pressed) || CTRL_CIRCLE(button_was_pressed))
		{
			menuStackindex--;
			old_state = state;
			return;
		}

		if (CTRL_R1(button_was_pressed))
		{
			switch(menu_obj->enum_id)
			{
				case GENERAL_VIDEO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_generalaudiosettings;
					old_state = state;
					break;
				case GENERAL_AUDIO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_settings;
					old_state = state;
					break;
				case EMU_GENERAL_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_videosettings;
					old_state = state;
					break;
				case EMU_VIDEO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_emu_audiosettings;
					old_state = state;
					break;
				case EMU_AUDIO_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_pathsettings;
					old_state = state;
					break;
				case PATH_MENU:
					menuStackindex++;
					menuStack[menuStackindex] = menu_controlssettings;
					old_state = state;
					break;
				case CONTROLS_MENU:
					break;
			}
		}

		/* down to next setting */

		if (CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))
		{
			menu_obj->selected++;

			if (menu_obj->selected >= menu_obj->max_settings)
				menu_obj->selected = menu_obj->first_setting; 

			if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
				menu_obj->page = menu_obj->items[menu_obj->selected].page;

			set_text_message("", 7);
		}

		/* up to previous setting */

		if (CTRL_UP(state) || CTRL_LSTICK_UP(state))
		{
			if (menu_obj->selected == menu_obj->first_setting)
				menu_obj->selected = menu_obj->max_settings-1;
			else
				menu_obj->selected--;

			if (menu_obj->items[menu_obj->selected].page != menu_obj->page)
				menu_obj->page = menu_obj->items[menu_obj->selected].page;

			set_text_message("", 7);
		}

		/* if a rom is loaded then resume it */

		if (CTRL_L3(state) && CTRL_R3(state))
		{
			if (Emulator_IsROMLoaded())
			{
				menu_is_running = 0;
				Emulator_StartROMRunning(1);
				set_text_message("", 15);
			}
			old_state = state;
			return;
		}


		producesettingentry(menu_obj->selected);
	}

	display_menubar(menu_obj->enum_id);
	cellDbgFontDraw();

	for ( i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
	{
		if(menu_obj->items[i].page == menu_obj->page)
		{
			cellDbgFontPuts(menu_obj->items[i].text_xpos, menu_obj->items[i].text_ypos, Emulator_GetFontSize(), menu_obj->selected == menu_obj->items[i].enum_id ? menu_obj->items[i].text_selected_color : menu_obj->items[i].text_unselected_color, menu_obj->items[i].text);
			display_label_value(i);
			cellDbgFontDraw();
		}
	}

	display_help_text(menu_obj->selected);

	cellDbgFontPuts(0.09f, 0.91f, Emulator_GetFontSize(), YELLOW, "UP/DOWN - select  L3+R3 - resume game   X/LEFT/RIGHT - change");
	cellDbgFontPuts(0.09f, 0.95f, Emulator_GetFontSize(), YELLOW, "START - default   L1/CIRCLE - go back   R1 - go forward");
	cellDbgFontDraw();
	old_state = state;
}

static void do_ROMMenu(void)
{
	char rom_path[MAX_PATH_LENGTH];
	char newpath[1024];

	uint64_t state = cell_pad_input_poll_device(0);
	static uint64_t old_state = 0;
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	UpdateBrowser(&browser);

	if (CTRL_SELECT(button_was_pressed))
	{
		menuStackindex++;
		menuStack[menuStackindex] = menu_generalvideosettings;
	}

	if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&browser, "/", ROM_EXTENSIONS);

	if (CTRL_CROSS(button_was_pressed))
	{
		if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
		{
			//if 'filename' is in fact '..' - then pop back directory instead of adding '..' to filename path
			if(browser.currently_selected == 0)
			{
				old_state = state;
				filebrowser_pop_directory(&browser);
			}
			else
			{
				const char * separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser),"/") == 0) ? "" : "/";
				snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(browser));
				filebrowser_push_directory(&browser, newpath, true);
			}
		}
		else if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
		{
			snprintf(rom_path, sizeof(rom_path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), FILEBROWSER_GET_CURRENT_FILENAME(browser));

			menu_is_running = 0;

			// switch emulator to emulate mode
			Emulator_StartROMRunning(1);

			Emulator_RequestLoadROM(rom_path);

			old_state = state;
			return;
		}
	}


	if (FILEBROWSER_IS_CURRENT_A_DIRECTORY(browser))
	{
		if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"app_home") || !strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),"host_root"))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, RED, "WARNING - This path only works on DEX PS3 systems. Do not attempt to open\n this directory on CEX PS3 systems, or you might have to restart!");
		else if(!strcmp(FILEBROWSER_GET_CURRENT_FILENAME(browser),".."))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to go back to the previous directory.");
		else
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to enter the directory.");
	}

	if (FILEBROWSER_IS_CURRENT_A_FILE(browser))
		cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	"FILE BROWSER");
	cellDbgFontPrintf (0.7f, 0.05f, 0.82f, WHITE, "%s v%s", EMULATOR_NAME, EMULATOR_VERSION);
	cellDbgFontPrintf (0.09f, 0.09f, Emulator_GetFontSize(), YELLOW, "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser));
	cellDbgFontPuts   (0.09f, 0.93f, Emulator_GetFontSize(), YELLOW,
	"L3 + R3 - resume game           SELECT - Settings screen");
	cellDbgFontDraw();

	RenderBrowser(&browser);
	old_state = state;
}

static void menu_init_settings_pages(menu * menu_obj)
{
	int page, i, j;
	float increment;

	page = 0;
	j = 0;
	increment = 0.13f;

	for(i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
	{
		if(!(j < (NUM_ENTRY_PER_PAGE)))
		{
			j = 0;
			increment = 0.13f;
			page++;
		}

		menu_obj->items[i].text_xpos = 0.09f;
		menu_obj->items[i].text_ypos = increment; 
		menu_obj->items[i].page = page;
		increment += 0.03f;
		j++;
	}
	menu_obj->refreshpage = 0;
}

void menu_init(void)
{
	filebrowser_new(&browser, Settings.PS3PathROMDirectory, ROM_EXTENSIONS);

	menu_init_settings_pages(&menu_generalvideosettings);
	menu_init_settings_pages(&menu_generalaudiosettings);
	menu_init_settings_pages(&menu_emu_settings);
	menu_init_settings_pages(&menu_emu_videosettings);
	menu_init_settings_pages(&menu_emu_audiosettings);
	menu_init_settings_pages(&menu_pathsettings);
	menu_init_settings_pages(&menu_controlssettings);
}

void menu_loop(void)
{
	menuStack[0] = menu_filebrowser;
	menuStack[0].enum_id = FILE_BROWSER_MENU;

	// menu loop
	menu_is_running = true;

	do
	{
		glClear(GL_COLOR_BUFFER_BIT);
		ps3graphics_draw_menu(1920, 1080);

		switch(menuStack[menuStackindex].enum_id)
		{
			case FILE_BROWSER_MENU:
				do_ROMMenu();
				break;
			case GENERAL_VIDEO_MENU:
			case GENERAL_AUDIO_MENU:
			case EMU_GENERAL_MENU:
			case EMU_VIDEO_MENU:
			case EMU_AUDIO_MENU:
			case PATH_MENU:
			case CONTROLS_MENU:
				select_setting(&menuStack[menuStackindex]);
				break;
			case SHADER_CHOICE:
			case PRESET_CHOICE:
			case BORDER_CHOICE:
			case PATH_BIOSCHOICE:
			case INPUT_PRESET_CHOICE:
				do_select_file(menuStack[menuStackindex].enum_id);
				break;
			case PATH_SAVESTATES_DIR_CHOICE:
			case PATH_DEFAULT_ROM_DIR_CHOICE:
			case PATH_SRAM_DIR_CHOICE:
				do_pathChoice(menuStack[menuStackindex].enum_id);
				break;
		}

		psglSwap();
		cellSysutilCheckCallback();
#ifdef CELL_DEBUG_CONSOLE
		cellConsolePoll();
#endif
	}while (menu_is_running);
}
