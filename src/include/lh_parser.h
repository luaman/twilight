/*
	$RCSfile$

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __LH_PARSER_H
#define __LH_PARSER_H

#include "qtypes.h"
#include "zone.h"

#define WORDFLAG_STRING		1
#define WORDFLAG_INTEGER	2
#define WORDFLAG_DOUBLE		4

typedef struct codeword_s
{
	struct codeword_s *next, *prev;
	struct codetree_s *parent;
	char *string;
	int intvalue;
	double doublevalue;
	int flags;
}
codeword_t;

typedef struct codetree_s
{
	struct codetree_s *prev, *next, *child, *parent;
	codeword_t *words;
	int beginsindent;
	int temporarybeginsindent;
	int linenumber;
} codetree_t;

extern codetree_t *LHP_parse(char *text, char *name, memzone_t *zone);
extern void LHP_printcodetree_c(int indentlevel, codetree_t *code);
extern void LHP_freecodetree(codetree_t *code);


#endif // __QTYPES_H
