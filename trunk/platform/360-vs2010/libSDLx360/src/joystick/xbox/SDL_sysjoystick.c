/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_sysjoystick.c,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

/* This is the system specific header for the SDL joystick API */

#include <xtl.h>
#include <math.h>
#include <stdio.h>		/* For the definition of NULL */
 
#include "SDL_error.h"
#include "SDL_joystick.h"
#include "SDL_sysjoystick.h"
#include "SDL_joystick_c.h"

/* Function to scan the system for joysticks.
 * This function should set SDL_numjoysticks to the number of available
 * joysticks.  Joystick 0 should be the system default joystick.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */

#define XBINPUT_DEADZONE 0.24f
#define AXIS_MIN	-32768  /* minimum value for axis coordinate */
#define AXIS_MAX	32767   /* maximum value for axis coordinate */
#define MAX_AXES	4		/* each joystick can have up to 6 axes */
#define MAX_BUTTONS	12		/* and 14 buttons                      */
#define	MAX_HATS	2

 
#ifndef _XBOX_DONT_USE_DEVICES

typedef struct GAMEPAD
{
    // The following members are inherited from XINPUT_GAMEPAD:
    WORD    wButtons;
    BYTE    bLeftTrigger;
    BYTE    bRightTrigger;
    SHORT   sThumbLX;
    SHORT   sThumbLY;
    SHORT   sThumbRX;
    SHORT   sThumbRY;

    // Thumb stick values converted to range [-1,+1]
    FLOAT      fX1;
    FLOAT      fY1;
    FLOAT      fX2;
    FLOAT      fY2;

    // Records the state (when last updated) of the buttons.
    // These remain set as long as the button is pressed.
    WORD       wLastButtons;
    BOOL       bLastLeftTrigger;
    BOOL       bLastRightTrigger;

    // Records which buttons were pressed this frame - only set on
    // the frame that the button is first pressed.
    WORD       wPressedButtons;
    BOOL       bPressedLeftTrigger;
    BOOL       bPressedRightTrigger;

    // Device properties
    XINPUT_CAPABILITIES caps;
    BOOL       bConnected;

    // Flags for whether game pad was just inserted or removed
    BOOL       bInserted;
    BOOL       bRemoved;

    // The user index associated with this gamepad
    DWORD      dwUserIndex;

    // Deadzone pseudo-constants for the thumbsticks
    SHORT	   LEFT_THUMB_DEADZONE;
    SHORT	   RIGHT_THUMB_DEADZONE;
} XBGAMEPAD;
 
// Global instance of input states
XINPUT_STATE g_InputStates[4];

// Global instance of custom gamepad devices
XBGAMEPAD g_Gamepads[4];

__inline float (xfabsf)(float x)
{
	if (x == 0)
		return x;
	else
		return (x < 0.0F ? -x : x);
}

VOID XBInput_RefreshDeviceList( XBGAMEPAD* pGamepads, int i );
#endif

/* The private structure used to keep track of a joystick */
struct joystick_hwdata
{
#ifndef _XBOX_DONT_USE_DEVICES
	XBGAMEPAD	pGamepads;
#endif
 
	/* values used to translate device-specific coordinates into
	   SDL-standard ranges */
	struct _transaxis {
		int offset;
		float scale;
	} transaxis[6];
};


int SDL_SYS_JoystickInit(void)
{
	SDL_numjoysticks = 4;
	return(SDL_numjoysticks);
}

/* Function to get the device-dependent name of a joystick */
const char *SDL_SYS_JoystickName(int index)
{
	return("XBOX Gamepad Plugin V0.02");
}

__inline FLOAT ConvertThumbstickValue( SHORT sThumbstickValue, SHORT sDeadZone )
{
    if( sThumbstickValue > +sDeadZone )
    {
        return (sThumbstickValue-sDeadZone) / (32767.0f-sDeadZone);
    }
    if( sThumbstickValue < -sDeadZone )
    {
        return (sThumbstickValue+sDeadZone+1.0f) / (32767.0f-sDeadZone);
    }
    return 0.0f;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the index field of the joystick.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int SDL_SYS_JoystickOpen(SDL_Joystick *joystick)
{

#ifndef _XBOX_DONT_USE_DEVICES
 
	BOOL bWasConnected;
	BOOL bPressed;
	DWORD b = 0;
 
	joystick->hwdata = XPhysicalAlloc(sizeof(*joystick->hwdata), MAXULONG_PTR ,0, PAGE_READWRITE | MEM_LARGE_PAGES );

	joystick->nbuttons = MAX_BUTTONS;
	joystick->naxes = MAX_AXES;
	joystick->nhats = MAX_HATS;
	joystick->name = "Xbox SDL Gamepad V0.04";
	
 
    ZeroMemory( &g_InputStates[joystick->index], sizeof(XINPUT_STATE) );
    ZeroMemory( &joystick->hwdata->pGamepads, sizeof(XBGAMEPAD) );

	joystick->hwdata->pGamepads.dwUserIndex = joystick->index;

	
	bWasConnected = joystick->hwdata->pGamepads.bConnected;
	joystick->hwdata->pGamepads.bConnected = ( XInputGetState( joystick->index, &g_InputStates[joystick->index] ) == ERROR_SUCCESS ) ? TRUE : FALSE;

	// Track insertion and removals
	joystick->hwdata->pGamepads.bRemoved  = (  bWasConnected && !joystick->hwdata->pGamepads.bConnected ) ? TRUE : FALSE;
	joystick->hwdata->pGamepads.bInserted = ( !bWasConnected &&  joystick->hwdata->pGamepads.bConnected ) ? TRUE : FALSE;

	// Store the capabilities of the device
	if( TRUE == joystick->hwdata->pGamepads.bInserted )
	{		 
		joystick->hwdata->pGamepads.bConnected = TRUE;
		joystick->hwdata->pGamepads.bInserted  = TRUE;
		XInputGetCapabilities( joystick->index, XINPUT_FLAG_GAMEPAD, &joystick->hwdata->pGamepads.caps );
	}

	// Copy gamepad to local structure
	memcpy( &joystick->hwdata->pGamepads, &g_InputStates[joystick->index], sizeof(XINPUT_GAMEPAD) );

	// Put Xbox device input for the gamepad into our custom format
	joystick->hwdata->pGamepads.fX1 = ConvertThumbstickValue( joystick->hwdata->pGamepads.sThumbLX, joystick->hwdata->pGamepads.LEFT_THUMB_DEADZONE );
	joystick->hwdata->pGamepads.fY1 = ConvertThumbstickValue( joystick->hwdata->pGamepads.sThumbLY, joystick->hwdata->pGamepads.LEFT_THUMB_DEADZONE );
	joystick->hwdata->pGamepads.fX2 = ConvertThumbstickValue( joystick->hwdata->pGamepads.sThumbRX, joystick->hwdata->pGamepads.RIGHT_THUMB_DEADZONE );
	joystick->hwdata->pGamepads.fY2 = ConvertThumbstickValue( joystick->hwdata->pGamepads.sThumbRY, joystick->hwdata->pGamepads.RIGHT_THUMB_DEADZONE );

	// Get the boolean buttons that have been pressed since the last
	// call. Each button is represented by one bit.
	joystick->hwdata->pGamepads.wPressedButtons = ( joystick->hwdata->pGamepads.wLastButtons ^ joystick->hwdata->pGamepads.wButtons ) & joystick->hwdata->pGamepads.wButtons;
	joystick->hwdata->pGamepads.wLastButtons    = joystick->hwdata->pGamepads.wButtons;

	// Figure out if the left trigger has been pressed or released
	bPressed = ( joystick->hwdata->pGamepads.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD );

	if( bPressed )
		joystick->hwdata->pGamepads.bPressedLeftTrigger = !joystick->hwdata->pGamepads.bLastLeftTrigger;
	else
		joystick->hwdata->pGamepads.bPressedLeftTrigger = FALSE;

	// Store the state for next time
	joystick->hwdata->pGamepads.bLastLeftTrigger = bPressed;

	// Figure out if the right trigger has been pressed or released
	bPressed = (joystick->hwdata->pGamepads.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD );

	if( bPressed )
		joystick->hwdata->pGamepads.bPressedRightTrigger = !joystick->hwdata->pGamepads.bLastRightTrigger;
	else
		joystick->hwdata->pGamepads.bPressedRightTrigger = FALSE;

	// Store the state for next time
	joystick->hwdata->pGamepads.bLastRightTrigger = bPressed;


#endif
	return 0;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void SDL_SYS_JoystickUpdate(SDL_Joystick *joystick)
{

#ifndef _XBOX_DONT_USE_DEVICES

	static int prev_buttons[4] = {0};
	static Sint16 nX = 0, nY = 0;
	static Sint16 nXR = 0, nYR = 0;

	DWORD b=0;
 
	int hat=0, changed=0;

    // Read the input state
    if (XInputGetState( joystick->hwdata->pGamepads.dwUserIndex, &g_InputStates[joystick->index] ) == ERROR_SUCCESS)
	{

        // Copy gamepad to local structure
        memcpy( &joystick->hwdata->pGamepads, &g_InputStates[joystick->index].Gamepad, sizeof(XINPUT_GAMEPAD) );
 
        // Get the boolean buttons that have been pressed since the last
        // call. Each button is represented by one bit.
        joystick->hwdata->pGamepads.wPressedButtons = ( joystick->hwdata->pGamepads.wLastButtons ^ joystick->hwdata->pGamepads.wButtons ) & joystick->hwdata->pGamepads.wButtons;
        joystick->hwdata->pGamepads.wLastButtons    = joystick->hwdata->pGamepads.wButtons;

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_A )
		{
			if (!joystick->buttons[0])
				SDL_PrivateJoystickButton(joystick, (Uint8)0, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[0])
				SDL_PrivateJoystickButton(joystick, (Uint8)0, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_B )
		{
			if (!joystick->buttons[1])
				SDL_PrivateJoystickButton(joystick, (Uint8)1, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[1])
				SDL_PrivateJoystickButton(joystick, (Uint8)1, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_X )
		{
			if (!joystick->buttons[2])
				SDL_PrivateJoystickButton(joystick, (Uint8)2, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[2])
				SDL_PrivateJoystickButton(joystick, (Uint8)2, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_Y )
		{
			if (!joystick->buttons[3])
				SDL_PrivateJoystickButton(joystick, (Uint8)3, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[3])
				SDL_PrivateJoystickButton(joystick, (Uint8)3, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
		{
			if (!joystick->buttons[4])
				SDL_PrivateJoystickButton(joystick, (Uint8)4, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[4])
				SDL_PrivateJoystickButton(joystick, (Uint8)4, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
		{
			if (!joystick->buttons[5])
				SDL_PrivateJoystickButton(joystick, (Uint8)5, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[5])
				SDL_PrivateJoystickButton(joystick, (Uint8)5, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_LEFT_THUMB )
		{
			if (!joystick->buttons[6])
				SDL_PrivateJoystickButton(joystick, (Uint8)6, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[6])
				SDL_PrivateJoystickButton(joystick, (Uint8)6, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
		{
			if (!joystick->buttons[7])
				SDL_PrivateJoystickButton(joystick, (Uint8)7, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[7])
				SDL_PrivateJoystickButton(joystick, (Uint8)7, SDL_RELEASED);
		}
			
		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_START)
		{
			if (!joystick->buttons[8])
				SDL_PrivateJoystickButton(joystick, (Uint8)8, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[8])
				SDL_PrivateJoystickButton(joystick, (Uint8)8, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_BACK)
		{
			if (!joystick->buttons[9])
				SDL_PrivateJoystickButton(joystick, (Uint8)9, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[9])
				SDL_PrivateJoystickButton(joystick, (Uint8)9, SDL_RELEASED);
		}
 
	    if (joystick->hwdata->pGamepads.bLeftTrigger > 200)
		{
			if (!joystick->buttons[10])
				SDL_PrivateJoystickButton(joystick, (Uint8)10, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[10])
				SDL_PrivateJoystickButton(joystick, (Uint8)10, SDL_RELEASED);
		}

		if (joystick->hwdata->pGamepads.bRightTrigger > 200)
		{
			if (!joystick->buttons[11])
				SDL_PrivateJoystickButton(joystick, (Uint8)11, SDL_PRESSED);
		}
		else
		{
			if (joystick->buttons[11])
				SDL_PrivateJoystickButton(joystick, (Uint8)11, SDL_RELEASED);
		}

        
    }
 
	// do the HATS baby

	hat = SDL_HAT_CENTERED;
	if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
		hat|=SDL_HAT_DOWN;
	if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_DPAD_UP)
		hat|=SDL_HAT_UP;
	if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
		hat|=SDL_HAT_LEFT;
	if (joystick->hwdata->pGamepads.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
		hat|=SDL_HAT_RIGHT;


	changed = hat^prev_buttons[joystick->index];

	if ( changed  ) {
		SDL_PrivateJoystickHat(joystick, 0, hat);
	}
	
	prev_buttons[joystick->index] = hat;

	// Axis - LStick

	if ((joystick->hwdata->pGamepads.sThumbLX <= -10000) || 
		(joystick->hwdata->pGamepads.sThumbLX >= 10000))
	{
		if (joystick->hwdata->pGamepads.sThumbLX < 0)
			joystick->hwdata->pGamepads.sThumbLX++;
		nX = ((Sint16)joystick->hwdata->pGamepads.sThumbLX);
	}
	else
		nX = 0;

	if ( nX != joystick->axes[0] ) 
		SDL_PrivateJoystickAxis(joystick, (Uint8)0, (Sint16)nX);

	
	if ((joystick->hwdata->pGamepads.sThumbLY <= -10000) || 
		(joystick->hwdata->pGamepads.sThumbLY >= 10000))
	{
		if (joystick->hwdata->pGamepads.sThumbLY < 0)
			joystick->hwdata->pGamepads.sThumbLY++;
		nY = -((Sint16)(joystick->hwdata->pGamepads.sThumbLY));
	}
	else
		nY = 0;

	if ( nY != joystick->axes[1] )
		SDL_PrivateJoystickAxis(joystick, (Uint8)1, (Sint16)nY); 


	// Axis - RStick

	if ((joystick->hwdata->pGamepads.sThumbRX <= -10000) || 
		(joystick->hwdata->pGamepads.sThumbRX >= 10000))
	{
		if (joystick->hwdata->pGamepads.sThumbRX < 0)
			joystick->hwdata->pGamepads.sThumbRX++;
		nXR = ((Sint16)joystick->hwdata->pGamepads.sThumbRX);
	}
	else
		nXR = 0;

	if ( nXR != joystick->axes[2] ) 
		SDL_PrivateJoystickAxis(joystick, (Uint8)2, (Sint16)nXR);

	
	if ((joystick->hwdata->pGamepads.sThumbRY <= -10000) || 
		(joystick->hwdata->pGamepads.sThumbRY >= 10000))
	{
		if (joystick->hwdata->pGamepads.sThumbRY < 0)
			joystick->hwdata->pGamepads.sThumbRY++;
		nYR = -((Sint16)joystick->hwdata->pGamepads.sThumbRY);
	}
	else
		nYR = 0;

	if ( nYR != joystick->axes[3] )
		SDL_PrivateJoystickAxis(joystick, (Uint8)3, (Sint16)nYR); 
	
#endif

	return;
}

/* Function to close a joystick after use */
void SDL_SYS_JoystickClose(SDL_Joystick *joystick)
{
	if (joystick->hwdata != NULL) {
		/* free system specific hardware data */
		XPhysicalFree(joystick->hwdata);
		joystick->hwdata=NULL;
	}

	return;
}

/* Function to perform any system-specific joystick related cleanup */
void SDL_SYS_JoystickQuit(void)
{
	return;
}
 