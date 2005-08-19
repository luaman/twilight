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
#include <stdarg.h>

#include "quakedef.h"
#include "strlib.h"
#include "host.h"
#include "server.h"
#include "sys.h"
#include "cvar.h"

typedef struct
{
	Uint			s;
	dfunction_t		*f;
}
prstack_t;

#define MAX_STACK_DEPTH 32
static prstack_t pr_stack[MAX_STACK_DEPTH];
static int pr_depth;

#define LOCALSTACK_SIZE 2048
static int localstack[LOCALSTACK_SIZE];
static int localstack_used;


qboolean pr_trace;
dfunction_t *pr_xfunction;
static int pr_xstatement;


Uint pr_argc;

static char *pr_opnames[] =
{
	"DONE",

	"MUL_F",
	"MUL_V",
	"MUL_FV",
	"MUL_VF",

	"DIV",

	"ADD_F",
	"ADD_V",

	"SUB_F",
	"SUB_V",

	"EQ_F",
	"EQ_V",
	"EQ_S",
	"EQ_E",
	"EQ_FNC",

	"NE_F",
	"NE_V",
	"NE_S",
	"NE_E",
	"NE_FNC",

	"LE",
	"GE",
	"LT",
	"GT",

	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",

	"ADDRESS",

	"STORE_F",
	"STORE_V",
	"STORE_S",
	"STORE_ENT",
	"STORE_FLD",
	"STORE_FNC",

	"STOREP_F",
	"STOREP_V",
	"STOREP_S",
	"STOREP_ENT",
	"STOREP_FLD",
	"STOREP_FNC",

	"RETURN",

	"NOT_F",
	"NOT_V",
	"NOT_S",
	"NOT_ENT",
	"NOT_FNC",

	"IF",
	"IFNOT",

	"CALL0",
	"CALL1",
	"CALL2",
	"CALL3",
	"CALL4",
	"CALL5",
	"CALL6",
	"CALL7",
	"CALL8",

	"STATE",

	"GOTO",

	"AND",
	"OR",

	"BITAND",
	"BITOR"
};

char *PR_GlobalString (int ofs);
char *PR_GlobalStringNoContents (int ofs);


//=============================================================================

static void
PR_PrintStatement (dstatement_t *s)
{
	int         i;

	if ((unsigned int) s->op < sizeof (pr_opnames) / sizeof (pr_opnames[0])) {
		Com_Printf ("%s ", pr_opnames[s->op]);
		i = strlen (pr_opnames[s->op]);
		for (; i < 10; i++)
			Com_Printf (" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
		Com_Printf ("%sbranch %i", PR_GlobalString ((unsigned int) s->a), s->b);
	else if (s->op == OP_GOTO) {
		Com_Printf ("branch %i", s->a);
	} else if ((unsigned int) (s->op - OP_STORE_F) < 6) {
		Com_Printf ("%s", PR_GlobalString ((unsigned int) s->a));
		Com_Printf ("%s", PR_GlobalStringNoContents ((unsigned int) s->b));
	} else {
		if (s->a)
			Com_Printf ("%s", PR_GlobalString ((unsigned int) s->a));
		if (s->b)
			Com_Printf ("%s", PR_GlobalString ((unsigned int) s->b));
		if (s->c)
			Com_Printf ("%s", PR_GlobalStringNoContents ((unsigned int) s->c));
	}
	Com_Printf ("\n");
}

static void
PR_StackTrace (void)
{
	dfunction_t	*f;
	int			i;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	for (i = pr_depth;i > 0;i--)
	{
		f = pr_stack[i].f;

		if (!f)
			Com_Printf ("<NULL FUNCTION>\n");
		else
			Com_Printf ("%12s : %s : statement %i\n", PRVM_GetString(f->s_file), PRVM_GetString(f->s_name), pr_stack[i].s - f->first_statement);
	}
}

void
PR_Profile_f (void)
{
	dfunction_t		*f, *best;
	Uint			max, num, i;

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i = 0; i < progs->numfunctions; i++)
		{
			f = &pr_functions[i];
			if (f->profile > max)
			{
				max = f->profile;
				best = f;
			}
		}
		if (best)
		{
			if (num < 10)
				Com_Printf ("%7i %s\n", best->profile,
							PRVM_GetString(best->s_name));
			num++;
			best->profile = 0;
		}
	}
	while (best);
}


/*
============
Aborts the currently executing function
============
*/
void PR_RunError (char *error, ...)
{
	int		i;
	va_list	argptr;
	char	string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	if (pr_xfunction)
	{
		for (i = -4; i <= 0; i++)
			if (pr_xstatement + i >= pr_xfunction->first_statement)
				PR_PrintStatement (pr_statements + pr_xstatement + i);
	}
	else
		Com_Printf ("null function executing??\n");
	PR_StackTrace ();

	Com_Printf ("%s\n", string);

	pr_depth = 0;	// dump the stack so host_error can shutdown functions

	Host_Error ("Program error");
}

/*
============================================================================
The interpretation main loop
============================================================================
*/

/*
====================
Returns the new program statement counter
====================
*/
static int
PR_EnterFunction (dfunction_t *f)
{
	Uint		i, j, c, o;

	if (!f)
		PR_RunError ("PR_EnterFunction: NULL function\n");

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
		PR_RunError ("stack overflow");

	// save off any locals that the new function steps on
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError ("PR_ExecuteProgram: locals stack overflow\n");

	for (i = 0; i < c; i++)
		localstack[localstack_used + i] =
			((int *) pr_globals)[f->parm_start + i];
	localstack_used += c;

	// copy parameters
	o = f->parm_start;
	for (i = 0; i < f->numparms; i++) {
		for (j = 0; j < f->parm_size[i]; j++) {
			((int *) pr_globals)[o] =
				((int *) pr_globals)[OFS_PARM0 + i * 3 + j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1;		// offset the s++
}

static int
PR_LeaveFunction (void)
{
	int	i, c;

	if (pr_depth <= 0)
		Sys_Error ("prog stack underflow");

	// restore locals from the stack
	c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		PR_RunError ("PR_ExecuteProgram: locals stack underflow\n");

	for (i = 0; i < c; i++)
		((int *) pr_globals)[pr_xfunction->parm_start + i] =
			localstack[localstack_used + i];

	// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}


// LordHavoc: optimized
#define OPA ((eval_t *)&pr_globals[(unsigned short int) st->a])
#define OPB ((eval_t *)&pr_globals[(unsigned short int) st->b])
#define OPC ((eval_t *)&pr_globals[(unsigned short int) st->c])
void
PR_ExecuteProgram (func_t fnum, const char *errormessage)
{
	dstatement_t	*st;
	dfunction_t		*f, *newf;
	edict_t			*ed;
	eval_t			*ptr;
	int				profile, startprofile, cachedpr_trace, exitdepth;

	if (!fnum || fnum >= progs->numfunctions)
	{
		if (pr_global_struct->self)
			ED_Print (PROG_TO_EDICT (pr_global_struct->self));
		Sys_Error ("PR_ExecuteProgram: %s", errormessage);
	}

	f = &pr_functions[fnum];

	pr_trace = false;

	// we know we're done when pr_depth drops to this
	exitdepth = pr_depth;

	// make a stack frame
	st = &pr_statements[PR_EnterFunction (f)];
	startprofile = profile = 0;

chooseexecprogram:
	cachedpr_trace = pr_trace;
	if (pr_boundscheck->ivalue)
	{
#define PRBOUNDSCHECK 1
		if (pr_trace)
		{
#define PRTRACE 1
#include "pr_execprogram.h"
		}
		else
		{
#undef PRTRACE
#include "pr_execprogram.h"
		}
	}
	else
	{
#undef PRBOUNDSCHECK
		if (pr_trace)
		{
#define PRTRACE 1
#include "pr_execprogram.h"
		}
		else
		{
#undef PRTRACE
#include "pr_execprogram.h"
		}
	}
}

