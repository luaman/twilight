/*
	$Id$

	Copyright (C) 2001  Joseph Carter

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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "quakedef.h"
#include "cvar.h"

typedef struct cvar_foreach_s {
	cvar_t					   *var;
	struct cvar_foreach_s	   *next;
} cvar_list_t;

static cvar_list_t *cvars;

cvar_t *developer;

void
Cvar_Init (void)
{
	cvars = NULL;

	developer = Cvar_Get ("developer", "0", CVAR_NONE, NULL);

	Cmd_AddCommand ("set", &Cvar_Set_f);
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
		var->name = Z_Malloc (Q_strlen (name) + 1);
		Q_strcpy (var->name, name);
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
		if (Q_strcasecmp (value, var->string) == 0)
			return;
		Z_Free (var->string);
	}

	var->string = Z_Malloc (Q_strlen(value) + 1);
	Q_strcpy (var->string, value);
	
	var->value = Q_atof (var->string);
	if (var->callback)
		var->callback (var);
	
	// NQism taken from NQ code
	if (var->flags & CVAR_USERINFO)
	{
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name,
					var->string);
	}
}

void
Cvar_Set_f (void)
{
	cvar_t	*var;

	if (Cmd_Argc () < 2 || Cmd_Argc () > 3)
	{
		Con_Printf ("usage: set <Cvar> [value]\n");
		return;
	}

	var = Cvar_Find (Cmd_Argv (1));
	if (!var)
	{
		Con_Printf ("Cvar \"%s\" does not exist.\n", Cmd_Argv (1));
		return;
	}

	if (Cmd_Argc () == 3)
	{
		if (!(var->flags & CVAR_ROM))
			Cvar_Set (var, Cmd_Argv (2));
		else
			Con_Printf ("Cvar \"%s\" is read-only.\n", var->name);
	} else
		Cvar_Show (var);

	return;
}


void
Cvar_Slide (cvar_t *var, const float change)
{
	static char		buf[128];

	var->value += change;
	Z_Free (var->string);
	Q_snprintf (buf, 128, "%f", var->value);
	var->string = Z_Malloc (Q_strlen (buf) + 1);
	Q_strcpy (var->string, buf);

	if (var->callback)
		var->callback (var);
}


void
Cvar_Show (cvar_t *var)
{
	if (!var)
	{
		Con_Printf ("Cvar does not exist.\n");
		return;
	}

	Con_Printf ("[] \"%s\" is \"%s\"\n", var->name, var->string);
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
			Con_Printf ("Cvar \"%s\" is read-only.\n", var->name);
	} else
		Cvar_Show (var);

	return true;
}


cvar_t *
Cvar_Find (const char *name)
{
	cvar_list_t	   *v = cvars;

	while (v)
	{
		if (Q_strcasecmp (name, v->var->name) == 0)
			return v->var;
		v = v->next;
	}

	return NULL;	// Cvar doesn't exist
}


// FIXME: Replace this mess with a bash-style complete
char *
Cvar_TabComplete (const char *partial)
{
	cvar_list_t	   *v;
	int				len;

	len = Q_strlen (partial);
	if (!len)
		return NULL;	// nothing to complete

	v = cvars;
	while (v)	// if it's an exact match, leave it alone
	{
		if (Q_strcasecmp (partial, v->var->name) == 0)
			return v->var->name;
		v = v->next;
	}

	v = cvars;
	while (v)   // if it's a partial match, find the first match
	{
		if (Q_strncmp (partial, v->var->name, len) == 0)
			return v->var->name;
		v = v->next;
	}

	return NULL;	// no match
}


void
Cvar_Cleanup (void)
{
}


void
Cvar_Archive (FILE *f)
{
	cvar_list_t	   *v;

	v = cvars;
	while (v)
	{
		// Can't use set in legacy NQ/QW
//		fprintf (f, "set %s \"%s\"\n", v->var->name, v->var->string);
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

