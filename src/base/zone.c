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
// Z_zone.c
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "cmd.h"
#include "common.h"
#include "strlib.h"
#include "sys.h"
#include "zone.h"
#include "stdlib.h"

memzone_t *zonechain = NULL;

void foo (char *x,...) { (*(int *) 0x0)=0; }
#define Sys_Error	foo

void *_Zone_Alloc(memzone_t *zone, int size, char *filename, int fileline)
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
		needed = (sizeof(memheader_t) + size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT;
		endbit = MEMBITS - needed;
		for (clumpchainpointer = &zone->clumpchain;*clumpchainpointer;clumpchainpointer = &(*clumpchainpointer)->chain)
		{
			clump = *clumpchainpointer;
			if (clump->sentinel1 != MEMCLUMP_SENTINEL)
				Sys_Error("Zone_Alloc: trashed clump sentinel 1 (alloc at %s:%d, zone %s)", filename, fileline, zone->name);
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
		mem = (memheader_t *)((long) clump->block + j * MEMUNIT);
		mem->clump = clump;
		clump->blocksinuse += needed;
		for (i = j + needed;j < i;j++)
			clump->bits[j >> 5] |= (1 << (j & 31));
	}
	else
	{
		// big allocations are not clumped
		zone->realsize += sizeof(memheader_t) + size + sizeof(int);
		mem = malloc(sizeof(memheader_t) + size + sizeof(int));
		if (mem == NULL)
			Sys_Error("Zone_Alloc: out of memory (alloc at %s:%i, zone %s)", filename, fileline, zone->name);
		mem->clump = NULL;
	}
	mem->filename = filename;
	mem->fileline = fileline;
	mem->size = size;
	mem->zone = zone;
	mem->sentinel1 = MEMHEADER_SENTINEL;
	*((int *)((long) mem + sizeof(memheader_t) + mem->size)) = MEMHEADER_SENTINEL;
	// append to head of list
	mem->chain = zone->chain;
	zone->chain = mem;
	memset((void *)((long) mem + sizeof(memheader_t)), 0, mem->size);
	return (void *)((long) mem + sizeof(memheader_t));
}

void _Zone_Free(void *data, char *filename, int fileline)
{
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
	memheader_t *mem, **memchainpointer;
	memzone_t *zone;
	if (data == NULL)
		Sys_Error("Zone_Free: data == NULL (called at %s:%i)", filename, fileline);


	mem = (memheader_t *)((long) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL)
		Sys_Error("Zone_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i, zone %s)", mem->filename, mem->fileline, filename, fileline, mem->zone->name);
	if (*((Uint32 *)((long) mem + sizeof(memheader_t) + mem->size)) != MEMHEADER_SENTINEL)
		Sys_Error("Zone_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i, zone %s)", mem->filename, mem->fileline, filename, fileline, mem->zone->name);
	zone = mem->zone;
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
				firstblock = ((long) mem - (long) clump->block);
				if (firstblock & (MEMUNIT - 1))
					Sys_Error("Zone_Free: address not valid in clump (free at %s:%i, zone %s)", filename, fileline, zone->name);
				firstblock /= MEMUNIT;
				endblock = firstblock + ((sizeof(memheader_t) + mem->size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT);
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
				zone->realsize -= sizeof(memheader_t) + mem->size + sizeof(int);
				memset(mem, 0xBF, sizeof(memheader_t) + mem->size + sizeof(int));
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
			Zone_Free((void *)((long) (*zone)->chain + sizeof(memheader_t)));

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
		Zone_Free((void *)((long) zone->chain + sizeof(memheader_t)));
}

void _Zone_CheckSentinels(void *data, char *filename, int fileline)
{
	memheader_t *mem;

	if (data == NULL)
		Sys_Error("Zone_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline);

	mem = (memheader_t *)((long) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL)
		Sys_Error("Zone_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i, zone %s)", mem->filename, mem->fileline, filename, fileline, mem->zone->name);
	if (*((Uint32 *)((long) mem + sizeof(memheader_t) + mem->size)) != MEMHEADER_SENTINEL)
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

void _Zone_CheckSentinelsGlobal(char *filename, int fileline)
{
	memheader_t *mem;
	memclump_t *clump;
	memzone_t *zone;
	for (zone = zonechain;zone;zone = zone->next)
	{
		for (mem = zone->chain;mem;mem = mem->chain)
			_Zone_CheckSentinels((void *)((long) mem + sizeof(memheader_t)), filename, fileline);
		for (clump = zone->clumpchain;clump;clump = clump->chain)
			_Zone_CheckClumpSentinels(clump, filename, fileline, zone);
	}
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


void        Cache_FreeLow (int new_low_hunk);
void        Cache_FreeHigh (int new_high_hunk);

//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

typedef struct {
	int         sentinal;
	int         size;					// including sizeof(hunk_t), -1 = not
										// allocated
	char        name[8];
} hunk_t;

Uint8      *hunk_base;
int         hunk_size;

int         hunk_low_used;
int         hunk_high_used;

qboolean    hunk_tempactive;
int         hunk_tempmark;

void        R_FreeTextures (void);

/*
==============
Hunk_Check

Run consistancy and sentinal trahing checks
==============
*/
void
Hunk_Check (void)
{
	hunk_t     *h;

	for (h = (hunk_t *) hunk_base; (Uint8 *) h != hunk_base + hunk_low_used;) {
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + (Uint8 *) h - hunk_base > hunk_size)
			Sys_Error ("Hunk_Check: bad size");
		h = (hunk_t *) ((Uint8 *) h + h->size);
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void
Hunk_Print (qboolean all)
{
	hunk_t     *h, *next, *endlow, *starthigh, *endhigh;
	int         count, sum;
	int         totalblocks;
	char        name[9];

	name[8] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;

	h = (hunk_t *) hunk_base;
	endlow = (hunk_t *) (hunk_base + hunk_low_used);
	starthigh = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);
	endhigh = (hunk_t *) (hunk_base + hunk_size);

	Com_Printf ("          :%8i total hunk size\n", hunk_size);
	Com_Printf ("-------------------------\n");

	while (1) {
		//
		// skip to the high hunk if done with low hunk
		//
		if (h == endlow) {
			Com_Printf ("-------------------------\n");
			Com_Printf ("          :%8i REMAINING\n",
						hunk_size - hunk_low_used - hunk_high_used);
			Com_Printf ("-------------------------\n");
			h = starthigh;
		}
		//
		// if totally done, break
		//
		if (h == endhigh)
			break;

		//
		// run consistancy checks
		//
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + (Uint8 *) h - hunk_base > hunk_size)
			Sys_Error ("Hunk_Check: bad size");

		next = (hunk_t *) ((Uint8 *) h + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		// 
		// print the single block
		// 
		memcpy (name, h->name, 8);
		if (all)
			Com_Printf ("%8p :%8i %8s\n", h, h->size, name);

		//
		// print the total
		//
		if (next == endlow || next == endhigh ||
			strncmp (h->name, next->name, 8)) {
			if (!all)
				Com_Printf ("          :%8i %8s (TOTAL)\n", sum, name);
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Com_Printf ("-------------------------\n");
	Com_Printf ("%8i total blocks\n", totalblocks);

}

/*
===================
Hunk_AllocName
===================
*/
void       *
Hunk_AllocName (int size, char *name)
{
	hunk_t     *h;

#ifdef PARANOID
	Hunk_Check ();
#endif

	if (size < 0)
		Sys_Error ("Hunk_Alloc: bad size: %i", size);

	size = sizeof (hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
//      Sys_Error ("Hunk_Alloc: failed on %i bytes",size);
		Sys_Error
			("Not enough RAM allocated.  Try starting using \"-mem 16\" on the command line.");

	h = (hunk_t *) (hunk_base + hunk_low_used);
	hunk_low_used += size;

	Cache_FreeLow (hunk_low_used);

	memset (h, 0, size);

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strlcpy (h->name, name, sizeof (h->name));

	return (void *) (h + 1);
}

/*
===================
Hunk_Alloc
===================
*/
void       *
Hunk_Alloc (int size)
{
	return Hunk_AllocName (size, "unknown");
}

int
Hunk_LowMark (void)
{
	return hunk_low_used;
}

void
Hunk_FreeToLowMark (int mark)
{
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %i", mark);
	memset (hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

int
Hunk_HighMark (void)
{
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark (hunk_tempmark);
	}

	return hunk_high_used;
}

void
Hunk_FreeToHighMark (int mark)
{
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark (hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used)
		Sys_Error ("Hunk_FreeToHighMark: bad mark %i", mark);
	memset (hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}


/*
===================
Hunk_HighAllocName
===================
*/
void       *
Hunk_HighAllocName (int size, char *name)
{
	hunk_t     *h;

	if (size < 0)
		Sys_Error ("Hunk_HighAllocName: bad size: %i", size);

	if (hunk_tempactive) {
		Hunk_FreeToHighMark (hunk_tempmark);
		hunk_tempactive = false;
	}
#ifdef PARANOID
	Hunk_Check ();
#endif

	size = sizeof (hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size) {
		Com_Printf ("Hunk_HighAlloc: failed on %i bytes\n", size);
		return NULL;
	}

	hunk_high_used += size;
	Cache_FreeHigh (hunk_high_used);

	h = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);

	memset (h, 0, size);
	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strlcpy (h->name, name, sizeof (h->name));

	return (void *) (h + 1);
}


/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void       *
Hunk_TempAlloc (int size)
{
	void       *buf;

	size = (size + 15) & ~15;

	if (hunk_tempactive) {
		Hunk_FreeToHighMark (hunk_tempmark);
		hunk_tempactive = false;
	}

	hunk_tempmark = Hunk_HighMark ();

	buf = Hunk_HighAllocName (size, "temp");

	hunk_tempactive = true;

	return buf;
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

typedef struct cache_system_s {
	int         size;					// including this header
	cache_user_t *user;
	char        name[16];
	struct cache_system_s *prev, *next;
	struct cache_system_s *lru_prev, *lru_next;	// for LRU flushing 
} cache_system_t;

cache_system_t *Cache_TryAlloc (int size, qboolean nobottom);

cache_system_t cache_head;

/*
===========
Cache_Move
===========
*/
void
Cache_Move (cache_system_t * c)
{
	cache_system_t *new;

// we are clearing up space at the bottom, so only allocate it late
	new = Cache_TryAlloc (c->size, true);
	if (new) {
//      Com_Printf ("cache_move ok\n");

		memcpy (new + 1, c + 1, c->size - sizeof (cache_system_t));
		new->user = c->user;
		memcpy (new->name, c->name, sizeof (new->name));
		Cache_Free (c->user);
		new->user->data = (void *) (new + 1);
	} else {
//      Com_Printf ("cache_move failed\n");

		Cache_Free (c->user);			// tough luck...
	}
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void
Cache_FreeLow (int new_low_hunk)
{
	cache_system_t *c;

	while (1) {
		c = cache_head.next;
		if (c == &cache_head)
			return;						// nothing in cache at all
		if ((Uint8 *) c >= hunk_base + new_low_hunk)
			return;						// there is space to grow the hunk
		Cache_Move (c);					// reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void
Cache_FreeHigh (int new_high_hunk)
{
	cache_system_t *c, *prev;

	prev = NULL;
	while (1) {
		c = cache_head.prev;
		if (c == &cache_head)
			return;						// nothing in cache at all
		if ((Uint8 *) c + c->size <= hunk_base + hunk_size - new_high_hunk)
			return;						// there is space to grow the hunk
		if (c == prev)
			Cache_Free (c->user);		// didn't move out of the way
		else {
			Cache_Move (c);				// try to move it
			prev = c;
		}
	}
}

void
Cache_UnlinkLRU (cache_system_t * cs)
{
	if (!cs->lru_next || !cs->lru_prev)
		Sys_Error ("Cache_UnlinkLRU: NULL link");

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = NULL;
}

void
Cache_MakeLRU (cache_system_t * cs)
{
	if (cs->lru_next || cs->lru_prev)
		Sys_Error ("Cache_MakeLRU: active link");

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t *
Cache_TryAlloc (int size, qboolean nobottom)
{
	cache_system_t *cs, *new;

// is the cache completely empty?

	if (!nobottom && cache_head.prev == &cache_head) {
		if (hunk_size - hunk_high_used - hunk_low_used < size)
			Sys_Error ("Cache_TryAlloc: %i is greater then free hunk", size);

		new = (cache_system_t *) (hunk_base + hunk_low_used);
		memset (new, 0, sizeof (*new));
		new->size = size;

		cache_head.prev = cache_head.next = new;
		new->prev = new->next = &cache_head;

		Cache_MakeLRU (new);
		return new;
	}
// search from the bottom up for space

	new = (cache_system_t *) (hunk_base + hunk_low_used);
	cs = cache_head.next;

	do {
		if (!nobottom || cs != cache_head.next) {
			if ((Uint8 *) cs - (Uint8 *) new >= size) {	// found space
				memset (new, 0, sizeof (*new));
				new->size = size;

				new->next = cs;
				new->prev = cs->prev;
				cs->prev->next = new;
				cs->prev = new;

				Cache_MakeLRU (new);

				return new;
			}
		}
		// continue looking 
		new = (cache_system_t *) ((Uint8 *) cs + cs->size);
		cs = cs->next;

	} while (cs != &cache_head);

// try to allocate one at the very end
	if (hunk_base + hunk_size - hunk_high_used - (Uint8 *) new >= size) {
		memset (new, 0, sizeof (*new));
		new->size = size;

		new->next = &cache_head;
		new->prev = cache_head.prev;
		cache_head.prev->next = new;
		cache_head.prev = new;

		Cache_MakeLRU (new);

		return new;
	}

	return NULL;						// couldn't allocate
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void
Cache_Flush (void)
{
	while (cache_head.next != &cache_head)
		Cache_Free (cache_head.next->user);	// reclaim the space
}


/*
============
Cache_Print

============
*/
void
Cache_Print (void)
{
	cache_system_t *cd;

	for (cd = cache_head.next; cd != &cache_head; cd = cd->next) {
		Com_Printf ("%8i : %s\n", cd->size, cd->name);
	}
}

/*
============
Cache_Report

============
*/
void
Cache_Report (void)
{
	Com_DPrintf ("%4.1f megabyte data cache\n",
				 (hunk_size - hunk_high_used -
				  hunk_low_used) / (float) (1024 * 1024));
}

/*
============
Cache_Compact

============
*/
void
Cache_Compact (void)
{
}

/*
============
Cache_Init

============
*/
void
Cache_Init (void)
{
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand ("flush", Cache_Flush);
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void
Cache_Free (cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		Sys_Error ("Cache_Free: not allocated");

	cs = ((cache_system_t *) c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU (cs);
}



/*
==============
Cache_Check
==============
*/
void       *
Cache_Check (cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		return NULL;

	cs = ((cache_system_t *) c->data) - 1;

// move to head of LRU
	Cache_UnlinkLRU (cs);
	Cache_MakeLRU (cs);

	return c->data;
}


/*
==============
Cache_Alloc
==============
*/
void       *
Cache_Alloc (cache_user_t *c, int size, char *name)
{
	cache_system_t *cs;

	if (c->data)
		Sys_Error ("Cache_Alloc: already allocated");

	if (size <= 0)
		Sys_Error ("Cache_Alloc: size %i", size);

	size = (size + sizeof (cache_system_t) + 15) & ~15;

// find memory for it
	while (1) {
		cs = Cache_TryAlloc (size, false);
		if (cs) {
			strlcpy (cs->name, name, sizeof (cs->name));
			c->data = (void *) (cs + 1);
			cs->user = c;
			break;
		}
		// free the least recently used cahedat
		if (cache_head.lru_prev == &cache_head)
			Sys_Error ("Cache_Alloc: out of memory");
		// not enough memory at all
		Cache_Free (cache_head.lru_prev->user);
	}

	return Cache_Check (c);
}

//============================================================================


/*
========================
Zone_Init
========================
*/
void Zone_Init (void)
{
	int j;
	tempzone = Zone_AllocZone("Temporary Memory");
	stringzone = Zone_AllocZone("Strings");
	hunkzone = Zone_AllocZone("Hunk Memory");

	// FIXME: kill off hunk after destroying cache
	j = COM_CheckParm ("-mem");
	if (j)
		hunk_size = (int) (Q_atof (com_argv[j + 1]) * 1024 * 1024);
	else
		hunk_size = 16 * 1024 * 1024;

	hunk_base = Zone_Alloc (hunkzone, hunk_size);

	hunk_low_used = 0;
	hunk_high_used = 0;

	// FIXME: take a chainsaw to cache
	Cache_Init ();
}

void Zone_Init_Commands (void)
{
	Cmd_AddCommand ("zonestats", ZoneStats_f);
	Cmd_AddCommand ("zonelist", ZoneList_f);
}
