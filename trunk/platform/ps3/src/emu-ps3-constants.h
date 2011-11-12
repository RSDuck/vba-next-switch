/******************************************************************************* 
 * emu-ps3-constants.h - VBANext PS3
 *
 *  Created on: Aug 19, 2011
********************************************************************************/

#ifndef EMUPS3_CONSTANTS_H_
#define EMUPS3_CONSTANT_H_

/* PS3 frontend - constants */

#define MAX_PATH_LENGTH			1024

#define MIN_SAVE_STATE_SLOT		0

#define CONFIG_FILE			0
#define SHADER_PRESET_FILE		1
#define INPUT_PRESET_FILE		2

#define EMULATOR_VERSION		"1.03"
#define EMULATOR_NAME			"VBANext PS3"
#define EMULATOR_CONTENT_DIR		"VBAM90000"

#define MODE_MENU		0
#define MODE_EMULATION		1
#define MODE_EXIT		2
#define MODE_MULTIMAN_STARTUP	3

/* Emulator-specific constants */

#define CONTROL_STYLE_ORIGINAL	0
#define CONTROL_STYLE_BETTER	1

// helpers
#ifdef CELL_DEBUG_CONSOLE
   #define cell_console_poll() cellConsolePoll();
#else
   #define cell_console_poll()
#endif


#define FILEBROWSER_DELAY         100000

/* PS3 frontend - file I/O */

#define extract_extension_from_filename(fname) \
	/* we extract the extension from the filename(fname) */ \
	const char * fname_extension = strrchr(fname, '.'); \
	uint32_t offset = strlen(fname) - strlen(fname_extension); \
	char extension_from_fname[MAX_PATH_LENGTH]; \
	int fname_length = strlen(fname); \
	for(int i = offset+1, no = 0; i < fname_length; i++, no++) \
	{ \
		extension_from_fname[no] = fname[i]; \
		extension_from_fname[no+1] = '\0'; \
	}

#define extract_filename_only_without_extension(fname) \
	/* we get the filename path without a extension */ \
	char fname_without_extension[MAX_PATH_LENGTH]; \
	const char * fname_extension = strrchr(fname, '.'); \
	int characters_to_copy = strlen(fname) - strlen(fname_extension); \
	strncpy(fname_without_extension, fname, characters_to_copy); \
	extract_filename_only(fname_without_extension);

#define extract_filename_only(fname) \
	/* we get the file name only */ \
	const char * fname_without_filepath = strrchr(fname, '/'); \
	int offset = strlen(fname) - strlen(fname_without_filepath); \
	int fname_without_extension_length = strlen(fname); \
	char fname_without_path_extension[MAX_PATH_LENGTH]; \
	for(int i = offset+1, no = 0; i < fname_without_extension_length; i++, no++) \
	{ \
		fname_without_path_extension[no] = fname[i]; \
		fname_without_path_extension[no+1] = '\0'; \
	}

#define make_new_filepath(fname, extension, basedir, output) \
{ \
	extract_filename_only_without_extension(fname); \
	/* Create output file name with specified extension */ \
	snprintf (output, sizeof(output), "%s/%s%s", basedir, fname_without_path_extension, extension); \
}

#endif
