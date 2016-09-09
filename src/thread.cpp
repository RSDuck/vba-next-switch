#include <stdint.h>
#include <retro_miscellaneous.h>
#include "thread.h"

#ifdef THREADED_RENDERER

#if VITA
	#include <psp2/kernel/threadmgr.h>

	static int _thread_func(SceSize args, void* p)
	{
		void** argp = static_cast<void**>(p);
		threadfunc_t func = reinterpret_cast<threadfunc_t>(argp[0]);
		(*func)(argp[1]);
		return sceKernelExitDeleteThread(0);
	}

	static int _thread_map_priority(int priority)
	{
		switch(priority) {
		case THREAD_PRIORITY_LOWEST:
			return 0x10000102;
		case THREAD_PRIORITY_LOW:
			return 0x10000101;
		case THREAD_PRIORITY_NORMAL:
		case THREAD_PRIORITY_HIGH:
		case THREAD_PRIORITY_HIGHEST:
		default:
			return 0x10000100;
		}		
	}

	thread_t thread_run(threadfunc_t func, void* p, int priority)
	{
		void* argp[2];
		argp[0] = reinterpret_cast<void*>(func);
		argp[1] = p;

		SceUID thid = sceKernelCreateThread("my_thread", (SceKernelThreadEntry)_thread_func, _thread_map_priority(priority), 0x10000, 0, 0, NULL);
		if (thid >= 0) sceKernelStartThread(thid, sizeof(argp), &argp);

		return thid;
	}

	//retro_sleep causes crash
	void thread_sleep(int ms) { sceKernelDelayThread(ms * 1000); }

	thread_t thread_get() { return sceKernelGetThreadId(); }
	
	void thread_set_priority(thread_t id, int priority) { sceKernelChangeThreadPriority(id, 0xFF & _thread_map_priority(priority)); }

#if 0
	waithandle_t waithandle_create() {
		return sceKernelCreateSema("my_sema", 0, 1, 1, 0);
	}

	void waithandle_release(waithandle_t wh) {
		sceKernelDeleteSema(wh);
	}

	void waithandle_lock(waithandle_t wh) {
		int rv = 0;
		do {
			rv = sceKernelWaitSema(wh, 1, NULL);
		} while(rv < 0);
	}

	void waithandle_unlock(waithandle_t wh) {
		sceKernelSignalSema(wh, 1);
	}
#endif

#else //non-vita

	#include <rthreads/rthreads.h>

	static void _thread_func(void* p)
	{
		void** argp = static_cast<void**>(p);
		threadfunc_t func = reinterpret_cast<threadfunc_t>(argp[0]);
		(*func)(argp[1]);
	}

	thread_t thread_run(threadfunc_t func, void* p, int priority)
	{
		void* argp[2];
		sthread_t *thid = NULL;
		argp[0] = reinterpret_cast<void*>(func);
		argp[1] = p;

		thid = sthread_create(_thread_func, &argp);
		sthread_detach(thid);
			
		return thid;	
	}

	void thread_sleep(int ms) { retro_sleep(ms); }

	thread_t thread_get() { return 0; }

	void thread_set_priority(thread_t id, int priority) { }

	#endif

#endif //THREADED_RENDERER
