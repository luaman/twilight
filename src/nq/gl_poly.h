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

	$Id$
*/

#define TPOLYTYPE_ALPHA 0
#define TPOLYTYPE_ADD 1

extern void transpolyclear(void);
extern void transpolyrender(void);
extern void transpolybegin(int texnum, int glowtexnum, int fogtexnum, int transpolytype);
extern void transpolyend(void);

#define MAX_TRANSPOLYS 65536
#define MAX_TRANSVERTS (MAX_TRANSPOLYS*4)

typedef struct
{
	float s, t;
	Uint8 r, g, b, a;
	float v[3];
}
transvert_t;

typedef struct
{
	int texnum, glowtexnum, firstvert, verts, transpolytype;
}
transpoly_t;

extern transvert_t transvert[MAX_TRANSVERTS];
extern transpoly_t transpoly[MAX_TRANSPOLYS];
extern int transpolyindex[MAX_TRANSPOLYS];

extern int currenttranspoly;
extern int currenttransvert;

#define transpolybegin(ttexnum, tglowtexnum, ttranspolytype)\
{\
	if (currenttranspoly < MAX_TRANSPOLYS && currenttransvert < MAX_TRANSVERTS)\
	{\
		transpoly[currenttranspoly].texnum = (unsigned short) (ttexnum);\
		transpoly[currenttranspoly].glowtexnum = (unsigned short) (tglowtexnum);\
		transpoly[currenttranspoly].transpolytype = (unsigned short) (ttranspolytype);\
		transpoly[currenttranspoly].firstvert = currenttransvert;\
		transpoly[currenttranspoly].verts = 0;\
	}\
}

#define transpolyvert(vx,vy,vz,vs,vt,vr,vg,vb,va) \
{\
	if (currenttranspoly < MAX_TRANSPOLYS && currenttransvert < MAX_TRANSVERTS)\
	{\
		transvert[currenttransvert].s = (vs);\
		transvert[currenttransvert].t = (vt);\
		transvert[currenttransvert].r = (byte) (bound(0, (int) (vr), 255));\
		transvert[currenttransvert].g = (byte) (bound(0, (int) (vg), 255));\
		transvert[currenttransvert].b = (byte) (bound(0, (int) (vb), 255));\
		transvert[currenttransvert].a = (byte) (bound(0, (int) (va), 255));\
		transvert[currenttransvert].v[0] = (vx);\
		transvert[currenttransvert].v[1] = (vy);\
		transvert[currenttransvert].v[2] = (vz);\
		currenttransvert++;\
		transpoly[currenttranspoly].verts++;\
	}\
}

#define transpolyvertub(vx,vy,vz,vs,vt,vr,vg,vb,va) \
{\
	if (currenttranspoly < MAX_TRANSPOLYS && currenttransvert < MAX_TRANSVERTS)\
	{\
		transvert[currenttransvert].s = (vs);\
		transvert[currenttransvert].t = (vt);\
		transvert[currenttransvert].r = (vr);\
		transvert[currenttransvert].g = (vg);\
		transvert[currenttransvert].b = (vb);\
		transvert[currenttransvert].a = (va);\
		transvert[currenttransvert].v[0] = (vx);\
		transvert[currenttransvert].v[1] = (vy);\
		transvert[currenttransvert].v[2] = (vz);\
		currenttransvert++;\
		transpoly[currenttranspoly].verts++;\
	}\
}
