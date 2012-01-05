// Sets up common environment for Shay Green's libraries.
// To change configuration options, modify blargg_config.h, not this file.

// Gb_Snd_Emu 0.2.0
#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

#include <stdlib.h>
#include <limits.h>

#undef BLARGG_COMMON_H
// allow blargg_config.h to #include blargg_common.h
#include "blargg_config.h"
#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

// STATIC_CAST(T,expr): Used in place of static_cast<T> (expr)
// CONST_CAST( T,expr): Used in place of const_cast<T> (expr)
#ifndef STATIC_CAST
	#if __GNUC__ >= 4
		#define STATIC_CAST(T,expr) static_cast<T> (expr)
		#define CONST_CAST( T,expr) const_cast<T> (expr)
	#else
		#define STATIC_CAST(T,expr) ((T) (expr))
		#define CONST_CAST( T,expr) ((T) (expr))
	#endif
#endif

// blargg_err_t (0 on success, otherwise error string)
#ifndef blargg_err_t
	typedef const char* blargg_err_t;
#endif

// blargg_vector - very lightweight vector of POD types (no constructor/destructor)
template<class T>
class blargg_vector {
	T* begin_;
	size_t size_;
public:
	blargg_vector() : begin_( 0 ), size_( 0 ) { }
	~blargg_vector() { free( begin_ ); }
	size_t size() const { return size_; }
	T* begin() const { return begin_; }
	T* end() const { return begin_ + size_; }
	blargg_err_t resize( size_t n )
	{
		// TODO: blargg_common.cpp to hold this as an outline function, ugh
		void* p = realloc( begin_, n * sizeof (T) );
		if ( p )
			begin_ = (T*) p;
		else if ( n > size_ ) // realloc failure only a problem if expanding
			return "Out of memory";
		size_ = n;
		return 0;
	}
	void clear() { void* p = begin_; begin_ = 0; size_ = 0; free( p ); }
	T& operator [] ( size_t n ) const
	{
		return begin_ [n];
	}
};

// BLARGG_COMPILER_HAS_BOOL: If 0, provides bool support for old compiler. If 1,
// compiler is assumed to support bool. If undefined, availability is determined.
#ifndef BLARGG_COMPILER_HAS_BOOL
	#if defined (__MWERKS__)
		#if !__option(bool)
			#define BLARGG_COMPILER_HAS_BOOL 0
		#endif
	#elif defined (_MSC_VER)
		#if _MSC_VER < 1100
			#define BLARGG_COMPILER_HAS_BOOL 0
		#endif
	#elif defined (__GNUC__)
		// supports bool
	#elif __cplusplus < 199711
		#define BLARGG_COMPILER_HAS_BOOL 0
	#endif
#endif
#if defined (BLARGG_COMPILER_HAS_BOOL) && !BLARGG_COMPILER_HAS_BOOL
	// If you get errors here, modify your blargg_config.h file
	typedef int bool;
	const bool true  = 1;
	const bool false = 0;
#endif

// blargg_long/blargg_ulong = at least 32 bits, int if it's big enough

#if INT_MAX < 0x7FFFFFFF || LONG_MAX == 0x7FFFFFFF
	typedef long blargg_long;
#else
	typedef int blargg_long;
#endif

#if UINT_MAX < 0xFFFFFFFF || ULONG_MAX == 0xFFFFFFFF
	typedef unsigned long blargg_ulong;
#else
	typedef unsigned blargg_ulong;
#endif

// HAVE_STDINT_H: If defined, use <stdint.h> for int8_t etc.
#if defined (HAVE_STDINT_H)
	#include <stdint.h>

// HAVE_INTTYPES_H: If defined, use <stdint.h> for int8_t etc.
#elif defined (HAVE_INTTYPES_H)
	#include <inttypes.h>
#endif

#endif
#endif
