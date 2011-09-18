#include "sysutil/sysutil_oskdialog.h"
#include "sys/memory.h"

#include "oskutil.h"

#define OSK_IN_USE	(0x00000001)

void oskutil_init(oskutil_params *params, unsigned int containersize)
{
	params->flags = 0;
	memset(params->osk_text_buffer, 0, sizeof(*params->osk_text_buffer));
	memset(params->osk_text_buffer_char, 0, 256);
	params->is_running = false;
   if(containersize)
	   params->osk_memorycontainer =  containersize; 
   else
	   params->osk_memorycontainer =  1024*1024*7;
}

static bool oskutil_enable_key_layout()
{
	int ret = cellOskDialogSetKeyLayoutOption(CELL_OSKDIALOG_10KEY_PANEL | \
	CELL_OSKDIALOG_FULLKEY_PANEL);
	if (ret < 0)
		return (false);
   else
      return (true);
}

static void oskutil_create_activation_parameters(oskutil_params *params)
{
	// Initial display psition of the OSK (On-Screen Keyboard) dialog [x, y]
	params->dialogParam.controlPoint.x = 0.0;
	params->dialogParam.controlPoint.y = 0.0;

	// Set standard position
	int32_t LayoutMode = CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | \ 
	CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP;
	cellOskDialogSetLayoutMode(LayoutMode);

	//Select panels to be used using flags
	// NOTE: We don't need CELL_OSKDIALOG_PANELMODE_JAPANESE_KATAKANA and \
	// CELL_OSKDIALOG_PANELMODE_JAPANESE obviously (and Korean), so I'm \
	// going to leave that all out	
	params->dialogParam.allowOskPanelFlg = 
	CELL_OSKDIALOG_PANELMODE_ALPHABET |
	CELL_OSKDIALOG_PANELMODE_NUMERAL | 
	CELL_OSKDIALOG_PANELMODE_NUMERAL_FULL_WIDTH |
	CELL_OSKDIALOG_PANELMODE_ENGLISH;
	// Panel to display first
	params->dialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ENGLISH;
	// Initial display position of the onscreen keyboard dialog
	params->dialogParam.prohibitFlgs = 0;
}

//Text to be displayed, as guide message, at upper left on the OSK
void oskutil_write_message(oskutil_params *params, const wchar_t* msg)
{
   params->inputFieldInfo.message = (uint16_t*)msg;
}

//Initial text to be displayed
void oskutil_write_initial_message(oskutil_params *params, const wchar_t* msg)
{
   params->inputFieldInfo.init_text = (uint16_t*)msg;
}

bool oskutil_abort(oskutil_params *params)
{
	if ((params->flags & OSK_IN_USE) == 0)
		return (false);

	int ret = cellOskDialogAbort();

	if (ret < 0)
		return (false);
   else
      return (true);
}

void oskutil_close(oskutil_params *params)
{
	params->is_running = false;
	sys_memory_container_destroy(params->containerid);
}

bool oskutil_start(oskutil_params *params) 
{
	params->is_running = true;
	if (params->flags & OSK_IN_USE)
	{
		return (true);
	}

	int ret = sys_memory_container_create(&params->containerid, params->osk_memorycontainer);
	// NOTE: Returns EAGAIN (0x80010001) (Kernel memory shortage) after spamming 
	// the OSK Util
	if(ret < 0)
	{
		return (false);
	}

	//Length limitation for input text
	params->inputFieldInfo.limit_length = CELL_OSKDIALOG_STRING_SIZE;	

	oskutil_create_activation_parameters(params);

	if(!oskutil_enable_key_layout())
	{
		return (false);
	}

	//NOTE
	//For future reference -
	//if you want to have two OSK input screens in quick succession of each
	//other - note that you might get timing issues. You can tell if this is
	//the case if 'ret' here becomes equivalent to CELL_SYSUTIL_ERROR_BUSY -
	//0x8002b105 (Service cannot run because another service is running).
	//
	//In this case, we will have to add a delay (through sys_timer_usleep
	//in our implementation code). 150000 seemed to get rid of the error,
	//but 100000 was too fast and still produced the error.
	ret = cellOskDialogLoadAsync(params->containerid, &params->dialogParam, &params->inputFieldInfo);
	if(ret < 0)
	{
		return (false);
	}
	params->flags |= OSK_IN_USE;
	return (true);
}

void oskutil_stop(oskutil_params *params)
{
	params->is_running = false;

	// Result onscreen keyboard dialog termination
	params->outputInfo.result = CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK;

	// Specify number of characters for returned text
	params->outputInfo.numCharsResultString = 16;

	// Buffer storing returned text
	params->outputInfo.pResultString = (uint16_t *)params->osk_text_buffer;	

	cellOskDialogUnloadAsync(&params->outputInfo);

	int num;
	switch (params->outputInfo.result)
	{
		case CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK:
			//The text we get from the outputInfo is Unicode, needs to be converted
			num=wcstombs(params->osk_text_buffer_char, params->osk_text_buffer, 256);
			params->osk_text_buffer_char[num]=0;
			break;
		case CELL_OSKDIALOG_INPUT_FIELD_RESULT_CANCELED:
		case CELL_OSKDIALOG_INPUT_FIELD_RESULT_ABORT:
		case CELL_OSKDIALOG_INPUT_FIELD_RESULT_NO_INPUT_TEXT:
		default:
			params->osk_text_buffer_char[0]=0;
			break;
	}

	params->flags &= ~OSK_IN_USE;
}
