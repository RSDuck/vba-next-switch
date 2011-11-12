#ifndef __OSKUTIL_H__
#define __OSKUTIL_H__

#include <sysutil/sysutil_oskdialog.h>
#include <sysutil/sysutil_common.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


typedef struct
{
	unsigned int osk_memorycontainer;
	wchar_t init_message[CELL_OSKDIALOG_STRING_SIZE + 1]; // buffer
	wchar_t message[CELL_OSKDIALOG_STRING_SIZE + 1]; //buffer
	wchar_t osk_text_buffer[CELL_OSKDIALOG_STRING_SIZE + 1];
	char osk_text_buffer_char[CELL_OSKDIALOG_STRING_SIZE + 1];
	uint32_t flags;
	bool is_running;
	bool text_can_be_fetched;
	sys_memory_container_t containerid;
	CellOskDialogPoint pos;
	CellOskDialogInputFieldInfo inputFieldInfo;
	CellOskDialogCallbackReturnParam outputInfo;
	CellOskDialogParam dialogParam;
} oskutil_params;

#define OSK_IS_RUNNING(object) object.is_running
#define OUTPUT_TEXT_STRING(object) object.osk_text_buffer_char

#ifdef __cplusplus
extern "C" {
#endif

void oskutil_write_message(oskutil_params *params, const wchar_t* msg);
void oskutil_write_initial_message(oskutil_params *params, const wchar_t* msg);
void oskutil_init(oskutil_params *params, unsigned int containersize);
bool oskutil_start(oskutil_params *params);
void oskutil_stop(oskutil_params *params);
void oskutil_finished(oskutil_params *params);
void oskutil_close(oskutil_params *params);
void oskutil_unload(oskutil_params *params);

#ifdef __cplusplus
}
#endif

#endif
