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

*/
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include <stdio.h>

#include "quakedef.h"
#include "cmd.h"
#include "crc.h"
#include "cvar.h"
#include "host.h"
#include "mathlib.h"
#include "server.h"
#include "strlib.h"
#include "sys.h"
#include "world.h"

dprograms_t		*progs;
dfunction_t		*pr_functions;
char			*pr_strings;
ddef_t			*pr_fielddefs;
ddef_t			*pr_globaldefs;
dstatement_t	*pr_statements;
globalvars_t	*pr_global_struct;
float			*pr_globals;		// same as pr_global_struct
int				pr_edict_size;		// in bytes
int				pr_edictareasize;	// in bytes

unsigned short pr_crc;

static memzone_t *progs_memzone;
static memzone_t *edictstring_memzone;

Uint type_size[8] =
{
	1,
	sizeof (string_t) / 4,
	1,
	3,
	1,
	1,
	sizeof (func_t) / 4,
	sizeof (void *) / 4
};

extern ddef_t *ED_FieldAtOfs (int ofs);
extern qboolean ED_ParseEpair (void *base, ddef_t *key, char *s);

cvar_t *nomonsters;
cvar_t *gamecfg;
cvar_t *scratch1;
cvar_t *scratch2;
cvar_t *scratch3;
cvar_t *scratch4;
cvar_t *savedgamecfg;
cvar_t *saved1;
cvar_t *saved2;
cvar_t *saved3;
cvar_t *saved4;
cvar_t *pr_checkextension;

// Used to flag whether or not to use LordHavoc's optional runtime bounds checking
// optional since some mods won't run with it :) Should be off by default...
cvar_t *pr_boundscheck;

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct
{
	ddef_t		*pcache;
	char		field[MAX_FIELD_LEN];
}
gefv_cache;

static gefv_cache gefvCache[GEFV_CACHESIZE] =
{
	{NULL, ""},
	{NULL, ""}
};

extern ddef_t *ED_FindField (char *name);
extern dfunction_t *ED_FindFunction (char *name);

// LordHavoc: in an effort to eliminate time wasted on GetEdictFieldValue...
// these are defined as externs in progs.h
int eval_gravity;
int eval_button3;
int eval_button4;
int eval_button5;
int eval_button6;
int eval_button7;
int eval_button8;
int eval_items2;
int eval_ammo_shells1;
int eval_ammo_nails1;
int eval_ammo_lava_nails;
int eval_ammo_rockets1;
int eval_ammo_multi_rockets;
int eval_ammo_cells1;
int eval_ammo_plasma;
int eval_idealpitch;
int eval_pitch_speed;
int eval_ping;
int eval_movement;
int eval_punchvector;

int FindFieldOffset(char *field)
{
	ddef_t *d;
	d = ED_FindField (field);
	if (!d)
		return 0;
	return d->ofs*4;
}

void FindEdictFieldOffsets(void)
{
	eval_gravity = FindFieldOffset ("gravity");
	eval_button3 = FindFieldOffset ("button3");
	eval_button4 = FindFieldOffset ("button4");
	eval_button5 = FindFieldOffset ("button5");
	eval_button6 = FindFieldOffset ("button6");
	eval_button7 = FindFieldOffset ("button7");
	eval_button8 = FindFieldOffset ("button8");
	eval_items2 = FindFieldOffset ("items2");
	eval_ammo_shells1 = FindFieldOffset ("ammo_shells1");
	eval_ammo_nails1 = FindFieldOffset ("ammo_nails1");
	eval_ammo_lava_nails = FindFieldOffset ("ammo_lava_nails");
	eval_ammo_rockets1 = FindFieldOffset ("ammo_rockets1");
	eval_ammo_multi_rockets = FindFieldOffset ("ammo_multi_rockets");
	eval_ammo_cells1 = FindFieldOffset ("ammo_cells1");
	eval_ammo_plasma = FindFieldOffset ("ammo_plasma");
	eval_idealpitch = FindFieldOffset ("idealpitch");
	eval_pitch_speed = FindFieldOffset ("pitch_speed");
	eval_ping = FindFieldOffset ("ping");
	eval_movement = FindFieldOffset ("movement");
	eval_punchvector = FindFieldOffset ("punchvector");
};

/*
=================
ED_ClearEdict

Sets everything to NULL
=================
*/
void
ED_ClearEdict (edict_t *e)
{
	memset (&e->v, 0, progs->entityfields * 4);
	e->free = false;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *
ED_Alloc (void)
{
	Uint32		i;
	edict_t		*e;

	for (i = svs.maxclients + 1; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM (i);

		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (e->freetime < 2 || sv.time - e->freetime > 0.5)) {
			ED_ClearEdict (e);
			return e;
		}
	}

	if (i == MAX_EDICTS)
		Host_Error ("ED_Alloc: no free edicts");

	sv.num_edicts++;
	e = EDICT_NUM (i);
	ED_ClearEdict (e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void
ED_Free (edict_t *ed)
{
	SV_UnlinkEdict (ed);				// unlink from world bsp

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorClear (ed->v.origin);
	VectorClear (ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->freetime = sv.time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
ddef_t *
ED_GlobalAtOfs (int ofs)
{
	ddef_t		*def;
	Uint		i;

	for (i = 0; i < progs->numglobaldefs; i++)
	{
		def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FieldAtOfs
============
*/
ddef_t *
ED_FieldAtOfs (int ofs)
{
	ddef_t		*def;
	Uint		i;

	for (i = 0; i < progs->numfielddefs; i++)
	{
		def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FindField
============
*/
ddef_t *
ED_FindField (char *name)
{
	ddef_t		*def;
	Uint		i;

	for (i = 0; i < progs->numfielddefs; i++)
	{
		def = &pr_fielddefs[i];
		if (!strcmp (pr_strings + def->s_name, name))
			return def;
	}
	return NULL;
}


/*
============
ED_FindGlobal
============
*/
ddef_t *
ED_FindGlobal (char *name)
{
	ddef_t		*def;
	Uint		i;

	for (i = 0; i < progs->numglobaldefs; i++)
	{
		def = &pr_globaldefs[i];
		if (!strcmp (pr_strings + def->s_name, name))
			return def;
	}
	return NULL;
}


/*
============
ED_FindFunction
============
*/
dfunction_t *
ED_FindFunction (char *name)
{
	dfunction_t		*func;
	Uint			i;

	for (i = 0; i < progs->numfunctions; i++)
	{
		func = &pr_functions[i];
		if (!strcmp (pr_strings + func->s_name, name))
			return func;
	}
	return NULL;
}

/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
int NoCrash_NUM_FOR_EDICT(edict_t *e, char *filename, int fileline);

char *
PR_ValueString (etype_t type, eval_t *val)
{
	static char		line[1024];
	ddef_t			*def;
	dfunction_t		*f;
	Uint			n;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
		case ev_string:
			snprintf (line, sizeof (line), "%s", pr_strings + val->string);
			break;
		case ev_entity:
			n = NoCrash_NUM_FOR_EDICT (PROG_TO_EDICT (val->edict), __FILE__, __LINE__);
			if (n >= MAX_EDICTS)
				snprintf (line, sizeof (line), "entity %i (invalid!)", n);
			else
				snprintf (line, sizeof (line), "entity %i", n);
			break;
		case ev_function:
			f = pr_functions + val->function;
			snprintf (line, sizeof (line), "%s()", pr_strings + f->s_name);
			break;
		case ev_field:
			def = ED_FieldAtOfs (val->_int);
			snprintf (line, sizeof (line), ".%s", pr_strings + def->s_name);
			break;
		case ev_void:
			snprintf (line, sizeof (line), "void");
			break;
		case ev_float:
			snprintf (line, sizeof (line), "%10.4f", val->_float);
			break;
		case ev_vector:
			snprintf (line, sizeof (line), "'%10.4f %10.4f %10.4f'",
					  val->vector[0], val->vector[1], val->vector[2]);
			break;
		case ev_pointer:
			snprintf (line, sizeof (line), "pointer");
			break;
		default:
			snprintf (line, sizeof (line), "bad type %i", type);
			break;
	}

	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *
PR_UglyValueString (etype_t type, eval_t *val)
{
	static char		line[4096];
	ddef_t			*def;
	dfunction_t		*f;
	Uint			n;
	char			*s;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
		case ev_string:
			snprintf (line, sizeof (line), "%s", pr_strings + val->string);
			break;
		case ev_entity:
			n = NoCrash_NUM_FOR_EDICT (PROG_TO_EDICT (val->edict), __FILE__, __LINE__);
			snprintf (line, sizeof (line), "%i", n);
			break;
		case ev_function:
			f = pr_functions + val->function;
			snprintf (line, sizeof (line), "%s", pr_strings + f->s_name);
			break;
		case ev_field:
			def = ED_FieldAtOfs (val->_int);
			s = pr_strings + def->s_name;
			n = 0;
			while (n < sizeof (line - 1) && *s)
			{
				if (*s == '\n')
				{
					line[n++] = '\\';
					line[n++] = 'n';
				}
				else if (*s == '\r')
				{
					line[n++] = '\\';
					line[n++] = 'r';
				}
				else
					line[n] = *s;
				s++;
			}
			line[n++] = 0;
			break;
		case ev_void:
			snprintf (line, sizeof (line), "void");
			break;
		case ev_float:
			snprintf (line, sizeof (line), "%f", val->_float);
			break;
		case ev_vector:
			snprintf (line, sizeof (line), "%f %f %f",
					  val->vector[0], val->vector[1], val->vector[2]);
			break;
		default:
			snprintf (line, sizeof (line), "bad type %i", type);
			break;
	}

	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *
PR_GlobalString (int ofs)
{
	char			*s;
	Uint			i;
	ddef_t			*def;
	void			*val;
	static char		line[128];

	val = (void *) &pr_globals[ofs];
	def = ED_GlobalAtOfs (ofs);

	if (!def)
		snprintf (line, sizeof (line), "%i(\?\?\?)", ofs);
	else
	{
		s = PR_ValueString (def->type, val);
		snprintf (line, sizeof (line), "%i(%s)%s", ofs,
				  pr_strings + def->s_name, s);
	}

	i = strlen (line);
	for (; i < 20; i++)
		strcat (line, " ");
	strcat (line, " ");

	return line;
}

char *
PR_GlobalStringNoContents (int ofs)
{
	Uint			i;
	ddef_t			*def;
	static char		line[128];

	def = ED_GlobalAtOfs (ofs);
	if (!def)
		snprintf (line, sizeof (line), "%i(\?\?\?)", ofs);
	else
		snprintf (line, sizeof (line), "%i(%s)", ofs, pr_strings + def->s_name);

	for (i = strlen (line); i < 20; i++)
		strcat (line, " ");
	strcat (line, " ");

	return line;
}


/*
=============
ED_Print

For debugging
LordHavoc:
	optimized this to print out much more quickly (tempstring)
	changed to print out every 4096 characters (incase there are
	a lot of fields to print)
=============
*/
void
ED_Print (edict_t *ed)
{
	Uint		*v, i, j, l, type;
	ddef_t		*d;
	char		*name, *value, tab[16];

	if (ed->free)
	{
		Com_Printf ("FREE\n");
		return;
	}

	Com_Printf ("\nEDICT %i:\n", NUM_FOR_EDICT (ed, __FILE__, __LINE__));
	for (i = 1; i < progs->numfielddefs; i++)
	{
		d = &pr_fielddefs[i];
		name = pr_strings + d->s_name;
		if (name[strlen (name) - 2] == '_')
			// skip _x, _y, _z vars
			continue;
			
		v = (int *)((char *)&ed->v + d->ofs * 4);

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		value = PR_ValueString (d->type, (eval_t *)v);
		l = strlen (name);
		
		for (j = 0; l < 15; j++, l++)
			tab[j] = ' ';
		tab[j] = '\0';

		Com_Printf ("%s%s%s\n", name, tab, value);
	}
}

/*
=============
ED_Write

For savegames
=============
*/
void
ED_Write (FILE *f, edict_t *ed)
{
	ddef_t		*d;
	Uint		*v, i, j, type;
	char		*name;

	fprintf (f, "{\n");

	if (ed->free)
	{
		fprintf (f, "}\n");
		return;
	}

	for (i = 1; i < progs->numfielddefs; i++)
	{
		d = &pr_fielddefs[i];
		name = pr_strings + d->s_name;
		if (name[strlen (name) - 2] == '_')
			// skip _x, _y, _z vars
			continue;

		v = (int *) ((char *) &ed->v + d->ofs * 4);

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		fprintf (f, "\"%s\" ", name);
		fprintf (f, "\"%s\"\n", PR_UglyValueString (d->type, (eval_t *) v));
	}

	fprintf (f, "}\n");
}

void
ED_PrintNum (Uint ent)
{
	ED_Print (EDICT_NUM (ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void
ED_PrintEdicts (void)
{
	Uint		i;

	Com_Printf ("%i entities\n", sv.num_edicts);
	for (i = 0; i < sv.num_edicts; i++)
		ED_PrintNum (i);
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edict
=============
*/
void
ED_PrintEdict_f (void)
{
	Uint		i;

	i = Q_atoi (Cmd_Argv (1));
	if (i >= sv.num_edicts)
	{
		Com_Printf ("Bad edict number\n");
		return;
	}
	ED_PrintNum (i);
}

/*
=============
ED_Count

For debugging
=============
*/
void
ED_Count (void)
{
	Uint		i, active, models, solid, step;
	edict_t		*ent;

	active = models = solid = step = 0;
	for (i = 0; i < sv.num_edicts; i++)
	{
		ent = EDICT_NUM (i);
		if (ent->free)
			continue;
		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Com_Printf ("num_edicts:%3i\n", sv.num_edicts);
	Com_Printf ("active    :%3i\n", active);
	Com_Printf ("view      :%3i\n", models);
	Com_Printf ("touch     :%3i\n", solid);
	Com_Printf ("step      :%3i\n", step);

}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_WriteGlobals
=============
*/
void
ED_WriteGlobals (FILE * f)
{
	ddef_t		*def;
	Uint		i;
	char		*name;
	Uint		type;

	fprintf (f, "{\n");
	for (i = 0; i < progs->numglobaldefs; i++)
	{
		def = &pr_globaldefs[i];
		type = def->type;
		if (!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = pr_strings + def->s_name;
		fprintf (f, "\"%s\" ", name);
		fprintf (f, "\"%s\"\n", PR_UglyValueString (type,
					(eval_t *) &pr_globals[def->ofs]));
	}
	fprintf (f, "}\n");
}

/*
=============
ED_ParseGlobals
=============
*/
void
ED_ParseGlobals (char *data)
{
	char		keyname[64];
	ddef_t		*key;

	while (1)
	{
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		strcpy (keyname, com_token);

		// parse value 
		data = COM_Parse (data);
		if (!data)
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Host_Error ("ED_ParseEntity: closing brace without data");

		key = ED_FindGlobal (keyname);
		if (!key)
		{
			Com_Printf ("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair ((void *) pr_globals, key, com_token))
			Host_Error ("ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
char *
ED_NewString (char *string)
{
	char		*new, *new_p;
	Uint		i, l;

	l = strlen(string) + 1;
	new = Zone_Alloc(edictstring_memzone, l);
	new_p = new;

	for (i = 0; i < l ;i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return new;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
qboolean
ED_ParseEpair (void *base, ddef_t *key, char *s)
{
	Uint			i;
	char			string[128], *v, *w;
	ddef_t			*def;
	void			*d;
	dfunction_t		*func;

	d = (void *) ((int *) base + key->ofs);

	switch (key->type & ~DEF_SAVEGLOBAL)
	{
		case ev_string:
			*(string_t *) d = ED_NewString (s) - pr_strings;
			break;

		case ev_float:
			*(float *) d = Q_atof (s);
			break;

		case ev_vector:
			strcpy (string, s);
			v = string;
			w = string;
			for (i = 0; i < 3; i++)
			{
				while (*v && *v != ' ')
					v++;
				*v = 0;
				((float *) d)[i] = Q_atof (w);
				w = v = v + 1;
			}
			break;

		case ev_entity:
			*(int *) d = EDICT_TO_PROG (EDICT_NUM ((Uint)atoi (s)));
			break;

		case ev_field:
			def = ED_FindField (s);
			if (!def)
			{
				Com_Printf ("Can't find field %s\n", s);
				return false;
			}
			*(int *) d = G_INT (def->ofs);
			break;

		case ev_function:
			func = ED_FindFunction (s);
			if (!func)
			{
				Com_Printf ("Can't find function %s\n", s);
				return false;
			}
			*(func_t *) d = func - pr_functions;
			break;

		default:
			break;
	}
	return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
char *
ED_ParseEdict (char *data, edict_t *ent)
{
	ddef_t		*key;
	qboolean	anglehack, init;
	char		keyname[256];
	Uint		n;
	char		temp[32];

	init = false;

	// clear it
	if (ent != sv.edicts)				// hack
		memset (&ent->v, 0, progs->entityfields * 4);

	// go through all the dictionary pairs
	while (1)
	{
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp (com_token, "angle"))
		{
			strcpy (com_token, "angles");
			anglehack = true;
		}
		else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp (com_token, "light"))
			strcpy (com_token, "light_lev");	// hack for single light def

		strcpy (keyname, com_token);

		// another hack to fix heynames with trailing spaces
		n = strlen (keyname);
		while (n && keyname[n - 1] == ' ')
		{
			keyname[n - 1] = 0;
			n--;
		}

		// parse value 
		data = COM_Parse (data);
		if (!data)
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Host_Error ("ED_ParseEntity: closing brace without data");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		key = ED_FindField (keyname);
		if (!key)
		{
			Com_Printf ("'%s' is not a field\n", keyname);
			continue;
		}

		if (anglehack)
		{
			strcpy (temp, com_token);
			snprintf (com_token, sizeof (com_token), "0 %s 0", temp);
		}

		if (!ED_ParseEpair ((void *) &ent->v, key, com_token))
			Host_Error ("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->free = true;

	return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void
ED_LoadFromFile (char *data)
{
	edict_t			*ent;
	Uint			inhibit;
	dfunction_t		*func;

	ent = NULL;
	inhibit = 0;
	pr_global_struct->time = sv.time;

	// parse ents
	while (1)
	{
		// parse the opening brace  
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Host_Error ("ED_LoadFromFile: found %s when expecting {", com_token);

		if (!ent)
			ent = EDICT_NUM (0);
		else
			ent = ED_Alloc ();
		data = ED_ParseEdict (data, ent);

		// remove things from different skill levels or deathmatch
		if (deathmatch->ivalue)
		{
			if (((int) ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
			{
				ED_Free (ent);
				inhibit++;
				continue;
			}
		}
		else if ((current_skill == 0
					&& ((int) ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
				|| (current_skill == 1
					&& ((int) ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
				|| (current_skill >= 2
					&& ((int) ent->v.spawnflags & SPAWNFLAG_NOT_HARD)))
		{
			ED_Free (ent);
			inhibit++;
			continue;
		}

		// immediately call spawn function
		if (!ent->v.classname)
		{
			Com_Printf ("No classname for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}
		// look for the spawn function
		func = ED_FindFunction (pr_strings + ent->v.classname);

		if (!func)
		{
			Com_Printf ("No spawn function for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG (ent);
		PR_ExecuteProgram (func - pr_functions, "");
	}

	Com_DPrintf ("%i entities inhibited\n", inhibit);
}

typedef struct eval_field_s
{
	Uint		type;
	char		*string;
}
eval_field_t;

#define EVALFIELDS (sizeof(eval_fields) / sizeof(eval_field_t))

eval_field_t eval_fields[] =
{
	{ev_float, "gravity"},
	{ev_float, "button3"},
	{ev_float, "button4"},
	{ev_float, "button5"},
	{ev_float, "button6"},
	{ev_float, "button7"},
	{ev_float, "button8"},
	{ev_float, "glow_size"},
	{ev_float, "glow_trail"},
	{ev_float, "glow_color"},
	{ev_float, "items2"},
	{ev_float, "scale"},
	{ev_float, "alpha"},
	{ev_float, "renderamt"},
	{ev_float, "rendermode"},
	{ev_float, "fullbright"},
	{ev_float, "ammo_shells1"},
	{ev_float, "ammo_nails1"},
	{ev_float, "ammo_lava_nails"},
	{ev_float, "ammo_rockets1"},
	{ev_float, "ammo_multi_rockets"},
	{ev_float, "ammo_cells1"},
	{ev_float, "ammo_plasma"},
	{ev_float, "idealpitch"},
	{ev_float, "pitch_speed"},
	{ev_entity, "viewmodelforclient"},
	{ev_entity, "nodrawtoclient"},
	{ev_entity, "exteriormodeltoclient"},
	{ev_entity, "drawonlytoclient"},
	{ev_float, "ping"},
	{ev_vector, "movement"},
	{ev_float, "pmodel"},
	{ev_vector, "punchvector"}
};

/*
===============
PR_LoadProgs
===============
*/
void
PR_LoadProgs (void)
{
	Uint			i;
	dstatement_t	*st;
	ddef_t			*infielddefs;

	// flush the non-C variable lookup cache
	for (i = 0; i < GEFV_CACHESIZE; i++)
		gefvCache[i].field[0] = 0;

	Zone_EmptyZone (progs_memzone);
	Zone_EmptyZone (edictstring_memzone);

	progs = (dprograms_t *) COM_LoadZoneFile("progs.dat", false, progs_memzone);
	if (!progs)
		Host_Error ("PR_LoadProgs: couldn't load progs.dat");

	Com_DPrintf ("Programs occupy %iK.\n", com_filesize / 1024);

	pr_crc = CRC_Block((Uint8 *) progs, com_filesize);

	// byte swap the header
	for (i = 0; i < sizeof (*progs) / 4; i++)
		((int *)progs)[i] = LittleLong ( ((int *) progs)[i] );

	if (progs->version != PROG_VERSION)
		Host_Error ("progs.dat has wrong version number (%i should be %i)",
				progs->version, PROG_VERSION);
	if (progs->crc != PROGHEADER_CRC)
		Host_Error ("progs.dat system vars have been modified, "
				"progdefs.h is out of date");

	pr_functions = (dfunction_t *)((Uint8 *) progs + progs->ofs_functions);
	pr_strings = (char *) progs + progs->ofs_strings;
	pr_globaldefs = (ddef_t *)((Uint8 * )progs + progs->ofs_globaldefs);

	// we need to expand the fielddefs list to include all the engine fields,
	// so allocate a new place for it
	infielddefs = (ddef_t *)((Uint8 *)progs + progs->ofs_fielddefs);
	pr_fielddefs = Zone_Alloc (progs_memzone, (progs->numfielddefs + EVALFIELDS) * sizeof(ddef_t));

	pr_statements = (dstatement_t *)((Uint8 *) progs + progs->ofs_statements);

	// moved edict_size calculation down below field adding code
	pr_global_struct = (globalvars_t *)((Uint8 *)progs + progs->ofs_globals);
	pr_globals = (float *)pr_global_struct;

	// byte swap the lumps
	for (i=0 ; i<progs->numstatements ; i++)
	{
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (i = 0; i < progs->numfunctions; i++)
	{
		pr_functions[i].first_statement = LittleLong
			(pr_functions[i].first_statement);
		pr_functions[i].parm_start = LittleLong (pr_functions[i].parm_start);
		pr_functions[i].s_name = LittleLong (pr_functions[i].s_name);
		pr_functions[i].s_file = LittleLong (pr_functions[i].s_file);
		pr_functions[i].numparms = LittleLong (pr_functions[i].numparms);
		pr_functions[i].locals = LittleLong (pr_functions[i].locals);
	}

	for (i = 0; i < progs->numglobaldefs; i++)
	{
		pr_globaldefs[i].type = LittleShort (pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort (pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong (pr_globaldefs[i].s_name);
	}

	// copy the progs fields to the new fields list
	for (i = 0; i < progs->numfielddefs; i++)
	{
		pr_fielddefs[i].type = LittleShort (infielddefs[i].type);
		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			Host_Error ("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr_fielddefs[i].ofs = LittleShort (infielddefs[i].ofs);
		pr_fielddefs[i].s_name = LittleLong (infielddefs[i].s_name);
	}

	// append the darkplaces fields
	for (i = 0; i < EVALFIELDS; i++)
	{
		pr_fielddefs[progs->numfielddefs].type = eval_fields[i].type;
		pr_fielddefs[progs->numfielddefs].ofs = progs->entityfields;
		pr_fielddefs[progs->numfielddefs].s_name = eval_fields[i].string - pr_strings;
		if (pr_fielddefs[progs->numfielddefs].type == ev_vector)
			progs->entityfields += 3;
		else
			progs->entityfields++;
		progs->numfielddefs++;
	}

	for (i = 0; i < progs->numglobals; i++)
		((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);

	// moved edict_size calculation down here, below field adding code
	pr_edict_size = progs->entityfields * 4 + sizeof (edict_t) - sizeof(entvars_t);
	// Alignment fix for 64 bit
	pr_edict_size = (pr_edict_size + 7) & ~7;

	pr_edictareasize = pr_edict_size * MAX_EDICTS;

	// LordHavoc: bounds check anything static
	for (i = 0, st = pr_statements; i < progs->numstatements; i++, st++)
	{
		switch (st->op)
		{
			case OP_IF:
			case OP_IFNOT:
				if (st->a >= (Sint16)progs->numglobals
						|| st->b + i >= progs->numstatements)
					Host_Error
						("PR_LoadProgs: out of bounds IF/IFNOT (statement %d)\n", i);
				break;
			case OP_GOTO:
				if (st->a + i >= progs->numstatements)
					Host_Error
						("PR_LoadProgs: out of bounds GOTO (statement %d)\n", i);
				break;
			// global global global
			case OP_ADD_F:
			case OP_ADD_V:
			case OP_SUB_F:
			case OP_SUB_V:
			case OP_MUL_F:
			case OP_MUL_V:
			case OP_MUL_FV:
			case OP_MUL_VF:
			case OP_DIV_F:
			case OP_BITAND:
			case OP_BITOR:
			case OP_GE:
			case OP_LE:
			case OP_GT:
			case OP_LT:
			case OP_AND:
			case OP_OR:
			case OP_EQ_F:
			case OP_EQ_V:
			case OP_EQ_S:
			case OP_EQ_E:
			case OP_EQ_FNC:
			case OP_NE_F:
			case OP_NE_V:
			case OP_NE_S:
			case OP_NE_E:
			case OP_NE_FNC:
			case OP_ADDRESS:
			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FNC:
			case OP_LOAD_V:
				if ((Uint16) st->a >= progs->numglobals
						|| (Uint16) st->b >= progs->numglobals
						|| (Uint16) st->c >= progs->numglobals)
					Host_Error
						("PR_LoadProgs: out of bounds global index (statement %d)\n", i);
				break;
			// global none global
			case OP_NOT_F:
			case OP_NOT_V:
			case OP_NOT_S:
			case OP_NOT_FNC:
			case OP_NOT_ENT:
				if ((Uint16) st->a >= progs->numglobals
						|| (Uint16) st->c >= progs->numglobals)
					Host_Error
						("PR_LoadProgs: out of bounds global index (statement %d)\n", i);
				break;
			// 2 globals
			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:
			case OP_STOREP_S:
			case OP_STOREP_FNC:
			case OP_STORE_F:
			case OP_STORE_ENT:
			case OP_STORE_FLD:
			case OP_STORE_S:
			case OP_STORE_FNC:
			case OP_STATE:
			case OP_STOREP_V:
			case OP_STORE_V:
				if ((Uint16) st->a >= progs->numglobals
						|| (Uint16) st->b >= progs->numglobals)
					Host_Error
						("PR_LoadProgs: out of bounds global index (statement %d)\n", i);
				break;
			// 1 global
			case OP_CALL0:
			case OP_CALL1:
			case OP_CALL2:
			case OP_CALL3:
			case OP_CALL4:
			case OP_CALL5:
			case OP_CALL6:
			case OP_CALL7:
			case OP_CALL8:
			case OP_DONE:
			case OP_RETURN:
				if ((Uint16) st->a >= progs->numglobals)
					Host_Error
						("PR_LoadProgs: out of bounds global index (statement %d)\n", i);
				break;
			default:
				Host_Error
					("PR_LoadProgs: unknown opcode %d at statement %d\n", st->op, i);
				break;
		}
	}

	FindEdictFieldOffsets(); // LordHavoc: update field offset list
}

void PR_Fields_f (void)
{
	Uint		i;

	if (!sv.active)
	{
		Com_Printf ("no progs loaded\n");
		return;
	}

	for (i = 0; i < progs->numfielddefs; i++)
		Com_Printf ("%s\n", (pr_strings + pr_fielddefs[i].s_name));

	Com_Printf ("%i entity fields, totalling %i bytes per edict, %i edicts, "
			"%i bytes total spent on edict fields\n", progs->entityfields,
			progs->entityfields * 4, MAX_EDICTS,
			progs->entityfields * 4 * MAX_EDICTS);
}

void PR_Globals_f (void)
{
	Uint		i;

	if (!sv.active)
	{
		Com_Printf ("no progs loaded\n");
		return;
	}

	for (i = 0; i < progs->numglobaldefs; i++)
		Com_Printf ("%s\n", (pr_strings + pr_globaldefs[i].s_name));

	Com_Printf ("%i global variables, totalling %i bytes\n",
		progs->numglobals, progs->numglobals * 4);
}

/*
===============
PR_Init_Cvars
===============
*/
void
PR_Init_Cvars (void)
{
	nomonsters = Cvar_Get ("nomonsters", "0", CVAR_NONE, NULL);
	gamecfg = Cvar_Get ("gamecfg", "0", CVAR_NONE, NULL);
	scratch1 = Cvar_Get ("scratch1", "0", CVAR_NONE, NULL);
	scratch2 = Cvar_Get ("scratch2", "0", CVAR_NONE, NULL);
	scratch3 = Cvar_Get ("scratch3", "0", CVAR_NONE, NULL);
	scratch4 = Cvar_Get ("scratch4", "0", CVAR_NONE, NULL);
	savedgamecfg = Cvar_Get ("savedgamecfg", "0", CVAR_ARCHIVE, NULL);
	saved1 = Cvar_Get ("saved1", "0", CVAR_ARCHIVE, NULL);
	saved2 = Cvar_Get ("saved2", "0", CVAR_ARCHIVE, NULL);
	saved3 = Cvar_Get ("saved3", "0", CVAR_ARCHIVE, NULL);
	saved4 = Cvar_Get ("saved4", "0", CVAR_ARCHIVE, NULL);
	pr_boundscheck = Cvar_Get ("pr_boundscheck", "1", CVAR_ARCHIVE, NULL);
	pr_checkextension = Cvar_Get ("pr_checkextension", "1", CVAR_ROM, NULL);
}

/*
===============
PR_Init
===============
*/
void
PR_Init (void)
{
	Cmd_AddCommand ("edict", ED_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED_PrintEdicts);
	Cmd_AddCommand ("edictcount", ED_Count);
	Cmd_AddCommand ("profile", PR_Profile_f);
	Cmd_AddCommand ("pr_fields", PR_Fields_f);
	Cmd_AddCommand ("pr_globals", PR_Globals_f);

	progs_memzone = Zone_AllocZone("progs.dat");
	edictstring_memzone = Zone_AllocZone("edict strings");
}

edict_t *
EDICT_NUM_ERROR (Uint n)
{
	Sys_Error ("EDICT_NUM: bad number %i", n);
	return NULL;
}

Uint
NUM_FOR_EDICT (edict_t *e, char *filename, int fileline)
{
	Uint		b;

	b = (Uint8 *) e - (Uint8 *) sv.edicts;
	b = b / pr_edict_size;

	if (b >= sv.num_edicts)
		Sys_Error ("NUM_FOR_EDICT: bad pointer, FILE: %s, LINE: %i", filename, fileline);
	return b;
}

int
NoCrash_NUM_FOR_EDICT (edict_t *e, char *filename, int fileline)
{
	Uint		b;

	filename = filename;
	fileline = fileline;

	b = (Uint8 *)e - (Uint8 *)sv.edicts;
	b = b / pr_edict_size;

	return b;
}

