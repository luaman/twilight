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

#include "qtypes.h"

#define ZONENAMESIZE 128				// to help avoid wasted pages
#define MEMCLUMPSIZE (65536 - 1536)		// smallest unit we care about
#define MEMUNIT 8
#define MEMBITS (MEMCLUMPSIZE / MEMUNIT)
#define MEMBITINTS (MEMBITS / 32)
#define MEMHEADER_SENTINEL1 0xDEADF00D
#define MEMHEADER_SENTINEL2 0xDF
#define MEMCLUMP_SENTINEL 0xABADCAFE

typedef struct memheader_s
{
	struct memheader_s	*chain;			// next memheader in this zone
	struct memzone_s	*zone;			// the parent zone
	size_t				size;			// allocated size, excludes header
	const char			*filename;		// source file of this alloc
	int					fileline;		// line of alloc in source file
	Uint32				sentinel1;	// MEMHEADER_SENTINEL1
	// followed by data and a MEMHEADER_SENTINEL2 byte
} memheader_t;

typedef struct memzone_s
{
	struct memheader_s	*chain;				// chain of individual allocs
	qboolean			single;				// free the zone on first alloc free
	int					totalsize;			// total size of allocs
	int					realsize;			// actual malloc size of zone
	int					lastchecksize;		// last listed size (for detecting leaks)
	char				name[ZONENAMESIZE];	// name of this zone
	struct memzone_s	*next;				// next zone in list
} memzone_t;

#define Zone_AllocName(name,size) _Zone_AllocName(name, size, __FILE__, __LINE__)
#define Zone_Alloc(zone,size) _Zone_Alloc(zone, size, __FILE__, __LINE__)
#define Zone_Free(mem) _Zone_Free(mem, __FILE__, __LINE__)
#define Zone_CheckSentinels(data) _Zone_CheckSentinels(data, __FILE__, __LINE__)
#define Zone_CheckSentinelsZone(zone) _Zone_CheckSentinelsZone(zone, __FILE__, __LINE__)
#define Zone_CheckSentinelsGlobal() _Zone_CheckSentinelsGlobal(__FILE__, __LINE__)
#define Zone_AllocZone(name) _Zone_AllocZone(name, __FILE__, __LINE__)
#define Zone_FreeZone(zone) _Zone_FreeZone(zone, __FILE__, __LINE__)
#define Zone_EmptyZone(zone) _Zone_EmptyZone(zone, __FILE__, __LINE__)

void *_Zone_Alloc(memzone_t *zone, const size_t size, const char *filename, const int fileline);
void *_Zone_AllocName(const char *name, const size_t size, const char *filename, const int fileline);
void _Zone_Free(void *data, const char *filename, const int fileline);
memzone_t *_Zone_AllocZone(const char *name, const char *filename, const int fileline);
void _Zone_FreeZone(memzone_t **zone, const char *filename, const int fileline);
void _Zone_EmptyZone(memzone_t *zone, const char *filename, const int fileline);
void _Zone_CheckSentinels(void *data, const char *filename, const int fileline);
void _Zone_CheckSentinelsZone(memzone_t *zone, const char *filename, const int fileline);
void _Zone_CheckSentinelsGlobal(const char *filename, const int fileline);

void Zone_PrintZone (const int all, memzone_t *zone);

void Zone_Init (void);
void Zone_Init_Commands (void);

// used for temporary allocations
extern memzone_t *tempzone;
extern memzone_t *stringzone;
#define Z_Malloc(size) Zone_Alloc(stringzone,size)
#define Z_Free(data) Zone_Free(data)

char *zasprintf (memzone_t *zone, const char *format, ...);

#endif // __ZONE_H

