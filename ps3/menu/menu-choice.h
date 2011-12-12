static void do_select_file(uint32_t menu_id)
{
	char extensions[256], title[256], object[256], comment[256], dir_path[MAX_PATH_LENGTH];

	switch(menu_id)
	{
		case GAME_AWARE_SHADER_CHOICE:
			strncpy(dir_path, GAME_AWARE_SHADER_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cfg|CFG", sizeof(extensions));
			strncpy(title, "GAME AWARE SHADER SELECTION", sizeof(title));
			strncpy(object, "Game Aware Shader", sizeof(object));
			strncpy(comment, "INFO - Select a 'Game Aware Shader' script from the menu by pressing X.", sizeof(comment));
			break;
		case SHADER_CHOICE:
			strncpy(dir_path, SHADERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "cg|CG", sizeof(extensions));
			strncpy(title, "SHADER SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(comment, "INFO - Select a shader from the menu by pressing the X button.", sizeof(comment));
			break;
		case PRESET_CHOICE:
			strncpy(dir_path, PRESETS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "conf|CONF", sizeof(extensions));
			strncpy(title, "SHADER PRESETS SELECTION", sizeof(title));
			strncpy(object, "Shader", sizeof(object));
			strncpy(object, "Shader preset", sizeof(object));
                        strncpy(comment, "INFO - Select a shader preset from the menu by pressing the X button. ", sizeof(comment));
			break;
		case INPUT_PRESET_CHOICE:
			strncpy(dir_path, INPUT_PRESETS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "conf|CONF", sizeof(extensions));
			strncpy(title, "INPUT PRESETS SELECTION", sizeof(title));
			strncpy(object, "Input", sizeof(object));
			strncpy(object, "Input preset", sizeof(object));
                        strncpy(comment, "INFO - Select an input preset from the menu by pressing the X button. ", sizeof(comment));
			break;
		case BORDER_CHOICE:
			strncpy(dir_path, BORDERS_DIR_PATH, sizeof(dir_path));
			strncpy(extensions, "png|PNG|jpg|JPG|JPEG|jpeg", sizeof(extensions));
			strncpy(title, "BORDER SELECTION", sizeof(title));
			strncpy(object, "Border", sizeof(object));
			strncpy(object, "Border image file", sizeof(object));
			strncpy(comment, "INFO - Select a border image file from the menu by pressing the X button. ", sizeof(comment));
			break;
		case PATH_BIOSCHOICE:
			strncpy(dir_path, "/\0", sizeof(dir_path));
			strncpy(extensions, "bin|BIN|gba|GBA", sizeof(extensions));
			strncpy(title, "GBA BIOS SELECTION", sizeof(title));
			strncpy(object, "GBA BIOS file", sizeof(object));
			strncpy(comment, "INFO - Select a GBA BIOS file from the menu by pressing the X button. ", sizeof(comment));
			break;
	}

	if(set_initial_dir_tmpbrowser)
	{
		filebrowser_new(&tmpBrowser, dir_path, extensions);
		set_initial_dir_tmpbrowser = false;
	}

	char path[MAX_PATH_LENGTH];

	uint64_t state = cell_pad_input_poll_device(0);
	static uint64_t old_state = 0;
	uint64_t diff_state = old_state ^ state;
	uint64_t button_was_pressed = old_state & diff_state;

	UpdateBrowser(&tmpBrowser);

	if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&tmpBrowser, "/", extensions);

	if (CTRL_CROSS(button_was_pressed))
	{
		if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
		{
			//if 'filename' is in fact '..' - then pop back directory instead of adding '..' to filename path
			if(tmpBrowser.currently_selected == 0)
			{
				old_state = state;
				filebrowser_pop_directory(&tmpBrowser);
			}
			else
			{
                                const char * separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
				snprintf(path, sizeof(path), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
				filebrowser_push_directory(&tmpBrowser, path, true);
			}
		}
		else if (FILEBROWSER_IS_CURRENT_A_FILE(tmpBrowser))
		{
			snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
			printf("path: %s\n", path);

			switch(menu_id)
			{
				case GAME_AWARE_SHADER_CHOICE:
					//emulator_implementation_set_gameaware(path);
					strncpy(Settings.GameAwareShaderPath, path, sizeof(Settings.GameAwareShaderPath));
					break;
				case SHADER_CHOICE:
					if(set_shader)
						strncpy(Settings.PS3CurrentShader2, path, sizeof(Settings.PS3CurrentShader2));
					else
						strncpy(Settings.PS3CurrentShader, path, sizeof(Settings.PS3CurrentShader));
					ps3graphics_load_fragment_shader(path, set_shader);
					break;
				case PRESET_CHOICE:
					emulator_implementation_set_shader_preset(path);
					break;
				case INPUT_PRESET_CHOICE:
					emulator_set_controls(path, READ_CONTROLS, "");
					break;
				case BORDER_CHOICE:
					strncpy(Settings.PS3CurrentBorder, path, sizeof(Settings.PS3CurrentBorder));
					emulator_implementation_set_texture(path);
					break;
				case PATH_BIOSCHOICE:
					strncpy(Settings.GBABIOS, path, sizeof(Settings.GBABIOS));
					menuStackindex--;
					break;
			}	

			menuStackindex--;
		}
	}

	if (CTRL_TRIANGLE(button_was_pressed))
		menuStackindex--;

        cellDbgFontPrintf(0.09f, 0.09f, Emulator_GetFontSize(), YELLOW, "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
	cellDbgFontPuts	(0.09f,	0.05f,	Emulator_GetFontSize(),	RED,	title);
	cellDbgFontPrintf(0.09f, 0.92f, 0.92, YELLOW, "X - Select %s  /\\ - return to settings  START - Reset Startdir", object);
	cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s", comment);
	cellDbgFontDraw();

	RenderBrowser(&tmpBrowser);
	old_state = state;
}

static void do_pathChoice(uint32_t menu_id)
{
	if(set_initial_dir_tmpbrowser)
	{
		filebrowser_new(&tmpBrowser, "/\0", "empty");
		set_initial_dir_tmpbrowser = false;
	}

        char path[1024];
        char newpath[1024];

        uint64_t state = cell_pad_input_poll_device(0);
        static uint64_t old_state = 0;
        uint64_t diff_state = old_state ^ state;
        uint64_t button_was_pressed = old_state & diff_state;

        UpdateBrowser(&tmpBrowser);

        if (CTRL_START(button_was_pressed))
		filebrowser_reset_start_directory(&tmpBrowser, "/","empty");

        if (CTRL_SQUARE(button_was_pressed))
        {
                if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
                {
                        snprintf(path, sizeof(path), "%s/%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
                        switch(menu_id)
                        {
                                case PATH_SAVESTATES_DIR_CHOICE:
                                        strcpy(Settings.PS3PathSaveStates, path);
                                        break;
                                case PATH_SRAM_DIR_CHOICE:
                                        strcpy(Settings.PS3PathSRAM, path);
                                        break;
                                case PATH_DEFAULT_ROM_DIR_CHOICE:
                                        strcpy(Settings.PS3PathROMDirectory, path);
                                        break;
                        }
                        menuStackindex--;
                }
        }
        if (CTRL_TRIANGLE(button_was_pressed))
        {
                strcpy(path, usrDirPath);
                switch(menu_id)
                {
                        case PATH_SAVESTATES_DIR_CHOICE:
                                strcpy(Settings.PS3PathSaveStates, path);
                                break;
                        case PATH_SRAM_DIR_CHOICE:
                                strcpy(Settings.PS3PathSRAM, path);
                                break;
                        case PATH_DEFAULT_ROM_DIR_CHOICE:
                                strcpy(Settings.PS3PathROMDirectory, path);
                                break;
                }
                menuStackindex--;
        }
        if (CTRL_CROSS(button_was_pressed))
        {
                if(FILEBROWSER_IS_CURRENT_A_DIRECTORY(tmpBrowser))
                {
                        //if 'filename' is in fact '..' - then pop back directory instead of adding '..' to filename path
                        if(tmpBrowser.currently_selected == 0)
                        {
                                old_state = state;
				filebrowser_pop_directory(&tmpBrowser);
                        }
                        else
                        {
                                const char * separatorslash = (strcmp(FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser),"/") == 0) ? "" : "/";
                                snprintf(newpath, sizeof(newpath), "%s%s%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser), separatorslash, FILEBROWSER_GET_CURRENT_FILENAME(tmpBrowser));
                                filebrowser_push_directory(&tmpBrowser, newpath, false);
                        }
                }
        }

        cellDbgFontPrintf (0.09f,  0.09f, Emulator_GetFontSize(), YELLOW,  "PATH: %s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmpBrowser));
        cellDbgFontPuts (0.09f, 0.05f,  Emulator_GetFontSize(), RED,    "DIRECTORY SELECTION");
        cellDbgFontPuts(0.09f, 0.93f, 0.92f, YELLOW,"X - Enter dir  /\\ - return to settings  START - Reset Startdir");
        cellDbgFontPrintf(0.09f, 0.83f, 0.91f, LIGHTBLUE, "%s",
                        "INFO - Browse to a directory and assign it as the path by\npressing SQUARE button.");
        cellDbgFontDraw();

        RenderBrowser(&tmpBrowser);
        old_state = state;
}
