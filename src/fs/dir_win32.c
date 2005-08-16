/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull.

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <io.h>
	
#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "fs.h"
#include "fs_hash.h"
#include "dir.h"

typedef struct fsd_group_s {
	char *path;
} fsd_group_t;

static void
FSD_Free (fs_group_t *group)
{
	Zone_Free (group->fs_data);
}

static void
FSD_Free_File (fs_file_t *file)
{
	Zone_Free (file->fs_data.path);
	Zone_Free (file->name_base);
}

static SDL_RWops *
FSD_Open_File (fs_file_t *file, Uint32 flags)
{
	SDL_RWops	*rw;

	if (flags & FSF_WRITE) {
		if (file->group->flags & FS_READ_ONLY) {
			Com_Printf ("Refusing to open '%s' in write mode.\n", file->fs_data.path);
			rw = NULL;
		} else
			rw = SDL_RWFromFile (file->fs_data.path, (flags & FSF_ASCII) ? "w" : "wb");
	} else
		rw = SDL_RWFromFile (file->fs_data.path, (flags & FSF_ASCII) ? "r" : "rb");

	return rw;
}

static void
FSD_Add_Dir (fs_group_t *group, fsd_group_t *g_dir, char *path, int depth)
{
	long int			dir;
	struct _finddata_t	n_file;
	char				*file, *tmp;
	fs_file_data_t	file_data;

	if (depth > 32)
		return;

	if (path)
		tmp = zasprintf(fs_zone, "%s/%s/*", g_dir->path, path);
	else
		tmp = zasprintf(fs_zone, "%s/*", g_dir->path);
	dir = _findfirst (tmp, &n_file);

	Com_DFPrintf (DEBUG_FS, "Win32 Add Dir: %s %p\n", tmp, dir);

	Zone_Free (tmp);
	if (dir != -1)
		goto fire;

	return;

	while (!_findnext (dir, &n_file)) {
fire:
		if (!strcmp(".", n_file.name) || !strcmp("..", n_file.name))
			continue;
		if (path) {
			file = zasprintf(fs_zone, "%s/%s", path, n_file.name);
			file_data.path = zasprintf (fs_zone, "%s/%s/%s", g_dir->path, path, n_file.name);
		} else {
			file = zasprintf(fs_zone, "%s", n_file.name);
			file_data.path = zasprintf (fs_zone, "%s/%s", g_dir->path, n_file.name);
		}

		if (n_file.attrib & _A_SUBDIR)
			FSD_Add_Dir (group, g_dir, file, depth + 1);
		else
			FS_Add_File (group, file, n_file.size, FSD_Open_File, file_data);
		Zone_Free (file);
	}
	_findclose (dir);
}

qboolean
FSD_Open_New (fs_group_t *group, fs_new_t *new)
{
	fsd_group_t	*dir = group->fs_data;
	char		*name;

	new->temp = zasprintf (fs_zone, "%s.tmp", new->wanted);
	name = zasprintf (tempzone, "%s/%s", dir->path, new->temp);
	new->rw = SDL_RWFromFile (name, (new->flags & FSF_ASCII) ? "w" : "wb");
	if (!new->rw) {
		Com_Printf ("FSD_Open_New: Unable to open %s. (%s) (%s)\n", name, strerror(errno), SDL_GetError());
		Zone_Free (new->temp);
		Zone_Free (name);
		return false;
	} else {
		Zone_Free (name);
		return true;
	}
}

void
FSD_Close_New (fs_group_t *group, fs_new_t *new)
{
	fsd_group_t	*dir = group->fs_data;
	int			len;
	fs_file_data_t	file_data;

	SDL_RWseek (new->rw, 0, SEEK_END);
	len = SDL_RWtell (new->rw);
	SDL_RWclose (new->rw);
	rename (va("%s/%s", dir->path, new->temp),
			va("%s/%s", dir->path, new->wanted));
	Zone_Free (new->temp);
	new->temp = NULL;
	file_data.path = zasprintf (fs_zone, "%s/%s", dir->path, new->wanted);
	FS_Add_File (group, new->wanted, len, FSD_Open_File, file_data);
}

fs_group_t *
FSD_New_Group (const char *path, fs_group_t *parent, const char *id,
		Uint32 flags)
{
	fs_group_t	*group;
	fsd_group_t	*dir;

	group = FS_Alloc_Group (parent, id);
	dir = Zone_Alloc (fs_zone, sizeof (fsd_group_t));
	group->fs_data = dir;
	group->free = FSD_Free;
	group->free_file = FSD_Free_File;
	group->flags |= flags;
	if (parent)
		group->flags |= parent->flags;
	if (!(group->flags & FS_READ_ONLY)) {
		group->open_new = FSD_Open_New;
		group->close_new = FSD_Close_New;
	}

	dir->path = Zstrdup (fs_zone, path);

	FSD_Add_Dir (group, dir, NULL, 0);

	return group;
}
