/*
	$RCSfile$

	Copyright (C) 2002  Joseph Carter

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
#  include <w32conf.h>
# endif
#endif

#include <stdlib.h>
#include "SDL.h"

#include "common.h"
#include "image.h"
#include "qlmp.h"
#include "strlib.h"
#include "vid.h"

static image_t *
QLMP_LoadQPic (Uint8 *p)
{
	Uint8	   *buf = p;
	Uint32		numpixels = 0;
	Uint32		i = 0;
	Uint32	   *qlmp_rgba;
	image_t	   *img;

	img = malloc (sizeof(image_t));
	
	img->width = LittleLong (*(Uint32 *)buf);
	buf += 4;
	img->height = LittleLong (*(Uint32 *)buf);
	buf += 4;

	if ((unsigned)img->width > 4096 || (unsigned)img->height > 4096)
	{
		Com_Printf ("QLMP_Load: invalid size (%ix%i)\n",
				img->width, img->height);
		free (img);
		return NULL;
	}

	numpixels = img->width * img->height;
	qlmp_rgba = malloc (numpixels * sizeof (Uint32));
	img->pixels = (Uint8 *)qlmp_rgba;
	while (i < numpixels)
		qlmp_rgba[i] = d_8to32table[*buf++];

	return img;
}


#define CONCHARS_W 128
#define CONCHARS_H 128
#define CONCHARS_SIZE (CONCHARS_W * CONCHARS_H)

static image_t *
QLMP_LoadFont (Uint8 *p)
{
	Uint8	   *buf = p;
	Uint32		i = 0;
	Uint32	   *qlmp_rgba;
	image_t	   *img;

	img = malloc (sizeof (image_t));
	qlmp_rgba = malloc (CONCHARS_SIZE * sizeof (Uint32));

	img->width = CONCHARS_W;
	img->height = CONCHARS_H;
	img->pixels = (Uint8 *)qlmp_rgba;

	while (i < CONCHARS_SIZE)
		if (*buf == 0)
		{
			// color 0 should be transparent in font
			qlmp_rgba[i] = d_8to32table[255];
			buf++;
		} else
			qlmp_rgba[i] = d_8to32table[*buf++];

		return img;
}

image_t *
QLMP_Load (char *name)
{
	Uint8	   *buf = COM_LoadTempFile (name, false);

	if (buf)
	{
		if (strncasecmp ("conchars.lmp", name, 12))
			return QLMP_LoadFont (buf);
		else
			return QLMP_LoadQPic (buf);
	}

	return NULL;
}

