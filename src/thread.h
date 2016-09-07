#ifndef __THREAD_H__
#define __THREAD_H__

#define THREAD_PRIORITY_HIGH 	1
#define THREAD_PRIORITY_NORMAL 	2
#define THREAD_PRIORITY_LOW 	3

#include <stdint.h>

#if VITA
	#include <psp2/types.h>
	typedef SceUID threadhandle_t;
	typedef SceUID waithandle_t;
#else
	typedef intptr_t threadhandle_t;
	typedef intptr_t waithandle_t;
#endif

#ifdef THREADED_RENDERER
typedef void(*threadfunc_t)(void*);

threadhandle_t thread_id();
void thread_run(threadfunc_t func, void* p, int priority);
void thread_sleep(int ms);
void thread_set_priority(threadhandle_t id, int priority);

#if 0
waithandle_t waithandle_create();
void waithandle_release(waithandle_t wh);
void waithandle_lock(waithandle_t wh);
void waithandle_unlock(waithandle_t wh);
#endif

#endif

#endif

