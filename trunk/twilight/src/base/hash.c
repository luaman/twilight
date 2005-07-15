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

*/
static const char rcsid[] =
    "$Id$";

#include <stdlib.h>
#include "twiconfig.h"
#include "hash.h"

hash_t *
hash_create (int bits, do_compare_t *do_compare, do_index_t *do_index,
			 do_free_t *do_free, memzone_t *zone)
{
	hash_t	*hash;

	hash = (hash_t *) Zone_Alloc(zone, sizeof(hash_t));

	hash->bits = bits;
	hash->length = 1 << bits;
	hash->do_compare = do_compare;
	hash->do_index = do_index;
	hash->do_free = do_free;
	hash->n_values = 0;
	hash->values = (hash_value_t **) Zone_Alloc(zone, hash->length * sizeof(hash_value_t *));
	hash->zone = zone;

	return hash;
}

void
hash_destroy (hash_t *hash)
{
	hash_value_t	*val, *next;
	int				i;

	for (i = 0; i < hash->length; i++) {
		val = hash->values[i];
		while (val) {
			next = val->next;
			hash->do_free (hash, val->data);
			hash->n_values--;
			free (val);
			val = next;
		}
	}
	Zone_Free (hash->values);
	Zone_Free (hash);
}

void *
hash_get (hash_t *hash, void *data)
{
	hash_value_t	*value;
	int				i;

	i = hash->do_index(hash, data);

	for (value = hash->values[i]; value; value = value->next) {
		if (!hash->do_compare(hash, value->data, data)) {
			return value->data;
		}
	}

	return NULL;
}

qboolean
hash_add (hash_t *hash, void *data)
{
	hash_value_t	*val;
	int				which;

	which = hash->do_index(hash, data);

	val = (hash_value_t *) Zone_Alloc(hash->zone, sizeof(hash_value_t));

	val->data = data;

	val->next = hash->values[which];
	hash->values[which] = val;
	hash->n_values++;
	return true;
}

qboolean
hash_del (hash_t *hash, void *data)
{
	hash_value_t	*val, *pval;
	int				i;

	i = hash->do_index(hash, data);
	pval = NULL;

	for (val = hash->values[i]; val; pval = val, val = val->next) {
		if (val->data == data) {
			if (!pval) {
				hash->values[i] = val->next;
			} else {
				pval->next = val->next;
			}
			hash->do_free(hash, val->data);
			hash->n_values--;
			Zone_Free (val);
			return true;
		}
	}
	return false;
}
