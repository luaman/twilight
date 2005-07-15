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

#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "fs.h"
#include "fs_hash.h"
#include "crc.h"
#include "dir.h"
#include "pak.h"
#include "bsp.h"
#include "wad.h"
#include "cmd.h"
#include "embedded.h"

memzone_t	*fs_zone;
fs_group_t	*fs_paths;

static void FS_AddGroup (fs_group_t *group);

static void
FS_Path_f (void)
{
	fs_group_t	*cur;

	for (cur = fs_paths; cur; cur = cur->next) {
		Com_Printf(" %s (%d files)\n", cur->id, cur->files->n_values);
	}
}

static void
FS_DumpPath_f (void)
{
	fs_group_t		*cur;
	fs_file_t		*file;
	hash_value_t	*val;
	int				i;

	for (cur = fs_paths; cur; cur = cur->next) {
		Com_Printf("%s (%d files)\n", cur->id, cur->files->n_values);
		for (i = 0; i < cur->files->length; i++) {
			for (val = cur->files->values[i]; val; val = val->next) {
				file = val->data;
				if (file->ext)
					Com_Printf("  %s.%s (%d)\n", file->name_base, file->ext,
							file->len);
				else
					Com_Printf("  %s (%d)\n", file->name_base, file->len);
			}
		}
	}
}

void
FS_Init (void)
{
	fs_zone = Zone_AllocZone("File system");
	fs_paths = NULL;

	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("dumppath", FS_DumpPath_f);
	FS_AddGroup(FSE_New_Group ("*Embedded*"));
}

char *
FS_MangleName (const char *name)
{
	char	*new;
	int		len, i, j;

	len = strlen(name);
	new = Zone_Alloc (fs_zone, len + 1);
	i = j = 0;
	while (name[i] == '/' || name[i] == '\\' || name[i] == ':') i++;
	while (i < len) {
		if (name[i] == '/' || name[i] == '\\' || name[i] == ':') {
			while (name[i] == '/' || name[i] == '\\' || name[i] == ':') i++;
			new[j++] = '/';
		} else
			new[j++] = tolower(name[i++]);
	}
	new[j] = '\0';
	return new;
}

fs_file_t *
FS_FindFiles_Complex (const char **names, char **exts)
{
	int			i;
	fs_file_t	*best = NULL, *cur = NULL;

	for (i = 0; names[i]; i++)
		if ((cur = FS_FindFile_Complex (names[i], exts)))
			if (!best || cur->group->path_num > best->group->path_num)
				best = cur;

	return best;
}

fs_file_t *
FS_FindFile_Complex (const char *name, char **exts)
{
	fs_file_t	*file = NULL;
	fs_search_t	search;
	fs_group_t	*cur;

	search.name_base = FS_MangleName(name);
	search.name_len = strlen(search.name_base);
	search.exts = exts;

	for (cur = fs_paths; cur; cur = cur->next) {
		if ((file = hash_get (cur->files, &search)))
			break;
	}
	Zone_Free (search.name_base);

	return file;
}

fs_file_t *
FS_FindFile (const char *name)
{
	char **exts;
	char *new_name;
	fs_file_t *ret;

	exts = Zone_Alloc (tempzone, sizeof (char *) * 2);
	new_name = Zstrdup(tempzone, name);
	if ((exts[0] = strrchr(new_name, '.'))) {
		exts[0][0] = '\0';
		exts[0]++;
	}

	ret = FS_FindFile_Complex (new_name, exts);
	Zone_Free (new_name);
	Zone_Free (exts);

	if (!ret)
		Com_DPrintf("Unable to find %s\n", name);

	return ret;
}

static void
FS_AddGroup (fs_group_t *group)
{
	static int	num = 0;

	Com_DFPrintf (DEBUG_FS, "AddGroup: %p\n", group);
	if (!group)
		return;

	Com_DFPrintf (DEBUG_FS, "AddGroup: %s %s 0x%x\n", group->id, group->prefix,
			group->flags);

	group->path_num = num++;
	group->next = fs_paths;
	fs_paths = group;
}

void
FS_Add_File (fs_group_t *group, const char *name, size_t len,
		fs_open_t *open, fs_file_data_t fs_data)
{
	fs_file_t	*file;

	file = Zone_Alloc(fs_zone, sizeof(fs_file_t));
	file->fs_data = fs_data;
	Com_DFPrintf (DEBUG_FS, "New file: (%s) %s\n", group->prefix, name);
	if (group->prefix)
		file->name_base = FS_MangleName (va("%s/%s", group->prefix, name));
	else
		file->name_base = FS_MangleName (name);
	file->ext = strrchr(file->name_base, '.');
	if (file->ext) {
		file->ext[0] = '\0';
		file->ext++;
	}
	file->name_len = strlen(file->name_base);
	file->open = open;
	file->group = group;
	file->len = len;

	hash_add (group->files, file);

	if (file->ext) {
		if (!strcasecmp("pak", file->ext))
			FS_AddGroup (FSP_New_Group (file, group, name));
		if (!strcasecmp("wad", file->ext))
			FS_AddGroup (FSW_New_Group (file, group, name));
		if (!strcasecmp("bsp", file->ext))
			FS_AddGroup (FSB_New_Group (file, group, name));
	}
}

void
FS_AddPath (const char *path, const char *id, const Uint32 flags)
{
	Com_DFPrintf (DEBUG_FS, "New Path: %s %s %x\n", path, id, flags);
	if (!id) {
		if ((id = strrchr(path, '/')))
			id++;
		else
			id = path;
	}
		
	FS_AddGroup (FSD_New_Group (path, NULL, id, flags));
}

fs_group_t *
FS_Alloc_Group (fs_group_t *parent, const char *id)
{
	fs_group_t	*group;
	char		*t0, *t1;

	group = Zone_Alloc (fs_zone, sizeof (fs_group_t));
	group->files = hash_create (10, (do_compare_t *) FSH_compare, (do_index_t *) FSH_hash, (do_free_t *) FSH_free, fs_zone);

	if (parent) {
		group->id = FS_MangleName(va("%s/%s", parent->id, id));
		group->up = parent;
		group->sub_next = parent->subs;
		parent->subs = group;
	} else
		group->id = FS_MangleName(id);

	if (!parent)
		group->prefix = Zstrdup (fs_zone, "/");
	else {
		t0 = zasprintf(fs_zone, "%s/%s", parent->prefix, id);
		if ((t1 = strrchr (t0, '/')) && t1 != t0)
			*t1 = '\0';
		group->prefix = t0;
	}

	return group;
}

void
FS_Free_Group (fs_group_t *group)
{
	fs_group_t *sub, *next;

	if (group->prev)
		group->prev->next = group->next;
	else if (group == fs_paths)
		fs_paths = group->next;

	sub = group->subs;
	while (sub) {
		next = sub->sub_next;
		FS_Free_Group (sub);
		sub = next;
	}

	hash_destroy (group->files);

	if (group->free)
		group->free(group);

	Zone_Free (group->id);
	Zone_Free (group->prefix);
	Zone_Free (group);
}

void
FS_Delete_ID (char *id)
{
	fs_group_t	*cur;

top:
	for (cur = fs_paths; cur; cur = cur->next) {
		if (!strcasecmp(id, cur->id)) {
			FS_Free_Group (cur);
			goto top;
		}
	}
}
