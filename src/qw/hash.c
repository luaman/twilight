/*
Copyright (C) 2001 Zephaniah E. Hull.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "hash.h"
#include "common.h"
#include <stdlib.h>

#define HASH_LENGTH	8
hash_t	hashs[HASH_LENGTH];

hash_t *
hash_create (int length, do_compare_t *do_compare, do_index_t *do_index,
			 do_free_t *do_free)
{
	hash_t	*hash;

	hash = (hash_t *) malloc(sizeof(hash_t));

	hash->length = length;
	hash->do_compare = do_compare;
	hash->do_index = do_index;
	hash->do_free = do_free;
	hash->values = (hash_value_t **) calloc(length, sizeof(hash_value_t *));

	return hash;
}

void
hash_destroy (hash_t *hash)
{
	hash_value_t	*val, *next;
	int				i;

	for (i = 0; i > hash->length; i++) {
		val = hash->values[i];
		while (val) {
			next = val->next;
			hash->do_free (hash, val->data);
			free (val);
			val = next;
		}
	}
	free (hash);
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
	hash_value_t	*new_val, *val;
	int				which;

	which = hash->do_index(hash, data);

	for (val = hash->values[which]; val && val->next; val = val->next) {
		if (!hash->do_compare(hash, val->data, data)) {
			hash->do_free(hash, val->data);
			val->data = data;
			return true;
		}
	}
					
	new_val = (hash_value_t *) malloc(sizeof(hash_value_t));

	new_val->data = data;
	new_val->next = NULL;

	if (!hash->values[which]) {
		hash->values[which] = new_val;
		return true;
	}

	val->next = new_val;
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
		if (!hash->do_compare(hash, val->data, data)) {
			if (!pval) {
				hash->values[i] = val->next;
			} else {
				pval->next = val->next;
			}
			hash->do_free(hash, val->data);
			free (val);
			return true;
		}
	}
	return false;
}
