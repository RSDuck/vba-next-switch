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

#define FILEBROWSER_DELAY	100000
#define FILEBROWSER_DELAY_DIVIDED_BY_3 33333
#define SETTINGS_DELAY		150000	

#define ROM_EXTENSIONS "gb|gbc|gba|GBA|GB|GBC|zip|ZIP"

int menu_is_running = 0;		// is the menu running
static menu menuStack[25];
static int menuStackindex = 0;
static bool update_item_colors = true;
FileBrowser* browser = NULL;		// main file browser for rom browser
FileBrowser* tmpBrowser = NULL;		// tmp file browser for everything else

#include "menu/menu-logic.h"

static void UpdateBrowser(FileBrowser* b)
{
	static uint64_t old_state = 0;
	uint64_t state = cell_pad_input_poll_device(0);
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	if (CTRL_DOWN(state))
	{
		if(b->GetCurrentEntryIndex() < b->get_current_directory_file_count()-1)
		{
			b->IncrementEntry();
			sys_timer_usleep(FILEBROWSER_DELAY);
		}
	}

	if (CTRL_LSTICK_DOWN(state))
	{
		if(b->GetCurrentEntryIndex() < b->get_current_directory_file_count()-1)
		{
			b->IncrementEntry();
			sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
		}
	}

	if (CTRL_UP(state))
	{
		if(b->GetCurrentEntryIndex() > 0)
		{
			b->DecrementEntry();
			sys_timer_usleep(FILEBROWSER_DELAY);
		}
	}

	if (CTRL_LSTICK_UP(state))
	{
		if(b->GetCurrentEntryIndex() > 0)
		{
			b->DecrementEntry();
			sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
		}
	}

	if (CTRL_RIGHT(state))
	{
		b->GotoEntry(MIN(b->GetCurrentEntryIndex()+5, b->get_current_directory_file_count()-1));
		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_LSTICK_RIGHT(state))
	{
		b->GotoEntry(MIN(b->GetCurrentEntryIndex()+5, b->get_current_directory_file_count()-1));
		sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
	}

	if (CTRL_LEFT(state))
	{
		if (b->GetCurrentEntryIndex() <= 5)
			b->GotoEntry(0);
		else
			b->GotoEntry(b->GetCurrentEntryIndex()-5);

		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_LSTICK_LEFT(state))
	{
		if (b->GetCurrentEntryIndex() <= 5)
			b->GotoEntry(0);
		else
			b->GotoEntry(b->GetCurrentEntryIndex()-5);

		sys_timer_usleep(FILEBROWSER_DELAY_DIVIDED_BY_3);
	}

	if (CTRL_R1(state))
	{
		b->GotoEntry(MIN(b->GetCurrentEntryIndex()+NUM_ENTRY_PER_PAGE, b->get_current_directory_file_count()-1));
		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_L1(state))
	{
		if (b->GetCurrentEntryIndex() <= NUM_ENTRY_PER_PAGE)
			b->GotoEntry(0);
		else
			b->GotoEntry(b->GetCurrentEntryIndex()-NUM_ENTRY_PER_PAGE);

		sys_timer_usleep(FILEBROWSER_DELAY);
	}

	if (CTRL_CIRCLE(button_was_pressed))
	{
		old_state = state;
		b->PopDirectory();
	}

	old_state = state;
}

static void RenderBrowser(FileBrowser* b)
{
	uint32_t file_count = b->get_current_directory_file_count();
	int current_index = b->GetCurrentEntryIndex();

	int page_number = current_index / NUM_ENTRY_PER_PAGE;
	int page_base = page_number * NUM_ENTRY_PER_PAGE;
	float currentX = 0.09f;
	float currentY = 0.09f;
	float ySpacing = 0.035f;

	for (int i = page_base; i < file_count && i < page_base + NUM_ENTRY_PER_PAGE; ++i)
	{
		currentY = currentY + ySpacing;
		cellDbgFontPuts(currentX, currentY, Emulator_GetFontSize(), i == current_index ? RED : b->_cur[i]->d_type == CELL_FS_TYPE_DIRECTORY ? GREEN : WHITE, b->_cur[i]->d_name);
		cellDbgFontDraw();
	}
	cellDbgFontDraw();
}

#include "menu/menu-choice.h"

#include "menu/menu-helpmessages.h"

#define do_controls_settings_entry(settingsptr, defaultvalue) \
	if(Settings.ControlScheme == CONTROL_SCHEME_CUSTOM) \
	{ \
		uint64_t state = cell_pad_input_poll_device(0); \
		static uint64_t old_state = 0; \
		uint64_t diff_state = old_state ^ state; \
		uint64_t button_was_pressed = old_state & diff_state; \
		\
		if(CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state)) \
		{ \
			INPUT_MAPBUTTON(settingsptr,false,NULL); \
			sys_timer_usleep(FILEBROWSER_DELAY);  \
		} \
		if(CTRL_RIGHT(state)  || CTRL_LSTICK_RIGHT(state) || CTRL_CROSS(button_was_pressed)) \
		{ \
			INPUT_MAPBUTTON(settingsptr,true,NULL); \
			sys_timer_usleep(FILEBROWSER_DELAY); \
		} \
		\
		if(CTRL_START(state)) \
		{ \
			INPUT_MAPBUTTON(settingsptr,true,defaultvalue); \
		} \
		old_state = state; \
	}

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
	static uint64_t old_state = 0;
	uint64_t state = cell_pad_input_poll_device(0);
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
		else if(menu_controlssettings.selected < FIRST_CONTROLS_SETTING_PAGE_3)
		{
			if(menu_controlssettings.page != 1)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 1;
		}
		else if(menu_controlssettings.selected < MAX_NO_OF_CONTROLS_SETTINGS)
		{
			if(menu_controlssettings.page != 2)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 2;
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
		else if(menu_controlssettings.selected < FIRST_CONTROLS_SETTING_PAGE_3)
		{
			if(menu_controlssettings.page != 1)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 1;
		}
		else if(menu_controlssettings.selected < MAX_NO_OF_CONTROLS_SETTINGS)
		{
			if(menu_controlssettings.page != 2)
				menu_controlssettings.refreshpage = 1;

			menu_controlssettings.page = 2;
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
				do_controls_refreshpage(SETTING_CONTROLS_BUTTON_L2_BUTTON_L3,SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS+1);
				break;
			case 2:
				do_controls_refreshpage(SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS,SETTING_CONTROLS_DEFAULT_ALL+1);
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
			if(CTRL_RIGHT(state) | CTRL_LSTICK_RIGHT(state))
			{
				switch(Settings.ControlScheme)
				{
					case CONTROL_SCHEME_DEFAULT:
						Settings.ControlScheme = CONTROL_SCHEME_CUSTOM;
						break;
					case CONTROL_SCHEME_CUSTOM:
						break;
				}
				emulator_implementation_switch_control_scheme();
				sys_timer_usleep(FILEBROWSER_DELAY);
			}

			if(CTRL_LEFT(state) | CTRL_LSTICK_LEFT(state))
			{
				switch(Settings.ControlScheme)
				{
					case CONTROL_SCHEME_CUSTOM:
						Settings.ControlScheme = CONTROL_SCHEME_DEFAULT;
						break;
					case CONTROL_SCHEME_DEFAULT:
						break;
				}
				emulator_implementation_switch_control_scheme();
				sys_timer_usleep(FILEBROWSER_DELAY);
			}

			if(CTRL_START(state))
				Settings.ControlScheme = CONTROL_SCHEME_DEFAULT;
			break;
		case SETTING_CONTROLS_NUMBER:
			if(CTRL_LEFT(state) || CTRL_LSTICK_LEFT(state) || CTRL_CROSS(button_was_pressed))
			{
				if(currently_selected_controller_menu == 0)
					break;
				else
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
			do_controls_settings_entry(PS3Input.DPad_Up[currently_selected_controller_menu],BTN_UP);
			break;
		case SETTING_CONTROLS_DPAD_DOWN:
			do_controls_settings_entry(PS3Input.DPad_Down[currently_selected_controller_menu],BTN_DOWN);
			break;
		case SETTING_CONTROLS_DPAD_LEFT:
			do_controls_settings_entry(PS3Input.DPad_Left[currently_selected_controller_menu],BTN_LEFT);
			break;
		case SETTING_CONTROLS_DPAD_RIGHT:
			do_controls_settings_entry(PS3Input.DPad_Right[currently_selected_controller_menu],BTN_RIGHT);
			break;
		case SETTING_CONTROLS_BUTTON_CIRCLE:
			do_controls_settings_entry(PS3Input.ButtonCircle[currently_selected_controller_menu],BTN_A);
			break;
		case SETTING_CONTROLS_BUTTON_CROSS:
			do_controls_settings_entry(PS3Input.ButtonCross[currently_selected_controller_menu],BTN_B);
			break;
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
			do_controls_settings_entry(PS3Input.ButtonTriangle[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_SQUARE:
			do_controls_settings_entry(PS3Input.ButtonSquare[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_SELECT:
			do_controls_settings_entry(PS3Input.ButtonSelect[currently_selected_controller_menu],BTN_SELECT);
			break;
		case SETTING_CONTROLS_BUTTON_START:
			do_controls_settings_entry(PS3Input.ButtonStart[currently_selected_controller_menu],BTN_START);
			break;
		case SETTING_CONTROLS_BUTTON_L1:
			do_controls_settings_entry(PS3Input.ButtonL1[currently_selected_controller_menu],BTN_L);
			break;
		case SETTING_CONTROLS_BUTTON_L2:
			do_controls_settings_entry(PS3Input.ButtonL2[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R2:
			do_controls_settings_entry(PS3Input.ButtonR2[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_L3:
			do_controls_settings_entry(PS3Input.ButtonL3[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R3:
			do_controls_settings_entry(PS3Input.ButtonR3[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R1:
			do_controls_settings_entry(PS3Input.ButtonR1[currently_selected_controller_menu],BTN_R);
			break;
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
			do_controls_settings_entry(PS3Input.ButtonL2_ButtonL3[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R2:
			do_controls_settings_entry(PS3Input.ButtonL2_ButtonR2[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
			do_controls_settings_entry(PS3Input.ButtonL2_ButtonR3[currently_selected_controller_menu],BTN_QUICKLOAD);
			break;
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
			do_controls_settings_entry(PS3Input.ButtonR2_ButtonR3[currently_selected_controller_menu],BTN_QUICKSAVE);
			break;
		case SETTING_CONTROLS_ANALOG_R_UP:
			do_controls_settings_entry(PS3Input.AnalogR_Up[currently_selected_controller_menu],BTN_NONE);
			if(CTRL_SELECT(button_was_pressed))
				PS3Input.AnalogR_Up_Type[currently_selected_controller_menu] = !PS3Input.AnalogR_Up_Type[currently_selected_controller_menu];
			break;
		case SETTING_CONTROLS_ANALOG_R_DOWN:
			do_controls_settings_entry(PS3Input.AnalogR_Down[currently_selected_controller_menu],BTN_NONE);
			if(CTRL_SELECT(button_was_pressed))
				PS3Input.AnalogR_Down_Type[currently_selected_controller_menu] = !PS3Input.AnalogR_Down_Type[currently_selected_controller_menu];
			break;
		case SETTING_CONTROLS_ANALOG_R_LEFT:
			do_controls_settings_entry(PS3Input.AnalogR_Left[currently_selected_controller_menu],BTN_DECREMENTSAVE);
			if(CTRL_SELECT(button_was_pressed))
				PS3Input.AnalogR_Left_Type[currently_selected_controller_menu] = !PS3Input.AnalogR_Left_Type[currently_selected_controller_menu];
			break;
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
			do_controls_settings_entry(PS3Input.AnalogR_Right[currently_selected_controller_menu],BTN_INCREMENTSAVE);
			if(CTRL_SELECT(button_was_pressed))
				PS3Input.AnalogR_Right_Type[currently_selected_controller_menu] = !PS3Input.AnalogR_Right_Type[currently_selected_controller_menu];
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
			do_controls_settings_entry(PS3Input.ButtonL2_AnalogR_Right[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
			do_controls_settings_entry(PS3Input.ButtonL2_AnalogR_Left[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
			do_controls_settings_entry(PS3Input.ButtonL2_AnalogR_Up[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
			do_controls_settings_entry(PS3Input.ButtonL2_AnalogR_Down[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
			do_controls_settings_entry(PS3Input.ButtonR2_AnalogR_Right[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
			do_controls_settings_entry(PS3Input.ButtonR2_AnalogR_Left[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
			do_controls_settings_entry(PS3Input.ButtonR2_AnalogR_Up[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
			do_controls_settings_entry(PS3Input.ButtonR2_AnalogR_Down[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
			do_controls_settings_entry(PS3Input.ButtonR3_ButtonL3[currently_selected_controller_menu],BTN_EXITTOMENU);
			break;
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R2_ANALOG_R_DOWN:
			do_controls_settings_entry(PS3Input.ButtonL2_ButtonR2_AnalogR_Down[currently_selected_controller_menu],BTN_NONE);
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			if(CTRL_LEFT(button_was_pressed) || CTRL_LSTICK_LEFT(button_was_pressed) || CTRL_RIGHT(button_was_pressed) ||  CTRL_LSTICK_RIGHT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_START(state))
				emulator_implementation_save_custom_controls(1);
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			if(CTRL_LEFT(button_was_pressed)  || CTRL_LSTICK_LEFT(button_was_pressed) || CTRL_RIGHT(button_was_pressed)  || CTRL_LSTICK_RIGHT(button_was_pressed) || CTRL_CROSS(button_was_pressed) || CTRL_START(state))
			{
				Settings.ControlScheme = CONTROL_SCHEME_DEFAULT;
				emulator_implementation_button_mapping_settings(MAP_BUTTONS_OPTION_DEFAULT);
			}
			break;
	} // end of switch

	produce_menubar(menu_controlssettings.enum_id);
	cellDbgFontDraw();

	//PAGE 1
	if(menu_controlssettings.page == 0)
	{
		cellDbgFontPuts(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_SCHEME].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_SCHEME ? YELLOW : WHITE,	menu_controlssettings.items[SETTING_CONTROLS_SCHEME].text);
		cellDbgFontPrintf(0.5f,   menu_controlssettings.items[SETTING_CONTROLS_SCHEME].text_ypos,   Emulator_GetFontSize(), Settings.ControlScheme == CONTROL_SCHEME_DEFAULT ? GREEN : ORANGE, Settings.ControlScheme == CONTROL_SCHEME_DEFAULT ? "Default" : "Custom");

		cellDbgFontPuts(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_NUMBER].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_NUMBER ? YELLOW : WHITE,	menu_controlssettings.items[SETTING_CONTROLS_NUMBER].text);
		cellDbgFontPrintf(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_NUMBER].text_ypos,	Emulator_GetFontSize(),	currently_selected_controller_menu == 0 ? GREEN : ORANGE, "%d", currently_selected_controller_menu+1);

		for(int i = SETTING_CONTROLS_DPAD_UP; i < FIRST_CONTROLS_SETTING_PAGE_2; i++)
			cellDbgFontPuts(0.09f,	menu_controlssettings.items[i].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == menu_controlssettings.items[i].enum_id ? YELLOW : WHITE,	menu_controlssettings.items[i].text);

		cellDbgFontPuts(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_DPAD_UP].text_ypos,	Emulator_GetFontSize(),	PS3Input.DPad_Up[currently_selected_controller_menu] == BTN_UP ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.DPad_Up[currently_selected_controller_menu]));

		cellDbgFontPuts	(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_DPAD_DOWN].text_ypos,	Emulator_GetFontSize(),	PS3Input.DPad_Down[currently_selected_controller_menu] == BTN_DOWN ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.DPad_Down[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_DPAD_LEFT].text_ypos,	Emulator_GetFontSize(),	PS3Input.DPad_Left[currently_selected_controller_menu] == BTN_LEFT ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.DPad_Left[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_DPAD_RIGHT].text_ypos,	Emulator_GetFontSize(),	PS3Input.DPad_Right[currently_selected_controller_menu] == BTN_RIGHT ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.DPad_Right[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_CIRCLE].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonCircle[currently_selected_controller_menu] == BTN_A ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonCircle[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_CROSS].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonCross[currently_selected_controller_menu] == BTN_B ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonCross[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_TRIANGLE].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonTriangle[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonTriangle[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_SQUARE].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonSquare[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonSquare[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_SELECT].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonSelect[currently_selected_controller_menu] == BTN_SELECT ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonSelect[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_START].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonStart[currently_selected_controller_menu] == BTN_START ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonStart[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L1].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL1[currently_selected_controller_menu] == BTN_L ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL1[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R1].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR1[currently_selected_controller_menu] == BTN_R ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR1[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R2].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR2[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR2[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L3].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL3[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL3[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R3].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR3[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR3[currently_selected_controller_menu]));
		cellDbgFontDraw();

	}

	//PAGE 2
	if(menu_controlssettings.page == 1)
	{
		for(int i = FIRST_CONTROLS_SETTING_PAGE_2; i < SETTING_CONTROLS_ANALOG_R_UP; i++)
			cellDbgFontPuts		(0.09f,	menu_controlssettings.items[i].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == menu_controlssettings.items[i].enum_id ? YELLOW : WHITE,	menu_controlssettings.items[i].text);

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_BUTTON_L3].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_ButtonL3[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_ButtonL3[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_BUTTON_R2].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_ButtonR2[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_ButtonR2[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_BUTTON_R3].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_ButtonR3[currently_selected_controller_menu] == BTN_QUICKLOAD ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_ButtonR3[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_AnalogR_Right[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Right[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_AnalogR_Left[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Left[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_AnalogR_Up[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Up[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_AnalogR_Down[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Down[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR2_AnalogR_Right[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Right[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR2_AnalogR_Left[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Left[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR2_AnalogR_Up[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Up[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR2_AnalogR_Down[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Down[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R2_BUTTON_R3].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR2_ButtonR3[currently_selected_controller_menu] == BTN_QUICKSAVE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR2_ButtonR3[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_R3_BUTTON_L3].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonR3_ButtonL3[currently_selected_controller_menu] == BTN_EXITTOMENU ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonR3_ButtonL3[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_BUTTON_L2_BUTTON_R2_ANALOG_R_DOWN].text_ypos,	Emulator_GetFontSize(),	PS3Input.ButtonL2_ButtonR2_AnalogR_Down[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.ButtonL2_ButtonR2_AnalogR_Down[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPrintf		(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_UP].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_ANALOG_R_UP ? YELLOW : WHITE,	"%s %s", menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_UP].text, PS3Input.AnalogR_Up_Type[currently_selected_controller_menu] ? "(IsPressed)" : "(WasPressed)");
		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_UP].text_ypos,	Emulator_GetFontSize(),	PS3Input.AnalogR_Up[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.AnalogR_Up[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPrintf		(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_DOWN].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_ANALOG_R_DOWN ? YELLOW : WHITE,	"%s %s", menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_DOWN].text, PS3Input.AnalogR_Down_Type[currently_selected_controller_menu] ? "(IsPressed)" : "(WasPressed)");
		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_DOWN].text_ypos,	Emulator_GetFontSize(),	PS3Input.AnalogR_Down[currently_selected_controller_menu] == BTN_NONE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.AnalogR_Down[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPrintf		(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_LEFT].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_ANALOG_R_LEFT ? YELLOW : WHITE,	"%s %s", menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_LEFT].text, PS3Input.AnalogR_Left_Type[currently_selected_controller_menu] ? "(IsPressed)" : "(WasPressed)");
		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_LEFT].text_ypos,	Emulator_GetFontSize(),	PS3Input.AnalogR_Left[currently_selected_controller_menu] == BTN_DECREMENTSAVE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.AnalogR_Left[currently_selected_controller_menu]));
		cellDbgFontDraw();

		cellDbgFontPrintf		(0.09f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_RIGHT].text_ypos,	Emulator_GetFontSize(),	menu_controlssettings.selected == SETTING_CONTROLS_ANALOG_R_RIGHT ? YELLOW : WHITE,	"%s %s", menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_RIGHT].text, PS3Input.AnalogR_Right_Type[currently_selected_controller_menu] ? "(IsPressed)" : "(WasPressed)");
		cellDbgFontPuts		(0.5f,	menu_controlssettings.items[SETTING_CONTROLS_ANALOG_R_RIGHT].text_ypos,	Emulator_GetFontSize(),	PS3Input.AnalogR_Right[currently_selected_controller_menu] == BTN_INCREMENTSAVE ? GREEN : ORANGE, Input_PrintMappedButton(PS3Input.AnalogR_Right[currently_selected_controller_menu]));
		cellDbgFontDraw();

	}

	//PAGE 3
	if (menu_controlssettings.page == 2)
	{
		cellDbgFontPrintf(0.09f, menu_controlssettings.items[SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS].text_ypos, Emulator_GetFontSize(), menu_controlssettings.selected == SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS ? YELLOW : GREEN, menu_controlssettings.items[SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS].text);

		cellDbgFontPrintf(0.09f, menu_controlssettings.items[SETTING_CONTROLS_DEFAULT_ALL].text_ypos, Emulator_GetFontSize(), menu_controlssettings.selected == SETTING_CONTROLS_DEFAULT_ALL ? YELLOW : GREEN, menu_controlssettings.items[SETTING_CONTROLS_DEFAULT_ALL].text);
	}

	DisplayHelpMessage(menu_controlssettings.selected);

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
	char rom_path[1024];
	char newpath[1024];

	input_code_state_begin();

	UpdateBrowser(browser);

	if (CTRL_SELECT(button_was_pressed))
	{
		menuStackindex++;
		menuStack[menuStackindex] = menu_generalvideosettings;
	}

	if (CTRL_START(button_was_pressed))
		browser->ResetStartDirectory("/", ROM_EXTENSIONS);

	if (CTRL_CROSS(button_was_pressed))
	{
		if(browser->IsCurrentADirectory())
		{
			//if 'filename' is in fact '..' - then pop back directory instead of adding '..' to filename path
			if(browser->GetCurrentEntryIndex() == 0)
			{
      				old_state = state;
      				browser->PopDirectory();
			}
			else
			{
			const char * separatorslash = (strcmp(browser->get_current_directory_name(),"/") == 0) ? "" : "/";
				snprintf(newpath, sizeof(newpath), "%s%s%s", browser->get_current_directory_name(), separatorslash, browser->get_current_filename());
				browser->PushDirectory(newpath, CELL_FS_TYPE_REGULAR | CELL_FS_TYPE_DIRECTORY, ROM_EXTENSIONS);
			}
		}
		else if (browser->IsCurrentAFile())
		{
			snprintf(rom_path, sizeof(rom_path), "%s/%s", browser->get_current_directory_name(), browser->get_current_filename());

			menu_is_running = false;

			// switch emulator to emulate mode
			Emulator_StartROMRunning();

			Emulator_RequestLoadROM(rom_path);
			old_state = state;
			return;
		}
	}

	if (CTRL_L2(state) && CTRL_R2(state))
	{
		/* if a rom is loaded then resume it */
		if (Emulator_IsROMLoaded())
		{
			menu_is_running = false;
			Emulator_StartROMRunning();
		}
		return;
	}

	if(browser->IsCurrentADirectory())
	{
		if(!strcmp(browser->get_current_filename(),"app_home") || !strcmp(browser->get_current_filename(),"host_root"))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, RED, "%s","WARNING - Do not open this directory, or you might have to restart!");
		else if(!strcmp(browser->get_current_filename(),".."))
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s","INFO - Press X to go back to the previous directory.");
		else
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s","INFO - Press X to enter the directory.");
	}

	if(browser->IsCurrentAFile())
		cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - Press X to load the game. ");

	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	"FILE BROWSER");
	cellDbgFontPrintf (0.7f, 0.05f, 0.82f, WHITE, "VBANext PS3 v%s", EMULATOR_VERSION);
	cellDbgFontPrintf (0.09f, 0.09f, Emulator_GetFontSize(), YELLOW, "PATH: %s", browser->get_current_directory_name());
	cellDbgFontPuts   (0.09f, 0.93f, Emulator_GetFontSize(), YELLOW,
			"L2 + R2 - resume game           SELECT - Settings screen");
	cellDbgFontDraw();

	RenderBrowser(browser);
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
			do_select_file(menuStack[menuStackindex].enum_id); \
			break; \
		case PATH_CHOICE: \
			do_pathChoice(); \
			break; \
	}

void MenuMainLoop()
{
	if (browser == NULL)
	{
		browser = new FileBrowser(Settings.PS3PathROMDirectory, ROM_EXTENSIONS);
		browser->SetEntryWrap(false);
	}

	menuStack[0] = menu_filebrowser;
	menuStack[0].enum_id = FILE_BROWSER_MENU;

	// menu loop
	menu_is_running = true;
	do
	{
		glClear(GL_COLOR_BUFFER_BIT);

		Graphics->DrawMenu(1920, 1080);

		MenuGoTo();

		psglSwap();

		cellSysutilCheckCallback();

#ifdef CELL_DEBUG_CONSOLE
		cellConsolePoll();
#endif
	}while (menu_is_running);
}
