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

static void
FSD_Free (fs_group_t *group)
{
	Zone_Free (group->fs_data);
}

static void
FSD_Free_File (fs_file_t *file)
{
	Zone_Free (file->name_base);
}

static SDL_RWops *
FSD_Open_File (fs_file_t *file, qboolean write)
{
	fsd_group_t	*dir;
	char		*name;
	SDL_RWops	*rw;

	dir = file->group->fs_data;
	if (file->ext)
		name = zasprintf (tempzone, "%s/%s.%s", dir->path, file->name_base, file->ext);
	else
		name = zasprintf (tempzone, "%s/%s", dir->path, file->name_base);

	if (write)
		rw = SDL_RWFromFile (name, "w");
	else
		rw = SDL_RWFromFile (name, "r");

	Zone_Free (name);
	return rw;
}

static void
FSD_Add_Dir (fs_group_t *group, fsd_group_t *g_dir, char *path, int depth)
{
	DIR				*dir;
	struct dirent	*dirent;
	char			*file, *full_path, *tmp;
	struct stat		f_stat;

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

		if (stat(va("%s/%s", full_path, tmp), &f_stat)) {
			Com_Printf("Can't stat %s: %s\n", va("%s/%s", full_path, tmp),
					strerror(errno));
			continue;
		}
		file = zasprintf(fs_zone, "%s/%s", path, tmp);
		if (S_ISDIR(f_stat.st_mode))
			FSD_Add_Dir (group, g_dir, file, depth + 1);
		else
			FS_Add_File (group, file, f_stat.st_size, FSD_Open_File, NULL);
		Zone_Free (file);
	}
	Zone_Free (full_path);
}

qboolean
FSD_Open_New (fs_group_t *group, fs_new_t *new)
{
	fsd_group_t	*dir = group->fs_data;
	int			fd;

	new->temp = zasprintf (fs_zone, "%s.tmp");
	fd = open (va("%s/%s", dir->path, new->temp), O_CREAT | O_EXCL | O_WRONLY | 0666);
	if (!fd) {
		Zone_Free (new->temp);
		new->temp = NULL;
		return false;
	}
	new->rw = SDL_RWFromFP (fdopen(fd, "w"), 1);
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
FSD_New_Group (const char *path, fs_group_t *parent, const char *id)
{
	fs_group_t	*group;
	fsd_group_t	*dir;

	group = FS_Alloc_Group (parent, id);
	dir = Zone_Alloc (fs_zone, sizeof (fsd_group_t));
	group->fs_data = dir;
	group->free = FSD_Free;
	group->free_file = FSD_Free_File;
	group->open_new = FSD_Open_New;
	group->close_new = FSD_Close_New;

	dir->path = Zstrdup (fs_zone, path);

	FSD_Add_Dir (group, dir, "", 0);

	return group;
}
