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

#include "twiconfig.h"

#include "common.h"
#include "image.h"
#include "strlib.h"
#include "pcx.h"
#include "qlmp.h"
#include "tga.h"
#include "sys.h"
#include "sdlimage.h"

image_t *
Image_Load (char *name)
{
	char woext[MAX_OSPATH];
	char buf[MAX_OSPATH];
	char *p;
	image_t *img;

	if (!name)
		Sys_Error ("IMG_Load: Attempt to load NULL image");

	// FIXME: HACK HACK HACK.
	Image_InitSDL ();

	strlcpy(woext, name, MAX_OSPATH);
	COM_StripExtension (woext, woext);

	// Concession for win32, # is used because DarkPlaces uses it
	for (p = woext; *p; p++)
		if (*p == '*')
			*p = '#';

	if ((img = Image_FromSDL(woext)))
		return img;

	snprintf (buf, MAX_OSPATH, "%s.tga", woext);
	if ((img = TGA_Load (buf)))
		return img;

	snprintf (buf, MAX_OSPATH, "%s.pcx", woext);
	if ((img = PCX_Load (buf)))
		return img;

	snprintf (buf, MAX_OSPATH, "%s.lmp", woext);
	if ((img = QLMP_Load (buf)))
		return img;

	return NULL;
}
