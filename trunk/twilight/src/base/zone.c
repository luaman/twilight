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

*/
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include "quakedef.h"
#include "cmd.h"
#include "common.h"
#include "strlib.h"
#include "sys.h"
#include "zone.h"
#include "stdlib.h"

static memzone_t *zonechain = NULL;

void *_Zone_AllocName (const char *name, const size_t size, const char *filename, const int fileline)
{
	memzone_t	*zone;
	zone = _Zone_AllocZone (name, filename, fileline);
	zone->single = true;

	return _Zone_Alloc (zone, size, filename, fileline);
}

void *_Zone_Alloc(memzone_t *zone, const size_t size, const char *filename, const int fileline)
{
	memheader_t *mem;
	if (zone == NULL)
		Sys_Error("Zone_Alloc: zone == NULL (alloc at %s:%i)", filename, fileline);
	if (size <= 0)
		Sys_Error("Zone_Alloc: size <= 0 (alloc at %s:%i, zone %s, size %i)", filename, fileline, zone->name, size);
	/*
	 * FIXME: This line causes an infinite loop, due to the console system
	 * calling Zone_Alloc which prints which calls the console system which
	 * calls Zone_Alloc...
	Com_DFPrintf(DEBUG_ZONE, "Zone_Alloc: zone %s, file %s:%i, size %i bytes\n", zone->name, filename, fileline, size);
	*/
	zone->totalsize += size;
	zone->realsize += sizeof(memheader_t) + size + sizeof(Uint32);
	mem = malloc(sizeof(memheader_t) + size + sizeof(Uint32));
	if (mem == NULL)
		Sys_Error("Zone_Alloc: out of memory (alloc at %s:%i, zone %s)", filename, fileline, zone->name);
	mem->filename = filename;
	mem->fileline = fileline;
	mem->size = size;
	mem->zone = zone;
	mem->sentinel1 = MEMHEADER_SENTINEL1;
	// some platforms (Sparc for instance) can not do unaligned writes, so this must be done as a char
	*((Uint8 *) mem + sizeof(memheader_t) + mem->size) = MEMHEADER_SENTINEL2;
	// append to head of list
	mem->chain = zone->chain;
	zone->chain = mem;
	memset((void *)((Uint8 *) mem + sizeof(memheader_t)), 0, mem->size);
	return (void *)((Uint8 *) mem + sizeof(memheader_t));
}

void _Zone_Free(void *data, const char *filename, const int fileline)
{
	memheader_t *mem, **memchainpointer;
	memzone_t *zone;
	if (data == NULL)
		Sys_Error("Zone_Free: data == NULL (called at %s:%i)", filename, fileline);


	mem = (memheader_t *)((Uint8 *) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		//Sys_Error("Zone_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i, zone %s)", mem->filename, mem->fileline, filename, fileline, mem->zone->name);
		Sys_Error("Zone_Free: trashed header sentinel 1 (free at %s:%i)", filename, fileline);
	if (*((Uint8 *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Zone_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i, zone %s)", mem->filename, mem->fileline, filename, fileline, mem->zone->name);
	zone = mem->zone;
	if (zone->single) {
		zone->single = false;
		_Zone_FreeZone(&zone, filename, fileline);
		return;
	}

	/*
	 * FIXME: This line causes an infinite loop, due to the console system
	 * which can call Zone_Free which prints which calls the console system
	 * which can call Zone_Free...
	 Com_DFPrintf(DEBUG_ZONE, "Zone_Free: zone %s, alloc %s:%i, free %s:%i, size %i bytes\n", zone->name, mem->filename, mem->fileline, filename, fileline, mem->size);
	*/
	for (memchainpointer = &zone->chain;*memchainpointer;memchainpointer = &(*memchainpointer)->chain)
	{
		if (*memchainpointer == mem)
		{
			*memchainpointer = mem->chain;
			zone->totalsize -= mem->size;
			zone->realsize -= sizeof(memheader_t) + mem->size + sizeof(Uint32);
			memset(mem, 0xBF, sizeof(memheader_t) + mem->size + sizeof(Uint32));
			free(mem);
			return;
		}
	}
	Sys_Error("Zone_Free: not allocated (free at %s:%i)", filename, fileline);
}

memzone_t *_Zone_AllocZone(const char *name, const char *filename, const int fileline)
{
	memzone_t *zone;
	zone = malloc(sizeof(memzone_t));
	if (zone == NULL)
		Sys_Error("Zone_AllocZone: out of memory (alloczone at %s:%i)", filename, fileline);
	memset(zone, 0, sizeof(memzone_t));
	zone->chain = NULL;
	zone->totalsize = 0;
	zone->realsize = sizeof(memzone_t);
	strlcpy_s (zone->name, name);
	zone->next = zonechain;
	zonechain = zone;
	return zone;
}

void _Zone_FreeZone(memzone_t **zone, const char *filename, const int fileline)
{
	memzone_t **chainaddress;
	if (*zone)
	{
		// unlink zone from chain
		for (chainaddress = &zonechain;*chainaddress && *chainaddress != *zone;chainaddress = &((*chainaddress)->next));
		if (*chainaddress != *zone)
			Sys_Error("Zone_FreeZone: zone already free (freezone at %s:%i)", filename, fileline);
		*chainaddress = (*zone)->next;

		// free memory owned by the zone
		while ((*zone)->chain)
			Zone_Free((void *)((Uint8 *) (*zone)->chain + sizeof(memheader_t)));

		// free the zone itself
		memset(*zone, 0xBF, sizeof(memzone_t));
		free(*zone);
		*zone = NULL;
	}
}

void _Zone_EmptyZone(memzone_t *zone, const char *filename, const int fileline)
{
	if (zone == NULL)
		Sys_Error("Zone_EmptyZone: zone == NULL (emptyzone at %s:%i)", filename, fileline);

	// free memory owned by the zone
	while (zone->chain)
		Zone_Free((void *)((Uint8 *) zone->chain + sizeof(memheader_t)));
}

void _Zone_CheckSentinels(void *data, const char *filename, const int fileline)
{
	memheader_t *mem;

	if (data == NULL)
		Sys_Error("Zone_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline);

	mem = (memheader_t *)((Uint8 *) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		//Sys_Error("Zone_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i, zone %s)", mem->filename, mem->fileline, filename, fileline, mem->zone->name);
		Sys_Error("Zone_CheckSentinels: trashed header sentinel 1 (sentinel check at %s:%i)", filename, fileline);
	if (*((Uint8 *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Zone_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i, zone %s)", mem->filename, mem->fileline, filename, fileline, mem->zone->name);
}

void _Zone_CheckSentinelsZone(memzone_t *zone, const char *filename, const int fileline)
{
	memheader_t *mem;
	for (mem = zone->chain;mem;mem = mem->chain)
		_Zone_CheckSentinels((void *)((Uint8 *) mem + sizeof(memheader_t)), filename, fileline);
}

void _Zone_CheckSentinelsGlobal(const char *filename, const int fileline)
{
	memzone_t *zone;
	for (zone = zonechain;zone;zone = zone->next)
		_Zone_CheckSentinelsZone(zone, filename, fileline);
}

// used for temporary memory allocations around the engine, not for longterm
// storage, if anything in this zone stays allocated during gameplay, it is
// considered a leak
memzone_t *tempzone;
// string storage for console mainly
memzone_t *stringzone;
// used only for hunk
memzone_t *hunkzone;

static void
Zone_PrintStats(void)
{
	int count = 0, size = 0;
	memzone_t *zone;
	memheader_t *mem;
	Zone_CheckSentinelsGlobal();
	for (zone = zonechain;zone;zone = zone->next)
	{
		count++;
		size += zone->totalsize;
	}
	Com_Printf("%i zones, totalling %i bytes (%.3fMB)\n", count, size, size / 1048576.0);
	if (tempzone == NULL)
		Com_Printf("Error: no tempzone allocated\n");
	else if (tempzone->chain)
	{
		Com_Printf("%i bytes (%.3fMB) of temporary memory still allocated (Leak!)\n", tempzone->totalsize, tempzone->totalsize / 1048576.0);
		Com_Printf("listing temporary memory allocations:\n");
		for (mem = tempzone->chain;mem;mem = mem->chain)
			Com_Printf("%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline);
	}
}

static void
Zone_PrintList(const int listallocations)
{
	memzone_t *zone;
	memheader_t *mem;
	Zone_CheckSentinelsGlobal();
	Com_Printf("memory zone list:\n"
	           "size    name\n");
	for (zone = zonechain;zone;zone = zone->next)
	{
		if (zone->lastchecksize != 0 && zone->totalsize != zone->lastchecksize)
			Com_Printf("%6ik (%6ik actual) %s (%i byte change)\n", (zone->totalsize + 1023) / 1024, (zone->realsize + 1023) / 1024, zone->name, zone->totalsize - zone->lastchecksize);
		else
			Com_Printf("%6ik (%6ik actual) %s\n", (zone->totalsize + 1023) / 1024, (zone->realsize + 1023) / 1024, zone->name);
		zone->lastchecksize = zone->totalsize;
		if (listallocations)
			for (mem = zone->chain;mem;mem = mem->chain)
				Com_Printf("%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline);
	}
}

static void
ZoneList_f(void)
{
	switch(Cmd_Argc())
	{
	case 1:
		Zone_PrintList(false);
		Zone_PrintStats();
		break;
	case 2:
		if (!strcmp(Cmd_Argv(1), "all"))
		{
			Zone_PrintList(true);
			Zone_PrintStats();
			break;
		}
		// drop through
	default:
		Com_Printf("ZoneList_f: unrecognized options\nusage: zonelist [all]\n");
		break;
	}
}

static void
ZoneStats_f(void)
{
	Zone_CheckSentinelsGlobal();
	Zone_PrintStats();
}


//============================================================================



void Zone_Init (void)
{
	tempzone = Zone_AllocZone("Temporary Memory");
	stringzone = Zone_AllocZone("Strings");
}

void Zone_Init_Commands (void)
{
	Cmd_AddCommand ("zonestats", ZoneStats_f);
	Cmd_AddCommand ("zonelist", ZoneList_f);
}


//============================================================================

#include <stdarg.h>

//Zone Allocating String Print Formatted
char *
zasprintf (memzone_t *zone, const char *format, ...)
{
	size_t length;
	va_list argptr;
	char *p;
	char text[4096];

	va_start (argptr, format);
	length = vsnprintf (text, sizeof (text), format, argptr);
	// note: assumes Zone_Alloc clears memory to zero
	p = Zone_Alloc(zone, length + 1);
	if (length > sizeof(text))
		length = vsprintf (p, format, argptr);
	else
		memcpy(p, text, length);
	va_end (argptr);

	return p;
}
