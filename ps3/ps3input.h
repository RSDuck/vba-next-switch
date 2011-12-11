/******************************************************************************* 
 * ps3input.h - VBANext PS3
 *
 *  Created on: Aug 19, 2011
********************************************************************************/
#ifndef PS3INPUT_H_
#define PS3INPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#define	BTN_A			1
#define BTN_B			2
#define BTN_SELECT		4
#define BTN_START		8
#define BTN_RIGHT		16
#define BTN_LEFT		32
#define BTN_UP			64
#define BTN_DOWN		128
#define BTN_R			256
#define BTN_L			512
#define BTN_QUICKLOAD		1024
#define BTN_QUICKSAVE		2048
#define BTN_EXITTOMENU		4096
#define BTN_INCREMENTSAVE	8192
#define BTN_DECREMENTSAVE	16384
#define BTN_NONE		32768
#define BTN_INGAME_MENU		65536
#define BTN_FASTFORWARD		131072

#define BTN_LASTGAMEBUTTON BTN_L

#define CONTROL_SCHEME_DEFAULT	0
#define CONTROL_SCHEME_CUSTOM	1

#define MAX_PADS 7

enum {
	CTRL_UP_DEF,
	CTRL_DOWN_DEF,
	CTRL_LEFT_DEF,
	CTRL_RIGHT_DEF,
	CTRL_CIRCLE_DEF,
	CTRL_CROSS_DEF,
	CTRL_TRIANGLE_DEF,
	CTRL_SQUARE_DEF,
	CTRL_SELECT_DEF,
	CTRL_START_DEF,
	CTRL_L1_DEF,
	CTRL_R1_DEF,
	CTRL_L2_DEF,
	CTRL_R2_DEF,
	CTRL_L3_DEF,
	CTRL_R3_DEF,
	CTRL_L2_L3_DEF,
	CTRL_L2_R3_DEF,
	CTRL_L2_RSTICK_RIGHT_DEF,
	CTRL_L2_RSTICK_LEFT_DEF,
	CTRL_L2_RSTICK_UP_DEF,
	CTRL_L2_RSTICK_DOWN_DEF,
	CTRL_R2_RSTICK_RIGHT_DEF,
	CTRL_R2_RSTICK_LEFT_DEF,
	CTRL_R2_RSTICK_UP_DEF,
	CTRL_R2_RSTICK_DOWN_DEF,
	CTRL_R2_R3_DEF,
	CTRL_R3_L3_DEF,
	CTRL_RSTICK_UP_DEF,
	CTRL_RSTICK_DOWN_DEF,
	CTRL_RSTICK_LEFT_DEF,
	CTRL_RSTICK_RIGHT_DEF
};

#define BTN_DEF_MAX CTRL_RSTICK_RIGHT_DEF+1

extern uint32_t control_binds[MAX_PADS][BTN_DEF_MAX];
extern uint32_t default_control_binds[BTN_DEF_MAX];

#define Input_MapButton(buttonmap, next, defaultbutton) \
	if(defaultbutton == NULL) \
		buttonmap = Input_GetAdjacentButtonmap(buttonmap, next); \
	else \
		buttonmap = defaultbutton;

const char * Input_PrintMappedButton(uint32_t mappedbutton);
uint32_t Input_GetAdjacentButtonmap(uint32_t buttonmap, uint32_t next);

#ifdef __cplusplus
}
#endif

#endif
