/*
	$RCSfile$

	Copyright (C) 2002  Mathieu Olivier

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

// jpeg.c
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include "libjpeg/jpeglib.h"
#include "SDL.h"

#include "image.h"
#include "jpeg.h"
#include "common.h"

static JOCTET jpeg_eoi_marker [2] = {0xFF, JPEG_EOI};

// Used for init_source and term_source
static void
JPEG_Noop (j_decompress_ptr cinfo)
{
}

static boolean
JPEG_FillInputBuffer (j_decompress_ptr cinfo)
{
    // Insert a fake EOI marker
    cinfo->src->next_input_byte = jpeg_eoi_marker;
    cinfo->src->bytes_in_buffer = 2;

	return TRUE;
}

static void
JPEG_SkipInputData (j_decompress_ptr cinfo, long num_bytes)
{
    if (cinfo->src->bytes_in_buffer <= (unsigned long)num_bytes)
	{
		cinfo->src->bytes_in_buffer = 0;
		return;
	}

    cinfo->src->next_input_byte += num_bytes;
    cinfo->src->bytes_in_buffer -= num_bytes;
}


/*
==============
JPEG_MemSrc
==============
*/
static void
JPEG_MemSrc (j_decompress_ptr cinfo, Uint8 *buffer)
{
	cinfo->src = (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
		sizeof (struct jpeg_source_mgr));

	cinfo->src->next_input_byte = buffer;
	cinfo->src->bytes_in_buffer = com_filesize;

	cinfo->src->init_source = JPEG_Noop;
	cinfo->src->fill_input_buffer = JPEG_FillInputBuffer;
	cinfo->src->skip_input_data = JPEG_SkipInputData;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart; // use the default method
	cinfo->src->term_source = JPEG_Noop;
}

/*
============
JPEG_LoadBuffer
============
*/
static image_t *
JPEG_LoadBuffer (Uint8 *buf)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	Uint8* scanline;
	image_t* img;
	unsigned int line;

	cinfo.err = jpeg_std_error (&jerr);
	jpeg_create_decompress (&cinfo);
	JPEG_MemSrc (&cinfo, buf);
	jpeg_read_header (&cinfo, TRUE);
	jpeg_start_decompress (&cinfo);

	// RGB images only
	if (cinfo.output_components != 3)
		return NULL;

	img = malloc (sizeof (*img));
	if (img == NULL)
		return NULL;
	img->type = IMG_RGBA;
	img->width = cinfo.image_width;
	img->height = cinfo.image_height;
	img->pixels = malloc (img->width * img->height * 4);
	if (img->pixels == NULL)
	{
		free (img);
		return NULL;
	}

	scanline = malloc (img->width * 3);
	if (scanline == NULL)
	{
		free (img->pixels);
		free (img);
		return NULL;
	}

	// Decompress the image, line by line
	line = 0;
	while (cinfo.output_scanline < cinfo.output_height)
	{
		Uint8* buffer_ptr;
		unsigned int ind;

		if (!jpeg_read_scanlines (&cinfo, &scanline, 1))
		{
			free (img->pixels);
			free (img);
			free (scanline);
			return NULL;
		}

		// Add an alpha component to each pixel in the scanline
		buffer_ptr = &img->pixels[img->width * line * 4];
		for (ind = 0; ind < img->width * 3; ind += 3, buffer_ptr += 4)
		{
			buffer_ptr[0] = scanline[ind];
			buffer_ptr[1] = scanline[ind + 1];
			buffer_ptr[2] = scanline[ind + 2];
			buffer_ptr[3] = 255;
		}

		line++;
	}
	free (scanline);

	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);
	return img;
}

/*
============
JPEG_Load
============
*/
image_t *
JPEG_Load (char *name)
{
	Uint8 *buf = COM_LoadTempFile (name, false);

	if (buf)
		return JPEG_LoadBuffer (buf);

	return NULL;
}
