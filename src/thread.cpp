#include <stdint.h>
#include <retro_miscellaneous.h>
#include "thread.h"

#ifdef THREADED_RENDERER

#if VITA
#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>

static int _thread_func(SceSize args, void* p)
{
	void** argp = static_cast<void**>(p);
	threadfunc_t func = reinterpret_cast<threadfunc_t>(argp[0]);
	(*func)(argp[1]);
	return sceKernelExitDeleteThread(0);
}

void thread_run(threadfunc_t func, void* p)
{
	void* argp[2];
	argp[0] = reinterpret_cast<void*>(func);
	argp[1] = p;

	//high priority
	//SceUID thid = sceKernelCreateThread("my_thread", (SceKernelThreadEntry)_thread_func, 0x10000100, 0x10000, 0, 0, NULL);

	//low priority
	SceUID thid = sceKernelCreateThread("my_thread", (SceKernelThreadEntry)_thread_func, 0x10000101, 0x10000, 0, 0, NULL);
	if (thid >= 0) sceKernelStartThread(thid, sizeof(argp), &argp);
}

//retro_sleep causes crash
void thread_sleep(int ms) { sceKernelDelayThread(ms * 1000); }

int thread_id() { return sceKernelGetThreadId(); }

#else //non-vita

#include <rthreads/rthreads.h>

static void _thread_func(void* p)
{
	void** argp = static_cast<void**>(p);
	threadfunc_t func = reinterpret_cast<threadfunc_t>(argp[0]);
	(*func)(argp[1]);
}

void thread_run(threadfunc_t func, void* p)
{
	void* argp[2];
    sthread_t *thid = NULL;
	argp[0] = reinterpret_cast<void*>(func);
	argp[1] = p;

    thid = sthread_create(_thread_func, &argp);
    sthread_detach(thid);
}

void thread_sleep(int ms) { retro_sleep(ms); }

//int thread_id() { return 0; }

#endif

#endif //THREADED_RENDERER
