#ifndef __THREAD_H__
#define __THREAD_H__

typedef void(*threadfunc_t)(void*);

void thread_run(threadfunc_t func, void* p);

#endif

