/*
	$RCSfile$

	Copyright (C) 2002  Zephaniah E. Hull

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
#include <stdio.h>

#include "locs.h"
#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "mathlib.h"
#include "zone.h"
#include "sys.h"
#include "fs/fs.h"
#include "fs/rw_ops.h"

#define LOC_BSIZE	128

static memzone_t	*loc_zone;

static location_t	**locs;
static int			num_locs, num_blocks;

void
loc_init ()
{
	loc_zone = Zone_AllocZone("Location tracker");
	
	num_locs = 0;
	num_blocks = 1;
	locs = Zone_Alloc(loc_zone, sizeof(location_t *) * LOC_BSIZE);
}

static void
loc_new_block ()
{
	location_t	**new_locs;
	int			i;

	num_blocks++;
	new_locs = Zone_Alloc(loc_zone, sizeof(void *) * num_blocks * LOC_BSIZE);
	for (i = 0; i < num_locs; i++) {
		new_locs[i] = locs[i];
	}
	Zone_Free(locs);
	locs = new_locs;
}

void
loc_new (vec3_t where, const char *name)
{
	location_t	*loc;

	if (num_locs >= (num_blocks * LOC_BSIZE))
		loc_new_block();

	loc = Zone_Alloc (loc_zone, sizeof (location_t));
	VectorCopy (where, loc->where);
	strlcpy(loc->name, name, sizeof(loc->name));

	locs[num_locs++] = loc;
}

void
loc_clear ()
{
	int	i;

	for (i = 0; i < num_locs; i++) {
		Zone_Free(locs[i]);
		locs[i] = NULL;
	}
	num_locs = 0;
}

location_t *
loc_search (vec3_t where)
{
	location_t	*best;
	float		best_range, t;
	int			i;

	best = NULL;
	best_range = 999999999;

	for (i = 0; i < num_locs; i++) {
		if ((t = VectorDistance_fast(where, locs[i]->where)) < best_range) {
			best = locs[i];
			best_range = t;
		}
	}

	return best;
}

void
loc_delete (location_t *del)
{
	int	i;
	
	for (i = 0; i < num_locs; i++) {
		if (locs[i] == del) {
			num_locs--;
			memmove(&locs[i], &locs[i + 1], sizeof(void *) * (num_locs - i));
			locs[num_locs + 1] = NULL;
			Zone_Free(del);
			return;
		}
	}
	Zone_Free(del);
}

static void
loc_parse (const char *line)
{
	int		x, y, z, i, ret;
	vec3_t	loc;

	while (isspace(*line) && *line)
		line++;

	if (line[0] == '#')
		return;

	ret = sscanf (line, "%d %d %d %n", &x, &y, &z, &i);
	if (ret != 3 && ret != 4)
		return;

	line += i;

	loc[0] = x * (1.0 / 8);
	loc[1] = y * (1.0 / 8);
	loc[2] = z * (1.0 / 8);

	loc_new (loc, line);
}

void
loc_load (const char *file)
{
	char	*orig, *data, *p;

	orig = data = (char *) COM_LoadTempFile (file, 1);
	if (!data)
		return;

	while ((p = strpbrk(data, "\n\r"))) {
		while (*p == '\n' || *p == '\r') {
			*p = '\0';
			p++;
		}
		loc_parse(data);
		data = p;
	}
	Zone_Free (orig);
}

char *
loc_locfile (const char *worldname)
{
	char	*name, *t;

	name = strdup(worldname);
	t = strrchr(name, '.');
	if (!t)
		Sys_Error("No . in world name! '%s'", name);

	*t = '\0';

	return name;
}

void
loc_newmap (const char *worldname)
{
	char	*name;

	loc_clear ();
	name = loc_locfile (worldname);
	loc_load (va ("%s.loc", name));
	free (name);
}

void
loc_write (const char *worldname)
{
	char		*name;
	fs_file_t	*file;
	SDL_RWops	*rw = NULL;
	int			i;

	name = loc_locfile (worldname);
	if ((file = FS_FindFile (name)))
		rw = file->open(file, FSF_WRITE | FSF_ASCII);

	if (!rw)
		rw = FS_Open_New (name, FSF_ASCII);

	if (!rw)
	{
		Com_Printf ("Error opening %s for writing loc file.", name);
		free (name);
		return;
	}
	free (name);

	for (i = 0; i < num_locs; i++)
		RWprintf(rw, "%d %d %d %s\n", (int) locs[i]->where[0] * 8,
				(int) locs[i]->where[1] * 8, (int) locs[i]->where[2] * 8,
				locs[i]->name);

	SDL_RWclose (rw);
}
