/*
	$RCSfile$

	Copyright (C) 2001  Joseph Carter

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "common.h"
#include "strlib.h"
#include "pcx.h"
#include "tga.h"

Uint8 **
IMG_Load (char *name, Uint8 **buf, int *w, int *h)
{
	char wext[256];

	strcpy(wext, name);
	COM_StripExtension (wext, wext);
	COM_DefaultExtension (wext, ".tga");

	if (TGA_Load (wext, buf, w, h))
		return buf;

	strcpy(wext, name);
	COM_StripExtension (wext, wext);
	COM_DefaultExtension (wext, ".pcx");

	if (PCX_Load (wext, buf, w, h))
		return buf;

	return NULL;
}

