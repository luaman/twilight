/*
	$RCSfile$

	Copyright (C) 2002  Zephaniah E. Hull.

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

#include "qtypes.h"
#include "zone.h"
#include "cvar.h"
#include "dyngl.h"
#include "gl_arrays.h"

static memzone_t	*vzone;

texcoord_t	*tc0_array_p;
texcoord_t	*tc1_array_p;
vertex_t	*v_array_p;
colorf_t	*cf_array_p;
colorub_t	*cub_array_p;
GLuint		*vindices;
float_int_t	*FtoUB_tmp;
/*
colorf_t	*scf_array_p;
colorub_t	*scub_array_p;
*/

GLint		v_index, i_index;
qboolean	va_locked;

GLint		MAX_VERTEX_ARRAYS, MAX_VERTEX_INDICES;

cvar_t *gl_varray_size;
cvar_t *gl_iarray_size;

void
GLArrays_Init_Cvars (void)
{
	gl_varray_size = Cvar_Get ("gl_varray_size", "2048", CVAR_ARCHIVE | CVAR_ROM, NULL);
	gl_iarray_size = Cvar_Get ("gl_iarray_size", "2048", CVAR_ARCHIVE | CVAR_ROM, NULL);
}

void
GLArrays_Init (void)
{
	vzone = Zone_AllocZone ("Vertex Arrays");

	MAX_VERTEX_ARRAYS = gl_varray_size->ivalue;
	MAX_VERTEX_INDICES = gl_iarray_size->ivalue;

	tc0_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(texcoord_t));
	tc1_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(texcoord_t));
	v_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(vertex_t));
	vindices = Zone_Alloc(vzone, MAX_VERTEX_INDICES * sizeof(GLuint));
	cf_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(colorf_t));
	cub_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(colorub_t));
	FtoUB_tmp = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(float_int_t) * 4);
	/*
	if (gl_secondary_color) {
	  scf_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(colorf_t));
	  scub_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(colorub_t));
	}
	*/

	qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0_array_p);
	qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), cub_array_p);
	qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v_array_p);

	if (gl_mtex) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1_array_p);
		qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
	/*
	if (gl_secondary_color)
		qglSecondaryColorPointerEXT (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), scub_array_p);
		*/

	qglDisableClientState (GL_COLOR_ARRAY);
	qglEnableClientState (GL_VERTEX_ARRAY);
	qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
}
