#include "thread.h"

#if VITA

#include <stdint.h>
#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>

typedef struct {
	threadfunc_t func;
	void* p;
} threadp;

static int _thread_func(SceSize args, void* p) {
	auto t = static_cast<threadp*>(p);
	t->func(t->p);
	delete t;
	return 0;
}

void thread_run(threadfunc_t func, void* p) {
	auto th = sceKernelCreateThread("my_thread", _thread_func, 0x10000100, 0x10000, 0, 0, NULL);
	auto t = new threadp();
	t->func = func;
	t->p = p;
	sceKernelStartThread(th, 0, t);
}

#endif
