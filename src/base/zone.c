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

memzone_t *zonechain = NULL;

void *_Zone_AllocName (char *name, size_t size, char *filename, int fileline)
{
	memzone_t	*zone;
	zone = _Zone_AllocZone (name, filename, fileline);
	zone->single = true;

	return _Zone_Alloc (zone, size, filename, fileline);
}

void *_Zone_Alloc(memzone_t *zone, size_t size, char *filename, int fileline)
{
	int i, j, k, needed, endbit, largest;
	memclump_t *clump, **clumpchainpointer;
	memheader_t *mem;
	if (zone == NULL)
		Sys_Error("Zone_Alloc: zone == NULL (alloc at %s:%i)", filename, fileline);
	if (size <= 0)
		Sys_Error("Zone_Alloc: size <= 0 (alloc at %s:%i, zone %s, size %i)", filename, fileline, zone->name, size);
	Com_DPrintf("Zone_Alloc: zone %s, file %s:%i, size %i bytes\n", zone->name, filename, fileline, size);
	zone->totalsize += size;
	if (size < 4096)
	{
		// clumping
		needed = (sizeof(memheader_t) + size + sizeof(Uint32) + (MEMUNIT - 1)) / MEMUNIT;
		endbit = MEMBITS - needed;
		for (clumpchainpointer = &zone->clumpchain;*clumpchainpointer;clumpchainpointer = &(*clumpchainpointer)->chain)
		{
			clump = *clumpchainpointer;
			if (clump->sentinel1 != MEMCLUMP_SENTINEL)
				//Sys_Error("Zone_Alloc: trashed clump sentinel 1 (alloc at %s:%d, zone %s)", filename, fileline, zone->name);
				Sys_Error("Zone_Alloc: trashed clump sentinel 1 (zone %s)", zone->name);
			if (clump->sentinel2 != MEMCLUMP_SENTINEL)
				Sys_Error("Zone_Alloc: trashed clump sentinel 2 (alloc at %s:%d, zone %s)", filename, fileline, zone->name);
			if (clump->largestavailable >= needed)
			{
				largest = 0;
				for (i = 0;i < endbit;i++)
				{
					if (clump->bits[i >> 5] & (1 << (i & 31)))
						continue;
					k = i + needed;
					for (j = i;i < k;i++)
						if (clump->bits[i >> 5] & (1 << (i & 31)))
							goto loopcontinue;
					goto choseclump;
	loopcontinue:;
					if (largest < j - i)
						largest = j - i;
				}
				// since clump falsely advertised enough space (nothing wrong
				// with that), update largest count to avoid wasting time in
				// later allocations
				clump->largestavailable = largest;
			}
		}
		zone->realsize += sizeof(memclump_t);
		clump = malloc(sizeof(memclump_t));
		if (clump == NULL)
			Sys_Error("Zone_Alloc: out of memory (alloc at %s:%i, zone %s)", filename, fileline, zone->name);
		memset(clump, 0, sizeof(memclump_t));
		*clumpchainpointer = clump;
		clump->sentinel1 = MEMCLUMP_SENTINEL;
		clump->sentinel2 = MEMCLUMP_SENTINEL;
		clump->chain = NULL;
		clump->blocksinuse = 0;
		clump->largestavailable = MEMBITS - needed;
		j = 0;
choseclump:
		mem = (memheader_t *)((Uint8 *) clump->block + j * MEMUNIT);
		mem->clump = clump;
		clump->blocksinuse += needed;
		for (i = j + needed;j < i;j++)
			clump->bits[j >> 5] |= (1 << (j & 31));
	}
	else
	{
		// big allocations are not clumped
		zone->realsize += sizeof(memheader_t) + size + sizeof(Uint32);
		mem = malloc(sizeof(memheader_t) + size + sizeof(Uint32));
		if (mem == NULL)
			Sys_Error("Zone_Alloc: out of memory (alloc at %s:%i, zone %s)", filename, fileline, zone->name);
		mem->clump = NULL;
	}
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

void _Zone_Free(void *data, char *filename, int fileline)
{
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
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
		return _Zone_FreeZone(&zone, filename, fileline);
	}

	Com_DPrintf("Zone_Free: zone %s, alloc %s:%i, free %s:%i, size %i bytes\n", zone->name, mem->filename, mem->fileline, filename, fileline, mem->size);
	for (memchainpointer = &zone->chain;*memchainpointer;memchainpointer = &(*memchainpointer)->chain)
	{
		if (*memchainpointer == mem)
		{
			*memchainpointer = mem->chain;
			zone->totalsize -= mem->size;
			if ((clump = mem->clump))
			{
				if (clump->sentinel1 != MEMCLUMP_SENTINEL)
					Sys_Error("Zone_Free: trashed clump sentinel 1 (free at %s:%i, zone %s)", filename, fileline, zone->name);
				if (clump->sentinel2 != MEMCLUMP_SENTINEL)
					Sys_Error("Zone_Free: trashed clump sentinel 2 (free at %s:%i, zone %s)", filename, fileline, zone->name);
				firstblock = ((Uint8 *) mem - (Uint8 *) clump->block);
				if (firstblock & (MEMUNIT - 1))
					Sys_Error("Zone_Free: address not valid in clump (free at %s:%i, zone %s)", filename, fileline, zone->name);
				firstblock /= MEMUNIT;
				endblock = firstblock + ((sizeof(memheader_t) + mem->size + sizeof(Uint32) + (MEMUNIT - 1)) / MEMUNIT);
				clump->blocksinuse -= endblock - firstblock;
				// could use &, but we know the bit is set
				for (i = firstblock;i < endblock;i++)
					clump->bits[i >> 5] -= (1 << (i & 31));
				if (clump->blocksinuse <= 0)
				{
					// unlink from chain
					for (clumpchainpointer = &zone->clumpchain;*clumpchainpointer;clumpchainpointer = &(*clumpchainpointer)->chain)
					{
						if (*clumpchainpointer == clump)
						{
							*clumpchainpointer = clump->chain;
							break;
						}
					}
					zone->realsize -= sizeof(memclump_t);
					memset(clump, 0xBF, sizeof(memclump_t));
					free(clump);
				}
				else
				{
					// clump still has some allocations
					// force re-check of largest available space on next alloc
					clump->largestavailable = MEMBITS - clump->blocksinuse;
				}
			}
			else
			{
				zone->realsize -= sizeof(memheader_t) + mem->size + sizeof(Uint32);
				memset(mem, 0xBF, sizeof(memheader_t) + mem->size + sizeof(Uint32));
				free(mem);
			}
			return;
		}
	}
	Sys_Error("Zone_Free: not allocated (free at %s:%i)", filename, fileline);
}

memzone_t *_Zone_AllocZone(char *name, char *filename, int fileline)
{
	memzone_t *zone;
	zone = malloc(sizeof(memzone_t));
	if (zone == NULL)
		Sys_Error("Zone_AllocZone: out of memory (alloczone at %s:%i)", filename, fileline);
	memset(zone, 0, sizeof(memzone_t));
	zone->chain = NULL;
	zone->totalsize = 0;
	zone->realsize = sizeof(memzone_t);
	strcpy(zone->name, name);
	zone->next = zonechain;
	zonechain = zone;
	return zone;
}

void _Zone_FreeZone(memzone_t **zone, char *filename, int fileline)
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

void _Zone_EmptyZone(memzone_t *zone, char *filename, int fileline)
{
	if (zone == NULL)
		Sys_Error("Zone_EmptyZone: zone == NULL (emptyzone at %s:%i)", filename, fileline);

	// free memory owned by the zone
	while (zone->chain)
		Zone_Free((void *)((Uint8 *) zone->chain + sizeof(memheader_t)));
}

void _Zone_CheckSentinels(void *data, char *filename, int fileline)
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

static void _Zone_CheckClumpSentinels(memclump_t *clump, char *filename, int fileline, memzone_t *zone)
{
	// this isn't really very useful
	if (clump->sentinel1 != MEMCLUMP_SENTINEL)
		Sys_Error("Zone_CheckClumpSentinels: trashed sentinel 1 (sentinel check at %s:%i, zone %s)", filename, fileline, zone->name);
	if (clump->sentinel2 != MEMCLUMP_SENTINEL)
		Sys_Error("Zone_CheckClumpSentinels: trashed sentinel 2 (sentinel check at %s:%i, zone %s)", filename, fileline, zone->name);
}

void _Zone_CheckSentinelsZone(memzone_t *zone, char *filename, int fileline)
{
	memheader_t *mem;
	memclump_t *clump;
	for (mem = zone->chain;mem;mem = mem->chain)
		_Zone_CheckSentinels((void *)((Uint8 *) mem + sizeof(memheader_t)), filename, fileline);
	for (clump = zone->clumpchain;clump;clump = clump->chain)
		_Zone_CheckClumpSentinels(clump, filename, fileline, zone);
}

void _Zone_CheckSentinelsGlobal(char *filename, int fileline)
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

void Zone_PrintStats(void)
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

void Zone_PrintList(int listallocations)
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

void ZoneList_f(void)
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

void ZoneStats_f(void)
{
	Zone_CheckSentinelsGlobal();
	Zone_PrintStats();
}


//============================================================================


/*
========================
Zone_Init
========================
*/
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
