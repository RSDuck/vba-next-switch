#ifndef __THREAD_H__
#define __THREAD_H__

#ifdef THREADED_RENDERER
typedef void(*threadfunc_t)(void*);

void thread_run(threadfunc_t func, void* p);
void thread_sleep(int ms);

#ifndef THREAD_LOCAL
	#if __STDC_VERSION__ >= 201112 && !defined __STDC_NO_THREADS__
		#define THREAD_LOCAL _Thread_local
	#elif defined _WIN32 && (defined _MSC_VER || defined __ICL || defined __DMC__ || defined __BORLANDC__ )
		#define THREAD_LOCAL __declspec(thread) 
	/* note that ICC (linux) and Clang are covered by __GNUC__ */
	#elif defined __GNUC__ || defined __SUNPRO_C || defined __xlC__
		#define THREAD_LOCAL __thread
	#else
		#error "THREAD_LOCAL is not defined."
	#endif
#endif

#endif

#endif

