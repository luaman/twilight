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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include <stdlib.h>		/* For malloc() */

#include "qtypes.h"
#include "cmd.h"
#include "common.h"
#include "cvar.h"
#include "strlib.h"
#include "zone.h"

typedef struct cvar_foreach_s {
	cvar_t					   *var;
	struct cvar_foreach_s	   *next;
} cvar_list_t;

static cvar_list_t *cvars;

cvar_callback engine_callback = NULL;

cvar_t *developer;

void
Cvar_Init (const cvar_callback callback)
{
	cvars = NULL;
	engine_callback = callback;

	developer = Cvar_Get ("developer", "0", CVAR_NONE, NULL);

	Cmd_AddCommand ("set", Cvar_Set_f);
}


void
Cvar_Shutdown (void)
{
	cvar_list_t	   *v, *t;

	v = cvars;
	while (v)
	{
		t = v;
		v = v->next;
		if (t->var)
		{
			if (t->var->string)
				Z_Free (t->var->string);
			Z_Free (t->var);
		}
		Z_Free (t);
	}
}


// FIXME: While no worse than Quake, this is EVIL!  Optimize it!
static void
Cvar_InsertVar (cvar_t *var)
{
	cvar_list_t	   *v;

	v = Z_Malloc (sizeof (cvar_list_t));
	v->var = var;
	v->next = cvars;
	cvars = v;
}


cvar_t *
Cvar_Get (const char *name, const char *value, const int flags,
				const cvar_callback callback)
{
	cvar_t		   *var;

	var = Cvar_Find (name);
	if (!var)	// Var does not exist, create it
	{
		var = Z_Malloc (sizeof(cvar_t));
		var->name = Z_Malloc (strlen (name) + 1);
		strcpy (var->name, name);
		var->string = NULL;		// force Cvar to change
		var->callback = callback;
		Cvar_InsertVar (var);
		Cvar_Set (var, value);
	}

	var->flags = flags;		// we always throw out flags
	return var;
}


void
Cvar_Set (cvar_t *var, const char *value)
{
	if (var->string)
	{
		if (strcasecmp (value, var->string) == 0)
			return;
		Z_Free (var->string);
	}

	var->string = Z_Malloc (strlen(value) + 1);
	strcpy (var->string, value);
	
	var->value = Q_atof (var->string);

	if (var->callback)
		var->callback (var);
	
	if (engine_callback)
		engine_callback (var);
}

void
Cvar_Set_f (void)
{
	cvar_t	*var;

	if (Cmd_Argc () < 2 || Cmd_Argc () > 3)
	{
		Com_Printf ("usage: set <Cvar> [value]\n");
		return;
	}

	var = Cvar_Find (Cmd_Argv (1));

	if (Cmd_Argc () == 2)
	{
		Cvar_Show (var);
		return;
	}

	if (!var)
	{
		var = Cvar_Get (Cmd_Argv (1), Cmd_Argv (2), CVAR_USER, NULL);
		return;
	}
	
	if (var->flags & CVAR_ROM)
	{
		Com_Printf ("Cvar \"%s\" is read-only.\n", var->name);
		return;
	}
	
	Cvar_Set (var, Cmd_Argv (2));
	return;
}


cvar_t *
Cvar_CreateTemp (const char *name, const char *value)
{
	cvar_t		   *var;

	var = Cvar_Find (name);
	if (var)	// the cvar already exists, and this shouldn't append
		return NULL;

	// Var does not exist, create it
	var = Z_Malloc (sizeof(cvar_t));
	var->name = Z_Malloc (strlen (name) + 1);
	strcpy (var->name, name);
	var->string = NULL;		// force Cvar to change
	var->callback = NULL;
	Cvar_InsertVar (var);
	Cvar_Set (var, value);

	var->flags = CVAR_TEMP;
	return var;
}


void
Cvar_Slide (cvar_t *var, const float change)
{
	static char		buf[128];

	var->value += change;
	Z_Free (var->string);
	snprintf (buf, 128, "%f", var->value);
	var->string = Z_Malloc (strlen (buf) + 1);
	strcpy (var->string, buf);

	if (var->callback)
		var->callback (var);
}


void
Cvar_Show (cvar_t *var)
{
	if (!var)
	{
		Com_Printf ("Cvar does not exist.\n");
		return;
	}

	Com_Printf ("[] \"%s\" is \"%s\"\n", var->name, var->string);
}


// FIXME: We want the option to disable this at some point
qboolean
Cvar_LegacyCmd (void)
{
	cvar_t	   *var;

	var = Cvar_Find (Cmd_Argv (0));
	if (!var)
		return false;

	if (Cmd_Argc () == 2)
	{
		if (!(var->flags & CVAR_ROM))
			Cvar_Set (var, Cmd_Argv (1));
		else
			Com_Printf ("Cvar \"%s\" is read-only.\n", var->name);
	} else
		Cvar_Show (var);

	return true;
}


cvar_t *
Cvar_Find (const char *name)
{
	cvar_list_t	   *v = cvars;

	while (v) {
		if (strcasecmp (name, v->var->name) == 0)
			return v->var;
		v = v->next;
	}

	return NULL;	// Cvar doesn't exist
}

/*
	CVar_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com

*/
int
Cvar_CompleteCountPossible (char *partial)
{
	cvar_list_t	*v;
	int			len;
	int			h;
	
	h = 0;
	len = strlen(partial);
	
	if (!len)
		return	0;
	
	// Loop through the cvars and count all possible matches
	for (v = cvars; v; v = v->next)
		if (!strncasecmp(partial, v->var->name, len))
			h++;
	
	return h;
}

/*
	CVar_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
char	**
Cvar_CompleteBuildList (char *partial)
{
	cvar_list_t	*v;
	int			len = 0;
	int			bpos = 0;
	int			sizeofbuf = (Cvar_CompleteCountPossible (partial) + 1) * sizeof (char *);
	char		**buf;

	len = strlen(partial);
	buf = malloc(sizeofbuf + sizeof (char *));
	// Loop through the alias list and print all matches
	for (v = cvars; v; v = v->next)
		if (!strncasecmp(partial, v->var->name, len))
			buf[bpos++] = v->var->name;

	buf[bpos] = NULL;
	return buf;
}	

// FIXME: Replace this mess with a bash-style complete
char *
Cvar_TabComplete (const char *partial)
{
	cvar_list_t	   *v;
	int				len;

	len = strlen (partial);
	if (!len)
		return NULL;	// nothing to complete

	v = cvars;
	while (v)	// if it's an exact match, leave it alone
	{
		if (strcasecmp (partial, v->var->name) == 0)
			return v->var->name;
		v = v->next;
	}

	v = cvars;
	while (v)   // if it's a partial match, find the first match
	{
		if (strncmp (partial, v->var->name, len) == 0)
			return v->var->name;
		v = v->next;
	}

	return NULL;	// no match
}


void
Cvar_Cleanup (void)
{
	cvar_list_t	   *v, *t;

	// clean cvars with the CVAR_TEMP flag
	v = cvars;
	while (v)
	{
		// if it's not a temporary cvar, skip it
		if (! (v->var->flags & CVAR_TEMP))
		{
			v = v->next;
			continue;
		}

		t = v;
		v = v->next;
		if (t->var)
		{
			if (t->var->string)
				Z_Free (t->var->string);
			Z_Free (t->var);
		}
		Z_Free (t);
	}
}


void
Cvar_WriteVars (FILE *f)
{
	cvar_list_t	   *v;

	v = cvars;
	while (v)
	{
		// Can't use set in legacy NQ/QW
//		fprintf (f, "set %s \"%s\"\n", v->var->name, v->var->string);
		if (v->var->flags & CVAR_ARCHIVE)
			fprintf (f, "%s \"%s\"\n", v->var->name, v->var->string);
		v = v->next;
	}
}


struct cvar_foreach_s *
Cvar_ForeachStart (void)
{
	return cvars;
}


cvar_t *
Cvar_ForeachNext (struct cvar_foreach_s *id)
{
	cvar_t	   *var;
	if (id)
	{
		var = id->var;
		id = id->next;
		return var;
	} else
		return NULL;
}


void
Cvar_ForeachEnd (struct cvar_foreach_s *id)
{
	id = NULL;
}

