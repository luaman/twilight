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

	$Id$
*/
// wad.h

#ifndef __FS_WAD_H
#define __FS_WAD_H

#include "fs.h"

fs_group_t *FSW_New_Group (fs_file_t *in, fs_group_t *parent, const char *id);

#endif // __FS_WAD_H

