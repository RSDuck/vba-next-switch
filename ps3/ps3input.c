/******************************************************************************* 
 * ps3input.cpp - VBANext PS3
 *
 *  Created on: May 4, 2011
********************************************************************************/
#include "ps3input.h"

const char * Input_PrintMappedButton(uint32_t mappedbutton)
{
	switch(mappedbutton)
	{
		case BTN_A:
			return "Button A";
		case BTN_B:
			return "Button B";
		case BTN_L:
			return "L Button";
		case BTN_R:
			return "R Button";
		case BTN_SELECT:
			return "Button Select";
		case BTN_START:
			return "Button Start";
		case BTN_LEFT:
			return "D-Pad Left";
		case BTN_RIGHT:
			return "D-Pad Right";
		case BTN_UP:
			return "D-Pad Up";
		case BTN_DOWN:
			return "D-Pad Down";
		case BTN_QUICKSAVE:
			return "Save State";
		case BTN_QUICKLOAD:
			return "Load State";
		case BTN_INCREMENTSAVE:
			return "Increment state position";
		case BTN_DECREMENTSAVE:
			return "Decrement state position";
		case BTN_EXITTOMENU:
			return "Exit to menu";
		case BTN_NONE:
			return "None";
		case BTN_INGAME_MENU:
			return "Ingame Menu";
		default:
			return "Unknown";
	}
}

//bool next: true is next, false is previous
uint32_t Input_GetAdjacentButtonmap(uint32_t buttonmap, uint32_t next)
{
	switch(buttonmap)
	{
		case BTN_UP:
			return next ? BTN_DOWN : BTN_NONE;
		case BTN_DOWN:
			return next ? BTN_LEFT : BTN_UP;
		case BTN_LEFT:
			return next ? BTN_RIGHT : BTN_DOWN;
		case BTN_RIGHT:
			return next ?  BTN_A : BTN_LEFT;
		case BTN_A:
			return next ? BTN_B : BTN_RIGHT;
		case BTN_B:
			return next ? BTN_L : BTN_A;
		case BTN_L:
			return next ? BTN_R : BTN_B;
		case BTN_R:
			return next ? BTN_SELECT : BTN_L;
		case BTN_SELECT:
			return next ? BTN_START : BTN_R;
		case BTN_START:
			return next ? BTN_QUICKSAVE : BTN_SELECT;
		case BTN_QUICKSAVE:
			return next ? BTN_QUICKLOAD : BTN_START;
		case BTN_QUICKLOAD:
			return next ? BTN_EXITTOMENU : BTN_QUICKSAVE;
		case BTN_EXITTOMENU:
			return next ? BTN_DECREMENTSAVE : BTN_QUICKLOAD;
		case BTN_DECREMENTSAVE:
			return next ? BTN_INCREMENTSAVE : BTN_EXITTOMENU;
		case BTN_INCREMENTSAVE:
			return next ? BTN_INGAME_MENU : BTN_DECREMENTSAVE;
		case BTN_INGAME_MENU:
			return next ? BTN_NONE : BTN_INCREMENTSAVE;
		case BTN_NONE:
			return next ? BTN_UP : BTN_INGAME_MENU;
		default:
			return BTN_NONE;
	}
}
