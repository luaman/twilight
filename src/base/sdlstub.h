/*
	$RCSfile$

	Copyright (C) 1997-2001  Sam Lantinga

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:
	
		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifndef _SDLSTUB_H
#define _SDLSTUB_H

/*
 * In order to build servers without SDL under UNIX, we're basically putting
 * the smallest bits of SDL right in to the engine source.  Everything in
 * this file comes from SDL and is Sam Lantinga's work.
 *
 * FIXME: Most of this crap should go away after we know it's not needed
 */


/*
 * SDL_types.h
 */


/* The number of elements in a table */
#define SDL_TABLESIZE(table)	(sizeof(table)/sizeof(table[0]))

/* Basic data types */
typedef enum {
	SDL_FALSE = 0,
	SDL_TRUE  = 1
} SDL_bool;
typedef unsigned char	Uint8;
typedef signed char	Sint8;
typedef unsigned short	Uint16;
typedef signed short	Sint16;
typedef unsigned int	Uint32;
typedef signed int	Sint32;

/* Figure out how to support 64-bit datatypes */
#if !defined(__STRICT_ANSI__)
#if defined(__GNUC__) || defined(__MWERKS__) /* MJS */
#define SDL_HAS_64BIT_TYPE	long long
#elif defined(_MSC_VER) /* VC++ */
#define SDL_HAS_64BIT_TYPE	__int64
#endif
#endif /* !__STRICT_ANSI__ */

/* The 64-bit datatype isn't supported on all platforms */
#ifdef SDL_HAS_64BIT_TYPE
typedef unsigned SDL_HAS_64BIT_TYPE Uint64;
typedef signed SDL_HAS_64BIT_TYPE Sint64;
#else
/* This is really just a hack to prevent the compiler from complaining */
typedef struct {
	Uint32 hi;
	Uint32 lo;
} Uint64, Sint64;
#endif

/* Make sure the types really have the right sizes */
#define SDL_COMPILE_TIME_ASSERT(name, x)               \
       typedef int SDL_dummy_ ## name[(x) * 2 - 1]

SDL_COMPILE_TIME_ASSERT(uint8, sizeof(Uint8) == 1);
SDL_COMPILE_TIME_ASSERT(sint8, sizeof(Sint8) == 1);
SDL_COMPILE_TIME_ASSERT(uint16, sizeof(Uint16) == 2);
SDL_COMPILE_TIME_ASSERT(sint16, sizeof(Sint16) == 2);
SDL_COMPILE_TIME_ASSERT(uint32, sizeof(Uint32) == 4);
SDL_COMPILE_TIME_ASSERT(sint32, sizeof(Sint32) == 4);
SDL_COMPILE_TIME_ASSERT(uint64, sizeof(Uint64) == 8);
SDL_COMPILE_TIME_ASSERT(sint64, sizeof(Sint64) == 8);

#undef SDL_COMPILE_TIME_ASSERT

/* General keyboard/mouse state definitions */
enum { SDL_PRESSED = 0x01, SDL_RELEASED = 0x00 };


/*
 * SDL_endian.h
 */

/* These functions read and write data of the specified endianness,
   dynamically translating to the host machine endianness.

   e.g.: If you want to read a 16 bit value on big-endian machine from
         an open file containing little endian values, you would use:
		value = SDL_ReadLE16(rp);
         Note that the read/write functions use SDL_RWops pointers
         instead of FILE pointers.  This allows you to read and write
         endian values from large chunks of memory as well as files
         and other data sources.
*/

#include <stdio.h>

//include "SDL_types.h"
//include "SDL_rwops.h"
//include "SDL_byteorder.h"

/* The macros used to swap values */
/* Try to use superfast macros on systems that support them */
#ifdef linux
#include <asm/byteorder.h>
#ifdef __arch__swab16
#define SDL_Swap16  __arch__swab16
#endif
#ifdef __arch__swab32
#define SDL_Swap32  __arch__swab32
#endif
#endif /* linux */
/* Use inline functions for compilers that support them, and static
   functions for those that do not.  Because these functions become
   static for compilers that do not support inline functions, this
   header should only be included in files that actually use them.
*/
#ifndef SDL_Swap16
static inline Uint16 SDL_Swap16(Uint16 D) {
	return((D<<8)|(D>>8));
}
#endif
#ifndef SDL_Swap32
static inline Uint32 SDL_Swap32(Uint32 D) {
	return((D<<24)|((D<<8)&0x00FF0000)|((D>>8)&0x0000FF00)|(D>>24));
}
#endif
#ifdef SDL_HAS_64BIT_TYPE
#ifndef SDL_Swap64
static inline Uint64 SDL_Swap64(Uint64 val) {
	Uint32 hi, lo;

	/* Separate into high and low 32-bit values and swap them */
	lo = (Uint32)(val&0xFFFFFFFF);
	val >>= 32;
	hi = (Uint32)(val&0xFFFFFFFF);
	val = SDL_Swap32(lo);
	val <<= 32;
	val |= SDL_Swap32(hi);
	return(val);
}
#endif
#else
#ifndef SDL_Swap64
/* This is mainly to keep compilers from complaining in SDL code.
   If there is no real 64-bit datatype, then compilers will complain about
   the fake 64-bit datatype that SDL provides when it compiles user code.
*/
#define SDL_Swap64(X)	(X)
#endif
#endif /* SDL_HAS_64BIT_TYPE */


/* Byteswap item from the specified endianness to the native endianness */
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define SDL_SwapLE16(X)	(X)
#define SDL_SwapLE32(X)	(X)
#define SDL_SwapLE64(X)	(X)
#define SDL_SwapBE16(X)	SDL_Swap16(X)
#define SDL_SwapBE32(X)	SDL_Swap32(X)
#define SDL_SwapBE64(X)	SDL_Swap64(X)
#else
#define SDL_SwapLE16(X)	SDL_Swap16(X)
#define SDL_SwapLE32(X)	SDL_Swap32(X)
#define SDL_SwapLE64(X)	SDL_Swap64(X)
#define SDL_SwapBE16(X)	(X)
#define SDL_SwapBE32(X)	(X)
#define SDL_SwapBE64(X)	(X)
#endif


// the only functions provided by sv_sdlstub.c
void SDL_Delay(Uint32 ms);
Uint32 SDL_GetTicks(void);
int SDL_Init(Uint32 flags);
void SDL_Quit(void);
// various #defines
#define   SDL_INIT_TIMER          0x00000001

#endif // _SDLSTUB_H

