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
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
	
#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "fs.h"
#include "fs_hash.h"
#include "dir.h"

typedef struct fsd_group_s {
	char *path;
} fsd_group_t;

typedef struct fsd_file_s {
	char *path;
} fsd_file_t;

static void
FSD_Free (fs_group_t *group)
{
	Zone_Free (group->fs_data);
}

static void
FSD_Free_File (fs_file_t *file)
{
	fsd_file_t	*f = file->fs_data;

	Zone_Free (f->path);
	Zone_Free (f);
	Zone_Free (file->name_base);
}

static SDL_RWops *
FSD_Open_File (fs_file_t *file, Uint32 flags)
{
	fsd_file_t	*f = file->fs_data;
	SDL_RWops	*rw;

	if (flags & FSF_WRITE) {
		if (file->group->flags & FS_READ_ONLY) {
			Com_Printf ("Refusing to open '%s' in write mode.\n", f->path);
			rw = NULL;
		} else
			rw = SDL_RWFromFile (f->path, (flags & FSF_ASCII) ? "w" : "wb");
	} else
		rw = SDL_RWFromFile (f->path, (flags & FSF_ASCII) ? "r" : "rb");

	return rw;
}

static void
FSD_Add_Dir (fs_group_t *group, fsd_group_t *g_dir, char *path, int depth)
{
	DIR				*dir;
	struct dirent	*dirent;
	char			*file, *full_path, *tmp;
	struct stat		f_stat;
	fsd_file_t		*f;

	if (depth > 32)
		return;

	full_path = zasprintf(fs_zone, "%s/%s/", g_dir->path, path);
	dir = opendir (full_path);

	while ((dirent = readdir(dir))) {
		if (!strcmp(".", dirent->d_name) || !strcmp("..", dirent->d_name))
			continue;
		tmp = strrchr(dirent->d_name, '/');
		if (!tmp)
			tmp = dirent->d_name;

		f = Zone_Alloc (fs_zone, sizeof (fsd_file_t));
		f->path = zasprintf (fs_zone, "%s/%s", full_path, tmp);

		if (stat(f->path, &f_stat)) {
			Com_Printf("Can't stat %s: %s\n", va("%s/%s", full_path, tmp),
					strerror(errno));
			Zone_Free (f->path);
			Zone_Free (f);
			continue;
		}
		file = zasprintf(fs_zone, "%s/%s", path, tmp);
		if (S_ISDIR(f_stat.st_mode))
			FSD_Add_Dir (group, g_dir, file, depth + 1);
		else
			FS_Add_File (group, file, f_stat.st_size, FSD_Open_File, f);
		Zone_Free (file);
	}
	Zone_Free (full_path);
}

qboolean
FSD_Open_New (fs_group_t *group, fs_new_t *new)
{
	fsd_group_t	*dir = group->fs_data;
	int			fd = 0;
	FILE		*file;

	new->temp = zasprintf (fs_zone, "%s.tmp", new->wanted);
	fd = creat (va("%s/%s", dir->path, new->temp), 0666);
	if (!fd || fd == -1) {
		Com_Printf ("FSD_Open_New: Unable to open. %s\n", strerror(errno));
		Zone_Free (new->temp);
		new->temp = NULL;
		return false;
	}
	if (!(file = fdopen(fd, (new->flags & FSF_ASCII) ? "w" : "wb"))) {
		Com_Printf ("FSD_Open_New: Unable to fdopen. %s\n", strerror(errno));
		Zone_Free (new->temp);
		new->temp = NULL;
		close (fd);
		return false;
	}
	new->rw = SDL_RWFromFP (file, 1);
	return true;
}

void
FSD_Close_New (fs_group_t *group, fs_new_t *new)
{
	fsd_group_t	*dir = group->fs_data;
	int			len;

	SDL_RWseek (new->rw, 0, SEEK_END);
	len = SDL_RWtell (new->rw);
	SDL_RWclose (new->rw);
	rename (va("%s/%s", dir->path, new->temp),
			va("%s/%s", dir->path, new->wanted));
	Zone_Free (new->temp);
	new->temp = NULL;
	FS_Add_File (group, new->wanted, len, FSD_Open_File, NULL);
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

	FSD_Add_Dir (group, dir, "", 0);

	return group;
}
