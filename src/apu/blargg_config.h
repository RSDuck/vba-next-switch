// $package user configuration file. Don't replace when updating library.

#ifndef BLARGG_CONFIG_H
#define BLARGG_CONFIG_H

// Uncomment to have Gb_Apu run at 4x normal clock rate (16777216 Hz), useful in
// a Game Boy Advance emulator.
#define GB_APU_OVERCLOCK 4

#define GB_APU_CUSTOM_STATE 1

// Uncomment to enable platform-specific (and possibly non-portable) optimizations.
#define BLARGG_NONPORTABLE 1

// Uncomment if automatic byte-order determination doesn't work
//#define BLARGG_BIG_ENDIAN 1

// Use standard config.h if present
#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#endif
