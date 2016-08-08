#ifndef __THREAD_H__
#define __THREAD_H__

#ifdef THREADED_RENDERER
typedef void(*threadfunc_t)(void*);

void thread_run(threadfunc_t func, void* p);
void thread_sleep(int ms);
#endif

#endif

