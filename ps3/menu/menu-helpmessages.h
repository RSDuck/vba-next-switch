#define create_help_message_item(text) \
	cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, text);

#define print_help_message(menu, currentsetting) \
			cellDbgFontPrintf(menu.items[currentsetting].comment_xpos, menu.items[currentsetting].comment_ypos, menu.items[currentsetting].comment_scalefont, menu.items[currentsetting].comment_color, menu.items[currentsetting].comment);

#define print_help_message_yesno(menu, currentsetting) \
			snprintf(menu.items[currentsetting].comment, sizeof(menu.items[currentsetting].comment), *(menu.items[currentsetting].setting_ptr) ? menu.items[currentsetting].comment_yes : menu.items[currentsetting].comment_no); \
			print_help_message(menu, currentsetting);

static void DisplayHelpMessage(int currentsetting)
{
	switch(currentsetting)
	{
		case SETTING_PATH_SAVESTATES_DIRECTORY:
		case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
		case SETTING_PATH_SRAM_DIRECTORY:
		case SETTING_PATH_DEFAULT_ALL:
			print_help_message(menu_pathsettings, currentsetting);
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
		case SETTING_EMU_DEFAULT_ALL:
			print_help_message(menu_emu_settings, currentsetting);
			break;
		case SETTING_SHADER:
		case SETTING_SHADER_2:
		case SETTING_CHANGE_RESOLUTION:
		case SETTING_HW_OVERSCAN_AMOUNT:
		case SETTING_DEFAULT_VIDEO_ALL:
		case SETTING_SAVE_SHADER_PRESET:
			print_help_message(menu_generalvideosettings, currentsetting);
			break;
		case SETTING_HW_TEXTURE_FILTER:
		case SETTING_HW_TEXTURE_FILTER_2:
		case SETTING_SCALE_ENABLED:
		case SETTING_ENABLE_SCREENSHOTS:
		case SETTING_TRIPLE_BUFFERING:
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			print_help_message_yesno(menu_generalvideosettings, currentsetting);
			break;
		case SETTING_SCALE_FACTOR:
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - [Custom Scaling Factor] is set to: '%dx'.", Settings.ScaleFactor);
			break;
		case SETTING_VBA_GBABIOS:
			create_help_message_item((strcmp(Settings.GBABIOS,"\0") == 0) ? "INFO - GBA BIOS has been disabled. If a game doesn't work, try selecting a BIOS\nwith this option (press X to select)" : "INFO - GBA BIOS has been selected. Some games might require this.\nNOTE: GB Classic games will not work with a GBA BIOS enabled.");
			break;
		case SETTING_VBA_SGB_BORDERS:
			print_help_message_yesno(menu_emu_videosettings, currentsetting);
			break;
		case SETTING_EMU_CONTROL_STYLE:
			print_help_message(menu_emu_settings, currentsetting);
			break;
			/*
			   case SETTING_PAL60_MODE:
			   create_help_message_item(Settings.PS3PALTemporalMode60Hz ? "INFO - PAL 60Hz mode is enabled - 60Hz NTSC games will run correctly at 576p PAL\nresolution. NOTE: This is configured on-the-fly." : "INFO - PAL 60Hz mode disabled - 50Hz PAL games will run correctly at 576p PAL\nresolution. NOTE: This is configured on-the-fly.");
			   break;
			 */
		case SETTING_KEEP_ASPECT_RATIO:
			cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "INFO - [Aspect ratio] is set to '%d:%d'.", ps3graphics_get_aspect_ratio_int(0), ps3graphics_get_aspect_ratio_int(1));
			break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			print_help_message(menu_generalaudiosettings, currentsetting);
			break;
		case SETTING_SOUND_MODE:
			snprintf(menu_generalaudiosettings.items[currentsetting].comment, sizeof(menu_generalaudiosettings.items[currentsetting].comment), Settings.SoundMode == SOUND_MODE_RSOUND ? "INFO - [Sound Output] is set to 'RSound' - the sound will be streamed over the\n network to the RSound audio server." : Settings.SoundMode == SOUND_MODE_HEADSET ? "INFO - [Sound Output] is set to 'USB/Bluetooth Headset' - sound will\n be output through the headset" : "INFO - [Sound Output] is set to 'Normal' - normal audio output will be\nused.");
			print_help_message(menu_generalaudiosettings, currentsetting);
			break;
	}
}

static void producelabelvalue(uint64_t switchvalue)
{
	switch(switchvalue)
	{
		case SETTING_CHANGE_RESOLUTION:
			{
				cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), ps3graphics_get_initial_resolution() == ps3graphics_get_current_resolution() ? GREEN : ORANGE, ps3graphics_get_resolution_label(ps3graphics_get_current_resolution()));
				cellDbgFontDraw();
				break;
			}
#if 0
		case SETTING_PAL60_MODE: 
			cellDbgFontPuts		(menu_generalvideosettings.items[switchvalue].text_xpos,	menu_generalvideosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	currently_selected_setting == menu_generalvideosettings.items[switchvalue].enum_id ? YELLOW : WHITE,	"PAL60 Mode (576p only)");
			cellDbgFontPrintf	(0.5f,	menu_generalvideosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	Settings.PS3PALTemporalMode60Hz ? ORANGE : GREEN, Settings.PS3PALTemporalMode60Hz ? "ON" : "OFF");
			break;
#endif
#if 0
		case SETTING_GAME_AWARE_SHADER:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), 
					Settings.GameAwareShaderPath == "" ? GREEN : ORANGE, 
					"%s", Settings.GameAwareShaderPath);
			break;
#endif
		case SETTING_SHADER_PRESETS:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), (menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].enabled == 0) ? SILVER : strcmp(Settings.ShaderPresetPath, DEFAULT_PRESET_FILE) == 0 ? GREEN : ORANGE, "%s", Settings.ShaderPresetTitle);
			break;
		case SETTING_BORDER:
			{
				extract_filename_only(Settings.PS3CurrentBorder);
				cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), GREEN, "%s", fname_without_path_extension);
			}
			break;
		case SETTING_SHADER:
			{
				extract_filename_only(ps3graphics_get_fragment_shader_path(0));
				cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), GREEN, "%s", fname_without_path_extension);
			}
			break;
		case SETTING_SHADER_2:
			{
				extract_filename_only(ps3graphics_get_fragment_shader_path(1));
				cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), !(Settings.ScaleEnabled) ? SILVER : GREEN, "%s", fname_without_path_extension);
			}
			break;
		case SETTING_FONT_SIZE:
			cellDbgFontPrintf(0.5f,	menu_generalvideosettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	Settings.PS3FontSize == 100 ? GREEN : ORANGE, "%f", Emulator_GetFontSize());
			break;
			//emulator-specific
		case SETTING_VBA_SGB_BORDERS:
			cellDbgFontPuts(0.5f, menu_emu_videosettings.items[menu_emu_videosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), *(menu_emu_videosettings.items[switchvalue].setting_ptr) ? ORANGE : GREEN, *(menu_emu_videosettings.items[switchvalue].setting_ptr) ? "ON" : "OFF");
			break;
		case SETTING_EMU_CONTROL_STYLE:
			cellDbgFontPuts(0.5f, menu_emu_settings.items[menu_emu_settings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), *(menu_emu_settings.items[switchvalue].setting_ptr) ? GREEN : ORANGE, *(menu_emu_settings.items[switchvalue].setting_ptr) ? "Original (X->B, O->A)" : "Better (X->A, []->B)");
			break;
		case SETTING_KEEP_ASPECT_RATIO:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, 0.91f, ps3graphics_get_aspect_ratio_float(Settings.PS3KeepAspect) == SCREEN_4_3_ASPECT_RATIO ? GREEN : ORANGE, "%s%d:%d", ps3graphics_calculate_aspect_ratio_before_game_load() ? "(Auto)" : "", (int)ps3graphics_get_aspect_ratio_int(0), (int)ps3graphics_get_aspect_ratio_int(1));
			cellDbgFontDraw();
			break;
		case SETTING_HW_TEXTURE_FILTER:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), Settings.PS3Smooth ? GREEN : ORANGE, Settings.PS3Smooth ? "Linear interpolation" : "Point filtering");
			break;
		case SETTING_HW_TEXTURE_FILTER_2:
			cellDbgFontPrintf(0.5f, menu_generalvideosettings.items[switchvalue].text_ypos, Emulator_GetFontSize(), !(menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].enabled) ? SILVER : Settings.PS3Smooth2 ? GREEN : ORANGE, Settings.PS3Smooth2 ? "Linear interpolation" : "Point filtering");
			break;
		case SETTING_SCALE_FACTOR:
			cellDbgFontPrintf	(0.5f,	menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos,	Emulator_GetFontSize(),	(menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].enabled == 0) ? SILVER : Settings.ScaleFactor == 2 ? GREEN : ORANGE, "%dx", Settings.ScaleFactor);
			break;
		case SETTING_HW_OVERSCAN_AMOUNT:
			cellDbgFontPrintf	(0.5f,	menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos,	Emulator_GetFontSize(),	Settings.PS3OverscanAmount == 0 ? GREEN : ORANGE, "%f", (float)Settings.PS3OverscanAmount/100);
			break;
		case SETTING_SOUND_MODE:
			cellDbgFontPuts(0.5f, menu_generalaudiosettings.items[menu_generalaudiosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), Settings.SoundMode == SOUND_MODE_NORMAL ? GREEN : ORANGE, Settings.SoundMode == SOUND_MODE_RSOUND ? "RSound" : Settings.SoundMode == SOUND_MODE_HEADSET ? "USB/Bluetooth Headset" : "Normal");
			break;
		case SETTING_RSOUND_SERVER_IP_ADDRESS:
			cellDbgFontPuts(0.5f, menu_generalaudiosettings.items[menu_generalaudiosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), strcmp(Settings.RSoundServerIPAddress,"0.0.0.0") ? ORANGE : GREEN, Settings.RSoundServerIPAddress);
			break;
		case SETTING_THROTTLE_MODE:
		case SETTING_ENABLE_SCREENSHOTS:
		case SETTING_TRIPLE_BUFFERING:
		case SETTING_SCALE_ENABLED:
		case SETTING_APPLY_SHADER_PRESET_ON_STARTUP:
			cellDbgFontPuts(0.5f, menu_generalvideosettings.items[menu_generalvideosettings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), *(menu_generalvideosettings.items[switchvalue].setting_ptr) ? GREEN : ORANGE, *(menu_generalvideosettings.items[switchvalue].setting_ptr) ? "ON" : "OFF");
			break;
		case SETTING_EMU_CURRENT_SAVE_STATE_SLOT:
			cellDbgFontPrintf(0.5f, menu_emu_settings.items[menu_emu_settings.items[switchvalue].enum_id].text_ypos, Emulator_GetFontSize(), Settings.CurrentSaveStateSlot == MIN_SAVE_STATE_SLOT ? GREEN : ORANGE, "%d", Settings.CurrentSaveStateSlot);
			break;
		case SETTING_PATH_DEFAULT_ROM_DIRECTORY:
			cellDbgFontPuts		(0.5f,	menu_pathsettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	!(strcmp(Settings.PS3PathROMDirectory,"/")) ? GREEN : ORANGE, Settings.PS3PathROMDirectory);
			break;
		case SETTING_PATH_SAVESTATES_DIRECTORY:
			cellDbgFontPuts		(0.5f,	menu_pathsettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	!(strcmp(Settings.PS3PathSaveStates,usrDirPath)) ? GREEN : ORANGE, Settings.PS3PathSaveStates);
			break;
		case SETTING_PATH_SRAM_DIRECTORY:
			cellDbgFontPuts		(0.5f,	menu_pathsettings.items[switchvalue].text_ypos,	Emulator_GetFontSize(),	!(strcmp(Settings.PS3PathSRAM,"")) ? GREEN : ORANGE, !(strcmp(Settings.PS3PathSRAM,"")) ? "Same dir as ROM" : Settings.PS3PathSRAM);
			break;
		case SETTING_CONTROLS_SCHEME:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Input Control scheme preset [%s] is selected.\n", Settings.PS3CurrentInputPresetTitle);
			break;
		case SETTING_CONTROLS_DPAD_UP:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [D-Pad Up] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.DPad_Up[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_DPAD_DOWN:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [D-Pad Down] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.DPad_Down[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_DPAD_LEFT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [D-Pad Left] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.DPad_Left[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_DPAD_RIGHT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [D-Pad Right] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.DPad_Right[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_CIRCLE:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Circle button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonCircle[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_CROSS:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Cross button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonCross[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_TRIANGLE:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Triangle button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonTriangle[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_SQUARE:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Square button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonSquare[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_SELECT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Select button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonSelect[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_START:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Start button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonStart[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L1:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [L1 button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonL1[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R1:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [R1 button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonR1[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L2:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [L2 button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonL2[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R2:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [R2 button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonR2[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L3:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [L3 button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonL3[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R3:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [R3 button] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonR3[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_L3:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [L2 + L3] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonL2_ButtonL3[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L2_BUTTON_R3:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [L2 + R3] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonL2_ButtonR3[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_RIGHT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [L2 + Right Analog Stick - Right] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Right[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_LEFT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [L2 + Right Analog Stick - Left] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Left[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_UP:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [L2 + Right Analog Stick - Up] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Up[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_L2_ANALOG_R_DOWN:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [L2 + Right Analog Stick - Down] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonL2_AnalogR_Down[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_RIGHT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [R2 + Right Analog Stick - Right] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Right[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_LEFT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [R2 + Right Analog Stick - Left] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Left[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_UP:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [R2 + Right Analog Stick - Up] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Up[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R2_ANALOG_R_DOWN:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [R2 + Right Analog Stick - Down] on the PS3 controller\nis mapped to action: [%s].", Input_PrintMappedButton(PS3Input.ButtonR2_AnalogR_Down[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R2_BUTTON_R3:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [R2 + R3] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonR2_ButtonR3[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_BUTTON_R3_BUTTON_L3:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Button combo [R3 + L3] on the PS3 controller is mapped to action:\n[%s].", Input_PrintMappedButton(PS3Input.ButtonR3_ButtonL3[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_ANALOG_R_UP:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Analog Stick Right - Up] on the PS3 controller is mapped to action:\n[%s] (NOTE: Press SELECT to change type).", Input_PrintMappedButton(PS3Input.AnalogR_Up[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_ANALOG_R_DOWN:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Analog Stick Right - Down] on the PS3 controller is mapped to action:\n[%s] (NOTE: Press SELECT to change type).", Input_PrintMappedButton(PS3Input.AnalogR_Down[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_ANALOG_R_LEFT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Analog Stick Right - Left] on the PS3 controller is mapped to action:\n[%s] (NOTE: Press SELECT to change type).", Input_PrintMappedButton(PS3Input.AnalogR_Left[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_ANALOG_R_RIGHT:
			cellDbgFontPrintf(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - [Analog Stick Right - Right] on the PS3 controller is mapped to action:\n[%s] (NOTE: Press SELECT to change type).", Input_PrintMappedButton(PS3Input.AnalogR_Right[currently_selected_controller_menu]));
			break;
		case SETTING_CONTROLS_SAVE_CUSTOM_CONTROLS:
			cellDbgFontPuts(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Save the custom control settings.\nNOTE: This option will not do anything with Control Scheme [New] or [Default].");
			break;
		case SETTING_CONTROLS_DEFAULT_ALL:
			cellDbgFontPuts(0.09f, 0.83f, 0.86f, LIGHTBLUE, "INFO - Set all 'Controls' settings back to their default values.");
			break;
		case SETTING_DEFAULT_VIDEO_ALL:
		case SETTING_SAVE_SHADER_PRESET:
		case SETTING_DEFAULT_AUDIO_ALL:
			cellDbgFontDraw();
			break;
	}
}
