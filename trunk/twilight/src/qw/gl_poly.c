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
int transvertindex[MAX_TRANSVERTS];

transpoly_t *transpolylist[MAX_TRANSPOLYS];

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

void
transpolyclear (void)
{
	int			i;

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

void transpolyend(void)
{
	float center, d, maxdist;
	int i;
	transvert_t *v;
	if (currenttranspoly >= MAX_TRANSPOLYS
			|| currenttransvert >= MAX_TRANSVERTS)
		return;

	// skip invalid polygons
	if (transpoly[currenttranspoly].verts < 3)
	{
		// reset vert pointer
		currenttransvert = transpoly[currenttranspoly].firstvert;
		return;
	}
	center = 0;
	maxdist = -1000000000000000.0f; // eh, it's definitely behind it, so...

	for (i = 0,v = &transvert[transpoly[currenttranspoly].firstvert];
			i < transpoly[currenttranspoly].verts; i++, v++)
	{
		d = DotProduct(v->v, vpn);
		center += d;
		if (d > maxdist)
			maxdist = d;
	}
	maxdist -= transviewdist;
	if (maxdist < 4.0f)				// behind view
	{
		// reset vert pointer
		currenttransvert = transpoly[currenttranspoly].firstvert;
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

void
transpolyrender (void)
{
	int					i, j, k, texnum;
	int					tpolytype, transpolylistindex, transvertindices;
	transpoly_t		   *p;
	translistitem	   *item;

	if (currenttranspoly < 1)
		return;

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// set up the vertex array
	qglTexCoordPointer (2, GL_FLOAT, sizeof(transvert[0]), &transvert[0].s);
	qglColorPointer (4, GL_FLOAT, sizeof(transvert[0]), &transvert[0].r);
	qglVertexPointer (3, GL_FLOAT, sizeof(transvert[0]), &transvert[0].v);
	qglEnableClientState(GL_COLOR_ARRAY);

	transpolylistindex = 0;
	transvertindices = 0;
	for (i = 4095;i >= 0;i--)
	{
		item = translisthash[i];
		while(item)
		{
			p = item->poly;
			item = item->next;
			transpolylist[transpolylistindex++] = p;
			for (j = 0, k = p->firstvert;j < p->verts;j++, k++)
				transvertindex[transvertindices++] = k;
			if (p->glowtexnum)
			{
				// making
				if (currenttranspoly <
						MAX_TRANSPOLYS && currenttransvert + p->verts
						<= MAX_TRANSVERTS)
				{
					transpoly[currenttranspoly].texnum =
						(unsigned short) p->glowtexnum;
					transpoly[currenttranspoly].glowtexnum = 0;
					transpoly[currenttranspoly].transpolytype = TPOLYTYPE_ADD;
					transpoly[currenttranspoly].firstvert = currenttransvert;
					transpoly[currenttranspoly].verts = p->verts;
					transpolylist[transpolylistindex++] =
						&transpoly[currenttranspoly];
					currenttranspoly++;
					memcpy(&transvert[currenttransvert],
							&transvert[p->firstvert],
							sizeof(transvert_t) * p->verts);
					for (j = 0, k = p->firstvert;j < p->verts;j++, k++)
					{
						transvert[currenttransvert].r =
							transvert[currenttransvert].g =
							transvert[currenttransvert].b = 255;
						transvertindex[transvertindices++] =
							currenttransvert++;
					}
				}
				p->glowtexnum = 0;
			}
		}
	}

	if (gl_cva)
		qglLockArraysEXT (0, currenttransvert);

	tpolytype = TPOLYTYPE_ALPHA;
	texnum = -1;
	transvertindices = 0;
	for (i = 0;i < transpolylistindex;)
	{
		p = transpolylist[i];
		if (p->texnum != texnum)
		{
			texnum = p->texnum;
			qglBindTexture(GL_TEXTURE_2D, texnum);
		}
		if (p->transpolytype != tpolytype)
		{
			tpolytype = p->transpolytype;
			if (tpolytype == TPOLYTYPE_ADD)
				// additive
				qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
			else
				// alpha
				qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		k = transvertindices;
		switch (p->verts)
		{
		case 3:
			do {
				transvertindices += p->verts;
				p = transpolylist[++i];
			} while (i < transpolylistindex && p->verts == 3
					&& p->texnum == texnum && p->transpolytype == tpolytype);
			qglDrawElements(GL_TRIANGLES, transvertindices - k,
					GL_UNSIGNED_INT, &transvertindex[k]);
			break;
		case 4:
			do {
				transvertindices += p->verts;
				p = transpolylist[++i];
			} while (i < transpolylistindex && p->verts == 4
					&& p->texnum == texnum && p->transpolytype == tpolytype);
			qglDrawElements(GL_QUADS, transvertindices - k,
					GL_UNSIGNED_INT, &transvertindex[k]);
			break;
		default:
			transvertindices += p->verts;
			p = transpolylist[++i];
			qglDrawElements(GL_POLYGON, transvertindices - k,
					GL_UNSIGNED_INT, &transvertindex[k]);
			break;
		}
	}

	if (gl_cva)
		qglUnlockArraysEXT ();

	qglDisableClientState(GL_COLOR_ARRAY);

	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglColor3f(1,1,1);

	qglTexCoordPointer (2, GL_FLOAT, sizeof(tc_array[0]), tc_array[0]);
	qglColorPointer (4, GL_FLOAT, sizeof(c_array[0]), c_array[0]);
	qglVertexPointer (3, GL_FLOAT, sizeof(v_array[0]), v_array[0]);
}

