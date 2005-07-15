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

#ifndef __FS_H
#define __FS_H

#include "twiconfig.h"
#include "qtypes.h"
#include "hash.h"
#include "zone.h"
#include "mathlib.h"

struct fs_file_s;
struct fs_new_s;
typedef SDL_RWops *(fs_open_t) (struct fs_file_s *, Uint32 flags);

#define FSF_WRITE										BIT(0)
#define FSF_ASCII										BIT(1)

#define FS_READ_ONLY									BIT(0)
#define FS_NO_UPLOAD									BIT(1)

typedef struct fs_group_s {
	char				*id;
	char				*prefix;
	hash_t				*files;
	Uint				path_num;
	Uint32				flags;

	qboolean			(*open_new) (struct fs_group_s *, struct fs_new_s *);
	void				(*close_new) (struct fs_group_s *, struct fs_new_s *);
	void				(*free) (struct fs_group_s *);
	void				(*free_file) (struct fs_file_s *);
	void				*fs_data;
	struct fs_group_s	*prev, *next;
	struct fs_group_s	*up, *subs, *sub_next;
} fs_group_t;

typedef union {
	void	*data;
	int		ofs;
	char	*path;
} fs_file_data_t;

typedef struct fs_file_s {
	size_t				name_len;
	char				*name_base, *ext;
	size_t				len;
	fs_open_t			*open;
	fs_file_data_t		fs_data;
	fs_group_t			*group;
} fs_file_t;

typedef struct fs_search_s {
	size_t	name_len;
	char	*name_base, **exts;
} fs_search_t;

#include "fs_new.h"

extern memzone_t	*fs_zone;
extern fs_group_t	*fs_paths;

void FS_Init (void);

void FS_Add_File (fs_group_t *group, const char *name, size_t len, fs_open_t *open, fs_file_data_t fs_data);
fs_file_t *FS_FindFiles_Complex (const char **names, char **exts);
fs_file_t *FS_FindFile_Complex (const char *name, char **exts);
fs_file_t *FS_FindFile (const char *name);
void FS_AddPath (const char *path, const char *id, const Uint32 flags);

fs_group_t * FS_Alloc_Group (fs_group_t *parent, const char *id);
void FS_Free_Group (fs_group_t *group);
void FS_Delete_ID (char *id);

#endif // __FS_H

