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

#ifndef __FS_HASH_H
#define __FS_HASH_H

#include "qtypes.h"
#include "hash.h"
#include "fs.h"

int FSH_hash (hash_t *hash, fs_search_t *what);
int FSH_compare (hash_t *hash, fs_file_t *file, fs_search_t *search);
void FSH_free (hash_t *hash, fs_file_t *file);

#endif // __FS_HASH_H

