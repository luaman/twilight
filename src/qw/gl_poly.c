/*
	$RCSfile$ -- gl_poly.c

	Copyright (C) 2001 Forest "LordHavoc" Hale

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

#include "quakedef.h"
#include "mathlib.h"
#include "common.h"
#include "glquake.h"
#include "strlib.h"

transvert_t transvert[MAX_TRANSVERTS];
transpoly_t transpoly[MAX_TRANSPOLYS];
int transpolyindex[MAX_TRANSPOLYS];

int currenttranspoly;
int currenttransvert;

typedef struct translistitem_s
{
	transpoly_t *poly;
	struct translistitem_s *next;
}
translistitem;

static translistitem translist[MAX_TRANSPOLYS];
static translistitem *currenttranslist;

static translistitem *translisthash[4096];

static float transviewdist; // distance of view origin along the view normal

static float transreciptable[256];
static int transreciptableinitialized = false;

void transpolyclear(void)
{
	int i;
	if (!transreciptableinitialized)
	{
		transreciptableinitialized = true;
		transreciptable[0] = 0.0f;
		for (i = 1;i < 256;i++)
			transreciptable[i] = 1.0f / i;
	}

	currenttranspoly = currenttransvert = 0;
	currenttranslist = translist;
	memset(translisthash, 0, sizeof(translisthash));
	transviewdist = DotProduct(r_origin, vpn);
}

// turned into a #define
/*
void transpolybegin(int texnum, int glowtexnum, int transpolytype)
{
	if (currenttranspoly >= MAX_TRANSPOLYS || currenttransvert >= MAX_TRANSVERTS)
		return;
	transpoly[currenttranspoly].texnum = (unsigned short) texnum;
	transpoly[currenttranspoly].glowtexnum = (unsigned short) glowtexnum;
	transpoly[currenttranspoly].transpolytype = (unsigned short) transpolytype;
	transpoly[currenttranspoly].firstvert = currenttransvert;
	transpoly[currenttranspoly].verts = 0;
//	transpoly[currenttranspoly].ndist = 0; // clear the normal
}
*/

// turned into a #define
/*
void transpolyvert(float x, float y, float z, float s, float t, int r, int g, int b, int a)
{
	int i;
	if (currenttranspoly >= MAX_TRANSPOLYS || currenttransvert >= MAX_TRANSVERTS)
		return;
	transvert[currenttransvert].s = s;
	transvert[currenttransvert].t = t;
	transvert[currenttransvert].r = bound(0, r, 255);
	transvert[currenttransvert].g = bound(0, g, 255);
	transvert[currenttransvert].b = bound(0, b, 255);
	transvert[currenttransvert].a = bound(0, a, 255);
	transvert[currenttransvert].v[0] = x;
	transvert[currenttransvert].v[1] = y;
	transvert[currenttransvert].v[2] = z;
	currenttransvert++;
	transpoly[currenttranspoly].verts++;
}
*/

void transpolyend(void)
{
	float center, d, maxdist;
	int i;
	transvert_t *v;
	if (currenttranspoly >= MAX_TRANSPOLYS || currenttransvert >= MAX_TRANSVERTS)
		return;
	if (transpoly[currenttranspoly].verts < 3) // skip invalid polygons
	{
		currenttransvert = transpoly[currenttranspoly].firstvert; // reset vert pointer
		return;
	}
	center = 0;
	maxdist = -1000000000000000.0f; // eh, it's definitely behind it, so...
	for (i = 0,v = &transvert[transpoly[currenttranspoly].firstvert];i < transpoly[currenttranspoly].verts;i++, v++)
	{
		d = DotProduct(v->v, vpn);
		center += d;
		if (d > maxdist)
			maxdist = d;
	}
	maxdist -= transviewdist;
	if (maxdist < 4.0f) // behind view
	{
		currenttransvert = transpoly[currenttranspoly].firstvert; // reset vert pointer
		return;
	}
	center *= transreciptable[transpoly[currenttranspoly].verts];
	center -= transviewdist;
	i = bound(0, (int) center, 4095);
	currenttranslist->next = translisthash[i];
	currenttranslist->poly = transpoly + currenttranspoly;
	translisthash[i] = currenttranslist;
	currenttranslist++;
	currenttranspoly++;
}

void transpolyrender(void)
{
	int i, j, k, tpolytype, texnum;
	transpoly_t *p;
	int points = -1;
	translistitem *item;
	transvert_t *vert;
	if (currenttranspoly < 1)
		return;
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglEnable(GL_BLEND);
	qglDepthMask(0); // disable zbuffer updates
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	tpolytype = TPOLYTYPE_ALPHA;
	texnum = -1;

	// set up the vertex array
	qglInterleavedArrays(GL_T2F_C4UB_V3F, sizeof(transvert[0]), transvert);

	for (i = 4095;i >= 0;i--)
	{
		item = translisthash[i];
		while (item)
		{
			p = item->poly;
			item = item->next;
			if (p->texnum != texnum || p->verts != points || p->transpolytype != tpolytype)
			{
				qglEnd();
				if (p->texnum != texnum)
				{
					texnum = p->texnum;
					qglBindTexture(GL_TEXTURE_2D, texnum);
				}
				if (p->transpolytype != tpolytype)
				{
					tpolytype = p->transpolytype;
					if (tpolytype == TPOLYTYPE_ADD) // additive
						qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
					else // alpha
						qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
				points = p->verts;
				switch (points)
				{
				case 3:
					qglBegin(GL_TRIANGLES);
					break;
				case 4:
					qglBegin(GL_QUADS);
					break;
				default:
					qglBegin(GL_POLYGON);
					points = -1; // to force a reinit on the next poly
					break;
				}
			}

			for (j = 0, k = p->firstvert;j < p->verts;j++, k++)
				qglArrayElement(k);

			if (p->glowtexnum)
			{
				qglEnd();
				texnum = p->glowtexnum; // highly unlikely to match next poly, but...
				qglBindTexture(GL_TEXTURE_2D, texnum);
				if (tpolytype != TPOLYTYPE_ADD)
				{
					tpolytype = TPOLYTYPE_ADD; // might match next poly
					qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
				}
				points = -1;
				qglBegin(GL_POLYGON);
				for (j = 0,vert = &transvert[p->firstvert];j < p->verts;j++, vert++)
				{
					qglColor4ub(255,255,255,vert->a);
					qglTexCoord2fv(&vert->s);
					qglVertex3fv(vert->v);
				}
			}
		}
	}
	qglEnd();

	qglDisable(GL_COLOR_ARRAY);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable(GL_BLEND);
	qglColor3f(1,1,1);
	qglDepthMask(1); // enable zbuffer updates

	qglTexCoordPointer (2, GL_FLOAT, sizeof(tc_array[0]), tc_array[0]);
	qglColorPointer (4, GL_FLOAT, sizeof(c_array[0]), c_array[0]);
	qglVertexPointer (3, GL_FLOAT, sizeof(v_array[0]), v_array[0]);
}
