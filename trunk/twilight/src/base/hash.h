/*
	$RCSfile$

	Copyright (C) 2001  Zephaniah E. Hull.

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

#ifndef __HASH_H
#define __HASH_H

#include "qtypes.h"
#include "zone.h"

typedef struct hash_value_s {
	void				*data;
	struct hash_value_s	*next;
} hash_value_t;

typedef	int	(do_compare_t)(void *hash, void *data1, void *data2);
typedef int	(do_index_t)(void *hash, void *data);
typedef void (do_free_t)(void *hash, void *data);

typedef struct hash_s {
	do_compare_t	*do_compare;
	do_index_t		*do_index;
	do_free_t		*do_free;
	int				bits, length;		// Length is derived from bits.
	int				n_values;
	hash_value_t	**values;
	memzone_t		*zone;
} hash_t;

hash_t *hash_create (int bits, do_compare_t *do_compare, do_index_t *do_index, do_free_t *do_free, memzone_t *zone);
void hash_destroy (hash_t *hash);
void *hash_get (hash_t *hash, void *data);
qboolean hash_add (hash_t *hash, void *data);
qboolean hash_del (hash_t *hash, void *data);

#endif // __HASH_H

