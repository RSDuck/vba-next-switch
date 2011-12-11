/******************************************************************************* 
 * menu.cpp - VBANext PS3
 *
 *  Created on: August 20, 2010
********************************************************************************/

#include  <string.h>

#include "cellframework2/input/pad_input.h"
#include "cellframework/fileio/FileBrowser.hpp"

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
static bool update_item_colors = true;
static bool set_initial_dir_tmpbrowser;
filebrowser_t browser;				// main file browser->for rom browser
filebrowser_t tmpBrowser;			// tmp file browser->for everything else

#include "menu/menu-logic.h"

#define FILEBROWSER_DELAY	100000
#define FILEBROWSER_DELAY_DIVIDED_BY_3 33333
#define SETTINGS_DELAY		150000	

#define ROM_EXTENSIONS "gb|gbc|gba|GBA|GB|GBC|zip|ZIP"

static void UpdateBrowser(filebrowser_t * b)
{
	static uint64_t old_state = 0;
	uint64_t state = cell_pad_input_poll_device(0);
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	if (CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))
	{
		if(b->currently_selected < b->dir[b->directory_stack_size].file_count-1)
		{
			filebrowser_increment_entry_pointer(b);

			if(CTRL_DOWN(state))
				sys_timer_usleep(FILEBROWSER_DELAY);
			else
				sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
		}
	}

	if (CTRL_UP(state) || CTRL_LSTICK_UP(state))
	{
		if(b->currently_selected > 0)
		{
			filebrowser_decrement_entry_pointer(b);

			if(CTRL_UP(state))
				sys_timer_usleep(FILEBROWSER_DELAY);
			else
				sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
		}
	}

	if (CTRL_RIGHT(state) || CTRL_LSTICK_RIGHT(state))
	{
		b->currently_selected = (MIN(b->currently_selected + 5, b->dir[b->directory_stack_size].file_count-1));
		if(CTRL_RIGHT(state))
			sys_timer_usleep(FILEBROWSER_DELAY);
		else
			sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
	}

	if (CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state))
	{
		if (b->currently_selected <= 5)
			b->currently_selected = 0;
		else
			b->currently_selected -= 5;

		if(CTRL_LEFT(state))
			sys_timer_usleep(FILEBROWSER_DELAY);
		else
			sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
	}

	if (CTRL_R1(state))
	{
		b->currently_selected = (MIN(b->currently_selected + NUM_ENTRY_PER_PAGE, b->dir[b->directory_stack_size].file_count-1));
		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_L1(state))
	{
		if (b->currently_selected <= NUM_ENTRY_PER_PAGE)
			b->currently_selected= 0;
		else
			b->currently_selected -= NUM_ENTRY_PER_PAGE;

		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_CIRCLE(button_was_pressed))
	{
		old_state = state;
		filebrowser_pop_directory(b);
	}

	old_state = state;
}

static void RenderBrowser(filebrowser_t * b)
{
	uint32_t file_count = b->dir[b->directory_stack_size].file_count;
	int current_index = b->currently_selected;

	int page_number = current_index / NUM_ENTRY_PER_PAGE;
	int page_base = page_number * NUM_ENTRY_PER_PAGE;
	float currentX = 0.09f;
	float currentY = 0.09f;
	float ySpacing = 0.035f;

	for (int i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
	{
		currentY = currentY + ySpacing;
		cellDbgFontPuts(currentX, currentY, Emulator_GetFontSize(), i == current_index ? RED : b->cur[i]->d_type == CELL_FS_TYPE_DIRECTORY ? GREEN : WHITE, b->cur[i]->d_name);
		cellDbgFontDraw();
	}
	cellDbgFontDraw();
}

#include "menu/menu-choice.h"

#include "menu/menu-helpmessages.h"

#define do_controls_refreshpage(beginvalue, endvalue) \
{ \
	float increment = 0.13f; \
	for (int i= beginvalue; i < endvalue; i++) \
	{ \
		menu_controlssettings.items[i].text_xpos = 0.09f; \
		menu_controlssettings.items[i].text_ypos = increment;  \
		increment += 0.03f; \
	} \
	menu_controlssettings.refreshpage = 0; \
}

#define FIRST_CONTROLS_SETTING_PAGE_2 menu_controlssettings.items[FIRST_CONTROLS_SETTING_PAGE_1+18].enum_id
#define FIRST_CONTROLS_SETTING_PAGE_3 menu_controlssettings.items[FIRST_CONTROLS_SETTING_PAGE_1+36].enum_id

static void do_controls_settings(void)
{
	uint64_t state = cell_pad_input_poll_device(0);
	static uint64_t old_state = 0;
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	// back to ROM menu if CIRCLE is pressed
	if (CTRL_L1(button_was_pressed) || CTRL_CIRCLE(button_was_pressed))
	{
		menuStackindex--;
		old_state = state;
		return;
	}

	if (CTRL_DOWN(state)  || CTRL_LSTICK_DOWN(state))	// down to next setting
	{
		menu_controlssettings.selected++;

		if (menu_controlssettings.selected >= MAX_NO_OF_CONTROLS_SETTINGS)
			menu_controlssettings.selected = FIRST_CONTROLS_SETTING_PAGE_1;

		sys_timer_usleep(FILEBROWSER_DELAY);

		if(menu_controlssettings.selected < FIRST_CONTROLS_SETTING_PAGE_2)
		{
			if(menu_controlssettings.page != 0)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 0;
		}
		else if(menu_controlssettings.selected < MAX_NO_OF_CONTROLS_SETTINGS)
		{
			if(menu_controlssettings.page != 1)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 1;
               }
	}

	if (CTRL_UP(state)  || CTRL_LSTICK_UP(state))	// up to previous setting
	{
		menu_controlssettings.selected--;

		if (menu_controlssettings.selected < FIRST_CONTROLS_SETTING_PAGE_1)
			menu_controlssettings.selected = MAX_NO_OF_CONTROLS_SETTINGS-1;

		sys_timer_usleep(FILEBROWSER_DELAY);

		if(menu_controlssettings.selected < FIRST_CONTROLS_SETTING_PAGE_2)
		{
			if(menu_controlssettings.page != 0)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 0;
		}
		else if(menu_controlssettings.selected < MAX_NO_OF_CONTROLS_SETTINGS)
		{
			if(menu_controlssettings.page != 1)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 1;
		}
	}

	if (menu_controlssettings.refreshpage)
	{
		switch(menu_controlssettings.page)
		{
			case 0:
				do_controls_refreshpage(FIRST_CONTROLS_SETTING_PAGE_1,SETTING_CONTROLS_BUTTON_L2_BUTTON_L3+1);
				break;
			case 1:
				do_controls_refreshpage(SETTING_CONTROLS_BUTTON_L2_BUTTON_L3,SETTING_CONTROLS_DEFAULT_ALL+1);
				break;
		}
            menu_controlssettings.refreshpage = 0;
         }

	if (CTRL_L2(state) && CTRL_R2(state))
	{
		// if a rom is loaded then resume it
		if (Emulator_IsROMLoaded())
		{
			menu_is_running = 0;
			Emulator_StartROMRunning();
		}
		old_state = state;
		return;
	}

	switch(menu_controlssettings.selected)
	{
		case SETTING_CONTROLS_SCHEME:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(button_was_pressed) | CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(button_was_pressed))
			{
				menuStackindex++;
				menuStack[menuStackindex] = menu_filebrowser;
				menuStack[menuStackindex].enum_id = INPUT_PRESET_CHOICE;
				set_initial_dir_tmpbrowser = true;
			}
			if(CTRL_START(state))
			{
				snprintf(Settings.PS3CurrentInputPresetTitle, sizeof(Settings.PS3CurrentInputPresetTitle), "%s", "Default");
				emulator_set_controls("", SET_ALL_CONTROLS_TO_DEFAULT, "Default");
			}
			break;
		case SETTING_CONTROLS_NUMBER:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(button_was_pressed))
			{
				if(currently_selected_controller_menu != 0)
					currently_selected_controller_menu--;
				sys_timer_usleep(FILEBROWSER_DELAY);
			}

			if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(button_was_pressed))
			{
				if(currently_selected_controller_menu < 6)
					currently_selected_controller_menu++;
				sys_timer_usleep(FILEBROWSER_DELAY);
			}

			if(CTRL_START(state))
				currently_selected_controller_menu = 0;

			break; 
		case SETTING_CONTROLS_DPAD_UP:
		case SETTING_CONTROLS_DPAD_DOWN:
		case SETTING_CONTROLS_DPAD_LEFT:
		case SETTING_CONTROLS_DPAD_RIGHT:
		case SETTING_CONTROLS_BUTTON_CIRCLE:
		case SETTING_CONTROLS_BUTTON_CROSS:
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
		case SETTING_CONTROLS_BUTTON_SQUARE:
		case SETTING_CONTROLS_BUTTON_SELECT:
		case SETTING_CONTROLS_BUTTON_START:
		case SETTING_CONTROLS_BUTTON_L1:
		case SETTING_CONTROLS_BUTTON_L2:
		case SETTING_CONTROLS_BUTTON_R2:
		case SETTING_CONTROLS_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_R1:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
		case SETTING_CONTROLS_ANALOG_R_UP:
		case SETTING_CONTROLS_ANALOG_R_DOWN:
		case SETTING_CONTROLS_ANALOG_R_LEFT:
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
			if(CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state))
			{
				Input_MapButton(control_binds[currently_selected_controller_menu][(menu_controlssettings.selected-SETTING_CONTROLS_DPAD_UP)],false,NULL);
				sys_timer_usleep(FILEBROWSER_DELAY);
			}
			if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(button_was_pressed))
			{
				Input_MapButton(control_binds[currently_selected_controller_menu][(menu_controlssettings.selected-SETTING_CONTROLS_DPAD_UP)],true,NULL);
				sys_timer_usleep(FILEBROWSER_DELAY);
			}
			if(CTRL_START(state))
			{
				Input_MapButton(control_binds[currently_selected_controller_menu][(menu_controlssettings.selected-SETTING_CONTROLS_DPAD_UP)],true, default_control_binds[menu_controlssettings.selected-SETTING_CONTROLS_DPAD_UP]);
			}
			old_state = state;
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed) || CTRL_RIGHT(button_was_pressed) ||  CTRL_LSTICK_RIGHT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_START(state))
				emulator_save_settings(INPUT_PRESET_FILE);
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			if(CTRL_LEFT(button_was_pressed)  || CTRL_LSTICK_LEFT(button_was_pressed) || CTRL_RIGHT(button_was_pressed)  || CTRL_LSTICK_RIGHT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_START(state))
			{
				emulator_set_controls("", SET_ALL_CONTROLS_TO_DEFAULT, "Default");
			}
			break;
	} // end of switch 
	produce_menubar(menu_controlssettings.enum_id);
	cellDbgFontDraw();

//PAGE 1
if(menu_controlssettings.page == 0)
{
	cellDbgFontPuts(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_SCHEME].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_SCHEME ? YELLOW : WHITE,	menu_controlssettings.items[SETTING_CONTROLS_SCHEME].text);
	cellDbgFontPrintf(0.5f,   menu_controlssettings.items[SETTING_CONTROLS_SCHEME].text_ypos,   Emulator_GetFontSize(), Settings.ControlScheme == CONTROL_SCHEME_DEFAULT ? GREEN : ORANGE, Settings.PS3CurrentInputPresetTitle);

	cellDbgFontPuts(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_NUMBER].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_NUMBER ? YELLOW : WHITE,	menu_controlssettings.items[SETTING_CONTROLS_NUMBER].text);
	cellDbgFontPrintf(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_NUMBER].text_ypos,	Emulator_GetFontSize(),	currently_selected_controller_menu == 0 ? GREEN : ORANGE, "%d", currently_selected_controller_menu+1);

	for(int i = SETTING_CONTROLS_DPAD_UP; i < FIRST_CONTROLS_SETTING_PAGE_2; i++)
	{
		cellDbgFontPuts(0.09f,	menu_controlssettings.items[i].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == menu_controlssettings.items[i].enum_id ? YELLOW : WHITE,	menu_controlssettings.items[i].text);
		cellDbgFontPuts(0.5f,	menu_controlssettings.items[i].text_ypos,	Emulator_GetFontSize(),	control_binds[currently_selected_controller_menu][i-(FIRST_CONTROL_BIND)] == default_control_binds[i-FIRST_CONTROL_BIND] ? GREEN : ORANGE, Input_PrintMappedButton(control_binds[currently_selected_controller_menu][i-FIRST_CONTROL_BIND]));
		cellDbgFontDraw();
	}
}

//PAGE 2
if(menu_controlssettings.page == 1)
{

	for(int i = FIRST_CONTROLS_SETTING_PAGE_2; i < SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS; i++)
	{
		cellDbgFontPuts		(0.09f,	menu_controlssettings.items[i].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == menu_controlssettings.items[i].enum_id ? YELLOW : WHITE,	menu_controlssettings.items[i].text);
		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[i].text_ypos,	Emulator_GetFontSize(),	control_binds[currently_selected_controller_menu][i-FIRST_CONTROL_BIND] == default_control_binds[i-FIRST_CONTROL_BIND] ? GREEN : ORANGE, Input_PrintMappedButton(control_binds[currently_selected_controller_menu][i-FIRST_CONTROL_BIND]));
		cellDbgFontDraw();
	}

	cellDbgFontPrintf(0.09f, menu_controlssettings.items[SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS].text_ypos, Emulator_GetFontSize(), menu_controlssettings.selected == SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS ? YELLOW : GREEN, menu_controlssettings.items[SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS].text);

	cellDbgFontPrintf(0.09f, menu_controlssettings.items[SETTING_CONTROLS_DEFAULT_ALL].text_ypos, Emulator_GetFontSize(), menu_controlssettings.selected == SETTING_CONTROLS_DEFAULT_ALL ? YELLOW : GREEN, menu_controlssettings.items[SETTING_CONTROLS_DEFAULT_ALL].text);
	cellDbgFontDraw();

}

	DisplayHelpMessage(menu_controlssettings.selected);
	producelabelvalue(menu_controlssettings.selected);

	cellDbgFontPuts(0.09f, 0.91f, Emulator_GetFontSize(), YELLOW,
	"UP/DOWN - select  L2+R2 - resume game   X/LEFT/RIGHT - change");
	cellDbgFontPuts(0.09f, 0.95f, Emulator_GetFontSize(), YELLOW,
	"START - default   L1/CIRCLE - go back");
	cellDbgFontDraw();
	old_state = state;
}

#include "menu/settings-logic.h"

#define set_next_menu(menu_obj) \
	menuStackindex++; \
	switch(menu_obj) \
	{ \
		case GENERAL_VIDEO_MENU: \
			menuStack[menuStackindex] = menu_generalaudiosettings; \
			old_state = state; \
			break; \
		case GENERAL_AUDIO_MENU: \
			menuStack[menuStackindex] = menu_emu_settings; \
			old_state = state; \
			break; \
		case EMU_GENERAL_MENU: \
			menuStack[menuStackindex] = menu_emu_videosettings; \
			old_state = state; \
			break; \
		case EMU_VIDEO_MENU: \
			menuStack[menuStackindex] = menu_emu_audiosettings; \
			old_state = state; \
			break; \
		case EMU_AUDIO_MENU: \
			menuStack[menuStackindex] = menu_pathsettings; \
			old_state = state; \
			break; \
		case PATH_MENU: \
			menuStack[menuStackindex] = menu_controlssettings; \
			old_state = state; \
			break; \
	}

static void do_settings(menu * menu_obj)
{
	uint64_t state = cell_pad_input_poll_device(0);
	static uint64_t old_state = 0;
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	if(update_item_colors)
	{
		toggle_settings_items();
	}

	// back to ROM menu if CIRCLE is pressed
	if (CTRL_CIRCLE(button_was_pressed) || CTRL_L1(button_was_pressed))
	{
		menuStackindex--;
		old_state = state;
		return;
	}

	if (menu_obj->refreshpage)
	{
		float increment = 0.13f;
		for (int i= menu_obj->first_setting; i < menu_obj->max_settings; i++)
		{
			menu_obj->items[i].text_xpos = 0.09f;
			menu_obj->items[i].text_ypos = increment; 
			increment += 0.03f;
		}
		//menu_generalvideosettings.page = 0;
		menu_obj->refreshpage = 0;
	}


	if (CTRL_R1(button_was_pressed))
	{
		set_next_menu(menu_obj->enum_id);
	}

	if (CTRL_DOWN(state) || CTRL_LSTICK_DOWN(state))	// down to next setting
	{
		menu_obj->selected++;

		while(menu_obj->items[menu_obj->selected].enabled == 0)
		{
			menu_obj->selected++;
		}

		if (menu_obj->selected >= menu_obj->max_settings)
			menu_obj->selected = menu_obj->first_setting;

		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_UP(state)  || CTRL_LSTICK_UP(state))	// up to previous setting
	{
		if (menu_obj->selected == menu_obj->first_setting)
			menu_obj->selected = menu_obj->max_settings-1;
		else
			menu_obj->selected--;

		while(items_generalsettings[menu_obj->selected].enabled == 0)
		{
			menu_obj->selected--;
		}
		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_L2(state) && CTRL_R2(state))
	{
		// if a rom is loaded then resume it
		if (Emulator_IsROMLoaded())
		{
			menu_is_running = false;
			Emulator_StartROMRunning();
		}
		old_state = state;
		return;
	}

	producesettingentry(menu_obj->selected);

	produce_menubar(menu_obj->enum_id);

	for (int i = menu_obj->first_setting; i < menu_obj->max_settings; i++)
	{
		cellDbgFontPuts(menu_obj->items[i].text_xpos, menu_obj->items[i].text_ypos, Emulator_GetFontSize(), menu_obj->selected == menu_obj->items[i].enum_id ? menu_obj->items[i].text_selected_color : menu_obj->items[i].text_unselected_color, menu_obj->items[i].text);
		producelabelvalue(i);
	}

	DisplayHelpMessage(menu_obj->selected);

	cellDbgFontPuts(0.09f, 0.91f, Emulator_GetFontSize(), YELLOW, "UP/DOWN - select  L2+R2 - resume game   X/LEFT/RIGHT - change");
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
		if(filebrowser_is_current_a_directory(browser))
		{
			//if 'filename' is in fact '..' - then pop back directory instead of adding '..' to filename path
			if(browser.currently_selected == 0)
			{
				old_state = state;
				filebrowser_pop_directory(&browser);
			}
			else
			{
				const char * separatorslash = (strcmp(filebrowser_get_current_directory_name(browser),"/") == 0) ? "" : "/";
				snprintf(newpath, sizeof(newpath), "%s%s%s", filebrowser_get_current_directory_name(browser), separatorslash, filebrowser_get_current_filename(browser));
				filebrowser_push_directory(&browser, newpath, CELL_FS_TYPE_REGULAR | CELL_FS_TYPE_DIRECTORY, ROM_EXTENSIONS);
			}
		}
		else if (filebrowser_is_current_a_file(browser))
		{
			snprintf(rom_path, sizeof(rom_path), "%s/%s", filebrowser_get_current_directory_name(browser), filebrowser_get_current_filename(browser));

			menu_is_running = 0;

			// switch emulator to emulate mode
			Emulator_StartROMRunning();

			Emulator_RequestLoadROM(rom_path);

			old_state = state;
			return;
		}
	}

	if (CTRL_L2(state) && CTRL_R2(state))
	{
		// if a rom is loaded then resume it
		if (Emulator_IsROMLoaded())
		{
			menu_is_running = 0;
			Emulator_StartROMRunning();
		}
		old_state = state;
		return;
	}

	if (filebrowser_is_current_a_directory(browser))
	{
		if(!strcmp(filebrowser_get_current_filename(browser),"app_home") || !strcmp(filebrowser_get_current_filename(browser),"host_root"))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, RED, "WARNING - This path only works on DEX PS3 systems. Do not attempt to open\n this directory on CEX PS3 systems, or you might have to restart!");
		else if(!strcmp(filebrowser_get_current_filename(browser),".."))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to go back to the previous directory.");
		else
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to enter the directory.");
	}

	if (filebrowser_is_current_a_file(browser))
		cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	"FILE BROWSER");
	cellDbgFontPrintf (0.7f, 0.05f, 0.82f, WHITE, "%s v%s", EMULATOR_NAME, EMULATOR_VERSION);
	cellDbgFontPrintf (0.09f, 0.09f, Emulator_GetFontSize(), YELLOW, "PATH: %s", filebrowser_get_current_directory_name(browser));
	cellDbgFontPuts   (0.09f, 0.93f, Emulator_GetFontSize(), YELLOW,
	"L2 + R2 - resume game           SELECT - Settings screen");
	cellDbgFontDraw();

	RenderBrowser(&browser);
	old_state = state;
}

#define MenuGoTo() \
	switch(menuStack[menuStackindex].enum_id) \
	{ \
		case FILE_BROWSER_MENU: \
			do_ROMMenu(); \
			break; \
		case GENERAL_VIDEO_MENU: \
		case GENERAL_AUDIO_MENU: \
		case EMU_GENERAL_MENU: \
		case EMU_VIDEO_MENU: \
		case EMU_AUDIO_MENU: \
		case PATH_MENU: \
			do_settings(&menuStack[menuStackindex]); \
			break; \
		case CONTROLS_MENU: \
			do_controls_settings(); \
			break; \
		case SHADER_CHOICE: \
		case PRESET_CHOICE: \
		case BORDER_CHOICE: \
		case PATH_BIOSCHOICE: \
		case INPUT_PRESET_CHOICE: \
			do_select_file(menuStack[menuStackindex].enum_id); \
			break; \
		case PATH_SAVESTATES_DIR_CHOICE: \
		case PATH_DEFAULT_ROM_DIR_CHOICE: \
		case PATH_SRAM_DIR_CHOICE: \
			do_pathChoice(menuStack[menuStackindex].enum_id); \
			break; \
	}

void MenuInit(void)
{
	filebrowser_new(&browser, Settings.PS3PathROMDirectory, ROM_EXTENSIONS);
}

void MenuMainLoop(void)
{
	menuStack[0] = menu_filebrowser;
	menuStack[0].enum_id = FILE_BROWSER_MENU;

	// menu loop
	menu_is_running = true;

	do
	{
		glClear(GL_COLOR_BUFFER_BIT);
		ps3graphics_draw_menu(1920, 1080);

		MenuGoTo();

		psglSwap();
		cellSysutilCheckCallback();
	#ifdef CELL_DEBUG_CONSOLE
		cellConsolePoll();
	#endif
	}while (menu_is_running);
}
