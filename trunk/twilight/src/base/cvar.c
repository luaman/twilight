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

#include <stdlib.h>		/* For malloc() */

#include "qtypes.h"
#include "cmd.h"
#include "common.h"
#include "cvar.h"
#include "strlib.h"
#include "zone.h"
#include "rw_ops.h"

static void Cvar_Show (cvar_t *var);
static void Cvar_Set_f (void);
static void Cvar_Reset_f (void);

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

	Cmd_AddCommand ("set", &Cvar_Set_f);
	Cmd_AddCommand ("reset", &Cvar_Reset_f);
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
			if (t->var->svalue)
				Z_Free (t->var->svalue);
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
Cvar_Get (const char *name, const char *svalue, const int flags,
		const cvar_callback callback)
{
	cvar_t	   *var;

	var = Cvar_Find (name);

	if (var)
	{
		// var exists, update flags and callback and leave it alone
		var->flags = flags;
		var->callback = callback;
		if (var->callback)
			var->callback (var);
		return var;
	}

	// Var does not exist, create it
	var = Z_Malloc (sizeof(cvar_t));
	var->name = Z_Malloc (strlen (name) + 1);
	strcpy (var->name, name);
	var->svalue = NULL;					// force Cvar to change
	var->callback = callback;
	var->initval = Z_Malloc (strlen(svalue) + 1);
	strcpy (var->initval, svalue);
	var->flags = flags;
	Cvar_InsertVar (var);
	Cvar_Set (var, svalue);

	return var;
}


void
Cvar_Set (cvar_t *var, const char *svalue)
{
	if (var->svalue)
	{
		if (strcasecmp (svalue, var->svalue) == 0)
			return;
		Z_Free (var->svalue);
	}

	var->s_len = strlen(svalue);
	var->svalue = Z_Malloc (var->s_len + 1);
	strcpy (var->svalue, svalue);
	
	var->fvalue = Q_atof (var->svalue);
	var->ivalue = Q_atoi (var->svalue);

	if (var->callback)
		var->callback (var);
	
	if (engine_callback)
		engine_callback (var);
}

static void
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

static void
Cvar_Reset_f (void)
{
	cvar_t	   *var;

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("usage: reset <Cvar>\n");
		return;
	}

	var = Cvar_Find (Cmd_Argv(1));

	if (!var)
	{
		Com_Printf ("Cvar \"%s\" does not exist.\n", Cmd_Argv (1));
		return;
	}

	if (var->flags & CVAR_ROM)
	{
		Com_Printf ("Cvar \"%s\" is read-only.\n", var->name);
		return;
	}

	Cvar_Set (var, var->initval);
}

cvar_t *
Cvar_CreateTemp (const char *name, const char *value)
{
	cvar_t		   *var;

	var = Cvar_Find (name);
	if (var)
		// the cvar already exists, and this shouldn't append
		return NULL;

	// Var does not exist, create it
	var = Z_Malloc (sizeof(cvar_t));
	var->name = Z_Malloc (strlen (name) + 1);
	strcpy (var->name, name);
	var->svalue = NULL;					// force Cvar to change
	var->callback = NULL;
	Cvar_InsertVar (var);
	Cvar_Set (var, value);

	var->flags = CVAR_TEMP;
	return var;
}


void
Cvar_Slide (cvar_t *var, const float change)
{
	Cvar_Set (var, va ("%f", var->fvalue + change));
}

// NOTE: This function must match the flags in cvar.h
static char *
Cvar_FlagString (cvar_t *var)
{
	static char		str[6];
	int				i = 0;

	if (var)
	{
		if (var->flags & CVAR_ARCHIVE)
			str[i++] = 'a';

		if (var->flags & CVAR_ROM)
			str[i++] = 'r';

		if (var->flags & CVAR_USER)
			str[i++] = 'u';
		else if (var->flags & CVAR_TEMP)
			str[i++] = 't';

		if (var->flags & (CVAR_USERINFO|CVAR_SERVERINFO))
			str[i++] = 'i';
	}
	
	str[i] = '\0';
	return str;
}

void
Cvar_Show (cvar_t *var)
{
	if (!var)
	{
		Com_Printf ("Cvar does not exist.\n");
		return;
	}

	Com_Printf ("[%s] \"%s\" is \"%s\" (default: \"%s\")\n",
			Cvar_FlagString (var), var->name, var->svalue, var->initval);
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
Cvar_CompleteCountPossible (const char *partial)
{
	cvar_list_t	*v;
	size_t		len;
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
const char	**
Cvar_CompleteBuildList (const char *partial)
{
	cvar_list_t	*v;
	size_t		len = 0;
	int			bpos = 0;
	int			sizeofbuf = (Cvar_CompleteCountPossible (partial) + 1) * sizeof (char *);
	const char	**buf;

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
const char *
Cvar_TabComplete (const char *partial)
{
	cvar_list_t	   *v;
	size_t			len;

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


#if 0
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
			if (t->var->svalue)
				Z_Free (t->var->svalue);
			Z_Free (t->var);
		}
		Z_Free (t);
	}
}
#endif


void
Cvar_WriteVars (SDL_RWops *rw)
{
	cvar_list_t	   *v;

	v = cvars;
	while (v)
	{
		if (v->var->flags & CVAR_ARCHIVE)
			RWprintf (rw, "%s \"%s\"\n", v->var->name, v->var->svalue);
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

