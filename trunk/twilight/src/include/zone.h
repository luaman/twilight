/*
	$RCSfile$

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __ZONE_H
#define __ZONE_H

#define ZONENAMESIZE 128				// to help avoid wasted pages
#define MEMCLUMPSIZE (65536 - 1536)		// smallest unit we care about
#define MEMUNIT 8
#define MEMBITS (MEMCLUMPSIZE / MEMUNIT)
#define MEMBITINTS (MEMBITS / 32)
#define MEMHEADER_SENTINEL 0xABADCAFE
#define MEMCLUMP_SENTINEL 0xDEADF00D

typedef struct memheader_s
{
	struct memheader_s *chain;			// next memheader in this zone
	struct memzone_s   *zone;			// the parent zone
	struct memclump_s  *clump;			// parent clump, if any
	int					size;			// allocated size, excludes header
	char			   *filename;		// source file of this alloc
	int					fileline;		// line of alloc in source file
	Uint32				sentinel1;		// MEMCLUMP_SENTINEL
	// followed by data and another MEMCLUMP_SENTINEL
} memheader_t;

typedef struct memclump_s
{
	Uint8				block[MEMCLUMPSIZE];	// contents of the clump
	Uint32				sentinel1;				// MEMCLUMP_SENTINEL
	int					bits[MEMBITINTS];		// used to mark allocations
	Uint32				sentinel2; 				// MEMCLUMP_SENTINEL
	int					blocksinuse;			// zone usage refcount
	int					largestavailable;		// updated on alloc/free
	struct memclump_s  *chain;					// next clump in chain
} memclump_t;

typedef struct memzone_s
{
	struct memheader_s *chain;				// chain of individual allocs
	struct memclump_s  *clumpchain;			// clain of clumps, if any
	int					totalsize;			// total size of allocs
	int					realsize;			// actual malloc size of zone
	int					lastchecksize;		// last listed size (for detecting leaks)
	char				name[ZONENAMESIZE];	// name of this zone
	struct memzone_s   *next;				// next zone in list
} memzone_t;

#define Zone_Alloc(zone,size) _Zone_Alloc(zone, size, __FILE__, __LINE__)
#define Zone_Free(mem) _Zone_Free(mem, __FILE__, __LINE__)
#define Zone_CheckSentinels(data) _Zone_CheckSentinels(data, __FILE__, __LINE__)
#define Zone_CheckSentinelsZone(zone) _Zone_CheckSentinelsZone(zone, __FILE__, __LINE__)
#define Zone_CheckSentinelsGlobal() _Zone_CheckSentinelsGlobal(__FILE__, __LINE__)
#define Zone_AllocZone(name) _Zone_AllocZone(name, __FILE__, __LINE__)
#define Zone_FreeZone(zone) _Zone_FreeZone(zone, __FILE__, __LINE__)
#define Zone_EmptyZone(zone) _Zone_EmptyZone(zone, __FILE__, __LINE__)

void *_Zone_Alloc(memzone_t *zone, int size, char *filename, int fileline);
void _Zone_Free(void *data, char *filename, int fileline);
memzone_t *_Zone_AllocZone(char *name, char *filename, int fileline);
void _Zone_FreeZone(memzone_t **zone, char *filename, int fileline);
void _Zone_EmptyZone(memzone_t *zone, char *filename, int fileline);
void _Zone_CheckSentinels(void *data, char *filename, int fileline);
void _Zone_CheckSentinelsZone(memzone_t *zone, char *filename, int fileline);
void _Zone_CheckSentinelsGlobal(char *filename, int fileline);

void Zone_Init (void);
void Zone_Init_Commands (void);

// used for temporary allocations
extern memzone_t *tempzone;

extern memzone_t *stringzone;
#define Z_Malloc(size) Zone_Alloc(stringzone,size)
#define Z_Free(data) Zone_Free(data)


/*
 memory allocation


H_??? The hunk manages the entire memory block given to quake.  It must be
contiguous.  Memory can be allocated from either the low or high end in a
stack fashion.  The only way memory is released is by resetting one of the
pointers.

Hunk allocations should be given a name, so the Hunk_Print () function
can display usage.

Hunk allocations are guaranteed to be 16 byte aligned.

The video buffers are allocated high to avoid leaving a hole underneath
server allocations when changing to a higher video mode.


Z_??? Zone memory functions used for small, dynamic allocations like text
strings from command input.  There is only about 48K for it, allocated at
the very bottom of the hunk.

Cache_??? Cache memory is for objects that can be dynamically loaded and
can usefully stay persistant between levels.  The size of the cache
fluctuates from level to level.

To allocate a cachable object


Temp_??? Temp memory is used for file loading and surface caching.  The size
of the cache memory is adjusted so that there is a minimum of 512k remaining
for temp memory.


------ Top of Memory -------

high hunk allocations

<--- high hunk reset point held by vid

video buffer

z buffer

surface cache

<--- high hunk used

cachable memory

<--- low hunk used

client and server low hunk allocations

<-- low hunk reset point held by host

startup hunk allocations

Zone block

----- Bottom of Memory -----



*/

extern int hunk_size;

void       *Hunk_Alloc (int size);		// returns 0 filled memory
void       *Hunk_AllocName (int size, char *name);

void       *Hunk_HighAllocName (int size, char *name);

int         Hunk_LowMark (void);
void        Hunk_FreeToLowMark (int mark);

int         Hunk_HighMark (void);
void        Hunk_FreeToHighMark (int mark);

void       *Hunk_TempAlloc (int size);

void        Hunk_Check (void);

typedef struct cache_user_s {
	void       *data;
} cache_user_t;

void        Cache_Flush (void);

void       *Cache_Check (cache_user_t *c);

// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL

void        Cache_Free (cache_user_t *c);

void       *Cache_Alloc (cache_user_t *c, int size, char *name);

// Returns NULL if all purgable data was tossed and there still
// wasn't enough room.

void        Cache_Report (void);

#endif // __ZONE_H

