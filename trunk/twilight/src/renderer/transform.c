/*
	$RCSfile$

	Copyright (C) 2000  Forest Hale

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

#include "mathlib.h"

vec3_t softwaretransform_x;
vec3_t softwaretransform_y;
vec3_t softwaretransform_z;
vec_t softwaretransform_scale;
vec3_t softwaretransform_offset;

static const vec3_t softwaretransform_offset_id = { 0.0f, 0.0f, 0.0f };
static const vec3_t softwaretransform_x_id = { 1.0f, 0.0f, 0.0f };
static const vec3_t softwaretransform_y_id = { 0.0f, 1.0f, 0.0f };
static const vec3_t softwaretransform_z_id = { 0.0f, 0.0f, 1.0f };
static const vec_t softwaretransform_scale_id = 1.0f;

// set to different transform code depending on complexity of transform
void (*softwaretransform) (vec3_t in, vec3_t out);

// the real deal
static void
softwaretransform_dorotatescaletranslate (vec3_t in, vec3_t out)
{
	out[0] = (in[0] * softwaretransform_x[0]
			+ in[1] * softwaretransform_y[0]
			+ in[2] * softwaretransform_z[0])
		* softwaretransform_scale + softwaretransform_offset[0];
	out[1] = (in[0] * softwaretransform_x[1]
			+ in[1] * softwaretransform_y[1]
			+ in[2] * softwaretransform_z[1])
		* softwaretransform_scale + softwaretransform_offset[1];
	out[2] = (in[0] * softwaretransform_x[2]
			+ in[1] * softwaretransform_y[2]
			+ in[2] * softwaretransform_z[2])
		* softwaretransform_scale + softwaretransform_offset[2];
}

static void
softwaretransform_doscaletranslate (vec3_t in, vec3_t out)
{
	out[0] = in[0] * softwaretransform_scale + softwaretransform_offset[0];
	out[1] = in[1] * softwaretransform_scale + softwaretransform_offset[1];
	out[2] = in[2] * softwaretransform_scale + softwaretransform_offset[2];
}

static void
softwaretransform_dorotatetranslate (vec3_t in, vec3_t out)
{
	out[0] = (in[0] * softwaretransform_x[0]
			+ in[1] * softwaretransform_y[0]
			+ in[2] * softwaretransform_z[0])
		+ softwaretransform_offset[0];
	out[1] = (in[0] * softwaretransform_x[1]
			+ in[1] * softwaretransform_y[1]
			+ in[2] * softwaretransform_z[1])
		+ softwaretransform_offset[1];
	out[2] = (in[0] * softwaretransform_x[2]
			+ in[1] * softwaretransform_y[2]
			+ in[2] * softwaretransform_z[2])
		+ softwaretransform_offset[2];
}

static void
softwaretransform_dotranslate (vec3_t in, vec3_t out)
{
	out[0] = in[0] + softwaretransform_offset[0];
	out[1] = in[1] + softwaretransform_offset[1];
	out[2] = in[2] + softwaretransform_offset[2];
}

static void
softwaretransform_dorotatescale (vec3_t in, vec3_t out)
{
	out[0] = (in[0] * softwaretransform_x[0]
			+ in[1] * softwaretransform_y[0]
			+ in[2] * softwaretransform_z[0])
		* softwaretransform_scale;
	out[1] = (in[0] * softwaretransform_x[1]
			+ in[1] * softwaretransform_y[1]
			+ in[2] * softwaretransform_z[1])
		* softwaretransform_scale;
	out[2] = (in[0] * softwaretransform_x[2]
			+ in[1] * softwaretransform_y[2]
			+ in[2] * softwaretransform_z[2])
		* softwaretransform_scale;
}

static void
softwaretransform_doscale (vec3_t in, vec3_t out)
{
	out[0] = in[0] * softwaretransform_scale + softwaretransform_offset[0];
	out[1] = in[1] * softwaretransform_scale + softwaretransform_offset[1];
	out[2] = in[2] * softwaretransform_scale + softwaretransform_offset[2];
}

static void
softwaretransform_dorotate (vec3_t in, vec3_t out)
{
	out[0] = (in[0] * softwaretransform_x[0]
			+ in[1] * softwaretransform_y[0]
			+ in[2] * softwaretransform_z[0]);
	out[1] = (in[0] * softwaretransform_x[1]
			+ in[1] * softwaretransform_y[1]
			+ in[2] * softwaretransform_z[1]);
	out[2] = (in[0] * softwaretransform_x[2]
			+ in[1] * softwaretransform_y[2]
			+ in[2] * softwaretransform_z[2]);
}

static void
softwaretransform_docopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void
softwareuntransform (vec3_t in, vec3_t out)
{
	vec3_t		v;
	float		s = 1.0f / softwaretransform_scale;

	v[0] = in[0] - softwaretransform_offset[0];
	v[1] = in[1] - softwaretransform_offset[1];
	v[2] = in[2] - softwaretransform_offset[2];
	out[0] = (v[0] * softwaretransform_x[0]
			+ v[1] * softwaretransform_x[1]
			+ v[2] * softwaretransform_x[2]) * s;
	out[1] = (v[0] * softwaretransform_y[0]
			+ v[1] * softwaretransform_y[1]
			+ v[2] * softwaretransform_y[2]) * s;
	out[2] = (v[0] * softwaretransform_z[0]
			+ v[1] * softwaretransform_z[1]
			+ v[2] * softwaretransform_z[2]) * s;
}


// to save time on transforms, choose the appropriate function
void
softwaretransform_classify (void)
{
	if (!VectorCompare(softwaretransform_offset, 
		softwaretransform_offset_id))
	{
		if (softwaretransform_scale != softwaretransform_scale_id)
		{
			if (!VectorCompare(softwaretransform_x,
					softwaretransform_x_id) ||
				!VectorCompare(softwaretransform_y,
					softwaretransform_y_id) ||
				!VectorCompare(softwaretransform_z,
					softwaretransform_z_id))
				softwaretransform = &softwaretransform_dorotatescaletranslate;
			else
				softwaretransform = &softwaretransform_doscaletranslate;
		} else {
			if (!VectorCompare(softwaretransform_x,
					softwaretransform_x_id) ||
				!VectorCompare(softwaretransform_y,
					softwaretransform_y_id) ||
				!VectorCompare(softwaretransform_z,
					softwaretransform_z_id))
				softwaretransform = &softwaretransform_dorotatetranslate;
			else
				softwaretransform = &softwaretransform_dotranslate;
		}
	} else {
		if (softwaretransform_scale != softwaretransform_scale_id)
		{
			if (!VectorCompare(softwaretransform_x,
					softwaretransform_x_id) ||
				!VectorCompare(softwaretransform_y,
					softwaretransform_y_id) ||
				!VectorCompare(softwaretransform_z,
					softwaretransform_z_id))
				softwaretransform = &softwaretransform_dorotatescale;
			else
				softwaretransform = &softwaretransform_doscale;
		} else {
			if (!VectorCompare(softwaretransform_x,
					softwaretransform_x_id) ||
				!VectorCompare(softwaretransform_y,
					softwaretransform_y_id) ||
				!VectorCompare(softwaretransform_z,
					softwaretransform_z_id))
				softwaretransform = &softwaretransform_dorotate;
			else
				softwaretransform = &softwaretransform_docopy;
		}
	}
}

void
softwaretransformidentity (void)
{
	VectorCopy (softwaretransform_offset_id, softwaretransform_offset);
	VectorCopy (softwaretransform_x_id, softwaretransform_x);
	VectorCopy (softwaretransform_y_id, softwaretransform_y);
	VectorCopy (softwaretransform_z_id, softwaretransform_z);

	softwaretransform_scale = softwaretransform_scale_id;

	// we know what it is
	softwaretransform = &softwaretransform_docopy;
}

static void 
softwaretransformset (vec3_t origin, vec3_t angles, vec_t scale)
{
	VectorCopy(origin, softwaretransform_offset);

	AngleVectors(angles, softwaretransform_x, softwaretransform_y,
			softwaretransform_z);

	VectorInverse (softwaretransform_y, softwaretransform_y);

	softwaretransform_scale = scale;

	// choose best transform code
	softwaretransform_classify();
}

void 
softwaretransformforentity (vec3_t origin, vec3_t angles)
{
	vec3_t	eangles;

	eangles[0] = -angles[0];
	eangles[1] = angles[1];
	eangles[2] = angles[2];
	softwaretransformset(origin, eangles, softwaretransform_scale_id);
}

// brush entities are not backwards like models and sprites are
void
softwaretransformforbrushentity (vec3_t origin, vec3_t angles)
{
	softwaretransformset(origin, angles, softwaretransform_scale_id);
}

